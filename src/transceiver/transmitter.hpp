#pragma once
#include "command.hpp"
#include "io.hpp"

#include <display/lcd.hpp>
#include <filesystem/bmp.hpp>
#include <platform/event.hpp>
#include <platform/gpio.hpp>
#include <storage/circular.hpp>

#include <tools/singleton.hpp>

#include <array>
#include <cstddef>
#include <limits>
#include <utility>

#include <stm32f4xx.h>

namespace io {
namespace details {
template <std::size_t QueueDepth>
class ParallelPort {
 public:
  struct Settings {
    std::uint16_t pass_delay;
    std::uint16_t retry_delay;
  };

 public:
  ParallelPort(Settings settings, std::uint8_t interrupt_priority) noexcept
      : m_settings{settings},
        m_timer{setup_transaction_timer(interrupt_priority)} {
    prepare_gpio();
    listen_cts_and_ov(interrupt_priority);
  }

  void Transfer(const std::byte* buffer, std::size_t bytes_count) noexcept {
    while (bytes_count > 0) {
      bytes_count -= m_buffer.produce(buffer, bytes_count);
      start_transmission();
    }
  }

  template <class... Types>
  void Transfer(const Serializable<Types>&... values) {
    std::array buffer{values.Serialize()...};
    Transfer(reinterpret_cast<const std::byte*>(std::data(buffer)),
             std::size(buffer));
  }

  void PassNext() noexcept {
    switch_rts(false);
    if (const auto value = m_buffer.consume(); !value) {
      m_active = false;
    } else {
      start_transmission_unchecked(*value);
    }
  }

  void Retry() const noexcept {
    // RTS high -> RTS low -> RTS high
    switch_rts(false);
    schedule_transaction(m_settings.retry_delay);
  }

  void SetReady() const noexcept { switch_rts(true); }

 private:
  void start_transmission() noexcept {
    if (!m_active) {
      if (const auto value = m_buffer.consume(); value) {
        m_active = true;
        start_transmission_unchecked(*value);
      }
    }
  }

  void start_transmission_unchecked(std::byte value) noexcept {
    expose_data(value);
    schedule_transaction(m_settings.pass_delay);
  }

  void schedule_transaction(std::uint16_t us) const noexcept {
    if (!us) {
      switch_rts(true);
    } else {
      set_one_pulse_timer(us);
    }
  }

  void set_one_pulse_timer(std::uint16_t us) const noexcept {
    m_timer->CNT = 0;
    m_timer->ARR = us;
    SET_BIT(m_timer->CR1, TIM_CR1_CEN);
  }

  static void prepare_gpio() noexcept {
    auto& channel_manager{gpio::ChannelManager::GetInstance()};
    auto& gpio{channel_manager.Activate<gpio::Channel::B>()};

    SET_BIT(gpio.MODER, GPIO_MODER_MODER7_0  // RTS
                            | GPIO_MODER_MODER8_0 | GPIO_MODER_MODER9_0 |
                            GPIO_MODER_MODER10_0 | GPIO_MODER_MODER11_0 |
                            GPIO_MODER_MODER12_0 | GPIO_MODER_MODER13_0 |
                            GPIO_MODER_MODER14_0 | GPIO_MODER_MODER15_0);
    SET_BIT(gpio.OTYPER, GPIO_OTYPER_OT_7);  // RTS
    SET_BIT(gpio.OSPEEDR,
            GPIO_OSPEEDER_OSPEEDR5        // OV
                | GPIO_OSPEEDER_OSPEEDR6  // CTS
                | GPIO_OSPEEDER_OSPEEDR7  // RTS
                | GPIO_OSPEEDER_OSPEEDR8 | GPIO_OSPEEDER_OSPEEDR9 |
                GPIO_OSPEEDER_OSPEEDR10 | GPIO_OSPEEDER_OSPEEDR11 |
                GPIO_OSPEEDER_OSPEEDR12 | GPIO_OSPEEDER_OSPEEDR13 |
                GPIO_OSPEEDER_OSPEEDR14 | GPIO_OSPEEDER_OSPEEDR15);
    SET_BIT(gpio.PUPDR, GPIO_PUPDR_PUPDR5_1          // OV
                            | GPIO_PUPDR_PUPDR6_1 |  // CTS
                            GPIO_PUPDR_PUPDR7_1      // RTS
                            | GPIO_PUPDR_PUPDR8_1 | GPIO_PUPDR_PUPDR9_1 |
                            GPIO_PUPDR_PUPDR10_1 | GPIO_PUPDR_PUPDR11_1 |
                            GPIO_PUPDR_PUPDR12_1 | GPIO_PUPDR_PUPDR13_1 |
                            GPIO_PUPDR_PUPDR14_1 | GPIO_PUPDR_PUPDR15_1);
  }

