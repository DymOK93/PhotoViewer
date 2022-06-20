#include "lcd.hpp"
#include "gpio.hpp"

#include <cstring>

using namespace std;

namespace lcd {
Panel::Panel() noexcept : m_bus{setup_bus()} {
  prepare_control_gpio();
  prepare_data_gpio();
  read_id();
  initialize();
}

void Panel::BackLightControl(bool on) const noexcept {
  auto& gpio{gpio::ChannelManager::GetInstance().Get<gpio::Channel::F>()};
  if (on) {
    gpio.BSRR = GPIO_BSRR_BS_5;
  } else {
    gpio.BSRR = GPIO_BSRR_BR_5;
  }
}

Panel& Panel::SendCommand(Command cmd) noexcept {
  m_bus[RS_PIN] = static_cast<uint16_t>(cmd);
  return *this;
}

uint16_t Panel::Read(size_t skip_count) const noexcept {
  skip(skip_count);
  return m_bus[DATA_IDX];
}

void Panel::Read(uint16_t* values,
                 size_t count,
                 size_t skip_count) const noexcept {
  skip(skip_count);
  while (count--) {
    *values++ = m_bus[DATA_IDX];
  }
}

Panel& Panel::Write(uint16_t value) noexcept {
  m_bus[DATA_IDX] = value;
  return *this;
}

Panel& Panel::Write(const uint16_t* values, size_t count) noexcept {
  while (count--) {
    Write(*values++);
  }
  return *this;
}

auto Panel::GetId() const noexcept -> const Id& {
  return m_id;
}

void Panel::skip(size_t skip_count) const noexcept {
  while (skip_count--) {
    [[maybe_unused]] const uint16_t dummy_value{m_bus[DATA_IDX]};
  }
}

void Panel::read_id() noexcept {
  std::array<uint16_t, 3> raw_id;
  SendCommand(Command::ReadId);
  Read(data(raw_id), size(raw_id), 1);
  m_id = parse_id(raw_id);
}

void Panel::initialize() noexcept {
  SendCommand(Command::ColorMode);
  Write(0x5);  // 16-bit color (RGB565)
  SendCommand(Command::InversionOn);
}

auto Panel::setup_bus() noexcept -> bus_t {
  auto bus{
      fsmc::Controller::GetInstance().GetMemoryBank<FSMC_BANK, uint16_t>()};

  auto& config{bus.GetConfig().BTCR[0]};
  config = (config & BUS_CONFIG_RESERVED_MASK) | BUS_CONFIG;

  auto& timings{bus.GetConfig().BTCR[1]};
  timings = (timings & BUS_TIMINGS_RESERVED_MASK) | BUS_TIMINGS;

  return bus;
}

void Panel::prepare_control_gpio() noexcept {
  auto& channel_manager{gpio::ChannelManager::GetInstance()};

  auto& gpio_d{channel_manager.Activate<gpio::Channel::D>()};
  SET_BIT(gpio_d.MODER,
          GPIO_MODER_MODER4_1           // RD
              | GPIO_MODER_MODER5_1     // WR
              | GPIO_MODER_MODER7_1     // CS
              | GPIO_MODER_MODER11_0);  // RESET - GP output
  SET_BIT(gpio_d.OSPEEDR,
          GPIO_OSPEEDER_OSPEEDR4                // RD
              | GPIO_OSPEEDER_OSPEEDR5          // WR
              | GPIO_OSPEEDER_OSPEEDR7          // CS
              | GPIO_OSPEEDER_OSPEEDR11);       // RESET
  SET_BIT(gpio_d.PUPDR, GPIO_PUPDR_PUPDR11_0);  // RESET - active low
  SET_BIT(gpio_d.AFR[0], 0xC0CC0000);
  gpio_d.BSRR = GPIO_BSRR_BS_11;  // Set RESET to HIGH

  auto& gpio_f{channel_manager.Activate<gpio::Channel::F>()};
  SET_BIT(gpio_f.MODER,
          GPIO_MODER_MODER0_1                       // RS - AF
              | GPIO_MODER_MODER5_0);               // Backlight - GP output
  SET_BIT(gpio_f.OSPEEDR, GPIO_OSPEEDER_OSPEEDR0);  // RS is high-speed (100MHz)
  SET_BIT(gpio_f.AFR[0], 0x0000000C);

  //  LCD_TE uses default input mode (0x0)
  auto& gpio_g{channel_manager.Activate<gpio::Channel::G>()};
  SET_BIT(gpio_g.OSPEEDR, GPIO_OSPEEDER_OSPEEDR4);
}

void Panel::prepare_data_gpio() noexcept {
  auto& channel_manager{gpio::ChannelManager::GetInstance()};

  auto& gpio_d{channel_manager.Get<gpio::Channel::D>()};
  SET_BIT(gpio_d.MODER, GPIO_MODER_MODER14_1               // D0
                            | GPIO_MODER_MODER15_1         // D1
                            | GPIO_MODER_MODER0_1          // D2
                            | GPIO_MODER_MODER1_1          // D3
                            | GPIO_MODER_MODER8_1          // D13
                            | GPIO_MODER_MODER9_1          // D14
                            | GPIO_MODER_MODER10_1);       // D15
  SET_BIT(gpio_d.OSPEEDR, GPIO_OSPEEDER_OSPEEDR14          // D0
                              | GPIO_OSPEEDER_OSPEEDR15    // D1
                              | GPIO_OSPEEDER_OSPEEDR0     // D2
                              | GPIO_OSPEEDER_OSPEEDR1     // D3
                              | GPIO_OSPEEDER_OSPEEDR8     // D13
                              | GPIO_OSPEEDER_OSPEEDR9     // D14
                              | GPIO_OSPEEDER_OSPEEDR10);  // D15
  SET_BIT(gpio_d.AFR[0], 0x000000CC);
  SET_BIT(gpio_d.AFR[1], 0xCC000CCC);

  auto& gpio_e{channel_manager.Activate<gpio::Channel::E>()};
  SET_BIT(gpio_e.MODER, GPIO_MODER_MODER7_1                // D4
                            | GPIO_MODER_MODER8_1          // D5
                            | GPIO_MODER_MODER9_1          // D6
                            | GPIO_MODER_MODER10_1         // D7
                            | GPIO_MODER_MODER11_1         // D8
                            | GPIO_MODER_MODER12_1         // D9
                            | GPIO_MODER_MODER13_1         // D10
                            | GPIO_MODER_MODER14_1         // D11
                            | GPIO_MODER_MODER15_1);       // D12
  SET_BIT(gpio_e.OSPEEDR, GPIO_OSPEEDER_OSPEEDR7           // D4
                              | GPIO_OSPEEDER_OSPEEDR8     // D5
                              | GPIO_OSPEEDER_OSPEEDR9     // D6
                              | GPIO_OSPEEDER_OSPEEDR10    // D7
                              | GPIO_OSPEEDER_OSPEEDR11    // D8
                              | GPIO_OSPEEDER_OSPEEDR12    // D9
                              | GPIO_OSPEEDER_OSPEEDR13    // D10
                              | GPIO_OSPEEDER_OSPEEDR14    // D11
                              | GPIO_OSPEEDER_OSPEEDR15);  // D12
  SET_BIT(gpio_e.AFR[0], 0xC0000000);
  SET_BIT(gpio_e.AFR[1], 0xCCCCCCCC);
}

auto Panel::parse_id(const std::array<uint16_t, 3>& raw_id) noexcept -> Id {
  return {static_cast<uint8_t>(raw_id[0]), static_cast<uint8_t>(raw_id[1]),
          static_cast<uint8_t>(raw_id[2])};
}
}  // namespace lcd