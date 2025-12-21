#pragma once

#include <iostream>
#include <string_view>
#include <format>
#include <chrono>
#include <source_location>
#include <atomic>

class Log {
public:
    enum class Level : uint8_t {
        Trace = 0, Debug, Info, Warn, Error, Critical, Fatal, Off
    };

private:
    struct LevelConfig {
        std::string_view Label;
        bool ShowLocation;
    };

    static constexpr LevelConfig GetConfig(Level level) {
        switch(level) {
        case Level::Trace: return {"TRACE", true};
        case Level::Debug: return {"DEBUG", true};
        case Level::Info: return {"INFO ", true};
        case Level::Warn: return {"WARN ", true};
        case Level::Error: return {"ERROR", true};
        case Level::Critical: return {"CRIT ", true};
        case Level::Fatal: return {"FATAL", true};
        default: return {"UNKNOWN", true};
        }
    }

    static inline std::atomic<Level> s_MinLevel{Level::Trace};

public:
    static void SetLevel(Level level) { s_MinLevel.store(level); }

    template<typename... Args>
    static void Trace(std::format_string<Args...> fmt, Args&&... args) {
        LogImpl<Level::Trace>(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Info(std::format_string<Args...> fmt, Args&&... args) {
        LogImpl<Level::Info>(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Warn(std::format_string<Args...> fmt, Args&&... args) {
        LogImpl<Level::Warn>(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Error(std::format_string<Args...> fmt, Args&&... args,
                      const std::source_location& loc =
                          std::source_location::current()) {
        LogImpl<Level::Error>(fmt, std::forward<Args>(args)..., loc);
    }

    template<typename... Args>
    static void Critical(std::format_string<Args...> fmt, Args&&... args,
                         const std::source_location& loc =
                             std::source_location::current()) {
        LogImpl<Level::Critical>(fmt, std::forward<Args>(args)..., loc);
    }

    template<typename... Args>
    static void Fatal(std::format_string<Args...> fmt, Args&&... args,
                      const std::source_location& loc =
                          std::source_location::current()) {
        LogImpl<Level::Fatal>(fmt, std::forward<Args>(args)..., loc);
    }

private:
    template<Level L, typename... Args>
    static void LogImpl(std::format_string<Args...> fmt, Args&&... args,
                        const std::source_location& loc =
                            std::source_location::current()) {
        if(L < s_MinLevel.load()) return;

        constexpr LevelConfig Config = GetConfig(L);

        const auto now = std::chrono::system_clock::now();

        std::string message = std::format(fmt, std::forward<Args>(args)...);

        if constexpr(Config.ShowLocation) {
            std::clog << std::format("[{:%T}] [{}] {} [at {}:{}]\n",
                                     now, Config.Label, message,
                                     loc.file_name(), loc.line());
        } else {
            std::clog << std::format("[{:%T}] [{}] {}\n",
                                     now, Config.Label, message);
        }
    }
};