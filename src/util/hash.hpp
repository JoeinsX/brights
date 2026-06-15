#pragma once

#include <cstdint>
#include <string_view>

constexpr uint64_t fnv1a(std::string_view s) noexcept {
   constexpr uint64_t prime = 0x100000001b3ull;
   constexpr uint64_t offsetBasis = 0xcbf29ce484222325ull;
   uint64_t h = offsetBasis;
   for (const char c : s) {
      h ^= static_cast<uint8_t>(c);
      h *= prime;
   }
   return h;
}
