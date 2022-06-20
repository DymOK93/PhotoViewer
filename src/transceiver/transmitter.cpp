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
    send_chunk(BlockHeader::Category::Data, buffer, MAX_BLOCK_LENGTH);
    buffer += MAX_BLOCK_LENGTH;
    bytes_count -= MAX_BLOCK_LENGTH;
  }
  if (bytes_count) {
    send_chunk(BlockHeader::Category::Data, buffer, bytes_count);
  }
}

void Transmitter::SendCommand(cmd::Command* command, size_t count) {
  using chunk_t = array<serialized_command_t,
                        MAX_BLOCK_LENGTH / sizeof(serialized_command_t)>;

  constexpr auto serializer{[](cmd::Command target) {
    return static_cast<const Serializable<cmd::Command>&>(target).Serialize();
  }};

  while (count > 0) {
    chunk_t chunk;
    const size_t chunk_size{min(count, size(chunk))};

    for (size_t idx = 0; idx < chunk_size; ++idx) {
      chunk[idx] = serializer(*command++);
    }
    send_chunk(BlockHeader::Category::Command,
               reinterpret_cast<const byte*>(data(chunk)),
               chunk_size * sizeof(serialized_command_t));
    count -= chunk_size;
  }
}

void Transmitter::send_chunk(BlockHeader::Category category,
                             const byte* buffer,
                             size_t bytes_count) {
  const BlockHeader header{category, static_cast<uint8_t>(bytes_count)};
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