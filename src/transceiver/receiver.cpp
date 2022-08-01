#include "receiver.hpp"

#include <platform/gpio.hpp>
#include <tools/attributes.hpp>

#include <cassert>

using namespace std;

namespace io {
void Receiver::Listen(IListener* listener) noexcept {
  set_transceiver_state(listener);
  m_listener = listener;
}

Receiver::Receiver() noexcept
    : m_transceiver{setup_transceiver(SPEED_MANTISSA, SPEED_FRACTION)} {}

void Receiver::set_transceiver_state(bool on) noexcept {
  if (on) {
    SET_BIT(m_transceiver->CR1, USART_CR1_UE);
  } else {
    CLEAR_BIT(m_transceiver->CR1, USART_CR1_UE);
  }
}

USART_TypeDef* Receiver::setup_transceiver(uint16_t mantissa,
                                           uint16_t fraction) noexcept {
  auto& gpio{gpio::ChannelManager::GetInstance().Activate<gpio::Channel::G>()};
  SET_BIT(gpio.MODER, GPIO_MODER_MODER9_1);
  SET_BIT(gpio.PUPDR, GPIO_PUPDR_PUPDR9_0);
  SET_BIT(gpio.AFR[1], 0x00000080);

  const auto transceiver{USART6};
  SET_BIT(RCC->APB2ENR, RCC_APB2ENR_USART6EN);
  SET_BIT(transceiver->BRR, (mantissa << 4) | fraction);
  SET_BIT(transceiver->CR3, USART_CR3_EIE);
  SET_BIT(transceiver->CR1, USART_CR1_RE | USART_CR1_RXNEIE);
  NVIC_SetPriority(USART6_IRQn, INTERRUPT_PRIORITY);
  NVIC_EnableIRQ(USART6_IRQn);

  return transceiver;
}

void OnReceive(byte value) noexcept {
  auto& listener{Receiver::GetInstance().m_listener};
  assert(listener && "listener not found");
  listener->Process(value);
}
}  // namespace io

EXTERN_C void USART6_IRQHandler() {
  if (!READ_BIT(USART6->SR, USART_SR_RXNE)) {
    [[maybe_unused]] const auto value{USART6->DR};
    // Do smth with error
  } else {
    io::OnReceive(static_cast<byte>(USART6->DR));
  }
}