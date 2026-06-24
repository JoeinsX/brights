#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <format>
#include <iostream>
#include <source_location>
#include <string_view>
#include <type_traits>
#include <utility>

class Settings;
struct LoggerSettings;

class Logger {
public:
   enum class Level : uint8_t {
      Trace = 0,
      Debug,
      Info,
      Warn,
      Error,
      Critical,
      Fatal,
      Off,
      Default
   };

   template<typename... Args>
   struct Message {
      std::format_string<Args...> fmt;
      std::source_location loc;

      template<typename S>
      // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
      consteval Message(const S& fmt, std::source_location loc = std::source_location::current()): fmt(fmt), loc(loc) {}
   };

   void initAppComponent(Settings& settings);

   static void setLevels(const LoggerSettings& next, std::source_location loc = std::source_location::current());

   template<typename... Args>
   static void trace(const Message<std::type_identity_t<Args>...> msg, Args&&... args) {
      write(Level::Trace, msg, std::forward<Args>(args)...);
   }

   template<typename... Args>
   static void debug(const Message<std::type_identity_t<Args>...> msg, Args&&... args) {
      write(Level::Debug, msg, std::forward<Args>(args)...);
   }

   template<typename... Args>
   static void info(const Message<std::type_identity_t<Args>...> msg, Args&&... args) {
      write(Level::Info, msg, std::forward<Args>(args)...);
   }

   template<typename... Args>
   static void warn(const Message<std::type_identity_t<Args>...> msg, Args&&... args) {
      write(Level::Warn, msg, std::forward<Args>(args)...);
   }

   template<typename... Args>
   static void error(const Message<std::type_identity_t<Args>...> msg, Args&&... args) {
      write(Level::Error, msg, std::forward<Args>(args)...);
   }

   template<typename... Args>
   static void critical(const Message<std::type_identity_t<Args>...> msg, Args&&... args) {
      write(Level::Critical, msg, std::forward<Args>(args)...);
   }

   template<typename... Args>
   static void fatal(const Message<std::type_identity_t<Args>...> msg, Args&&... args) {
      write(Level::Fatal, msg, std::forward<Args>(args)...);
   }

private:
   template<typename... Args>
   static void write(const Level level, const Message<Args...>& msg, Args&&... args) {
      if (level < writeLevel.load(std::memory_order_relaxed)) {
         return;
      }

      const std::string text = std::format(msg.fmt, std::forward<Args>(args)...);
      emit(label(level), text, msg.loc, level >= showLocationLevel.load(std::memory_order_relaxed), level >= flushLevel.load(std::memory_order_relaxed));

      if (level >= panicLevel.load(std::memory_order_relaxed)) {
         panic();
      }
   }

   static void emit(const std::string_view tag, const std::string_view text, const std::source_location loc, const bool showLocation, const bool flush) {
      const auto now = std::chrono::system_clock::now();

      if (showLocation) {
         std::string_view file = loc.file_name();
         file.remove_prefix(file.find_last_of("/\\") + 1);
         std::clog << std::format("[{:%T}] [{:5}] {} [at {}:{}]\n", now, tag, text, file, loc.line());
      } else {
         std::clog << std::format("[{:%T}] [{:5}] {}\n", now, tag, text);
      }

      if (flush) {
         std::clog.flush();
      }
   }

   [[noreturn]] static void panic() {
      std::clog.flush();
#ifdef LOG_PANIC_HANG
      while (true) {
      }
#else
      std::terminate();
#endif
   }

   static constexpr std::string_view label(const Level level) {
      switch (level) {
      case Level::Trace:    return "TRACE";
      case Level::Debug:    return "DEBUG";
      case Level::Info:     return "INFO";
      case Level::Warn:     return "WARN";
      case Level::Error:    return "ERROR";
      case Level::Critical: return "CRIT";
      case Level::Fatal:    return "FATAL";
      case Level::Off:      return "OFF";
      default:              return "?????";
      }
   }

   static inline std::atomic<Level> writeLevel{Level::Trace};
   static inline std::atomic<Level> showLocationLevel{Level::Trace};
   static inline std::atomic<Level> flushLevel{Level::Error};
   static inline std::atomic<Level> panicLevel{Level::Off};
};

struct LoggerSettings {
   Logger::Level writeLevel{Logger::Level::Default};
   Logger::Level showLocationLevel{Logger::Level::Default};
   Logger::Level flushLevel{Logger::Level::Default};
   Logger::Level panicLevel{Logger::Level::Default};

   static constexpr const char* key = "logger";

   template<typename Self, typename Fn>
   static void forEachField(Self& self, Fn&& fn) {
      fn("writeLevel", self.writeLevel);
      fn("showLocationLevel", self.showLocationLevel);
      fn("flushLevel", self.flushLevel);
      fn("panicLevel", self.panicLevel);
   }
};
