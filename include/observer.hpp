#pragma once

#include <sc2api/sc2_api.h>

#include "database.hpp"

#include <filesystem>
#include <string_view>

namespace cvt {

class Converter : public sc2::ReplayObserver
{
  public:
    auto loadDB(const std::filesystem::path &path) noexcept -> bool;

    void setReplayHash(const std::string_view hash) noexcept;

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
