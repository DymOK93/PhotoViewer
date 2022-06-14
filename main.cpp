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
#define EXIT_ON_TRUE(cond) \
  if (cond) {              \
    result = EXIT_FAILURE; \
    break;                 \
  }

#define EXIT_ON_FALSE(cond) \
  if (!cond) {              \
    result = EXIT_FAILURE;  \
    break;                  \
  }

int EventLoop(fs::CyclicDirectoryIterator dir_it) noexcept {
  pv::RequestParser<pv::COMMAND_QUEUE_SIZE, pv::PIXEL_QUEUE_SIZE> parser;
  io::Receiver::GetInstance().Listen(addressof(parser));
  auto& cmd_queue{parser.GetCommands()};
  auto& pixel_queue{parser.GetData()};

  auto& transmitter{io::Transmitter::GetInstance()};
  auto& command_manager{cmd::CommandManager::GetInstance()};
  auto& display{pv::Display::GetInstance()};

  optional<Image> image;

  int result{EXIT_SUCCESS};
  bool display_on{false};

  for (;;) {
    bool cmd_success{true};
    for (size_t idx = 0; idx < COMMAND_TIMESLICE && !empty(cmd_queue); ++idx) {
      const auto command{cmd_queue.consume()};
      command_manager.Execute(command, [&](cmd::NextPictureTag) {
        /*cmd_success =
            FindNextFile(dir_it, [&image](const fs::DirectoryEntry& entry) {
              image = TryOpenImageFile(entry);
              return image.has_value();
            }) != fs::CyclicDirectoryIterator{};

        if (!cmd_success) {
          return;
        }

        display.Refresh();
        for (size_t y = 0; y < lcd::Panel::PIXEL_VERTICAL; ++y) {
          bmp::Bgr888 row[lcd::Panel::PIXEL_HORIZONTAL];
          const auto bytes_read{
              image->file.Read(reinterpret_cast<byte*>(row), sizeof row)};
          if (!bytes_read) {
            cmd_success = false;
            return;
          }

          for (uint32_t x = 0; x < lcd::Panel::PIXEL_HORIZONTAL; ++x) {
            const bmp::Bgr888 color{row[lcd::Panel::PIXEL_HORIZONTAL - x - 1]};
            const uint16_t red = (color.red & 0b11111100) << 8;
            const uint16_t green = (color.green & 0b11111100);
            const uint16_t blue = color.blue << 8;
            display.Draw({static_cast<uint16_t>(red | green), blue});
          }
        }
        if (!display_on) {
          display.Show(true);
          display_on = true;
        }*/
      });
    }
    EXIT_ON_FALSE(cmd_success);

    /*for (size_t idx = 0;
         idx < PIXEL_TIMESLICE && size(pixel_queue) >= sizeof(bmp::Rgb666);
         ++idx) {
      bmp::Rgb666 pixel;
      pixel_queue.consume(reinterpret_cast<std::byte*>(addressof(pixel)),
                          sizeof(bmp::Rgb666));
      display.Draw(pixel);
    }*/

    /*if (transmitter.GetRemainingQueueSize() >= lcd::Panel::PIXEL_HORIZONTAL) {
      bmp::Bgr888 row[lcd::Panel::PIXEL_HORIZONTAL];
      const auto image_width{image->GetWidth()};
      EXIT_ON_TRUE(image_width > size(row));

      const auto bytes_read{
          file.Read(reinterpret_cast<byte*>(row), sizeof row)};
      EXIT_ON_FALSE(bytes_read);

      for (uint16_t idx = 0; idx < image_width; ++idx) {
        const bmp::Bgr888 pixel{row[image_width - idx - 1]};
        transmitter.SendData(reinterpret_cast<const byte*>(addressof(pixel)),
                             sizeof(bmp::Bgr888));
      }
    }*/
  }
  return result;
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
}  // namespace pv