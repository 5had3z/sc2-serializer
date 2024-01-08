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

    [[nodiscard]] auto getReplayHash() noexcept -> std::string & { return header.replayHash; }
    [[nodiscard]] auto getReplayHash() const noexcept -> const std::string & { return header.replayHash; }
    [[nodiscard]] auto getPlayerId() noexcept -> std::uint32_t & { return header.playerId; }
    [[nodiscard]] auto getPlayerId() const noexcept -> std::uint32_t { return header.playerId; }
};

struct ReplayData2SoA
{
    using struct_type = ReplayData2;
    ReplayInfo header;
    StepDataSoA data;

    [[nodiscard]] auto getReplayHash() noexcept -> std::string & { return header.replayHash; }
    [[nodiscard]] auto getReplayHash() const noexcept -> const std::string & { return header.replayHash; }
    [[nodiscard]] auto getPlayerId() noexcept -> std::uint32_t & { return header.playerId; }
    [[nodiscard]] auto getPlayerId() const noexcept -> std::uint32_t { return header.playerId; }

    [[nodiscard]] auto operator[](std::size_t idx) const noexcept -> StepData { return data[idx]; }
};

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
    [[nodiscard]] auto getReplayHash() noexcept -> std::string & { return replayHash; }
    [[nodiscard]] auto getReplayHash() const noexcept -> const std::string & { return replayHash; }
    [[nodiscard]] auto getPlayerId() noexcept -> std::uint32_t & { return playerId; }
    [[nodiscard]] auto getPlayerId() const noexcept -> std::uint32_t { return playerId; }
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
    [[nodiscard]] auto getReplayHash() noexcept -> std::string & { return replayHash; }
    [[nodiscard]] auto getReplayHash() const noexcept -> const std::string & { return replayHash; }
    [[nodiscard]] auto getPlayerId() noexcept -> std::uint32_t & { return playerId; }
    [[nodiscard]] auto getPlayerId() const noexcept -> std::uint32_t { return playerId; }

    [[nodiscard]] auto operator[](std::size_t idx) const noexcept -> StepData
    {
        StepData stepData;
        if (idx < gameStep.size()) { stepData.gameStep = gameStep[idx]; }
        if (idx < minearals.size()) { stepData.minearals = minearals[idx]; }
        if (idx < vespene.size()) { stepData.vespene = vespene[idx]; }
        if (idx < popMax.size()) { stepData.popMax = popMax[idx]; }
        if (idx < popArmy.size()) { stepData.popArmy = popArmy[idx]; }
        if (idx < popWorkers.size()) { stepData.popWorkers = popWorkers[idx]; }
        if (idx < score.size()) { stepData.score = score[idx]; }
        if (idx < visibility.size()) { stepData.visibility = visibility[idx]; }
        if (idx < creep.size()) { stepData.creep = creep[idx]; }
        if (idx < player_relative.size()) { stepData.player_relative = player_relative[idx]; }
        if (idx < alerts.size()) { stepData.alerts = alerts[idx]; }
        if (idx < buildable.size()) { stepData.buildable = buildable[idx]; }
        if (idx < pathable.size()) { stepData.pathable = pathable[idx]; }
        if (idx < actions.size()) { stepData.actions = actions[idx]; }
        if (idx < units.size()) { stepData.units = units[idx]; }
        if (idx < neutralUnits.size()) { stepData.neutralUnits = neutralUnits[idx]; }
        return stepData;
    }
};

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
[[nodiscard]] auto flattenAndSortUnits(
    const std::vector<std::vector<typename UnitSoAT::struct_type>> &replayUnits) noexcept -> FlattenedUnits<UnitSoAT>
{
    using UnitT = UnitSoAT::struct_type;
    using UnitStepT = std::pair<std::uint32_t, UnitT>;
    std::vector<UnitStepT> unitStepFlatten;
    for (std::size_t idx = 0; idx < replayUnits.size(); ++idx) {
        std::ranges::transform(
            replayUnits[idx], std::back_inserter(unitStepFlatten), [=](UnitT u) { return std::make_pair(idx, u); });
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
[[nodiscard]] auto recoverFlattenedSortedUnits(const FlattenedUnits<UnitSoAT> &flattenedUnits) noexcept
    -> std::vector<std::vector<typename UnitSoAT::struct_type>>
{
    // Create outer dimension with the maximum game step index
    std::vector<std::vector<typename UnitSoAT::struct_type>> replayUnits;
    replayUnits.resize(std::ranges::max(flattenedUnits.indicies) + 1);

    // Copy units to correct timestep
    const auto &indicies = flattenedUnits.indicies;
    for (std::size_t idx = 0; idx < indicies.size(); ++idx) {
        replayUnits[indicies[idx]].emplace_back(flattenedUnits.units[idx]);
    }
    return replayUnits;
}

}// namespace cvt
