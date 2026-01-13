#pragma once

#include <atomic>
#include <chrono>
#include <format>
#include <iostream>
#include <source_location>
#include <string_view>

class Log {
public:
   enum class Level : uint8_t {
      Trace = 0,
      Debug,
      Info,
      Warn,
      Error,
      Critical,
      Fatal,
      Off
   };

private:
   struct LevelConfig {
      std::string_view Label;
      bool ShowLocation;
   };

   static constexpr LevelConfig getConfig(Level level) {
      switch (level) {
      case Level::Trace:    return {"TRACE", true};
      case Level::Debug:    return {"DEBUG", true};
      case Level::Info:     return {"INFO ", true};
      case Level::Warn:     return {"WARN ", true};
      case Level::Error:    return {"ERROR", true};
      case Level::Critical: return {"CRIT ", true};
      case Level::Fatal:    return {"FATAL", true};
      default:              return {"UNKNOWN", true};
      }
   }

   static inline std::atomic<Level> s_MinLevel{Level::Trace};

public:
   static void setLevel(Level level) { s_MinLevel.store(level); }

   template<typename... Args>
   static void trace(std::format_string<Args...> fmt, Args&&... args) {
      LogImpl<Level::Trace>(fmt, std::forward<Args>(args)...);
   }

   template<typename... Args>
   static void info(std::format_string<Args...> fmt, Args&&... args) {
      LogImpl<Level::Info>(fmt, std::forward<Args>(args)...);
   }

   template<typename... Args>
   static void warn(std::format_string<Args...> fmt, Args&&... args) {
      LogImpl<Level::Warn>(fmt, std::forward<Args>(args)...);
   }

   template<typename... Args>
   static void error(std::format_string<Args...> fmt, Args&&... args, const std::source_location& loc = std::source_location::current()) {
      logImpl<Level::Error>(fmt, std::forward<Args>(args)..., loc);
   }

   template<typename... Args>
   static void critical(std::format_string<Args...> fmt, Args&&... args, const std::source_location& loc = std::source_location::current()) {
      LogImpl<Level::Critical>(fmt, std::forward<Args>(args)..., loc);
   }

   template<typename... Args>
   static void fatal(std::format_string<Args...> fmt, Args&&... args, const std::source_location& loc = std::source_location::current()) {
      logImpl<Level::Fatal>(fmt, std::forward<Args>(args)..., loc);
   }

private:
   template<Level L, typename... Args>
   static void logImpl(std::format_string<Args...> fmt, Args&&... args, const std::source_location& loc = std::source_location::current()) {
      if (L < s_MinLevel.load()) {
         return;
      }

      constexpr LevelConfig Config = getConfig(L);

      const auto now = std::chrono::system_clock::now();

      std::string message = std::format(fmt, std::forward<Args>(args)...);

      if constexpr (Config.ShowLocation) {
         std::clog << std::format("[{:%T}] [{}] {} [at {}:{}]\n", now, Config.Label, message, loc.file_name(), loc.line());
      } else {
         std::clog << std::format("[{:%T}] [{}] {}\n", now, Config.Label, message);
      }
   }
};
