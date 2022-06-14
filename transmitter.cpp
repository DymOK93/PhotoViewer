#include "transmitter.hpp"
#include "attributes.hpp"

using namespace std;

namespace io {
Transmitter& Transmitter::GetInstance() noexcept {
  static Transmitter transmitter;
  return transmitter;
}

void Transmitter::SendData(const byte* buffer, size_t bytes_count) {
  while (bytes_count > MAX_BLOCK_LENGTH) {
    send_chunk(buffer, MAX_BLOCK_LENGTH);
    buffer += MAX_BLOCK_LENGTH;
    bytes_count -= MAX_BLOCK_LENGTH;
  }
  if (bytes_count) {
    send_chunk(buffer, bytes_count);
  }
}

void Transmitter::SendCommand(cmd::Command command) {
  const BlockHeader header{BlockHeader::Category::Command, 1};
  m_port.Transfer(header, command);
}

size_t Transmitter::GetRemainingQueueSize() const noexcept {
  return QUEUE_DEPTH - m_port.GetQueueSize();
}

void Transmitter::send_chunk(const byte* buffer, size_t bytes_count) {
  const BlockHeader header{BlockHeader::Category::Data,
                           static_cast<uint8_t>(bytes_count)};
  m_port.Transfer(header);
  m_port.Transfer(buffer, bytes_count);
}

void OnOverwrite() noexcept {
  auto& transmitter{Transmitter::GetInstance()};
  transmitter.m_port.Retry();
}

void OnClearToSend() noexcept {
  auto& transmitter{Transmitter::GetInstance()};
  transmitter.m_port.PassNext();
}

void OnReadyToSend() noexcept {
  auto& transmitter{Transmitter::GetInstance()};
  transmitter.m_port.SetReady();
}
}  // namespace io

EXTERN_C void EXTI9_5_IRQHandler() {
  if (const auto mask = EXTI->PR; READ_BIT(mask, EXTI_PR_PR5)) {
    io::OnOverwrite();
    SET_BIT(EXTI->PR, EXTI_PR_PR5);
  } else if (READ_BIT(mask, EXTI_PR_PR6)) {
    io::OnClearToSend();
    SET_BIT(EXTI->PR, EXTI_PR_PR6);
  }
}

EXTERN_C void TIM6_IRQHandler() {
  io::OnReadyToSend();
  CLEAR_BIT(TIM6->SR, TIM_SR_UIF);
}