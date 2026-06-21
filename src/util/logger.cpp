#include "logger.hpp"

inline void Logger::init(const LoggerSettings& levels) {
   {
      setLevels(levels);
   }
}
