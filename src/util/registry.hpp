#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <vector>

template<typename Id, typename Definition, std::size_t Capacity = 256>
class Registry {
public:
   void add(const Id id, const Definition& definition) {
      assert(std::find(order.begin(), order.end(), id) == order.end());
      defs[static_cast<std::size_t>(id)] = definition;
      order.push_back(id);
   }

   [[nodiscard]] const Definition& get(const Id id) const { return defs[static_cast<std::size_t>(id)]; }

   [[nodiscard]] const std::vector<Id>& list() const { return order; }

private:
   std::array<Definition, Capacity> defs{};
   std::vector<Id> order;
};
