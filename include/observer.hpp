#pragma once

#include <sc2api/sc2_api.h>

#include "database.hpp"

#include <filesystem>
#include <string_view>
#include <unordered_map>

namespace cvt {


class BaseConverter : public sc2::ReplayObserver
{
  public:
    auto loadDB(const std::filesystem::path &path) noexcept -> bool;

    // Set Replay file hash + playerId before launching the coordinator
    void setReplayInfo(const std::string_view hash, std::uint32_t playerId) noexcept;

    void OnGameStart() final;

    void OnGameEnd() final;

  protected:
    void copyHeightMapData() noexcept;

    void copyUnitData() noexcept;

    void copyActionData() noexcept;

    void copyDynamicMapData() noexcept;

    // Run over the neutral units in database_.stepData.back()
    // if the neutral unit is a resource and not in resouceQty_,
    // initialise and assign default.
    // If the resource observation is Snapshot, assign the
    // last resourceQty_ value, if Viewable update resourceQty_
    void manageResourceObservation() noexcept;

    ReplayDatabase database_;
    ReplayData currentReplay_;
    std::unordered_map<UID, int> resourceQty_;
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
 *        action and its associated preceeding observation.
 */
class ActionConverter : public BaseConverter
{
    void OnStep() final;
};

}// namespace cvt
