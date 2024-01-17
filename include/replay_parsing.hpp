#pragma once

#include "replay_structures.hpp"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <bitset>
#include <filesystem>
#include <set>
#include <span>
#include <type_traits>


namespace py = pybind11;

namespace cvt {
class UpgradeTiming
{
  public:
    // cppcheck-suppress noExplicitConstructor
    UpgradeTiming(std::filesystem::path dataFile);

    // Set the game version
    void setVersion(std::string_view version);

    // Set the race of the player
    void setRace(Race race) noexcept;

    // Create the expected research timings from the actions and gameStep vectors
    void setActions(const std::vector<std::vector<Action>> &actions, const std::vector<unsigned int> &timeIdxs);

    // Get the state of research at timeIdx, (0 false, 1 true)
    template<typename T>
        requires std::is_arithmetic_v<T>
    [[nodiscard]] auto getState(std::size_t timeIdx) const -> py::array_t<T>
    {
        py::array_t<T> state({ static_cast<py::ssize_t>(upgradeTimes_.size()) });
        std::ranges::transform(
            upgradeTimes_, state.mutable_data(), [=](int32_t time) { return static_cast<T>(timeIdx > time ? 1 : 0); });
        return state;
    }


  private:
    [[nodiscard]] auto getValidIds() const -> const std::set<int> &;

    [[nodiscard]] auto getValidRemap() const -> const std::unordered_map<int, std::array<int, 3>> &;

    void loadInfo();

