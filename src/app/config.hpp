#pragma once

#include "platform/window.hpp"
#include "util/logger.hpp"

#include <filesystem>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <yaml-cpp/yaml.h>

struct Config {
   WindowConfig windowSection;
   LoggerConfig loggerSection;

   static Config load(const std::filesystem::path& path = "config.yaml") {
      Config config;

      YAML::Node root;
      try {
         root = YAML::LoadFile(path.string());
      } catch (const std::exception& e) {
         Log::warn("config: could not load '{}' ({}); using defaults", path.string(), e.what());
         return config;
      }

      if (const YAML::Node windowNode = root["window"]) {
         config.windowSection.position = readIVec2(windowNode["position"], config.windowSection.position);
         config.windowSection.size = readIVec2(windowNode["size"], config.windowSection.size);
      }

      if (const YAML::Node loggerNode = root["logger"]) {
         config.loggerSection.writeLevel = readLevel(loggerNode["writeLevel"], config.loggerSection.writeLevel);
         config.loggerSection.showLocationLevel = readLevel(loggerNode["showLocationLevel"], config.loggerSection.showLocationLevel);
         config.loggerSection.flushLevel = readLevel(loggerNode["flushLevel"], config.loggerSection.flushLevel);
         config.loggerSection.panicLevel = readLevel(loggerNode["panicLevel"], config.loggerSection.panicLevel);
      }

      return config;
   }

private:
   static glm::ivec2 readIVec2(const YAML::Node& node, const glm::ivec2 fallback) {
      if (node && node.IsSequence() && node.size() == 2) {
         return {node[0].as<int>(), node[1].as<int>()};
      }
      return fallback;
   }

   static Log::Level readLevel(const YAML::Node& node, const Log::Level fallback) {
      if (!node) {
         return fallback;
      }
      static const std::unordered_map<std::string, Log::Level> levels{{"Trace", Log::Level::Trace}, {"Debug", Log::Level::Debug}, {"Info", Log::Level::Info},
                                                                      {"Warn", Log::Level::Warn},   {"Error", Log::Level::Error}, {"Critical", Log::Level::Critical},
                                                                      {"Fatal", Log::Level::Fatal}, {"Off", Log::Level::Off}};
      const auto name = node.as<std::string>();
      if (const auto it = levels.find(name); it != levels.end()) {
         return it->second;
      }
      Log::warn("config: unknown log level '{}'; keeping default", name);
      return fallback;
   }
};
