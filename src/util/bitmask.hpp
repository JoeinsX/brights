#pragma once
#include <utility>

template<typename T>
struct EnableBitMaskOperators {
   static constexpr bool enable = false;
};

template<typename E>
concept BitmaskEnum = EnableBitMaskOperators<E>::enable;

template<BitmaskEnum E>
constexpr E operator |(E lhs, E rhs) {
   return static_cast<E>(std::to_underlying(lhs) | std::to_underlying(rhs));
}

template<BitmaskEnum E>
constexpr E& operator |=(E& lhs, E rhs) {
   lhs = lhs | rhs;
   return lhs;
}

template<BitmaskEnum E>
constexpr E operator &(E lhs, E rhs) {
   return static_cast<E>(std::to_underlying(lhs) & std::to_underlying(rhs));
}

template<BitmaskEnum E>
constexpr E& operator &=(E& lhs, E rhs) {
   lhs = lhs & rhs;
   return lhs;
}

template<BitmaskEnum E>
constexpr E operator ^(E lhs, E rhs) {
   return static_cast<E>(std::to_underlying(lhs) ^ std::to_underlying(rhs));
}

template<BitmaskEnum E>
constexpr E& operator ^=(E& lhs, E rhs) {
   lhs = lhs ^ rhs;
   return lhs;
}

template<BitmaskEnum E>
constexpr E operator ~(E val) {
   return static_cast<E>(~std::to_underlying(val));
}

#define ENABLE_BITMASK_OPERATORS(T)        \
   template<>                              \
   struct EnableBitMaskOperators<T> {      \
      static constexpr bool enable = true; \
   };
