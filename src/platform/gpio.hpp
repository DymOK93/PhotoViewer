#pragma once
#include <tools/singleton.hpp>

#include <stm32f4xx.h>

#define GPIO_GET_CHANNEL(Tag) \
  case Channel::Tag:          \
    ch = GPIO##Tag;           \
    break;

#define GPIO_ACTIVATE_CHANNEL(Tag)                    \
  case Channel::Tag:                                  \
    ch = GPIO##Tag;                                   \
    SET_BIT(RCC->AHB1ENR, RCC_AHB1ENR_GPIO##Tag##EN); \
    break;

namespace gpio {
using channel_t = GPIO_TypeDef;

enum class Channel : uint8_t { A, B, C, D, E, F, G, H };

class ChannelManager : public pv::Singleton<ChannelManager> {
 public:
  template <Channel Ch>
  channel_t& Get() const noexcept {
    channel_t* ch;
    switch (Ch) {
      GPIO_GET_CHANNEL(A);
      GPIO_GET_CHANNEL(B);
      GPIO_GET_CHANNEL(C);
      GPIO_GET_CHANNEL(D);
      GPIO_GET_CHANNEL(E);
      GPIO_GET_CHANNEL(F);
      GPIO_GET_CHANNEL(G);
      GPIO_GET_CHANNEL(H);
    }
    return *ch;
  }

  template <Channel Ch>
  channel_t& Activate() const noexcept {
    channel_t* ch;
    switch (Ch) {
      GPIO_ACTIVATE_CHANNEL(A);
      GPIO_ACTIVATE_CHANNEL(B);
      GPIO_ACTIVATE_CHANNEL(C);
      GPIO_ACTIVATE_CHANNEL(D);
      GPIO_ACTIVATE_CHANNEL(E);
      GPIO_ACTIVATE_CHANNEL(F);
      GPIO_ACTIVATE_CHANNEL(G);
      GPIO_ACTIVATE_CHANNEL(H);
    }
    return *ch;
  }

 private:
  friend Singleton;

  ChannelManager() = default;
};
}  // namespace gpio

#undef GPIO_GET_CHANNEL
#undef GPIO_ACTIVATE_CHANNEL