  static TIM_TypeDef* setup_transaction_timer(std::uint8_t interrupt_priority) {
    TIM_TypeDef* timer{TIM6};
    SET_BIT(RCC->APB1ENR, RCC_APB1ENR_TIM6EN);
    SET_BIT(timer->CR1, TIM_CR1_OPM);
    SET_BIT(timer->DIER, TIM_DIER_UIE);
    timer->PSC = SystemCoreClock / 1'000'000;
    NVIC_SetPriority(TIM6_IRQn, interrupt_priority);
    NVIC_EnableIRQ(TIM6_IRQn);
    return timer;
  }

  static void listen_cts_and_ov(std::uint8_t interrupt_priority) {
    auto& exti{event::ExtiManager::GetInstance().Get()};
    SET_BIT(exti.IMR, EXTI_IMR_MR5 | EXTI_IMR_MR6);
    SET_BIT(exti.RTSR, EXTI_RTSR_TR5 | EXTI_RTSR_TR6);
    SET_BIT(SYSCFG->EXTICR[1],
            SYSCFG_EXTICR2_EXTI5_PB | SYSCFG_EXTICR2_EXTI6_PB);
    NVIC_SetPriority(EXTI9_5_IRQn, interrupt_priority);
    NVIC_EnableIRQ(EXTI9_5_IRQn);
  }

  static void switch_rts(bool on) noexcept {
    auto& channel_manager{gpio::ChannelManager::GetInstance()};
    if (auto& gpio = channel_manager.Get<gpio::Channel::B>(); on) {
      gpio.BSRR = GPIO_BSRR_BS_7;
    } else {
      gpio.BSRR = GPIO_BSRR_BR_7;
    }
  }

  static void expose_data(std::byte value) noexcept {
    auto& channel_manager{gpio::ChannelManager::GetInstance()};
    auto& odr{channel_manager.Get<gpio::Channel::B>().ODR};
    odr = (odr & 0xFFFF00FF) | (static_cast<std::uint16_t>(value) << 8);
  }

 private:
  storage::CircularBuffer<QueueDepth, std::byte> m_buffer;
  Settings m_settings;
  TIM_TypeDef* m_timer;
  volatile bool m_active{false};
};
}  // namespace details

class Transmitter : public pv::Singleton<Transmitter> {
  static constexpr std::size_t MAX_BLOCK_LENGTH{BlockHeader::MAX_LENGTH};

  static constexpr std::size_t DATA_SIZE{lcd::Panel::PIXEL_COUNT *
                                         sizeof(bmp::Bgr888)};
  static constexpr std::size_t BLOCKS_COUNT{DATA_SIZE /
                                            BlockHeader::MAX_LENGTH};

  static constexpr std::uint8_t INTERRUPT_PRIORITY{12};
  static constexpr std::uint16_t PASS_DELAY{100},
      RETRY_DELAY{std::numeric_limits<std::uint16_t>::max()};

  template <class Ty>
  using serialized_t =
      decltype(std::declval<const Serializable<Ty>&>().Serialize());

  using serialized_header_t = serialized_t<BlockHeader>;
  using serialized_command_t = serialized_t<cmd::Command>;

 public:
  static constexpr std::size_t QUEUE_DEPTH{
      DATA_SIZE + BLOCKS_COUNT * sizeof(serialized_header_t)};

 public:
  static Transmitter& GetInstance() noexcept;

  void SendData(const std::byte* buffer, std::size_t bytes_count);

  void SendCommand(cmd::Command* command, std::size_t count);

  template <
      class... Commands,
      std::enable_if_t<(sizeof...(Commands) > 0) &&
                           std::conjunction_v<
                               std::is_convertible<Commands, cmd::Command>...>,
                       int> = 0>
  void SendCommand(Commands... commands) {
    if constexpr (sizeof...(Commands) == 1) {
      const BlockHeader header{BlockHeader::Category::Command, 1};
      m_port.Transfer(header, commands...);
    } else {
      std::array storage{static_cast<cmd::Command>(commands)...};
      SendCommand(std::data(storage), std::size(storage));
    }
  }

 private:
  friend void OnOverwrite() noexcept;
  friend void OnClearToSend() noexcept;
  friend void OnReadyToSend() noexcept;
  friend Singleton;

  Transmitter() = default;
  void send_chunk(BlockHeader::Category category,
                  const std::byte* buffer,
                  std::size_t bytes_count);

  template <class Ty>
  static constexpr std::size_t calc_chunk_size() noexcept {
    return MAX_BLOCK_LENGTH - MAX_BLOCK_LENGTH % sizeof(Ty);
  }

 private:
  details::ParallelPort<QUEUE_DEPTH> m_port{{PASS_DELAY, RETRY_DELAY},
                                            INTERRUPT_PRIORITY};
};
}  // namespace io