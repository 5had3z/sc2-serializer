/**
 * @brief Version 2 of the observer and replay data structure based on ReplayData2
 */

#include "observer.hpp"
#include "observer_utils.hpp"
#include "serialize.hpp"

namespace cvt {

template<> void BaseConverter<ReplayData2SoA>::clear() noexcept
{
    replayData_.data.clear();
    replayData_.header.heightMap.clear();
    resourceObs_.clear();

    mapDynHasLogged_ = false;
    mapHeightHasLogged_ = false;
    writeSuccess_ = false;
}

template<> void BaseConverter<ReplayData2SoA>::OnGameStart()
{
    this->clear();
    const auto replayInfo = this->ReplayControl()->GetReplayInfo();
    assert(replayInfo.num_players >= replayData_.getPlayerId() && "Player ID should be at most be num_players");
    auto &replayHeader = replayData_.header;
    const auto &playerInfo = replayInfo.players[replayHeader.playerId - 1];
    replayHeader.playerRace = static_cast<Race>(playerInfo.race);
    replayHeader.playerResult = static_cast<Result>(playerInfo.game_result);
    replayHeader.playerMMR = playerInfo.mmr;
    replayHeader.playerAPM = playerInfo.apm;
    replayHeader.gameVersion = replayInfo.version;
    replayHeader.durationSteps = replayInfo.duration_gameloops;

    const auto gameInfo = this->Observation()->GetGameInfo();
    if (!(gameInfo.height > 0 && gameInfo.width > 0)) { throw std::runtime_error("Missing map size data"); }
    replayHeader.mapHeight = gameInfo.height;
    replayHeader.mapWidth = gameInfo.width;

    // Preallocate Step Data with Maximum Game Loops
    replayData_.data.reserve(replayInfo.duration_gameloops);
}

template<> void BaseConverter<ReplayData2SoA>::OnGameEnd()
{
    // Don't save replay if its cooked
    if (this->Control()->GetAppState() != sc2::AppState::normal) {
        SPDLOG_ERROR("Not writing replay with bad SC2 AppState: {}", static_cast<int>(this->Control()->GetAppState()));
        return;
    }
    // Transform SoA to AoS and Write to database
    writeSuccess_ = database_.addEntry(AoStoSoA<ReplayData2SoA::struct_type, ReplayData2SoA>(replayData_));
}

template<> void BaseConverter<ReplayData2SoA>::copyHeightMapData() noexcept
{
    const auto *rawObs = this->Observation()->GetRawObservation();
    const auto &minimapFeats = rawObs->feature_layer_data().minimap_renders();
    if (!mapHeightHasLogged_) {
        SPDLOG_INFO("Static HeightMap Availability : {}", minimapFeats.has_height_map());
        mapHeightHasLogged_ = true;
    }
    if (!minimapFeats.has_height_map()) { return; }
    copyMapData(replayData_.header.heightMap, minimapFeats.height_map());
}


template<> void BaseConverter<ReplayData2SoA>::copyUnitData() noexcept
{
    const auto unitData = this->Observation()->GetUnits();
    auto &units = replayData_.data.back().units;
    units.clear();
    units.reserve(unitData.size());
    auto &neutralUnits = replayData_.data.back().neutralUnits;
    neutralUnits.clear();
    neutralUnits.reserve(unitData.size());

    ::cvt::copyUnitData(std::back_inserter(units), std::back_inserter(neutralUnits), unitData);

    if (resourceObs_.empty()) { this->initResourceObs(neutralUnits); }
    this->updateResourceObs(neutralUnits);
}

template<> void BaseConverter<ReplayData2SoA>::copyActionData() noexcept
{
    const auto actionData = this->Observation()->GetRawActions();
    auto &actions = replayData_.data.back().actions;
    actions.reserve(actionData.size());
    ::cvt::copyActionData(std::back_inserter(actions), actionData);
}

template<> void BaseConverter<ReplayData2SoA>::copyDynamicMapData() noexcept
{
    const auto *rawObs = this->Observation()->GetRawObservation();
    const auto &minimapFeats = rawObs->feature_layer_data().minimap_renders();

    // Log available visibility per replay
    if (!mapDynHasLogged_) {
        mapDynHasLogged_ = true;
        SPDLOG_DEBUG(
            "Minimap Features: visibility {}, creep: {}, player_relative: {}, "
            "alerts: {}, buildable: {}, pathable: {}",
            minimapFeats.has_visibility_map(),
            minimapFeats.has_creep(),
            minimapFeats.has_player_relative(),
            minimapFeats.has_alerts(),
            minimapFeats.has_buildable(),
            minimapFeats.has_pathable());
    }

    auto &step = replayData_.data.back();
    if (minimapFeats.has_visibility_map()) { copyMapData(step.visibility, minimapFeats.visibility_map()); }
    if (minimapFeats.has_creep()) { copyMapData(step.creep, minimapFeats.creep()); }
    if (minimapFeats.has_player_relative()) { copyMapData(step.player_relative, minimapFeats.player_relative()); }
    if (minimapFeats.has_alerts()) { copyMapData(step.alerts, minimapFeats.alerts()); }
    if (minimapFeats.has_buildable()) { copyMapData(step.buildable, minimapFeats.buildable()); }
    if (minimapFeats.has_pathable()) { copyMapData(step.pathable, minimapFeats.pathable()); }
}

template<> void BaseConverter<ReplayData2SoA>::copyCommonData() noexcept
{
    // Logging performance
    static FrequencyTimer timer("Converter", std::chrono::seconds(30));
    timer.step(fmt::format("Step {} of {}", this->Observation()->GetGameLoop(), replayData_.header.durationSteps));

    // Copy static height map if not already done
    if (replayData_.data.empty()) { this->copyHeightMapData(); }

    // Write directly into stepData.back()
    auto &currentStep = replayData_.data.back();
    currentStep.gameStep = this->Observation()->GetGameLoop();
    currentStep.minearals = this->Observation()->GetMinerals();
    currentStep.vespene = this->Observation()->GetVespene();
    currentStep.popMax = this->Observation()->GetFoodCap();
    currentStep.popArmy = this->Observation()->GetFoodArmy();
    currentStep.popWorkers = this->Observation()->GetFoodWorkers();

    const sc2::Score &score = this->Observation()->GetScore();
    currentStep.score = convertScore(&score);
}

template<> void FullConverter<ReplayData2SoA>::OnStep()
{
    // "Initialize" next item
    replayData_.data.resize(replayData_.data.size() + 1);

    this->copyCommonData();
    this->copyUnitData();
    this->copyActionData();
    this->copyDynamicMapData();
}

template<> void ActionConverter<ReplayData2SoA>::OnStep()
{
    // Need to have at least one buffer
    if (replayData_.data.empty()) { replayData_.data.resize(1); }

    if (!this->Observation()->GetRawActions().empty()) {
        this->copyActionData();
        // Previous observation locked in, current will write to new "space"
        replayData_.data.resize(replayData_.data.size() + 1);
    }

    // Always copy observation, the next step might have an action
    this->copyCommonData();
    this->copyUnitData();
    this->copyDynamicMapData();
}

template<> void StridedConverter<ReplayData2SoA>::OnStep()
{
    // Check if a logging step
    const auto gameStep = this->Observation()->GetGameLoop();
    if (gameStep % stride_ != 0) { return; }

    // "Initialize" next item
    replayData_.data.resize(replayData_.data.size() + 1);

    // Write directly into stepData.back()
    this->copyCommonData();
    this->copyUnitData();
    this->copyActionData();
    this->copyDynamicMapData();
}

}// namespace cvt
