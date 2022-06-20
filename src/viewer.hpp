#pragma once
#include "bmp.hpp"
#include "display.hpp"
#include "file.hpp"
#include "lcd.hpp"
#include "receiver.hpp"
#include "transmitter.hpp"

#include <array>
#include <cstdio>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>

namespace pv {
inline constexpr auto* IMAGE_ROOT{R"(\)"};
inline constexpr auto* IMAGE_EXTENSION{"bmp"};

inline constexpr std::size_t COMMAND_QUEUE_SIZE{64};
inline constexpr std::size_t COMMAND_TIMESLICE{8};

inline constexpr std::size_t PIXEL_QUEUE_SIZE{lcd::Panel::PIXEL_HORIZONTAL *
                                              sizeof(bmp::Rgb666)};
inline constexpr std::size_t PIXEL_TIMESLICE{lcd::Panel::PIXEL_HORIZONTAL};

struct Image {
  fs::File file;
  bmp::Image bitmap;
};

int EventLoop(fs::CyclicDirectoryIterator dir_it) noexcept;

std::optional<Image> TryOpenImageFile(const fs::DirectoryEntry& entry) noexcept;
bool IsSupportedImageFile(const fs::DirectoryEntry& entry) noexcept;

template <class UnaryPredicate>
size_t CountFiles(const char* directory, UnaryPredicate pred) noexcept {
  size_t count{0};
  for (fs::DirectoryIterator dir_it{directory};
       dir_it != fs::DirectoryIterator{}; ++dir_it) {
    if (dir_it->IsRegularFile() && std::invoke(pred, *dir_it)) {
      ++count;
    }
  }
  return count;
}

template <
    class DirIt,
    class UnaryPredicate,
    std::enable_if_t<std::is_base_of_v<fs::DirectoryIterator, DirIt>, int> = 0>
DirIt& FindNextFile(DirIt& dir_it, UnaryPredicate pred) noexcept {
  for (++dir_it; dir_it != DirIt{}; ++dir_it) {
    if (const fs::DirectoryEntry& entry = *dir_it;
        entry.IsRegularFile() && std::invoke(pred, entry)) {
      break;
    }
  }
  return dir_it;
}

class DisplayGuard {
 public:
  explicit DisplayGuard(Display& display) noexcept;

  void Activate() noexcept;
  void Refresh() noexcept;
  void Draw(bmp::Rgb666 pixel) noexcept;

  void NotifyFillPixel() noexcept;
  [[nodiscard]] bool IsFilled() noexcept;

 private:
  Display& m_display;
  bool m_active{false};
  std::size_t m_pixels_filled{};
};

class ListenerGuard {
 public:
  ListenerGuard(io::IListener& listener, io::Receiver& receiver) noexcept;
  ListenerGuard(const ListenerGuard&) = delete;
  ListenerGuard(ListenerGuard&&) = delete;
  ListenerGuard& operator=(const ListenerGuard&) = delete;
  ListenerGuard& operator=(ListenerGuard&&) = delete;
  ~ListenerGuard() noexcept;

 private:
  io::Receiver& m_receiver;
};

class PixelPart {
  using pixel_t = bmp::Rgb666;

 public:
  PixelPart() noexcept;
  PixelPart(const PixelPart&) = delete;
  PixelPart(PixelPart&&) = delete;
  PixelPart& operator=(const PixelPart&) = delete;
  PixelPart& operator=(PixelPart&&) = delete;
  ~PixelPart() = default;

  template <class CircularBuffer>
  bool Update(CircularBuffer& cb) noexcept {
    m_bytes_updated += cb.consume(std::data(m_pixel) + m_bytes_updated,
                                  sizeof(pixel_t) - m_bytes_updated);
    return m_bytes_updated == sizeof(pixel_t);
  }

  pixel_t Get() const& noexcept;
  pixel_t Get() && noexcept;

 private:
  std::array<std::byte, sizeof(pixel_t)> m_pixel;
  std::uint8_t m_bytes_updated;
};

class ImageSender {
 public:
  enum class Status { Completed, InProgress, IoError };

 private:
  using pixel_t = bmp::Bgr888;
  using pixel_row_t = std::array<pixel_t, lcd::Panel::PIXEL_HORIZONTAL>;

 public:
  ImageSender(Image& image) noexcept;
  ImageSender(const ImageSender&) = delete;
  ImageSender(ImageSender&&) = delete;
  ImageSender& operator=(const ImageSender&) = delete;
  ImageSender& operator=(ImageSender&&) = delete;
  ~ImageSender() = default;

  Status Transmit(io::Transmitter& transmitter) noexcept;

 private:
  Image& m_image;
  std::optional<pixel_row_t> m_row;
  std::size_t m_rows_idx{0};
};
}  // namespace pv