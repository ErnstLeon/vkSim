#pragma once

#include "spdlog/spdlog.h"

namespace vksim::logging
{

/**
 * @brief Sets the spdlog runtime log level according to compile-time log
 * level.
 *
 * This function reads the compile-time log level defined by
 * SPDLOG_ACTIVE_LEVEL and sets the corresponding runtime log level in
 * spdlog. It ensures that the runtime logger respects the same verbosity
 * as configured at compile time.
 */
inline void set_runtime_log_level()
{
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
  spdlog::set_level(spdlog::level::trace);
#elif SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG
  spdlog::set_level(spdlog::level::debug);
#elif SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_INFO
  spdlog::set_level(spdlog::level::info);
#elif SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_WARN
  spdlog::set_level(spdlog::level::warn);
#elif SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_ERROR
  spdlog::set_level(spdlog::level::err);
#elif SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_CRITICAL
  spdlog::set_level(spdlog::level::critical);
#else
  spdlog::set_level(spdlog::level::off);
#endif
}

} // namespace vksim::logging