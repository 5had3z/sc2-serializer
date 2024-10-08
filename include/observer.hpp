/**
 * @file observer.hpp
 * @author Bryce Ferenczi
 * @brief Contains StarCraft II game observation classes. Each variant saves observations at different rates to balance
 * between verbosity and dataset size.
 * @version 0.1
 * @date 2024-05-27
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include <sc2api/sc2_api.h>
#include <spdlog/spdlog.h>

#include "data_structures/units.hpp"
#include "database.hpp"
#include "generated_info.hpp"

#include <filesystem>
#include <string_view>
#include <unordered_map>

namespace cvt {

/**
 * @brief Vespene/Minearal resource observation
 */
struct ResourceObs
{
    /**
     * @brief Original ID
     */
    UID id;

    /**
     * @brief Location on map
     */
    Point3f pos;

    /**
     * @brief Last observation quantity
     */
    int qty;
};

/**
 * @brief Base replay observer and converter that implements functions common to all the other sampling variants.
 * @tparam DataSoA observation data structure to be observed and serialized
 */
template<typename DataSoA> class BaseConverter : public sc2::ReplayObserver
{
  public:
    /**
     * @brief Loads the database from the specified path.
     * @param path The path to the database file.
     * @return True if the database was loaded successfully, false otherwise.
     */
    auto loadDB(const std::filesystem::path &path) -> bool
    {
        auto result = database_.open(path);
        if (result) { knownHashes_ = database_.getAllUIDs(); }
        return result;
    }

    /**
     * @brief Sets the replay information for the BaseConverter.
     * @param hash The hash of the replay.
     * @param playerId The ID of the player.
     */
    void setReplayInfo(std::string_view hash, std::uint32_t playerId) noexcept
    {
        replayData_.getReplayHash() = hash;
        replayData_.getPlayerId() = playerId;
    }

    /**
     * @brief This function is called when the game starts. Gathers basic replay info such as player mmr.
     */
    void OnGameStart() override;

    /**
     * @brief Checks if the game has ended in a normal state and then writes replay to database.
     */
    void OnGameEnd() override;

    // void OnError(const std::vector<sc2::ClientError> &clientErrors,
    //     const std::vector<std::string> &protocolErrors = {}) override;

    /**
     * @brief Checks if the BaseConverter has successfully written data.
     * @return true if the BaseConverter has successfully written data, false otherwise.
     */
    [[nodiscard]] auto hasWritten() const noexcept -> bool { return writeSuccess_; }

    /**
     * @brief Checks if a given hash is known in the database.
     * @param hash The hash to check.
     * @note const std::string& used rather than std::string_view due to lack of set::contains compatibility.
     * @return True if the hash is known, false otherwise.
     */
    [[nodiscard]] auto isKnownHash(const std::string &hash) const noexcept -> bool
    {
        return knownHashes_.contains(hash);
    }

    /**
     * @brief Adds a known hash to the BaseConverter's list of known hashes.
     * @param hash The hash to be added.
     */
    void addKnownHash(std::string_view hash) noexcept { knownHashes_.emplace(hash); }

    /**
     * @brief Clears the BaseConverter object. This function clears the internal state of the BaseConverter object. It
     * resets all the member variables to their default values.
     *
     * @note This function does not deallocate any memory.
     *
     * @see BaseConverter
     */
    void clear() noexcept;

  protected:
    /**
     * @brief Copies the height map data for the match.
     */
    void copyHeightMapData() noexcept;

    /**
     * @brief Copies the unit data from observation to stepData.back().
     */
    void copyUnitData() noexcept;

    /**
     * @brief Copies the action data to stepData.back().
     */
    void copyActionData() noexcept;

    /**
     * @brief Copies the dynamic map data to stepData.back().
     */
    void copyDynamicMapData() noexcept;

    /**
     * @brief Copies the common data such as scalars gameStep, minerals, vespene to stepData.back().
     */
    void copyCommonData() noexcept;

    /**
     * @brief Reassigns the resource ID for a given NeutralUnit when it changes visibility. It gets reassociated with an
     * old id based on locality.
     * @param unit The NeutralUnit for which the resource ID needs to be reassigned.
     * @return True if the resource ID was successfully reassigned, false otherwise.
     */
    auto reassignResourceId(const NeutralUnit &unit) noexcept -> bool
    {
        // Check if there's an existing unit with the same x,y coordinate
        // (may move a little bit in z, but its fundamentally the same unit)
        auto oldKV = std::ranges::find_if(resourceObs_, [=](auto &&keyValue) {
            const auto &value = keyValue.second;
            return value.pos.x == unit.pos.x && value.pos.y == unit.pos.y;
        });
        if (oldKV == resourceObs_.end()) {
            SPDLOG_WARN(
                "No matching position for unit {} (id: {}) adding new", sc2::UnitTypeToName(unit.unitType), unit.id);
            return false;
        } else {
            resourceObs_.emplace(unit.id, std::move(oldKV->second));
            resourceObs_.erase(oldKV->first);
            return true;
        }
    }

    /**
     * @brief Initializes the resource observations with the first observation and default values.
     */
    template<std::ranges::range R>
        requires std::same_as<std::ranges::range_value_t<R>, NeutralUnit>
    void initResourceObs(R &&neutralUnits) noexcept
    {
        for (auto &unit : neutralUnits) {
            // Skip Not a mineral/gas resource
            if (!defaultResources.contains(unit.unitType)) { continue; }
            resourceObs_.emplace(unit.id, ResourceObs{ unit.id, unit.pos, defaultResources.at(unit.unitType) });
        }
    }

    /**
     * @brief Updates the resource observer.
     * Update resourceObs_ based on visible units. Reassign jumbled UIDs to
     * be consistent over the game assign snapshot unis the last known quantity.
     */
    template<std::ranges::range R>
        requires std::same_as<std::ranges::range_value_t<R>, NeutralUnit>
    void updateResourceObs(R &&neutralUnits) noexcept
    {
        for (auto &unit : neutralUnits) {
            // Skip Not a mineral/gas resource
            if (!defaultResources.contains(unit.unitType)) { continue; }
            // Reassign map id if identical position found, otherwise add to set
            if (!resourceObs_.contains(unit.id)) {
                bool hasReassigned = this->reassignResourceId(unit);
                if (!hasReassigned) {
                    resourceObs_.emplace(unit.id, ResourceObs{ unit.id, unit.pos, defaultResources.at(unit.unitType) });
                }
            }

            auto &prev = resourceObs_.at(unit.id);
            // Update resourceObs_ if visible
            if (unit.observation == Visibility::Visible) { prev.qty = unit.contents; }

            // Set current step's neutral unit with consistent id and last observed qty
            unit.contents = prev.qty;
            unit.id = prev.id;
        }
    }

    ReplayDatabase<DataSoA> database_;
    DataSoA::struct_type replayData_;
    std::unordered_map<UID, ResourceObs> resourceObs_;
    std::unordered_set<std::string> knownHashes_{};
    bool mapDynHasLogged_{ false };
    bool mapHeightHasLogged_{ false };
    bool writeSuccess_{ false };
    std::chrono::high_resolution_clock::time_point start_{};
};

