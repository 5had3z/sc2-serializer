#include "database.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;
namespace bio = boost::iostreams;

namespace cvt {

std::shared_ptr<spdlog::logger> gLoggerDB = spdlog::stdout_color_mt("ReplayDatabase");

void setReplayDBLoggingLevel(spdlog::level::level_enum lvl) noexcept { gLoggerDB->set_level(lvl); }

}// namespace cvt
