#pragma once

#include <sc2api/sc2_api.h>

#include "database.hpp"

#include <filesystem>
#include <string_view>

namespace cvt {

/**
 * @brief The alphastar dataset only saves the preceeding observation to the current action
 *        and writes that to disk for behaviour cloning.
 */
class Converter : public sc2::ReplayObserver
{
  public:
    auto loadDB(const std::filesystem::path &path) noexcept -> bool;

    // Set Replay file hash + playerId before launching the coordinator
    void setReplayInfo(const std::string_view hash, std::uint32_t playerId) noexcept;

    void OnGameStart() final;

    void OnGameEnd() final;

    void OnStep() final;

  private:
    void copyHeightMapData() noexcept;

    void copyUnitData() noexcept;

    void copyActionData() noexcept;

    void copyDynamicMapData() noexcept;

    ReplayDatabase database_;
    ReplayData currentReplay_;
    bool mapDynHasLogged_{ false };
    bool mapHeightHasLogged_{ false };
};

}// namespace cvt
