/**
 * @brief Version 1 of the observer and replay data structure based on ReplayDataSoA
 */

#include "observer.hpp"
#include "observer_utils.hpp"
#include "serialize.hpp"

namespace cvt {

static_assert(std::is_same_v<UID, sc2::Tag> && "Mismatch between unique id tags in SC2 and this Lib");

template<> void BaseConverter<ReplayDataSoA>::clear() noexcept
{
    replayData_.stepData.clear();
    replayData_.heightMap.clear();
    resourceObs_.clear();

    mapDynHasLogged_ = false;
    mapHeightHasLogged_ = false;
    writeSuccess_ = false;
}


template<> void BaseConverter<ReplayDataSoA>::OnGameStart()
{
    // Clear data collection structures, sc2api calls OnStep before OnGameStart
    // but some of the units are scuffed (particularly the resources), so we want
    // to clear it all out and only collect data from normal steps.
    this->clear();

    const auto replayInfo = this->ReplayControl()->GetReplayInfo();
    assert(replayInfo.num_players >= replayData_.playerId && "Player ID should be at most be num_players");
    const auto &playerInfo = replayInfo.players[replayData_.playerId - 1];
    replayData_.playerRace = static_cast<Race>(playerInfo.race);
    replayData_.playerResult = static_cast<Result>(playerInfo.game_result);
    replayData_.playerMMR = playerInfo.mmr;
    replayData_.playerAPM = playerInfo.apm;
    replayData_.gameVersion = replayInfo.version;

    const auto gameInfo = this->Observation()->GetGameInfo();
    if (!(gameInfo.height > 0 && gameInfo.width > 0)) { throw std::runtime_error("Missing map size data"); }
    replayData_.mapHeight = gameInfo.height;
    replayData_.mapWidth = gameInfo.width;

    // Preallocate Step Data with Maximum Game Loops
    replayData_.stepData.reserve(replayInfo.duration_gameloops);
}

template<> void BaseConverter<ReplayDataSoA>::OnGameEnd()
{
    // Don't save replay if its cooked
    if (this->Control()->GetAppState() != sc2::AppState::normal) {
        SPDLOG_ERROR("Not writing replay with bad SC2 AppState: {}", static_cast<int>(this->Control()->GetAppState()));
        return;
    }
    // Transform SoA to AoS and Write to database
    writeSuccess_ = database_.addEntry(AoStoSoA<ReplayDataSoA::struct_type, ReplayDataSoA>(replayData_));
}


template<> void BaseConverter<ReplayDataSoA>::copyHeightMapData() noexcept
{
    const auto *rawObs = this->Observation()->GetRawObservation();
    const auto &minimapFeats = rawObs->feature_layer_data().minimap_renders();
    if (!mapHeightHasLogged_) {
        SPDLOG_INFO("Static HeightMap Availability : {}", minimapFeats.has_height_map());
        mapHeightHasLogged_ = true;
    }
    if (!minimapFeats.has_height_map()) { return; }
    copyMapData(replayData_.heightMap, minimapFeats.height_map());
}


template<> void BaseConverter<ReplayDataSoA>::copyUnitData() noexcept
{
    const auto unitData = this->Observation()->GetUnits();
    auto &units = replayData_.stepData.back().units;
    units.clear();
    units.reserve(unitData.size());
    auto &neutralUnits = replayData_.stepData.back().neutralUnits;
    neutralUnits.clear();
    neutralUnits.reserve(unitData.size());

    ::cvt::copyUnitData(std::back_inserter(units), std::back_inserter(neutralUnits), unitData);

    if (resourceObs_.empty()) { this->initResourceObs(neutralUnits); }
    this->updateResourceObs(neutralUnits);
}

template<> void BaseConverter<ReplayDataSoA>::copyActionData() noexcept
{
    const auto actionData = this->Observation()->GetRawActions();
    auto &actions = replayData_.stepData.back().actions;
    actions.reserve(actionData.size());
    ::cvt::copyActionData(std::back_inserter(actions), actionData);
}

template<> void BaseConverter<ReplayDataSoA>::copyDynamicMapData() noexcept
{
    const auto *rawObs = this->Observation()->GetRawObservation();
    const auto &minimapFeats = rawObs->feature_layer_data().minimap_renders();

    // Log available visibility per replay
    if (!mapDynHasLogged_) {
        mapDynHasLogged_ = true;
        SPDLOG_INFO(
            "Minimap Features: visibility {}, creep: {}, player_relative: {}, "
            "alerts: {}, buildable: {}, pathable: {}",
            minimapFeats.has_visibility_map(),
            minimapFeats.has_creep(),
            minimapFeats.has_player_relative(),
            minimapFeats.has_alerts(),
            minimapFeats.has_buildable(),
            minimapFeats.has_pathable());
    }

    auto &step = replayData_.stepData.back();
    if (minimapFeats.has_visibility_map()) { copyMapData(step.visibility, minimapFeats.visibility_map()); }
    if (minimapFeats.has_creep()) { copyMapData(step.creep, minimapFeats.creep()); }
    if (minimapFeats.has_player_relative()) { copyMapData(step.player_relative, minimapFeats.player_relative()); }
    if (minimapFeats.has_alerts()) { copyMapData(step.alerts, minimapFeats.alerts()); }
    if (minimapFeats.has_buildable()) { copyMapData(step.buildable, minimapFeats.buildable()); }
    if (minimapFeats.has_pathable()) { copyMapData(step.pathable, minimapFeats.pathable()); }
}

template<> void BaseConverter<ReplayDataSoA>::copyCommonData() noexcept
{
    // Logging performance
    static FrequencyTimer timer("Converter", std::chrono::seconds(30));
    timer.step(fmt::format("Step {} of {}",
        this->Observation()->GetGameLoop(),
        this->ReplayControl()->GetReplayInfo().duration_gameloops));

    // Copy static height map if not already done
    if (replayData_.heightMap.empty()) { this->copyHeightMapData(); }

    // Write directly into stepData.back()
    replayData_.stepData.back().gameStep = this->Observation()->GetGameLoop();
    replayData_.stepData.back().minearals = this->Observation()->GetMinerals();
    replayData_.stepData.back().vespene = this->Observation()->GetVespene();
    replayData_.stepData.back().popMax = this->Observation()->GetFoodCap();
    replayData_.stepData.back().popArmy = this->Observation()->GetFoodArmy();
    replayData_.stepData.back().popWorkers = this->Observation()->GetFoodWorkers();

    const sc2::Score &score = this->Observation()->GetScore();
    replayData_.stepData.back().score = convertScore(&score);
}

template<> void FullConverter<ReplayDataSoA>::OnStep()
{
    // "Initialize" next item
    replayData_.stepData.resize(replayData_.stepData.size() + 1);

    this->copyCommonData();
    this->copyUnitData();
    this->copyActionData();
    this->copyDynamicMapData();
}

template<> void ActionConverter<ReplayDataSoA>::OnStep()
{
    // Need to have at least one buffer
    if (replayData_.stepData.empty()) { replayData_.stepData.resize(1); }

    if (!this->Observation()->GetRawActions().empty()) {
        this->copyActionData();
        // Previous observation locked in, current will write to new "space"
        replayData_.stepData.resize(replayData_.stepData.size() + 1);
    }

    // Always copy observation, the next step might have an action
    this->copyCommonData();
    this->copyUnitData();
    this->copyDynamicMapData();
}

template<> void StridedConverter<ReplayDataSoA>::OnStep()
{
    // Check if a logging step
    const auto gameStep = this->Observation()->GetGameLoop();
    if (gameStep % stride_ != 0) { return; }

    // "Initialize" next item
    replayData_.stepData.resize(replayData_.stepData.size() + 1);

    // Write directly into stepData.back()
    this->copyCommonData();
    this->copyUnitData();
    this->copyActionData();
    this->copyDynamicMapData();
}

}// namespace cvt
