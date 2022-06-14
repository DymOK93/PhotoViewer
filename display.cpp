#include "display.hpp"

using namespace std;

namespace pv {
Display::Display() noexcept {
  auto& lcd{lcd::Panel::GetInstance()};
  lcd.SendCommand(lcd::Command::WakeUp);
  setup_18bit_color(lcd);
}

void Display::Show(bool on) {
  auto& lcd{lcd::Panel::GetInstance()};
  lcd.BackLightControl(on);
  if (on) {
    lcd.SendCommand(lcd::Command::DisplayOn);
  } else {
    lcd.SendCommand(lcd::Command::DisplayOff);
  }
}

void Display::Refresh() noexcept {
  lcd::Panel::GetInstance().SendCommand(lcd::Command::WriteMemory);
}

void Display::Draw(bmp::Rgb666 pixel) noexcept {
  const uint16_t color[2]{pixel.red_green, pixel.blue};
  lcd::Panel::GetInstance().Write(color, size(color));
}

void Display::setup_18bit_color(lcd::Panel& lcd) noexcept {
  lcd.SendCommand(lcd::Command::RamControl)
      .Write(0b00000000)
      .Write(0b11110001)
      .SendCommand(lcd::Command::ColorMode)
      .Write(0b00000110);
}
}  // namespace pv