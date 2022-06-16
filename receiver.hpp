#pragma once
#include "singleton.hpp"

#include <cstddef>
#include <cstdint>

#include <stm32f4xx.h>

namespace io {
struct IListener {
  IListener() = default;
  IListener(const IListener&) = default;
  IListener(IListener&&) = default;
  IListener& operator=(const IListener&) = default;
  IListener& operator=(IListener&&) = default;
  virtual ~IListener() = default;

  virtual void Process(std::byte value) = 0;
};

class Receiver : public pv::Singleton<Receiver> {
  static constexpr std::uint16_t SPEED_MANTISSA{8};
  static constexpr std::uint16_t SPEED_FRACTION{11};
  static constexpr std::uint8_t INTERRUPT_PRIORITY{14};

 public:
  void Listen(IListener* listener) noexcept;

 private:
  friend void OnReceive(std::byte value) noexcept;
  friend Singleton;

  Receiver() noexcept;
  void set_transceiver_state(bool on) noexcept;

  static USART_TypeDef* setup_transceiver(std::uint16_t mantissa,
                                          std::uint16_t fraction);

 private:
  USART_TypeDef* m_transceiver;
  IListener* m_listener{nullptr};
};
}  // namespace io