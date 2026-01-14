#pragma once
#include <type_traits>

template<typename T>
struct EnableBitMaskOperators {
   static constexpr bool enable = false;
};

namespace detail {
   template<typename E>
   using underlying = std::underlying_type_t<E>;

   template<typename E, typename ReturnType = E>
   using EnableIf = std::enable_if_t<EnableBitMaskOperators<E>::enable, ReturnType>;

   template<typename E>
   constexpr E doOr(E lhs, E rhs) {
      return static_cast<E>(static_cast<underlying<E>>(lhs) | static_cast<underlying<E>>(rhs));
   }

   template<typename E>
   constexpr E doAnd(E lhs, E rhs) {
      return static_cast<E>(static_cast<underlying<E>>(lhs) & static_cast<underlying<E>>(rhs));
   }

   template<typename E>
   constexpr E doXor(E lhs, E rhs) {
      return static_cast<E>(static_cast<underlying<E>>(lhs) ^ static_cast<underlying<E>>(rhs));
   }

   template<typename E>
   constexpr E doNot(E val) {
      return static_cast<E>(~static_cast<underlying<E>>(val));
   }
}   // namespace detail

template<typename E>
constexpr detail::EnableIf<E> operator |(E lhs, E rhs) {
   return detail::doOr(lhs, rhs);
}

template<typename E>
constexpr detail::EnableIf<E, E&> operator |=(E& lhs, E rhs) {
   lhs = detail::doOr(lhs, rhs);
   return lhs;
}

template<typename E>
constexpr detail::EnableIf<E> operator &(E lhs, E rhs) {
   return detail::doAnd(lhs, rhs);
}

template<typename E>
constexpr detail::EnableIf<E, E&> operator &=(E& lhs, E rhs) {
   lhs = detail::doAnd(lhs, rhs);
   return lhs;
}

template<typename E>
constexpr detail::EnableIf<E> operator ^(E lhs, E rhs) {
   return detail::doXor(lhs, rhs);
}

template<typename E>
constexpr detail::EnableIf<E, E&> operator ^=(E& lhs, E rhs) {
   lhs = detail::doXor(lhs, rhs);
   return lhs;
}

template<typename E>
constexpr detail::EnableIf<E> operator ~(E val) {
   return detail::doNot(val);
}

#define ENABLE_BITMASK_OPERATORS(T)        \
   template<>                              \
   struct EnableBitMaskOperators<T> {      \
      static constexpr bool enable = true; \
   };
