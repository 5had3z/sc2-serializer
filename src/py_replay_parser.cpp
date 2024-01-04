#include "replay_parsing.hpp"

#include <pybind11/numpy.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <span>
#include <type_traits>

namespace cvt {

// Converts vector of units to a {n_unit, feature} array
template<typename T, typename UnitT>
    requires std::is_arithmetic_v<T>
[[nodiscard]] auto transformUnits(const std::vector<UnitT> &units) noexcept -> py::array_t<T>
{
    // Return empty array if no units
    if (units.empty()) { return py::array_t<T>(); }
    // Lambda wrapper around vectorize to set onehot to true
    auto vecFn = [](const UnitT &unit) { return vectorize<T>(unit, true); };

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
[[nodiscard]] auto transformUnitsByAlliance(const std::vector<Unit> &units) noexcept -> py::dict
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
        for (auto &&name : enum2str | std::views::values) { pyDict[py::cast(name)] = py::array_t<T>(); }
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


template<typename T, std::output_iterator<T> It>
[[maybe_unused]] auto expandPlayerRelative(const Image<std::uint8_t> &img, It out) noexcept -> It
{
    // First zero out image data
    constexpr std::size_t nAlliance = 4;
    std::ranges::fill(std::ranges::subrange(out, out + img.nelem() * nAlliance), 0);
    const auto imgData = img.as_span();
    for (std::size_t idx = 0; idx < img.nelem(); ++idx) {
        assert(imgData[idx] < nAlliance && "Got invalid player relative > 4");
        if (imgData[idx] > 0) {
            std::size_t chOffset = imgData[idx] - 1;
            auto outPtr = std::next(out, idx + chOffset * img.nelem());
            *outPtr = 1;
        }
    }
    return std::next(out, img.nelem() * nAlliance);
}

template<typename B>
    requires std::is_arithmetic_v<B>
struct Caster
{
    [[nodiscard]] auto operator()(auto a) const noexcept -> B { return static_cast<B>(a); }
};

/**
 * @brief Create Stacked Features Image from Minimap Data (C, H, W) in the order
 *        HeightMap, Visibility, Creep, Alerts, Buildable, Pathable, PlayerRelative
 * @tparam T Underlying type of returned image
 * @param data Replay data
 * @param timeIdx Time index to sample from
 * @param expandPlayerRel Expand the Player Relative to 1hot channels (Self1, Ally2, Neutral3,
 * Enemy4)
 * @return Returns (C,H,W) Image of Type T
 */
template<typename T>
    requires std::is_arithmetic_v<T>
auto createMinimapFeatures(const ReplayDataSoA &data, std::size_t timeIdx, bool expandPlayerRel = true)
    -> py::array_t<T>
{
    const std::size_t nChannels = expandPlayerRel ? 10 : 7;
    py::array_t<T> featureMap(
        { nChannels, static_cast<std::size_t>(data.heightMap._h), static_cast<std::size_t>(data.heightMap._w) });
    std::span outData(featureMap.mutable_data(), featureMap.size());
    auto dataPtr = outData.begin();
    dataPtr = std::ranges::transform(data.heightMap.as_span(), dataPtr, Caster<T>()).out;
    dataPtr = std::ranges::transform(data.visibility[timeIdx].as_span(), dataPtr, Caster<T>()).out;
    dataPtr = unpackBoolImage<T>(data.creep[timeIdx], dataPtr);
    dataPtr = std::ranges::transform(data.alerts[timeIdx].as_span(), dataPtr, Caster<T>()).out;
    dataPtr = unpackBoolImage<T>(data.buildable[timeIdx], dataPtr);
    dataPtr = unpackBoolImage<T>(data.pathable[timeIdx], dataPtr);
    if (expandPlayerRel) {
        // cppcheck-suppress unreadVariable
        dataPtr = expandPlayerRelative<T>(data.player_relative[timeIdx], dataPtr);
    } else {
        // cppcheck-suppress unreadVariable
        dataPtr = std::ranges::transform(data.player_relative[timeIdx].as_span(), dataPtr, Caster<T>()).out;
    }
    return featureMap;
}


/**
 * @brief Convert game state of scalars into a feature vector.
 *        TODO Have a dictionary of lambda functions that can normalize each of these features.
 *        This could also be a convenient way to turn features on and off (i.e. lambda returns zero)
 * @tparam T feature vector arithmetic type
 * @param data replay data
 * @param timeIdx time index to sample from replay
 * @return Feature vector of data
 */
template<typename T>
    requires std::is_arithmetic_v<T>
auto createScalarFeatures(const ReplayDataSoA &data, std::size_t timeIdx) -> py::array_t<T>
{
    auto feats = vectorize<T>(data.score[timeIdx]);
    feats.emplace_back(data.minearals[timeIdx]);
    feats.emplace_back(data.vespene[timeIdx]);
    feats.emplace_back(data.popMax[timeIdx]);
    feats.emplace_back(data.popArmy[timeIdx]);
    feats.emplace_back(data.popWorkers[timeIdx]);
    feats.emplace_back(data.gameStep[timeIdx]);
    py::array_t<T> array({ static_cast<py::ssize_t>(feats.size()) });
    std::ranges::copy(feats, array.mutable_data());
    return array;
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
    using feature_t = float;
    py::dict result;

    // Get one-hot active upgrade/research state
    result["upgrade_state"] = upgrade_.getState<feature_t>(timeIdx);

    // Process Units into feature vectors, maybe in inner dictionary separated by alliance
    if (unit_alliance) {
        result["units"] = transformUnitsByAlliance<feature_t>(replayData_.units[timeIdx]);
    } else {
        result["units"] = transformUnits<feature_t>(replayData_.units[timeIdx]);
    }
    result["neutral_units"] = transformUnits<feature_t>(replayData_.neutralUnits[timeIdx]);

    // Create python list of actions, but keep in native struct rather than feature vector
    py::list actions;
    std::ranges::for_each(replayData_.actions[timeIdx], [&](const Action &a) { actions.append(a); });
    result["actions"] = actions;

    // Create feature image or minimap and feature vector of game state scalars (score, vespene, pop army etc.)
    result["minimap_features"] = createMinimapFeatures<feature_t>(replayData_, timeIdx);
    result["scalar_features"] = createScalarFeatures<feature_t>(replayData_, timeIdx);

    return result;
}

auto ReplayParser::size() const noexcept -> std::size_t { return replayData_.gameStep.size(); }

auto ReplayParser::empty() const noexcept -> bool { return replayData_.gameStep.empty(); }

auto ReplayParser::data() const noexcept -> const ReplayDataSoA & { return replayData_; }

}// namespace cvt
