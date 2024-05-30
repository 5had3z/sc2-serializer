/**
 * @file replay_parsing.hpp
 * @author Bryce Ferenczi
 * @brief Set of tools for parsing replay data into a more amenable form for machine learning. These are mainly used via
 * their Python API.
 * @version 0.1
 * @date 2024-05-27
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include "data_structures/replay_interface.hpp"
#include "data_structures/units.hpp"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <bitset>
#include <filesystem>
#include <ranges>
#include <set>
#include <span>
#include <type_traits>


namespace py = pybind11;

namespace cvt {

/**
 * @brief UpgradeState class loads information stored in yaml and determines the time points of the game when an
 * upgrade is active for the player. This class can be queried with getState with a gameStep to return a one-hot
 * encoding of the currently active upgrades.
 */
class UpgradeState
{
  public:
    /**
     * @brief Create upgrade timing instance using information based on dataFile.
     * @param dataFile Path to the yaml containing upgrade action data for each game version and race.
     */
    explicit UpgradeState(std::filesystem::path dataFile);

    /**
     * @brief Set the version of the game
     * @param version string description of the game version i.e. 4.9.1.12345
     */
    void setVersion(std::string_view version);

    /**
     * @brief Set the race of the player
     * @param race The race to set
     */
    void setRace(Race race) noexcept;

    /**
     * @brief Calculate and set the expected reserach completion times based on the actions and their timepoint
     * @param playerActions Vector of actions the player has performed at each timepoint
     * @param gameTime The timepoint in the game each action is associated with
     */
    void calculateTimes(const std::vector<std::vector<Action>> &playerActions,
        const std::vector<unsigned int> &gameTime);

    /**
     * @brief Get a one-hot encoding vector of the state of upgrades (0 false, 1 true)
     * @tparam T datatype of the one-hot encoding
     * @param timeIdx The timepoint to calculate the state of upgrades
     * @return one-hot encoding of currently active upgrades as a py::array
     */
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
    /**
     * @brief Get the set of actions ids that correspond to upgrade actions based on currentRace_ and gameVersion_
     * @return Set of upgrade actions
     */
    [[nodiscard]] auto getValidIds() const -> const std::set<int> &;

    /**
     * @brief Get remapping of generic tiered "upgrade" action to the distinct tiers.
     * @return Mapping from generic tiered action id to actual tier ids.
     */
    [[nodiscard]] auto getValidRemap() const -> const std::unordered_map<int, std::array<int, 3>> &;

    /**
     * @brief Fill out the id2delay_ mapping with the information from dataFile_ based on the currentRace_ and
     * gameVersion_
     */
    void loadInfo();

    /**
     * @brief Path to yaml file that contains upgrade action data from each game version and faction.
     */
    std::filesystem::path dataFile_;

    /**
     * @brief Mapping from action/upgrade id to the time it takes to research in game steps.
     */
    std::unordered_map<int, int> id2delay_{};

    /**
     * @brief Current race of the player.
     */
    Race currentRace_{ Race::Random };

    /**
     * @brief The times when the upgrade is active for the player.
     */
    std::vector<int32_t> upgradeTimes_{};

    /**
     * @brief The current version of the game.
     */
    std::string gameVersion_{};
};

/**
 * @brief Converts vector of unit structures to a {n_unit, feature} array
 * @tparam T output data type of the array
 * @tparam UnitT type of unit data
 * @param units Units to convert to array
 * @return 2D feature array of dimension {n_unit, feature}
 */
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

/**
 * @brief Unit transformation and return grouped by alliance {self, ally, enemy, neutral}
 * @tparam T output datatype of feature array
 * @param units Vector of units to convert to feature arrays
 * @return Dictionary mapping from alliance to feature array
 */
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
 * @brief Class that maps string description of minimap feature layers to a bit flag.
 */
struct MinimapFeatureFlags
{
    /**
     * @brief Minimap feature layers
     */
    inline static const std::array
        keys = { "heightMap", "visibility", "creep", "player_relative", "alerts", "buildable", "pathable" };

    /**
     * @brief Layer activation bits
     */
    std::bitset<keys.size()> flags = std::bitset<keys.size()>().set();

    /**
     * @brief Find the index of the layer in the bitset
     * @param key Name of the layer to search
     * @return Index in the layer array
     */
    constexpr auto getOffset(std::string_view key) const -> std::size_t
    {
        auto it = std::ranges::find(keys, key);
        if (it == keys.end()) {
            throw std::out_of_range{ fmt::format("Minimap feature key \"{}\" doesn't exist", key) };
        }
        return std::distance(keys.begin(), it);
    }

    /**
     * @brief Set all bits
     */
    void set() noexcept { flags.set(); }

    /**
     * @brief Set minimap layer to value
     * @param key Name of minimap layer
     * @param value Value to set (default: True)
     */
    void set(std::string_view key, bool value = true) { flags.set(getOffset(key), value); }

    /**
     * @brief Get the current value of the minimap layer
     * @param key Name of the minimap layer to test
     * @return Current activation status
     */
    auto test(std::string_view key) const -> bool { return flags.test(getOffset(key)); }

