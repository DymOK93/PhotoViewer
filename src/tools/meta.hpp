#pragma once
#include <cstdint>

namespace meta {
namespace details {
template <std::size_t Idx, class Head, class... Tail>
struct TypeByIndexImpl {
  using type = typename TypeByIndexImpl<Idx - 1, Tail...>::type;
};

template <class Head, class... Tail>
struct TypeByIndexImpl<0, Head, Tail...> {
  using type = Head;
};
}  // namespace details

template <std::size_t Idx, class... Types>
struct TypeByIndex {
  using type = typename details::TypeByIndexImpl<Idx, Types...>::type;
};

template <std::size_t Idx, class... Types>
using type_by_index_t = typename TypeByIndex<Idx, Types...>::type;

template <class... Types>
struct TypeList{};

template <std::size_t Idx, class... Types>
struct TypeByIndex<Idx, TypeList<Types...>> {
  using type = typename details::TypeByIndexImpl<Idx, Types...>::type;
};

template <class... Types>
inline constexpr bool always_false_v = false;
}  // namespace meta