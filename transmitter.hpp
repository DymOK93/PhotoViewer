#pragma once
#include "bmp.hpp"
#include "circular.hpp"
#include "command.hpp"
#include "event.hpp"
#include "gpio.hpp"
#include "io.hpp"
#include "lcd.hpp"
#include "singleton.hpp"

#include <array>
#include <cstddef>
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
  ParallelPort(Settings settings) noexcept
      : m_settings{settings}, m_timer{setup_transaction_timer()} {
    prepare_gpio();
    setup_transaction_timer();
    listen_cts_and_ov();
  }

  void Transfer(const std::byte* buffer, std::size_t bytes_count) {
    m_buffer.produce(buffer, bytes_count);
    start_transmission();
  }

  template <class... Types>
  void Transfer(const Serializable<Types>&... values) {
    const auto producer{[this](const auto& value) {
      const auto serialized{value.Serialize()};
      m_buffer.produce(
          reinterpret_cast<const std::byte*>(std::addressof(serialized)),
          sizeof(serialized));
    }};
    (producer(values), ...);
    start_transmission();
  }

  void PassNext() noexcept {
    switch_rts(false);
    if (std::empty(m_buffer)) {
      m_active = false;
    } else {
      start_transmission_unchecked();
    }
  }

  void Retry() const noexcept {
    // RTS high -> RTS low -> RTS high
    switch_rts(false);
    schedule_transaction(m_settings.retry_delay);
  }

  void SetReady() const noexcept { switch_rts(true); }

  [[nodiscard]] std::size_t GetQueueSize() const noexcept {
    return std::size(m_buffer);
  }

 private:
  void start_transmission() noexcept {
    if (!m_active) {
      m_active = true;
      start_transmission_unchecked();
    }
  }

  void start_transmission_unchecked() noexcept {
    expose_data(m_buffer.consume());
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
    SET_BIT(gpio.PUPDR, GPIO_PUPDR_PUPDR7_1  // RTS
                            | GPIO_PUPDR_PUPDR8_1 | GPIO_PUPDR_PUPDR9_1 |
                            GPIO_PUPDR_PUPDR10_1 | GPIO_PUPDR_PUPDR11_1 |
                            GPIO_PUPDR_PUPDR12_1 | GPIO_PUPDR_PUPDR13_1 |
                            GPIO_PUPDR_PUPDR14_1 | GPIO_PUPDR_PUPDR15_1);
  }

  static TIM_TypeDef* setup_transaction_timer() {
    TIM_TypeDef* timer{TIM6};
    SET_BIT(RCC->APB1ENR, RCC_APB1ENR_TIM6EN);
    SET_BIT(timer->CR1, TIM_CR1_OPM);
    SET_BIT(timer->DIER, TIM_DIER_UIE);
    timer->PSC = SystemCoreClock / 1'000'000;
    NVIC_SetPriority(TIM6_IRQn, 15);
    NVIC_EnableIRQ(TIM6_IRQn);
    return timer;
  }

  static void listen_cts_and_ov() {
    auto& exti{event::ExtiManager::GetInstance().Get()};
    SET_BIT(exti.IMR, EXTI_IMR_MR5 | EXTI_IMR_MR6);
    SET_BIT(exti.RTSR, EXTI_RTSR_TR5 | EXTI_RTSR_TR6);
    SET_BIT(SYSCFG->EXTICR[1],
            SYSCFG_EXTICR2_EXTI5_PB | SYSCFG_EXTICR2_EXTI6_PB);
    NVIC_SetPriority(EXTI9_5_IRQn, 15);
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

 public:
  static constexpr std::size_t QUEUE_DEPTH{
      DATA_SIZE + BLOCKS_COUNT * sizeof(static_cast<Serializable<BlockHeader>&>(
                                            std::declval<BlockHeader&>())
                                            .Serialize())};
  static constexpr std::uint16_t PASS_DELAY{100}, RETRY_DELAY{1000};

 public:
  static Transmitter& GetInstance() noexcept;

  void SendData(const std::byte* buffer, std::size_t bytes_count);

  void SendCommand(cmd::Command command);

  template <
      class... Commands,
      std::enable_if_t<
          std::conjunction_v<std::is_convertible<Commands, cmd::Command>...>,
          int> = 0>
  void SendCommands(Commands... commands) {
    (SendCommand(commands), ...);
  }

  std::size_t GetRemainingQueueSize() const noexcept;

 private:
  friend void OnOverwrite() noexcept;
  friend void OnClearToSend() noexcept;
  friend void OnReadyToSend() noexcept;
  friend Singleton;

  Transmitter() = default;
  void send_chunk(const std::byte* buffer, std::size_t bytes_count);

 private:
  details::ParallelPort<QUEUE_DEPTH> m_port{{PASS_DELAY, RETRY_DELAY}};
};
}  // namespace io