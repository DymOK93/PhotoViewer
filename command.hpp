#pragma once
#include "io.hpp"
#include "singleton.hpp"

#include <cstdint>
#include <functional>

namespace cmd {
struct Command : io::Serializable<Command> {
  enum class Type : std::uint8_t {
    Empty = 0x0,
    GreenLedOn = 0x1,
    GreenLedOff = 0x2,
    GreenLedToggle = GreenLedOn | GreenLedOff,
    BlueLedOn = 0x4,
    BlueLedOff = 0x8,
    BlueLedToggle = BlueLedOn | BlueLedOff,
    NextPicture = 0x80
  };

  [[nodiscard]] std::uint8_t Serialize() const noexcept {
    return static_cast<std::uint8_t>(type);
  }

  [[nodiscard]] static std::optional<Command> Deserialize(
      const std::byte* raw_data,
      std::size_t bytes_count) noexcept {
    if (!bytes_count) {
      return std::nullopt;
    }
    return Command{static_cast<Type>(*raw_data)};
  }

  Command() = default;
  Command(Type t) noexcept : type{t} {}

  Type type;
};

constexpr bool operator==(Command lhs, Command rhs) {
  return lhs.type == rhs.type;
}

struct NextPictureTag {};

namespace details {
class Joystick {
  static constexpr std::uint8_t INTERRUPT_PRIORITY{15};

 public:
  enum class Button { Center, Down, Up, Left, Right };

 public:
  Joystick() noexcept {
    prepare_gpio();
    setup_notifications();
  }

 private:
  static void prepare_gpio() noexcept;
  static void setup_notifications() noexcept;
};
}  // namespace details

class CommandManager : public pv::Singleton<CommandManager> {
 public:
  template <class Handler>
  void Execute(Command command, Handler&& handler) {
    if (command == Command::Type::NextPicture) {
      std::invoke(std::forward<Handler>(handler), NextPictureTag{});
    }
  }

 private:
  friend void OnJoystickButton(details::Joystick::Button button) noexcept;
  friend Singleton;

  CommandManager() = default;
  void on_joystick_button(details::Joystick::Button button) noexcept;

 private:
  details::Joystick m_joystick;
};
}  // namespace cmd