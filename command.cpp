#include "command.hpp"
#include "attributes.hpp"
#include "event.hpp"
#include "gpio.hpp"
#include "transmitter.hpp"

#include <stm32f4xx.h>

using Joystick = cmd::details::Joystick;

namespace cmd {
void CommandManager::on_joystick_button(Joystick::Button button) noexcept {
  auto& transmitter{io::Transmitter::GetInstance()};
  switch (button) {
    case Joystick::Button::Down:
      transmitter.SendCommand(Command::Type::GreenLedOff);
      break;
    case Joystick::Button::Up:
      transmitter.SendCommand(Command::Type::GreenLedOn);
      break;
    case Joystick::Button::Left:
      transmitter.SendCommand(Command::Type::BlueLedOff);
      break;
    case Joystick::Button::Right:
      transmitter.SendCommand(Command::Type::BlueLedOn);
      break;
    default:
      break;
  }
}

namespace details {
void Joystick::prepare_gpio() noexcept {
  auto& channel_manager{gpio::ChannelManager::GetInstance()};

  // G0 - Up, G1 - Down, F14 - Right, F15 - Left
  channel_manager.Activate<gpio::Channel::F>();
  channel_manager.Activate<gpio::Channel::G>();
}

void Joystick::setup_notifications() noexcept {
  auto& exti{event::ExtiManager::GetInstance().Get()};
  SET_BIT(exti.IMR,
          EXTI_IMR_MR0 | EXTI_IMR_MR1 | EXTI_IMR_MR14 | EXTI_IMR_MR15);
  SET_BIT(exti.RTSR,
          EXTI_RTSR_TR0 | EXTI_RTSR_TR1 | EXTI_RTSR_TR14 | EXTI_RTSR_TR15);
  SET_BIT(SYSCFG->EXTICR[0], SYSCFG_EXTICR1_EXTI0_PG | SYSCFG_EXTICR1_EXTI1_PG);
  SET_BIT(SYSCFG->EXTICR[3],
          SYSCFG_EXTICR4_EXTI14_PF | SYSCFG_EXTICR4_EXTI15_PF);
  NVIC_SetPriority(EXTI0_IRQn, INTERRUPT_PRIORITY);
  NVIC_SetPriority(EXTI1_IRQn, INTERRUPT_PRIORITY);
  NVIC_SetPriority(EXTI15_10_IRQn, INTERRUPT_PRIORITY);
  NVIC_EnableIRQ(EXTI0_IRQn);
  NVIC_EnableIRQ(EXTI1_IRQn);
  NVIC_EnableIRQ(EXTI15_10_IRQn);
}
}  // namespace details

void OnJoystickButton(Joystick::Button button) noexcept {
  CommandManager::GetInstance().on_joystick_button(button);
}
}  // namespace cmd

EXTERN_C void EXTI0_IRQHandler() {
  cmd::OnJoystickButton(Joystick::Button::Up);
  SET_BIT(EXTI->PR, EXTI_PR_PR0);
}

EXTERN_C void EXTI1_IRQHandler() {
  cmd::OnJoystickButton(Joystick::Button::Down);
  SET_BIT(EXTI->PR, EXTI_PR_PR1);
}

EXTERN_C void EXTI15_10_IRQHandler() {
  if (const auto mask = EXTI->PR; READ_BIT(mask, EXTI_PR_PR14)) {
    SET_BIT(EXTI->PR, EXTI_PR_PR14);
    cmd::OnJoystickButton(Joystick::Button::Right);
  } else if (READ_BIT(mask, EXTI_PR_PR15)) {
    SET_BIT(EXTI->PR, EXTI_PR_PR15);
    cmd::OnJoystickButton(Joystick::Button::Left);
  }
}