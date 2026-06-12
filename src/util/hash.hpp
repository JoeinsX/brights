#pragma once

#include <string_view>

static constexpr uint64_t fnv1a(std::string_view s) noexcept {
   static constexpr uint64_t fnv1aPrime = 0x100000001b3ull;
   static constexpr uint64_t fnv1aOffsetBasis = 0xcbf29ce484222325ull;
   uint64_t h = fnv1aOffsetBasis;
   for (const char c : s) {
      h ^= static_cast<uint8_t>(c);
      h *= fnv1aPrime;
   }
   return h;
}
