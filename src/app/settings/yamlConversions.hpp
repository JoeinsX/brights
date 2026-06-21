#pragma once

#include "util/logger.hpp"

#include <array>
#include <glm/glm.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <yaml-cpp/yaml.h>

namespace YAML {
   template<>
   struct convert<glm::ivec2> {
      static Node encode(const glm::ivec2& v) {
         Node node;
         node.push_back(v.x);
         node.push_back(v.y);
         node.SetStyle(EmitterStyle::Flow);
         return node;
      }

      static bool decode(const Node& node, glm::ivec2& v) {
         if (!node.IsSequence() || node.size() != 2) {
            return false;
         }
         v.x = node[0].as<int>();
         v.y = node[1].as<int>();
         return true;
      }
   };

   template<>
   struct convert<Logger::Level> {
      static Node encode(const Logger::Level& level) {
         for (const auto& [value, label] : levelNames) {
            if (value == level) {
               return Node(std::string(label));
            }
         }
         return Node(std::string("Default"));
      }

      static bool decode(const Node& node, Logger::Level& level) {
         if (!node.IsScalar()) {
            return false;
         }
         const std::string_view text = node.Scalar();
         for (const auto& [value, label] : levelNames) {
            if (text == label) {
               level = value;
               return true;
            }
         }
         return false;
      }

   private:
      static constexpr std::array<std::pair<Logger::Level, std::string_view>, 9> levelNames{{{Logger::Level::Trace, "Trace"},
                                                                                             {Logger::Level::Debug, "Debug"},
                                                                                             {Logger::Level::Info, "Info"},
                                                                                             {Logger::Level::Warn, "Warn"},
                                                                                             {Logger::Level::Error, "Error"},
                                                                                             {Logger::Level::Critical, "Critical"},
                                                                                             {Logger::Level::Fatal, "Fatal"},
                                                                                             {Logger::Level::Off, "Off"},
                                                                                             {Logger::Level::Default, "Default"}}};
   };
}   // namespace YAML
