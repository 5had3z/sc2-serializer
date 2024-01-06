#include "database.hpp"
#include "serialize.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <fstream>

namespace fs = std::filesystem;
namespace bio = boost::iostreams;

namespace cvt {

std::shared_ptr<spdlog::logger> gLoggerDB = spdlog::stdout_color_mt("ReplayDatabase");

void setReplayDBLoggingLevel(spdlog::level::level_enum lvl) noexcept { gLoggerDB->set_level(lvl); }

template<> struct DatabaseInterface<ReplayDataSoA>
{
    static auto getHashIdImpl(std::istream &dbStream) -> std::pair<std::string, std::uint32_t>
    {
        std::string replayHash{};
        std::string gameVersion{};
        std::uint32_t playerId{};
        deserialize(replayHash, dbStream);
        deserialize(gameVersion, dbStream);
        deserialize(playerId, dbStream);
        return std::make_pair(replayHash, playerId);
    }

    static auto getEntryImpl(std::istream &dbStream) noexcept -> ReplayDataSoA
    {
        ReplayDataSoA result;
        deserialize(result, dbStream);
        return result;
    }

    static auto addEntryImpl(const ReplayDataSoA &d, std::ostream &dbStream) noexcept -> bool
    {
        serialize(d, dbStream);
        return true;
    }
};


}// namespace cvt
