#include "main.hpp"
#include "break_on.hpp"
#include "command.hpp"
#include "display.hpp"
#include "request_parser.hpp"
#include "transmitter.hpp"

#include <array>
#include <cstring>
#include <utility>

using namespace std;

int main() {
  do {
    const fs::LogicalDrive drive{nullopt, false};
    BREAK_ON_FALSE(drive);

    const size_t image_count{
        pv::CountFiles(pv::IMAGE_ROOT, [](const fs::DirectoryEntry& entry) {
          return pv::IsSupportedImageFile(entry);
        })};
    BREAK_ON_FALSE(image_count > 0);

    fs::CyclicDirectoryIterator dir_it{pv::IMAGE_ROOT};
    BREAK_ON_FALSE(dir_it != fs::CyclicDirectoryIterator{});

    return pv::EventLoop(move(dir_it));

  } while (false);

  return EXIT_FAILURE;
}

namespace pv {
int EventLoop(fs::CyclicDirectoryIterator dir_it) noexcept {
  pv::RequestParser<pv::COMMAND_QUEUE_SIZE, pv::PIXEL_QUEUE_SIZE> parser;
  ListenerGuard listener_guard{parser, io::Receiver::GetInstance()};

  auto& cmd_queue{parser.GetCommands()};
  auto& pixel_queue{parser.GetData()};

  auto& transmitter{io::Transmitter::GetInstance()};
  auto& command_manager{cmd::CommandManager::GetInstance()};
  const auto command_flusher{[&command_manager, &transmitter]() {
    command_manager.Flush(transmitter);
  }};

  DisplayGuard display{pv::Display::GetInstance()};
  optional<Image> image;

  display.Activate();

  for (;;) {
    command_flusher();

    bool cmd_success{true};
    for (size_t idx = 0; idx < COMMAND_TIMESLICE && !empty(cmd_queue); ++idx) {
      const auto command{cmd_queue.consume()};
      command_manager.Execute(command, [&](cmd::NextPictureTag) {
        if (FindNextFile(dir_it, [&image](const fs::DirectoryEntry& entry) {
              image = TryOpenImageFile(entry);
              return image.has_value();
            }) != fs::CyclicDirectoryIterator{}) {
          display.Refresh();
        } else {
          cmd_success = false;
        }
      });
      command_flusher();
    }
    if (!cmd_success) {
      return EXIT_FAILURE;
    }

    if (!display.IsFilled()) {
      for (size_t idx = 0;
           idx < PIXEL_TIMESLICE && size(pixel_queue) >= sizeof(bmp::Rgb666) &&
           display.NotifyFillPixel();
           ++idx) {
        bmp::Rgb666 pixel;
        pixel_queue.consume(reinterpret_cast<std::byte*>(addressof(pixel)),
                            sizeof(bmp::Rgb666));
        display.Draw(pixel);
      }
    }

    if (image.has_value() &&
        transmitter.GetRemainingQueueSize<bmp::Rgb666>() >=
            lcd::Panel::PIXEL_HORIZONTAL &&
        display.NotifyReadRow()) {
      bmp::Bgr888 row[lcd::Panel::PIXEL_HORIZONTAL];
      const auto bytes_read{
          image->file.Read(reinterpret_cast<byte*>(row), sizeof row)};
      if (bytes_read.value_or(0) != sizeof row) {
        return EXIT_FAILURE;
      }

      for (uint16_t idx = 0; idx < lcd::Panel::PIXEL_HORIZONTAL; ++idx) {
        const bmp::Bgr888 pixel{row[lcd::Panel::PIXEL_HORIZONTAL - idx - 1]};
        transmitter.SendData(reinterpret_cast<const byte*>(addressof(pixel)),
                             sizeof(bmp::Bgr888));
        command_flusher();
      }
    }
  }
  return EXIT_SUCCESS;
}

optional<Image> TryOpenImageFile(const fs::DirectoryEntry& entry) noexcept {
  do {
    BREAK_ON_TRUE(strcmp(entry.Extension(), pv::IMAGE_EXTENSION));

    fs::File file{entry.Path(), FA_READ | FA_OPEN_EXISTING};
    BREAK_ON_FALSE(file);

    const optional image{bmp::Image::FromFile(file)};
    BREAK_ON_FALSE(image.has_value());

    BREAK_ON_FALSE(image->GetWidth() == lcd::Panel::PIXEL_HORIZONTAL);
    BREAK_ON_FALSE(image->GetHeight() == lcd::Panel::PIXEL_VERTICAL);
    BREAK_ON_FALSE(file.Seek(image->GetBitmapOffset()));

    return Image{move(file), *image};

  } while (false);

  return nullopt;
}

bool IsSupportedImageFile(const fs::DirectoryEntry& entry) noexcept {
  return TryOpenImageFile(entry).has_value();
}

DisplayGuard::DisplayGuard(Display& display) noexcept : m_display{display} {}

void DisplayGuard::Activate() noexcept {
  if (!m_active) {
    m_display.Show(true);
    m_active = true;
  }
}

void DisplayGuard::Refresh() noexcept {
  m_display.Refresh();
  m_state = {};
}

void DisplayGuard::Draw(bmp::Rgb666 pixel) noexcept {
  m_display.Draw(pixel);
}

bool DisplayGuard::IsFilled() noexcept {
  return m_state.pixels_filled == lcd::Panel::PIXEL_COUNT;
}

bool DisplayGuard::IsAllRowsRead() noexcept {
  return m_state.rows_read == lcd::Panel::PIXEL_VERTICAL;
}

bool DisplayGuard::NotifyFillPixel() noexcept {
  if (IsFilled()) {
    return false;
  }
  ++m_state.pixels_filled;
  return true;
}

bool DisplayGuard::NotifyReadRow() noexcept {
  if (IsAllRowsRead()) {
    return false;
  }
  ++m_state.rows_read;
  return true;
}

ListenerGuard::ListenerGuard(io::IListener& listener,
                             io::Receiver& receiver) noexcept
    : m_receiver{receiver} {
  receiver.Listen(addressof(listener));
}

ListenerGuard::~ListenerGuard() noexcept {
  m_receiver.Listen(nullptr);
}
}  // namespace pv