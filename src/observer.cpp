#include "observer.hpp"
#include "serialize.hpp"

#include <spdlog/spdlog.h>

#include <cstring>

namespace cvt {


/**
 * @brief Copy map data from protobuf return to Image struct.
 * @tparam T underlying type of image
 * @param dest Destination Image to copy to
 * @param mapData Source Protobuf Data
 */
template<typename T> void copyMapData(Image<T> &dest, const SC2APIProtocol::ImageData &mapData)
{
    dest.resize(mapData.size().y(), mapData.size().x());
    assert(dest.size() == mapData.data().size() && "Expected mapData size doesn't match actual size");
    std::memcpy(dest.data(), mapData.data().data(), dest.size());
}

auto Converter::loadDB(const std::filesystem::path &path) noexcept -> bool { return database_.open(path); }

void Converter::OnGameStart()
{
    auto replayInfo = this->ReplayControl()->GetReplayInfo();
    // Preallocate Vector with expected duration
    currentReplay_.stepData.reserve(replayInfo.duration_gameloops);
}

void Converter::OnGameEnd()
{
    // Save entry to DB
    database_.addEntry(currentReplay_);
    currentReplay_.clear();
    mapDynHasLogged_ = false;
    mapHeightHasLogged_ = false;
}

void Converter::OnStep()
{
    // Copy static height map if not already done
    if (currentReplay_.heightMap.empty()) { this->copyHeightMapData(); }

    // "Initialize" next item
    currentReplay_.stepData.resize(currentReplay_.stepData.size() + 1);

    // Write directly into stepData.back()
    this->copyUnitData();
    this->copyActionData();
    this->copyDynamicMapData();
}

void Converter::setReplayHash(const std::string_view hash) noexcept { currentReplay_.replayHash = hash; }

void Converter::copyHeightMapData() noexcept
{
    const auto *rawObs = this->Observation()->GetRawObservation();
    const auto &minimapFeats = rawObs->feature_layer_data().minimap_renders();
    if (!mapHeightHasLogged_) {
        SPDLOG_INFO("Static HeightMap Availablity : {}", minimapFeats.has_height_map());
        mapHeightHasLogged_ = true;
    }
    if (!minimapFeats.has_height_map()) { return; }
    copyMapData(currentReplay_.heightMap, minimapFeats.height_map());
}

void Converter::copyUnitData() noexcept
{
    // Foo
    const auto unitData = this->Observation()->GetUnits();
}
void Converter::copyActionData() noexcept
{
    // Foo
    const auto actionData = this->Observation()->GetRawActions();
}

void Converter::copyDynamicMapData() noexcept
{
    const auto *rawObs = this->Observation()->GetRawObservation();
    const auto &minimapFeats = rawObs->feature_layer_data().minimap_renders();

    // Log available visibilty per replay
    if (!mapDynHasLogged_) {
        mapDynHasLogged_ = true;
        SPDLOG_INFO(
          "Minimap Features: visibility {}, creep: {}, player_relative: {}, alerts: {}, buildable: {}, pathable: {}",
          minimapFeats.has_visibility_map(),
          minimapFeats.has_creep(),
          minimapFeats.has_player_relative(),
          minimapFeats.has_alerts(),
          minimapFeats.has_buildable(),
          minimapFeats.has_pathable());
    }

    auto &step = currentReplay_.stepData.back();
    if (minimapFeats.has_visibility_map()) { copyMapData(step.visibility, minimapFeats.visibility_map()); }
    if (minimapFeats.has_creep()) { copyMapData(step.creep, minimapFeats.creep()); }
    if (minimapFeats.has_player_relative()) { copyMapData(step.player_relative, minimapFeats.player_relative()); }
    if (minimapFeats.has_alerts()) { copyMapData(step.alerts, minimapFeats.alerts()); }
    if (minimapFeats.has_buildable()) { copyMapData(step.buildable, minimapFeats.buildable()); }
    if (minimapFeats.has_pathable()) { copyMapData(step.buildable, minimapFeats.pathable()); }
}

}// namespace cvt
