#include "replay_parsing.hpp"

#include <pybind11/numpy.h>
#include <spdlog/spdlog.h>

#include <span>
#include <type_traits>

namespace cvt {

// Converts vector of units to a {n_unit, feature} array
template<typename T>
    requires std::is_floating_point_v<T> || std::is_integral_v<T>
auto transformUnits(const std::vector<Unit> &units) noexcept -> py::array_t<T>
{
    if (units.empty()) { return py::array_t<T>(); }

    auto vecFn = [](const Unit &unit) { return vectorize<T>(unit, true); };
    auto unitFeats = vecFn(units.front());
    py::array_t<T> featureArray({ units.size(), unitFeats.size() });
    auto rawData = std::span<T>(featureArray.mutable_data(), units.size() * unitFeats.size());
    auto rawDataIt = rawData.begin();
    for (const auto &unitFeats : units | std::views::drop(1) | std::views::transform(vecFn)) {
        rawDataIt = std::copy(unitFeats.begin(), unitFeats.end(), rawDataIt);
    }
    return featureArray;
}

ReplayParser::ReplayParser(const std::filesystem::path &dataFile) noexcept : upgrade_(dataFile) {}

void ReplayParser::parseReplay(ReplayDataSoA replayData)
{
    replayData_ = std::move(replayData);
    // SPDLOG_INFO("Replay: {}, Player: {}, Race: {}, Last Step: {}",
    //     replayData_.replayHash,
    //     replayData_.playerId,
    //     static_cast<int>(replayData_.playerRace),
    //     replayData_.gameStep.back());
    upgrade_.setRace(replayData_.playerRace);
    upgrade_.setVersion(replayData_.gameVersion);
    upgrade_.setActions(replayData_.actions, replayData_.gameStep);
}

auto ReplayParser::sample(std::size_t timeIdx) const noexcept -> py::dict
{
    py::dict result;
    result["upgrades"] = upgrade_.getState<float>(timeIdx);
    result["units"] = transformUnits<float>(replayData_.units[timeIdx]);
    return result;
}

}// namespace cvt
