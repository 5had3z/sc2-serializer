#pragma once

#include <sc2api/sc2_api.h>
#include <sc2api/sc2_score.h>

#include "database.hpp"

#include <filesystem>
#include <string_view>
#include <unordered_map>

namespace cvt {

struct ResourceObs
{
    UID id;// Original ID
    // cppcheck-suppress unusedStructMember
    Point3f pos;// Point on map
    // cppcheck-suppress unusedStructMember
    int qty;// Last observation
};

class BaseConverter : public sc2::ReplayObserver
{
  public:
    /**
     * @brief Loads the database from the specified path.
     * @param path The path to the database file.
     * @return True if the database was loaded successfully, false otherwise.
     */
    auto loadDB(const std::filesystem::path &path) noexcept -> bool;

    /**
     * @brief Sets the replay information for the BaseConverter.
     * @param hash The hash of the replay.
     * @param playerId The ID of the player.
     */
    void setReplayInfo(const std::string_view hash, std::uint32_t playerId) noexcept;

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
    [[nodiscard]] auto hasWritten() const noexcept -> bool;

    /**
     * @brief Checks if a given hash is known in the database.
     * @param hash The hash to check.
     * @return True if the hash is known, false otherwise.
     */
    [[nodiscard]] auto isKnownHash(const std::string &hash) const noexcept -> bool;


    /**
     * @brief Adds a known hash to the BaseConverter's list of known hashes.
     * @param hash The hash to be added.
     */
    void addKnownHash(std::string hash) noexcept;

    /**
     * @brief Clears the BaseConverter object.
     *
     * This function clears the internal state of the BaseConverter object.
     * It resets all the member variables to their default values.
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
     * @brief Updates the resource observer.
     * Update resourceObs_ based on visible units. Reassign jumbled UIDs to
     * be consistent over the game assign snapshot unis the last known quantity.
     */
    void updateResourceObs() noexcept;

    /**
     * @brief Reassigns the resource ID for a given NeutralUnit when it changes visibility.
     *        It gets reassociated with an old id based on locality.
     * @param unit The NeutralUnit for which the resource ID needs to be reassigned.
     * @return True if the resource ID was successfully reassigned, false otherwise.
     */
    auto reassignResourceId(const NeutralUnit &unit) noexcept -> bool;

    /**
     * @brief Initializes the resource observations with the first observation and default values.
     */
    void initResourceObs() noexcept;

    ReplayDatabase database_;
    ReplayData currentReplay_;
    std::unordered_map<UID, ResourceObs> resourceObs_;
    std::unordered_set<std::string> knownHashes_{};
    bool mapDynHasLogged_{ false };
    bool mapHeightHasLogged_{ false };
    bool writeSuccess_{ false };
};

/**
 * @brief Convert and serialize every observation. This could be big.
 */
class FullConverter : public BaseConverter
{
    /**
     * @brief Copies observation data every step.
     */
    void OnStep() final;
};

/**
 * @brief The alphastar dataset only saves if the player makes an
 *        action and its associated preceding observation.
 */
class ActionConverter : public BaseConverter
{
    /**
     * @brief Copies common data each step, will only commit that data on action observation.
     * Each step will have the previous step's observation and current step's action.
     */
    void OnStep() final;
};

/**
 * @brief Convert and serialize at a particular stride (i.e. every 10 steps)
 */
class StridedConverter : public BaseConverter
{
  public:
    /**
     * @brief Set the sampling stride
     * @param stride stride to set between 1 and 10'000
     */
    void SetStride(std::size_t stride) noexcept;

    /**
     * @brief Get the currently set stride
     * @return Currently set stride
     */
    auto GetStride() const noexcept -> std::size_t;

    /**
     * @brief Checks the stride has been set before normal start.
     */
    void OnGameStart() final;

  private:
    /**
     * @brief Only samples observation/action at strided intervals
     */
    void OnStep() final;

    // cppcheck-suppress unusedStructMember
    std::size_t stride_{ 0 };
};

}// namespace cvt
