#pragma once
#include <tools/meta.hpp>
#include <tools/singleton.hpp>

#include <stm32f4xx.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace fsmc {
namespace details {
template <typename Ty>
class MemoryBank {
 public:
  constexpr explicit MemoryBank(volatile Ty* address) noexcept
      : m_address{address} {}

  constexpr volatile Ty& operator[](std::size_t idx) noexcept {
    return m_address[idx];
  }

  constexpr const volatile Ty& operator[](std::size_t idx) const noexcept {
    return m_address[idx];
  }

 private:
  volatile Ty* m_address;
};
}  // namespace details

template <typename Ty>
class NorSram : public details::MemoryBank<Ty> {
 public:
  using MyBase = details::MemoryBank<Ty>;

  using config_t = FSMC_Bank1_TypeDef;
  using write_timings_t = FSMC_Bank1E_TypeDef;

 public:
  constexpr explicit NorSram(volatile Ty* address,
                             config_t& config,
                             write_timings_t write_timings) noexcept
      : MyBase(address),
        m_config{std::addressof(config)},
        m_write_timings{std::addressof(write_timings)} {}

  config_t& GetConfig() noexcept { return *m_config; }

  const config_t& GetConfig() const noexcept { return *m_config; }

  write_timings_t& GetWriteTimings() noexcept { return *m_write_timings; }

  const write_timings_t& GetWriteTimings() const noexcept {
    return *m_write_timings;
  }

 private:
  config_t* m_config;
  write_timings_t* m_write_timings;
};

namespace details {
template <class Ty>
inline constexpr bool is_supported_bus_type_v =
    std::is_same_v<std::remove_cv_t<Ty>, std::uint8_t> ||
    std::is_same_v<std::remove_cv_t<Ty>, std::uint16_t>;
}  // namespace details

class Controller : public pv::Singleton<Controller> {
  static volatile std::byte* BASE_ADDRESS;
  static constexpr size_t BANK_SIZE{256 * 1024 * 1024};
  static constexpr size_t BANK_COUNT{4};

  static constexpr uint32_t CONFIG_RESERVED_MASK{0xFFC00480};
  static constexpr uint32_t TIMINGS_RESERVED_MASK{0xC0000000};

 public:
  template <std::uint8_t BankNumber,
            class Ty,
            std::enable_if_t<details::is_supported_bus_type_v<Ty>, int> = 0>
  auto GetMemoryBank() const noexcept {
    const auto address{
        reinterpret_cast<volatile Ty*>(calc_bank_address(BankNumber))};
    if constexpr (BankNumber == 1) {
      return NorSram<Ty>{address, *FSMC_Bank1, *FSMC_Bank1E};
    } else {
      static_assert(meta::always_false_v<Ty>, "invalid bank number");
    }
  }

 private:
  friend Singleton;

  Controller() noexcept;

  static volatile std::byte* calc_bank_address(
      std::uint8_t bank_number) noexcept;
};
}  // namespace fsmc