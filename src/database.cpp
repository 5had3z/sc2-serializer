/**
 * @file database.cpp
 * @author Bryce Ferenczi
 * @brief Loose ends from database.hpp
 * @version 0.1
 * @date 2024-05-28
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "database.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;
namespace bio = boost::iostreams;

namespace cvt {

std::shared_ptr<spdlog::logger> gLoggerDB = spdlog::stdout_color_mt("ReplayDatabase");

void setReplayDBLoggingLevel(spdlog::level::level_enum lvl) noexcept { gLoggerDB->set_level(lvl); }

}// namespace cvt
