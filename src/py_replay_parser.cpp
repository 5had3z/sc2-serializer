#include "replay_parsing.hpp"

#include <pybind11/numpy.h>
#include <spdlog/spdlog.h>

#include <span>
#include <type_traits>

namespace cvt {

// Converts vector of units to a {n_unit, feature} array
template<typename T>
    requires std::is_arithmetic_v<T>
auto transformUnits(const std::vector<Unit> &units) noexcept -> py::array_t<T>
{
    // Return empty array if no units
    if (units.empty()) { return py::array_t<T>(); }
    // Lambda wrapper around vectorize to set onehot to true
    auto vecFn = [](const Unit &unit) { return vectorize<T>(unit, true); };

    // Create numpy array based on unit feature size
    const auto firstUnitFeats = vecFn(units.front());
    py::array_t<T> featureArray({ units.size(), firstUnitFeats.size() });

    // Interpret numpy array as contiguous span to copy transformed data
    std::span<T> rawData(featureArray.mutable_data(), units.size() * firstUnitFeats.size());
    auto rawDataIt = rawData.begin();

    // Start off by copying the already transformed data, then loop over the rest
    rawDataIt = std::copy(firstUnitFeats.begin(), firstUnitFeats.end(), rawDataIt);
    for (const auto &unitFeats : units | std::views::drop(1) | std::views::transform(vecFn)) {
        rawDataIt = std::copy(unitFeats.begin(), unitFeats.end(), rawDataIt);
    }
    return featureArray;
}

// Do unit transformation but group them by alliance (self, ally, enemy, neutral)
template<typename T>
    requires std::is_arithmetic_v<T>
auto transformUnitsByAlliance(const std::vector<Unit> &units) noexcept -> py::dict
{
    const static std::unordered_map<cvt::Alliance, std::string> enum2str = {
        { cvt::Alliance::Ally, "ally" },
        { cvt::Alliance::Neutral, "neutral" },
        { cvt::Alliance::Self, "self" },
        { cvt::Alliance::Enemy, "enemy" },
    };

    // Return dict of empty arrays if no units
    if (units.empty()) {
        py::dict pyDict;
        for (auto &&[ignore, name] : enum2str) { pyDict[py::cast(name)] = py::array_t<T>(); }
        return pyDict;
    }

    // Lambda wrapper around vectorize to set onehot to true
    auto vecFn = [](const Unit &unit) { return vectorize<T>(unit, true); };

    std::unordered_map<cvt::Alliance, std::vector<T>> groupedUnitFeatures = { { cvt::Alliance::Self, {} },
        { cvt::Alliance::Ally, {} },
        { cvt::Alliance::Enemy, {} },
        { cvt::Alliance::Neutral, {} } };

    for (auto &&unit : units) {
        auto &group = groupedUnitFeatures.at(unit.alliance);
        const auto unitFeat = vecFn(unit);
        std::ranges::copy(unitFeat, std::back_inserter(group));
    }

    const std::size_t featureSize = vecFn(units.front()).size();
    py::dict pyReturn;
    for (auto &&[group, features] : groupedUnitFeatures) {
        const std::size_t nUnits = features.size() / featureSize;
        py::array_t<T> pyArray({ nUnits, featureSize });
        std::ranges::copy(features, pyArray.mutable_data());
        pyReturn[py::cast(enum2str.at(group))] = pyArray;
    }
    return pyReturn;
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

auto ReplayParser::sample(std::size_t timeIdx, bool unit_alliance) const noexcept -> py::dict
{
    py::dict result;
    result["upgrades"] = upgrade_.getState<float>(timeIdx);
    if (unit_alliance) {
        result["units"] = transformUnitsByAlliance<float>(replayData_.units[timeIdx]);
    } else {
        result["units"] = transformUnits<float>(replayData_.units[timeIdx]);
    }
    py::list actions;
    std::ranges::for_each(replayData_.actions[timeIdx], [&](const Action &a) { actions.append(a); });
    result["actions"] = actions;
    return result;
}

auto ReplayParser::size() const noexcept -> std::size_t { return replayData_.gameStep.size(); }

auto ReplayParser::empty() const noexcept -> bool { return replayData_.gameStep.empty(); }

auto ReplayParser::data() const noexcept -> const ReplayDataSoA & { return replayData_; }

}// namespace cvt
