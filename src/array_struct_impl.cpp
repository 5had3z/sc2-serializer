/**
 * @brief Specialisation implementations for AoS<->SoA for structures to prevent redefinition errors if it were in the
 * header files.
 */

#include "data_structures/replay_all.hpp"
#include "data_structures/replay_old.hpp"

namespace cvt {

template<> auto AoStoSoA(const ReplayData &aos) noexcept -> ReplayDataSoA
{
    ReplayDataSoA soa = {
        .replayHash = aos.replayHash,
        .gameVersion = aos.gameVersion,
        .playerId = aos.playerId,
        .playerRace = aos.playerRace,
        .playerResult = aos.playerResult,
        .playerMMR = aos.playerMMR,
        .playerAPM = aos.playerAPM,
        .mapWidth = aos.mapWidth,
        .mapHeight = aos.mapHeight,
        .heightMap = aos.heightMap,
    };

    for (const StepData &step : aos.stepData) {
        soa.gameStep.push_back(step.gameStep);
        soa.minearals.push_back(step.minearals);
        soa.vespene.push_back(step.vespene);
        soa.popMax.push_back(step.popMax);
        soa.popArmy.push_back(step.popArmy);
        soa.popWorkers.push_back(step.popWorkers);
        soa.score.push_back(step.score);
        soa.visibility.push_back(step.visibility);
        soa.creep.push_back(step.creep);
        soa.player_relative.push_back(step.player_relative);
        soa.alerts.push_back(step.alerts);
        soa.buildable.push_back(step.buildable);
        soa.pathable.push_back(step.pathable);
        soa.actions.push_back(step.actions);
        soa.units.push_back(step.units);
        soa.neutralUnits.push_back(step.neutralUnits);
    }
    return soa;
}

template<> auto SoAtoAoS(const ReplayDataSoA &soa) noexcept -> ReplayData
{
    ReplayData aos = {
        .replayHash = soa.replayHash,
        .gameVersion = soa.gameVersion,
        .playerId = soa.playerId,
        .playerRace = soa.playerRace,
        .playerResult = soa.playerResult,
        .playerMMR = soa.playerMMR,
        .playerAPM = soa.playerAPM,
        .mapWidth = soa.mapWidth,
        .mapHeight = soa.mapHeight,
        .heightMap = soa.heightMap,
    };

    auto &stepDataVec = aos.stepData;
    stepDataVec.resize(soa.units.size());

    for (std::size_t idx = 0; idx < stepDataVec.size(); ++idx) { stepDataVec[idx] = soa[idx]; }
    return aos;
}

template<> auto AoStoSoA(const ReplayData2 &aos) noexcept -> ReplayData2SoA
{
    ReplayData2SoA soa;
    soa.header = aos.header;
    soa.data = AoStoSoA(aos.data);
    return soa;
}

template<> auto SoAtoAoS(const ReplayData2SoA &soa) noexcept -> ReplayData2
{
    ReplayData2 aos;
    aos.header = soa.header;
    aos.data = SoAtoAoS(soa.data);
    return aos;
}

}// namespace cvt
