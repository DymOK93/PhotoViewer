#include "viewer.hpp"
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

  DisplayGuard display{pv::Display::GetInstance()};
  display.Activate();

  optional<Image> image;
  optional<ImageSender> image_sender;
  PixelPart current_pixel;

  for (;;) {
    command_manager.Flush(transmitter);

    if (!display.IsFilled() && image.has_value()) {
      for (size_t idx = 0; idx < PIXEL_TIMESLICE; ++idx) {
        if (!current_pixel.Update(pixel_queue)) {
          break;
        }
        display.Draw(move(current_pixel).Get());
        display.NotifyFillPixel();
      }
    } else {
      bool cmd_success{true};
      for (size_t idx = 0; idx < COMMAND_TIMESLICE; ++idx) {
        const auto command{cmd_queue.consume()};
        if (!command) {
          break;
        }
        command_manager.Execute(*command, [&](cmd::NextPictureTag) {
          if (FindNextFile(dir_it, [&image](const fs::DirectoryEntry& entry) {
                image = TryOpenImageFile(entry);
                return image.has_value();
              }) == fs::CyclicDirectoryIterator{}) {
            cmd_success = false;
          } else {
            display.Refresh();
            image_sender.emplace(*image);
          }
        });
      }
      if (!cmd_success) {
        return EXIT_FAILURE;
      }
    }

    if (image_sender.has_value() &&
        image_sender->Transmit(transmitter) == ImageSender::Status::IoError) {
      return EXIT_FAILURE;
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
  m_pixels_filled = 0;
}

void DisplayGuard::Draw(bmp::Rgb666 pixel) noexcept {
  m_display.Draw(pixel);
}

void DisplayGuard::NotifyFillPixel() noexcept {
  ++m_pixels_filled;
}

bool DisplayGuard::IsFilled() noexcept {
  return m_pixels_filled == lcd::Panel::PIXEL_COUNT;
}

ListenerGuard::ListenerGuard(io::IListener& listener,
                             io::Receiver& receiver) noexcept
    : m_receiver{receiver} {
  receiver.Listen(addressof(listener));
}

ListenerGuard::~ListenerGuard() noexcept {
  m_receiver.Listen(nullptr);
}

PixelPart::PixelPart() noexcept {
  new (std::data(m_pixel)) pixel_t;
}

auto PixelPart::Get() const& noexcept -> pixel_t {
  const auto as_pixel{reinterpret_cast<const pixel_t*>(std::data(m_pixel))};
  return *as_pixel;
}

auto PixelPart::Get() && noexcept -> pixel_t {
  const auto as_pixel{reinterpret_cast<const pixel_t*>(std::data(m_pixel))};
  m_bytes_updated = 0;
  return *as_pixel;
}

ImageSender::ImageSender(Image& image) noexcept : m_image{image} {
  m_row.emplace();
}

auto ImageSender::Transmit(io::Transmitter& transmitter) noexcept -> Status {
  if (m_rows_idx == lcd::Panel::PIXEL_VERTICAL) {
    return Status::Completed;
  }

  if (m_row) {
    transmitter.SendData(reinterpret_cast<byte*>(data(*m_row)),
                         sizeof(pixel_row_t));
    ++m_rows_idx;
    m_row.reset();
    return Status::InProgress;
  }

  auto& [file, image]{m_image};
  do {
    BREAK_ON_FALSE(m_rows_idx > 0 || file.Seek(image.GetBitmapOffset()));

    BREAK_ON_FALSE(file.Read(reinterpret_cast<byte*>(data(m_row.emplace())),
                             sizeof(pixel_row_t)) == sizeof(pixel_row_t));

    reverse(begin(*m_row), end(*m_row));
    return Status::InProgress;

  } while (false);

  m_row.reset();
  return Status::IoError;
}
}  // namespace pv