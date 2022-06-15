#pragma once
#include "bmp.hpp"
#include "display.hpp"
#include "file.hpp"
#include "lcd.hpp"

#include <cstdio>
#include <functional>
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
size_t CountFiles(const char* directory, UnaryPredicate pred) {
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
  struct State {
    std::uint32_t pixels_filled;
    std::uint32_t rows_read;
  };

 public:
  explicit DisplayGuard(Display& display) noexcept;

  void Activate() noexcept;
  void Refresh() noexcept;
  void Draw(bmp::Rgb666 pixel) noexcept;

  [[nodiscard]] bool IsFilled() noexcept;
  [[nodiscard]] bool IsAllRowsRead() noexcept;

  [[nodiscard]] bool NotifyFillPixel() noexcept;
  [[nodiscard]] bool NotifyReadRow() noexcept;

 private:
  Display& m_display;
  bool m_active{false};
  State m_state{};
};
}  // namespace pv