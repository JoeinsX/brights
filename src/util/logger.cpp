#include "util/logger.hpp"

#include "app/settings/settings.hpp"

void Logger::initAppComponent(Settings& settings) {
   settings.addSection<LoggerSettings>();
}

void Logger::setLevels(const LoggerSettings& next, const std::source_location loc) {
   if (next.writeLevel != Level::Default) {
      writeLevel.store(next.writeLevel, std::memory_order_relaxed);
   }
   if (next.showLocationLevel != Level::Default) {
      showLocationLevel.store(next.showLocationLevel, std::memory_order_relaxed);
   }
   if (next.flushLevel != Level::Default) {
      flushLevel.store(next.flushLevel, std::memory_order_relaxed);
   }
   if (next.panicLevel != Level::Default) {
      panicLevel.store(next.panicLevel, std::memory_order_relaxed);
   }

   const std::string summary = std::
      format("policy changed -> write={} loc={} flush={} panic={}, flushing", label(writeLevel.load(std::memory_order_relaxed)),
             label(showLocationLevel.load(std::memory_order_relaxed)), label(flushLevel.load(std::memory_order_relaxed)), label(panicLevel.load(std::memory_order_relaxed)));
   emit("logger-meta", summary, loc, true, true);
}