    std::filesystem::path dataFile_;
    std::unordered_map<int, int> id2delay_{};
    Race currentRace_{ Race::Random };
    std::vector<int32_t> upgradeTimes_{};
    std::string gameVersion_{};
};

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


/**
 * @brief Converts an image of player relative enums to one-hot encoding, i.e. a mask of each class type
 * @tparam T value type of the image returned
 * @tparam It iterator type to write result to
 * @param img enum image of player_relative data, assumes SC2 data [1-4] (cvt::Alliance)
 * @param out output iterator to write result to, must be preallocated contiguous data to write image to
 * @return output iterator one past the output image data
 */
template<typename T, std::output_iterator<T> It>
[[maybe_unused]] auto expandPlayerRelative(const Image<std::uint8_t> &img, It out) noexcept -> It
{
    // First zero out image data
    constexpr std::size_t nAlliance = 4;
    std::ranges::fill(std::ranges::subrange(out, out + img.nelem() * nAlliance), 0);
    const auto imgData = img.as_span();
    for (std::size_t idx = 0; idx < img.nelem(); ++idx) {
        assert(imgData[idx] <= nAlliance && "Got invalid player relative > 4");
        if (imgData[idx] > 0) {
            std::size_t chOffset = imgData[idx] - 1;
            auto outPtr = std::next(out, idx + chOffset * img.nelem());
            *outPtr = 1;
        }
    }
    return std::next(out, img.nelem() * nAlliance);
}


/**
 * @brief Helper Struct to static_cast one type to an arithmetic type
 * @tparam B type to cast to
 */
template<typename B>
    requires std::is_arithmetic_v<B>
struct Caster
{
    /**
     * @brief Cast the argument to arithmetic type B
     * @param a value to cast
     * @return casted value
     */
    [[nodiscard]] auto operator()(auto a) const noexcept -> B { return static_cast<B>(a); }
};

/**
 * @brief Create Stacked Features Image from Minimap Data (C, H, W) in the order
 *        HeightMap, Visibility, Creep, Alerts, Buildable, Pathable, PlayerRelative
 * @tparam T Underlying type of returned image
 * @param data Replay data
 * @param timeIdx Time index to sample from
 * @param expandPlayerRel Expand the Player Relative to four 1-hot channels (see cvt::Alliance)
 * @return Returns (C,H,W) Image of Type T
 */
template<typename T>
    requires std::is_arithmetic_v<T>
auto createMinimapFeatures(const ReplayData2SoA &replay, std::size_t timeIdx, bool expandPlayerRel = true)
    -> py::array_t<T>
{
    const std::size_t nChannels = expandPlayerRel ? 10 : 7;
    py::array_t<T> featureMap({ nChannels,
        static_cast<std::size_t>(replay.header.heightMap._h),
        static_cast<std::size_t>(replay.header.heightMap._w) });
    std::span outData(featureMap.mutable_data(), featureMap.size());
    auto dataPtr = outData.begin();
    dataPtr = std::ranges::transform(replay.header.heightMap.as_span(), dataPtr, Caster<T>()).out;
    dataPtr = std::ranges::transform(replay.data.visibility[timeIdx].as_span(), dataPtr, Caster<T>()).out;
    dataPtr = unpackBoolImage<T>(replay.data.creep[timeIdx], dataPtr);
    dataPtr = std::ranges::transform(replay.data.alerts[timeIdx].as_span(), dataPtr, Caster<T>()).out;
    dataPtr = unpackBoolImage<T>(replay.data.buildable[timeIdx], dataPtr);
    dataPtr = unpackBoolImage<T>(replay.data.pathable[timeIdx], dataPtr);
    if (expandPlayerRel) {
        dataPtr = expandPlayerRelative<T>(replay.data.player_relative[timeIdx], dataPtr);
    } else {
        dataPtr = std::ranges::transform(replay.data.player_relative[timeIdx].as_span(), dataPtr, Caster<T>()).out;
    }
    return featureMap;
}


// Convenience wrapper around ReplayDataSOA to return map of features at each timestep
template<typename ReplayDataType> class ReplayParser
{
  public:
    explicit ReplayParser(const std::filesystem::path &dataPath) noexcept : upgrade_(dataPath) {}

    // Parse replay data, ready to sample from
    void parseReplay(ReplayDataType replayData)
    {
        replayData_ = std::move(replayData);
        SPDLOG_DEBUG("Replay: {}, Player: {}, Race: {}, Last Step: {}",
            replayData_.header.replayHash,
            replayData_.header.playerId,
            static_cast<int>(replayData_.playerRace),
            replayData_.gameStep.back());

        // Can only parse upgrade state if actions are present
        if constexpr (HasActionData<decltype(replayData_.data)>) {
            upgrade_.setRace(replayData_.header.playerRace);
            upgrade_.setVersion(replayData_.header.gameVersion);
            upgrade_.setActions(replayData_.data.actions, replayData_.data.gameStep);
        }
    }

    // Returns a python dictionary containing features from that timestep
    [[nodiscard]] auto sample(std::size_t timeIdx, bool unit_alliance = false) const noexcept -> py::dict
    {
        using feature_t = float;
        py::dict result;

        if constexpr (HasUnitData<ReplayDataType>) {
            // Process Units into feature vectors, maybe in inner dictionary separated by alliance
            if (unit_alliance) {
                result["units"] = transformUnitsByAlliance<feature_t>(replayData_.data.units[timeIdx]);
            } else {
                result["units"] = transformUnits<feature_t>(replayData_.data.units[timeIdx]);
            }
            result["neutral_units"] = transformUnits<feature_t>(replayData_.data.neutralUnits[timeIdx]);
        }

        if constexpr (HasActionData<decltype(replayData_.data)>) {
            // Create python list of actions, but keep in native struct rather than feature vector
            py::list actions;
            std::ranges::for_each(replayData_.data.actions[timeIdx], [&](const Action &a) { actions.append(a); });
            result["actions"] = actions;

            // Get one-hot active upgrade/research state
            result["upgrade_state"] = upgrade_.getState<feature_t>(timeIdx);
        }

        if constexpr (HasMinimapData<ReplayDataType>) {
            // Create feature image or minimap and feature vector of game state scalars (score, vespene, pop army etc.)
            result["minimap_features"] = createMinimapFeatures<feature_t>(replayData_, timeIdx);
        }

        if constexpr (HasScalarData<ReplayDataType>) {
            result["scalar_features"] = createScalarFeatures<feature_t>(replayData_.data, timeIdx);
        }

        return result;
    }

    // Return the number of timesteps in the replay
    [[nodiscard]] auto size() const noexcept -> std::size_t { return replayData_.data.gameStep.size(); }

    // Check if parser is empty
    [[nodiscard]] auto empty() const noexcept -> bool { return replayData_.data.gameStep.empty(); }

    // Return read-only reference to currently loaded replay data
    [[nodiscard]] auto data() const noexcept -> const auto & { return replayData_.data; }

    [[nodiscard]] auto info() const noexcept -> const ReplayInfo & { return replayData_.header; }

  private:
    UpgradeTiming upgrade_;
    ReplayDataType replayData_{};
};


/**
 * @brief Unpack bool image to output iterator
 * @tparam T value type to unpack to
 * @param image input image
 * @param out span to write unpacked data into
 * @return vector of value type of bool image
 */
template<typename T, std::output_iterator<T> It>
    requires std::is_arithmetic_v<T>
[[maybe_unused]] auto unpackBoolImage(const Image<bool> &img, It out) -> It
{
    for (std::size_t i = 0; i < img.size(); ++i) {
        const auto bitset = std::bitset<8>(std::to_integer<uint8_t>(img._data[i]));
#pragma unroll
        for (int j = 7; j > -1; --j) { *out++ = static_cast<T>(bitset[j]); }
    }
    return out;
}

/**
 * @brief Unpack bool image to flattened std::vector
 * @tparam T value type to unpack to
 * @param image input image
 * @return vector of value type of bool image
 */
template<typename T>
    requires std::is_arithmetic_v<T>
auto unpackBoolImage(const Image<bool> &img) noexcept -> std::vector<T>
{
    std::vector<T> unpacked_data(img.nelem(), 0);
    unpackBoolImage(img, unpacked_data.begin());
    return unpacked_data;
}

/**
 * @brief Convert game state of scalars into a feature vector.
 *        TODO: Have a dictionary of lambda functions that can normalize each of these features?
 * @tparam T feature vector arithmetic type
 * @param data replay data
 * @param timeIdx time index to sample from replay
 * @return Feature vector of data
 */
template<typename T, IsSoAType StepDataType>
    requires std::is_arithmetic_v<T>
auto createScalarFeatures(const StepDataType &data, std::size_t timeIdx) -> py::array_t<T>
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

}// namespace cvt
