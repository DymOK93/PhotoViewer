#pragma once
#include "fsmc.hpp"

#include <tools/singleton.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace lcd {
enum class Command : uint16_t {
  Empty = 0x00,                     // NOP
  SoftwareReset = 0x01,             // SWRESET
  ReadId = 0x04,                    // RDDID
  ReadStatus = 0x09,                // RDDST
  ReadPowerMode = 0x0A,             // RDDPM
  ReadMadCtl = 0x0B,                // RDDMADCTL
  ReadImageMode = 0x0D,             // RDDIM
  ReadSignalMode = 0x0E,            // RDDSM
  ReadSelfDiagnosticResult = 0x0F,  // RDDSDR
  Sleep = 0x10,                     // SLPIN
  WakeUp = 0x11,                    // SLPOUT

  InversionOff = 0x20,  // INVOFF
  InversionOn = 0x21,   // INVON

  DisplayOff = 0x28,  // DISPOFF
  DisplayOn = 0x29,   // DISPON

  SetColumn = 0x2A,    // CASET
  SetRow = 0x2B,       // RASET
  WriteMemory = 0x2C,  // RAMWR
  ColorMode = 0x3A,    // COLMOD

  RamControl = 0xB0,  // RAMCTRL
};

class Panel : public pv::Singleton<Panel> {
 public:
  static constexpr uint16_t PIXEL_VERTICAL{240};
  static constexpr uint16_t PIXEL_HORIZONTAL{240};
  static constexpr uint16_t PIXEL_COUNT{PIXEL_VERTICAL * PIXEL_HORIZONTAL};

  struct Id {
    uint8_t manufacturer;
    uint8_t version;
    uint8_t module;
  };

 private:
  static constexpr uint8_t FSMC_BANK{1};
  static constexpr uint8_t RS_PIN{0};
  static constexpr uint8_t DATA_IDX{RS_PIN + 1};

  using bus_t = fsmc::NorSram<uint16_t>;

  static constexpr uint32_t BUS_CONFIG{FSMC_BCR1_WREN | FSMC_BCR1_FACCEN |
                                       FSMC_BCR1_MWID_0 | FSMC_BCR1_MTYP_1 |
                                       FSMC_BCR1_MBKEN};
  static constexpr uint32_t BUS_CONFIG_RESERVED_MASK{0xFFC00480};

  // DATAST == 3, ADDHLD == 2, ADDSET == 6
  static constexpr uint32_t BUS_TIMINGS{0x00000326};
  static constexpr uint32_t BUS_TIMINGS_RESERVED_MASK{0xC0000000};

 public:
  void BackLightControl(bool on) const noexcept;

  Panel& SendCommand(Command cmd) noexcept;

  [[nodiscard]] std::uint16_t Read(std::size_t skip_count = 0) const noexcept;

  void Read(std::uint16_t* values,
            std::size_t count,
            std::size_t skip_count = 0) const noexcept;

  Panel& Write(std::uint16_t value) noexcept;
  Panel& Write(const std::uint16_t* values, std::size_t count) noexcept;

  [[nodiscard]] const Id& GetId() const noexcept;

 private:
  friend Singleton;

  Panel() noexcept;

  void skip(std::size_t skip_count) const noexcept;
  void read_id() noexcept;
  void initialize() noexcept;

  static bus_t setup_bus() noexcept;
  static void prepare_control_gpio() noexcept;
  static void prepare_data_gpio() noexcept;
  static Id parse_id(const std::array<uint16_t, 3>& raw_id) noexcept;

 private:
  bus_t m_bus;
  Id m_id;
};
}  // namespace lcd