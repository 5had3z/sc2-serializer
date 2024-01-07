#pragma once

#include "data_structures.hpp"

namespace cvt {

struct ReplayInfo
{
    std::string replayHash{};
    std::string gameVersion{};
    std::uint32_t playerId{};
    std::uint32_t durationSteps{};
    Race playerRace{ Race::Random };
    Result playerResult{ Result::Undecided };
    int playerMMR{};
    int playerAPM{};
    int mapWidth{};
    int mapHeight{};
    Image<std::uint8_t> heightMap{};
};

struct ReplayData2
{
    ReplayInfo header;
    std::vector<StepData> data;
};

struct ReplayData2SoA
{
    using struct_type = ReplayData2;
    ReplayInfo header;
    StepDataSoA data;
};

template<> constexpr auto AoStoSoA(const ReplayData2 &aos) noexcept -> ReplayData2SoA
{
    ReplayData2SoA soa;
    soa.header = aos.header;
    soa.data = AoStoSoA(aos.data);
    return soa;
}

template<> constexpr auto SoAtoAoS(const ReplayData2SoA &soa) noexcept -> ReplayData2
{
    ReplayData2 aos;
    aos.header = soa.header;
    aos.data = SoAtoAoS<std::vector<StepData>, StepDataSoA>(soa.data);
    return aos;
}


struct ReplayData
{
    std::string replayHash{};
    std::string gameVersion{};
    std::uint32_t playerId{};
    Race playerRace{ Race::Random };
    Result playerResult{ Result::Undecided };
    int playerMMR{};
    int playerAPM{};
    int mapWidth{};
    int mapHeight{};
    Image<std::uint8_t> heightMap{};
    std::vector<StepData> stepData{};

    [[nodiscard]] auto operator==(const ReplayData &other) const noexcept -> bool = default;
};

struct ReplayDataSoA
{
    using struct_type = ReplayData;
    std::string replayHash{};
    std::string gameVersion{};
    std::uint32_t playerId{};
    Race playerRace{ Race::Random };
    Result playerResult{ Result::Undecided };
    int playerMMR{};
    int playerAPM{};
    int mapWidth{};
    int mapHeight{};
    Image<std::uint8_t> heightMap{};

    // Step data
    std::vector<std::uint32_t> gameStep{};
    std::vector<std::uint16_t> minearals{};
    std::vector<std::uint16_t> vespene{};
    std::vector<std::uint16_t> popMax{};
    std::vector<std::uint16_t> popArmy{};
    std::vector<std::uint16_t> popWorkers{};
    std::vector<Score> score{};
    std::vector<Image<std::uint8_t>> visibility{};
    std::vector<Image<bool>> creep{};
    std::vector<Image<std::uint8_t>> player_relative{};
    std::vector<Image<std::uint8_t>> alerts{};
    std::vector<Image<bool>> buildable{};
    std::vector<Image<bool>> pathable{};
    std::vector<std::vector<Action>> actions{};
    std::vector<std::vector<Unit>> units{};
    std::vector<std::vector<NeutralUnit>> neutralUnits{};

    [[nodiscard]] auto operator==(const ReplayDataSoA &other) const noexcept -> bool = default;
};

template<> constexpr auto AoStoSoA(const ReplayData &aos) noexcept -> ReplayDataSoA
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

template<> constexpr auto SoAtoAoS(const ReplayDataSoA &soa) noexcept -> ReplayData
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

    for (std::size_t idx = 0; idx < stepDataVec.size(); ++idx) {
        auto &stepData = stepDataVec[idx];
        if (idx < soa.gameStep.size()) { stepData.gameStep = soa.gameStep[idx]; }
        if (idx < soa.minearals.size()) { stepData.minearals = soa.minearals[idx]; }
        if (idx < soa.vespene.size()) { stepData.vespene = soa.vespene[idx]; }
        if (idx < soa.popMax.size()) { stepData.popMax = soa.popMax[idx]; }
        if (idx < soa.popArmy.size()) { stepData.popArmy = soa.popArmy[idx]; }
        if (idx < soa.popWorkers.size()) { stepData.popWorkers = soa.popWorkers[idx]; }
        if (idx < soa.score.size()) { stepData.score = soa.score[idx]; }
        if (idx < soa.visibility.size()) { stepData.visibility = soa.visibility[idx]; }
        if (idx < soa.creep.size()) { stepData.creep = soa.creep[idx]; }
        if (idx < soa.player_relative.size()) { stepData.player_relative = soa.player_relative[idx]; }
        if (idx < soa.alerts.size()) { stepData.alerts = soa.alerts[idx]; }
        if (idx < soa.buildable.size()) { stepData.buildable = soa.buildable[idx]; }
        if (idx < soa.pathable.size()) { stepData.pathable = soa.pathable[idx]; }
        if (idx < soa.actions.size()) { stepData.actions = soa.actions[idx]; }
        if (idx < soa.units.size()) { stepData.units = soa.units[idx]; }
        if (idx < soa.neutralUnits.size()) { stepData.neutralUnits = soa.neutralUnits[idx]; }
    }
    return aos;
}

/**
 * @brief Flattened units in SoA form with associated step index
 * @tparam UnitSoAT Type of flattened unit
 */
template<typename UnitSoAT> struct FlattenedUnits
{
    UnitSoAT units;
    std::vector<std::uint32_t> indicies;
};

template<IsSoAType UnitSoAT>
[[nodiscard]] constexpr auto flattenAndSortUnits(
    const std::vector<std::vector<typename UnitSoAT::struct_type>> &replayUnits) noexcept -> FlattenedUnits<UnitSoAT>
{
    using UnitT = UnitSoAT::struct_type;
    using UnitStepT = std::pair<std::uint32_t, UnitT>;
    std::vector<UnitStepT> unitStepFlatten;
    for (auto &&[idx, units] : std::views::enumerate(replayUnits)) {
        std::ranges::transform(
            units, std::back_inserter(unitStepFlatten), [=](UnitT u) { return std::make_pair(idx, u); });
    }

    // Significantly better compressibility when sorted by unit (and implicity time)
    std::ranges::stable_sort(
        unitStepFlatten, [](const UnitStepT &a, const UnitStepT &b) { return a.second.id < b.second.id; });

    // Create flattened SoA
    UnitSoAT unitsSoA = cvt::AoStoSoA(std::views::values(unitStepFlatten));

    // Create accompanying step indicies for reconstruction
    std::vector<uint32_t> indicies;
    indicies.resize(unitStepFlatten.size());
    std::ranges::copy(std::views::keys(unitStepFlatten), indicies.begin());

    return { unitsSoA, indicies };
}

template<IsSoAType UnitSoAT>
[[nodiscard]] constexpr auto recoverFlattenedSortedUnits(const FlattenedUnits<UnitSoAT> &flattenedUnits) noexcept
    -> std::vector<std::vector<typename UnitSoAT::struct_type>>
{
    // Create outer dimension with the maximum game step index
    std::vector<std::vector<typename UnitSoAT::struct_type>> replayUnits;
    replayUnits.resize(std::ranges::max(flattenedUnits.indicies) + 1);

    // Copy units to correct timestep
    for (auto &&[unitIdx, stepIdx] : std::views::enumerate(flattenedUnits.indicies)) {
        replayUnits[stepIdx].emplace_back(flattenedUnits.units[unitIdx]);
    }

    return replayUnits;
}

}// namespace cvt