/**
 * @brief Convert and serialize every observation. This could be big.
 */
template<typename DataSoA> class FullConverter : public BaseConverter<DataSoA>
{
    /**
     * @brief Copies observation data every step.
     */
    void OnStep() final;
};

/**
 * @brief The alphastar dataset only saves if the player makes an action and its associated preceding observation.
 */
template<typename DataSoA> class ActionConverter : public BaseConverter<DataSoA>
{
    /**
     * @brief Copies common data each step, will only commit that data on action observation.
     * Each step will have the previous step's observation and current step's action.
     */
    void OnStep() final;
};

/**
 * @brief Convert and serialize at a particular stride (i.e. every 10 steps). Also has flag which enables saving on
 * player actions.
 */
template<typename DataSoA> class StridedConverter : public BaseConverter<DataSoA>
{
  public:
    /**
     * @brief Set the sampling stride
     * @param stride stride to set between 1 and 10'000
     */
    void SetStride(std::size_t stride)
    {
        if (stride == 0 || stride > 10'000) {
            throw std::logic_error(fmt::format("SetStride doesn't satisfy 0 < {} < 10'000", stride));
        }
        stride_ = stride;
    }

    /**
     * @brief Set whether gamesteps with actions should be saved
     * @param should_save true if actions are saved
     */
    void SetActionSaving(bool should_save) noexcept { saveActions_ = should_save; }

    /**
     * @brief Query if actions are being saved
     * @return True if actions are being saved
     */
    auto ActionsAreSaved() const noexcept -> bool { return saveActions_; }

    /**
     * @brief Get the currently set stride
     * @return Currently set stride
     */
    auto GetStride() const noexcept -> std::size_t { return stride_; }

    /**
     * @brief Checks the stride has been set before normal start.
     */
    void OnGameStart() final
    {
        if (stride_ == 0) { throw std::logic_error(fmt::format("Stride not set: {}", stride_)); }
        BaseConverter<DataSoA>::OnGameStart();
    }

  private:
    /**
     * @brief Only samples observation/action at strided intervals
     */
    void OnStep() final;

    /**
     * @brief Number of replay steps between saving observations.
     */
    std::size_t stride_{ 0 };

    /**
     * @brief Flag to also enable saving on player actions.
     */
    bool saveActions_{ false };
};

}// namespace cvt
