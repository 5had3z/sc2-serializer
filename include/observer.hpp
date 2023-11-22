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
    Point3f pos;// Point on map
    int qty;// Last observation
};

class BaseConverter : public sc2::ReplayObserver
{
  public:
    auto loadDB(const std::filesystem::path &path) noexcept -> bool;

    // Set Replay file hash + playerId before launching the coordinator
    void setReplayInfo(const std::string_view hash, std::uint32_t playerId) noexcept;

    void OnGameStart() override;

    void OnGameEnd() override;
    std::unordered_set<std::string> knownHashes{};

  protected:
    void copyHeightMapData() noexcept;

    void copyUnitData() noexcept;

    void copyActionData() noexcept;

    void copyDynamicMapData() noexcept;

    // Update resourceObs_ based on visible units
    void copyCommonData() noexcept;

    // Update resourceObs_ based on visible units
    // reassign jumbled UIDs to be consistent over the game
    // assign snapshot unis the last known quantity
    void updateResourceObs() noexcept;

    // Resource UID changes when going in and out of view
    // this is chat and we want to make UID consistent
    void reassignResourceId(const NeutralUnit &unit) noexcept;

    // Get the initial UID for each natural resource and
    // initialize with the default value
    void initResourceObs() noexcept;

    ReplayDatabase database_;
    ReplayData currentReplay_;
    std::unordered_map<UID, ResourceObs> resourceObs_;
    bool mapDynHasLogged_{ false };
    bool mapHeightHasLogged_{ false };
};

/**
 * @brief Convert and serialize every observation. This could be big.
 */
class FullConverter : public BaseConverter
{
    void OnStep() final;
};

/**
 * @brief The alphastar dataset only saves if the player makes an
 *        action and its associated preceding observation.
 */
class ActionConverter : public BaseConverter
{
    void OnStep() final;
};

/**
 * @brief Convert and serialize at a particular stride (i.e. every 10 steps)
 */
class StridedConverter : public BaseConverter
{
  public:
    // Set the sampling stride
    void SetStride(std::size_t stride) noexcept;

    // Get the current sampling stride
    auto GetStride() const noexcept -> std::size_t;

    void OnGameStart() final;

  private:
    void OnStep() final;

    std::size_t stride_{ 0 };
};

}// namespace cvt
