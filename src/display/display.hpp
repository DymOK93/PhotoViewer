#pragma once
#include "bmp.hpp"
#include "lcd.hpp"
#include "singleton.hpp"

#include <cstdint>

namespace pv {
class Display : public pv::Singleton<Display> {
 public:
  void Show(bool on);
  void Refresh() noexcept;
  void Draw(bmp::Rgb666 pixel) noexcept;

 private:
  friend Singleton;

  Display() noexcept;
  static void setup_18bit_color(lcd::Panel& lcd) noexcept;
};
}  // namespace pv