    /**
     * @brief Number of active minimap layers
     * @return Number of active minimap layers
     */
    auto count() const noexcept -> std::size_t { return flags.count(); }

    /**
     * @brief Set all minimap layers to false
     */
    void reset() noexcept { flags.reset(); }
};

/**
 * @brief Convenience wrapper around ReplayDataSOA to return map of features at each timestep
 * @tparam ReplayDataType Type of replay data being parsed
 */
template<typename ReplayDataType> class ReplayParser
{
  public:
    explicit ReplayParser(const std::filesystem::path &dataPath) noexcept : upgrade_(dataPath) {}

    /**
     * @brief Ingest replay data for sampling.
     * @param replayData Replay data to parse
     */
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
            upgrade_.calculateTimes(replayData_.data.actions, replayData_.data.gameStep);
        }
    }

    /**
     * @brief Set whether to expand player_relative to one hot encoding or keep as enum values
     * @param flag true will expand player_relaitive
     */
    void setPlayerMinimapExpansion(bool flag) noexcept { expandPlayerRelative_ = flag; }

    /**
     * @brief Get if the player_relative expansion flag has been set
     * @return if true then player_relative is expanded from enum to one-hot
     */
    [[nodiscard]] auto getPlayerMinimapExpansion() const noexcept -> bool { return expandPlayerRelative_; }

    /**
     * @brief Set the minimap features to stack and emit from the parser, if an empty list is given then all flags are
     * simply cleared/reset, if a single special key "all" is given then all flags are set.
     * @param features list of features to set, must be a member of cvt::MinimapFeatureFlags::keys or single key "all"
     */
    void setMinimapFeatures(const std::vector<std::string> &features)
    {
        // If only "all" key is in list of features then set all and return
        if (features.size() == 1 && features.front() == "all") {
            minimapFeatureFlags_.set();
            return;
        }

        minimapFeatureFlags_.reset();
        for (auto &&feature : features) { minimapFeatureFlags_.set(feature); }
    }

    /**
     * @brief Get the names of the minimap features of the currently set bits. If expandPlayerRelative is set
     *        then this will replace [..., player_relative, ...] with [..., self, ally, neutral, enemy, ...]
     * @return py::list of strings of the currently enabled features
     */
    [[nodiscard]] auto getMinimapFeatures() const -> py::list
    {
        py::list ret;
        for (const auto &feat : minimapFeatureFlags_.keys) {
            if (feat == "player_relative" && minimapFeatureFlags_.test(feat)) {
                ret.append("self");
                ret.append("ally");
                ret.append("neutral");
                ret.append("enemy");
            } else if (minimapFeatureFlags_.test(feat)) {
                ret.append(std::string(feat));
            }
        }
        return ret;
    }

    /**
     * @brief Get a python dictionary containing features from that timestep
     * @param timeIdx time index to sample
     * @param unit_alliance Whether to group units by alliance in a dictionary (default: false)
     * @return Dictionary containing feature data at that point in time
     */
    [[nodiscard]] auto sample(std::size_t timeIdx, bool unit_alliance = false) const -> py::dict
    {
        using feature_t = float;
        using step_data_t = typename ReplayDataType::step_type;
        py::dict result;

        if constexpr (HasUnitData<step_data_t>) {
            // Process Units into feature vectors, maybe in inner dictionary separated by alliance
            if (unit_alliance) {
                result["units"] = transformUnitsByAlliance<feature_t>(replayData_.data.units[timeIdx]);
            } else {
                result["units"] = transformUnits<feature_t>(replayData_.data.units[timeIdx]);
            }
            result["neutral_units"] = transformUnits<feature_t>(replayData_.data.neutralUnits[timeIdx]);
            static_assert(
                HasActionData<decltype(replayData_.data)> && "If unit data is present, so should action data");
        }

        if constexpr (HasActionData<decltype(replayData_.data)>) {
            // Create python list of actions, but keep in native struct rather than feature vector
            py::list actions;
            std::ranges::for_each(replayData_.data.actions[timeIdx], [&](const Action &a) { actions.append(a); });
            result["actions"] = actions;

            // Get one-hot active upgrade/research state
            result["upgrade_state"] = upgrade_.getState<feature_t>(timeIdx);
        }

        if constexpr (HasMinimapData<step_data_t>) {
            // Create feature image or minimap and feature vector of game state scalars (score, vespene, pop army etc.)
            result["minimaps"] =
                createMinimapFeatures<feature_t>(replayData_, timeIdx, minimapFeatureFlags_, expandPlayerRelative_);
        }

        if constexpr (HasScalarData<step_data_t>) {
            result["scalars"] = createScalarFeatures<feature_t>(replayData_.data, timeIdx);
        }

        static_assert((HasScalarData<step_data_t> || HasMinimapData<step_data_t> || HasUnitData<step_data_t>
                       || HasActionData<decltype(replayData_.data)>)&&"At least one data type should be present");

        return result;
    }

    /**
     * @brief Number of timesteps in the replay
     * @return Number of timesteps in the replay
     */
    [[nodiscard]] auto size() const noexcept -> std::size_t { return replayData_.data.gameStep.size(); }

    /**
     * @brief Check if the replay/parser is empty
     * @return True if empty
     */
    [[nodiscard]] auto empty() const noexcept -> bool { return replayData_.data.gameStep.empty(); }

    /**
     * @brief Read-only reference to the currently loaded data
     * @return Read-only reference to the currently loaded replay data
     */
    [[nodiscard]] auto data() const noexcept -> const auto & { return replayData_.data; }


    /**
     * @brief Read-only reference to the currently loaded replay header
     * @return Read-only reference to the currently loaded replay header
     */
    [[nodiscard]] auto info() const noexcept -> const ReplayInfo & { return replayData_.header; }

  private:
    /**
     * @brief Upgrade timing calculator
     */
    UpgradeState upgrade_;


    /**
     * @brief Replay data
     */
    ReplayDataType replayData_{};


    /**
     * @brief Minimap feature flags requested
     */
    MinimapFeatureFlags minimapFeatureFlags_{};

    /**
     * @brief Flag to expand player relative from enum to one-hot in minimap.
     */
    bool expandPlayerRelative_{ true };
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
[[maybe_unused]] auto unpackBoolImage(const Image<bool> &image, It out) -> It
{
    for (std::size_t i = 0; i < image.size(); ++i) {
        const auto bitset = std::bitset<8>(std::to_integer<uint8_t>(image._data[i]));
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
auto unpackBoolImage(const Image<bool> &image) noexcept -> std::vector<T>
{
    std::vector<T> unpacked_data(image.nelem(), 0);
    unpackBoolImage(image, unpacked_data.begin());
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

/**
 * @brief Create Stacked Features Image from Minimap Data (C, H, W) in the order
 *        HeightMap, Visibility, Creep, Alerts, Buildable, Pathable, PlayerRelative
 * @tparam T Underlying type of returned image
 * @param data Replay data
 * @param timeIdx Time index to sample from
 * @param expandPlayerRel Expand the Player Relative to four 1-hot channels (see cvt::Alliance)
 * @return Returns (C,H,W) Image of Type T
 */
template<typename T, typename ReplayDataType>
    requires std::is_arithmetic_v<T>
auto createMinimapFeatures(const ReplayDataType &replay,
    std::size_t timeIdx,
    MinimapFeatureFlags includedLayers,
    bool expandPlayerRel = true) -> py::array_t<T>
{
    const std::size_t nChannels = includedLayers.count() + (expandPlayerRel ? 3 : 0);
    py::array_t<T> featureMap({ nChannels,
        static_cast<std::size_t>(replay.header.heightMap._h),
        static_cast<std::size_t>(replay.header.heightMap._w) });
    std::span outData(featureMap.mutable_data(), featureMap.size());
    auto dataPtr = outData.begin();

    if (includedLayers.test("heightMap")) {
        if (replay.header.heightMap.empty()) {
            throw std::runtime_error{ "Tried to get heightMap data but it was empty" };
        }
        dataPtr = std::ranges::transform(replay.header.heightMap.as_span(), dataPtr, Caster<T>()).out;
    }

    if (includedLayers.test("visibility")) {
        if (replay.data.visibility[timeIdx].empty()) {
            throw std::runtime_error{ "Tried to get visibility data but it was empty" };
        }
        dataPtr = std::ranges::transform(replay.data.visibility[timeIdx].as_span(), dataPtr, Caster<T>()).out;
    }

    if (includedLayers.test("creep")) {
        if (replay.data.creep[timeIdx].empty()) {
            throw std::runtime_error{ "Tried to get creep data but it was empty" };
        }
        dataPtr = unpackBoolImage<T>(replay.data.creep[timeIdx], dataPtr);
    }

    if (includedLayers.test("player_relative")) {
        if (replay.data.player_relative[timeIdx].empty()) {
            throw std::runtime_error{ "Tried to get player_relative data but it was empty" };
        }
        if (expandPlayerRel) {
            dataPtr = expandPlayerRelative<T>(replay.data.player_relative[timeIdx], dataPtr);
        } else {
            dataPtr = std::ranges::transform(replay.data.player_relative[timeIdx].as_span(), dataPtr, Caster<T>()).out;
        }
    }

    if (includedLayers.test("alerts")) {
        if (replay.data.alerts[timeIdx].empty()) {
            throw std::runtime_error{ "Tried to get alerts data but it was empty" };
        }
        dataPtr = std::ranges::transform(replay.data.alerts[timeIdx].as_span(), dataPtr, Caster<T>()).out;
    }

    if (includedLayers.test("buildable")) {
        if (replay.data.buildable[timeIdx].empty()) {
            throw std::runtime_error{ "Tried to get buildable data but it was empty" };
        }
        dataPtr = unpackBoolImage<T>(replay.data.buildable[timeIdx], dataPtr);
    }

    if (includedLayers.test("pathable")) {
        if (replay.data.pathable[timeIdx].empty()) {
            throw std::runtime_error{ "Tried to get pathable data but it was empty" };
        }
        dataPtr = unpackBoolImage<T>(replay.data.pathable[timeIdx], dataPtr);
    }

    return featureMap;
}

}// namespace cvt
