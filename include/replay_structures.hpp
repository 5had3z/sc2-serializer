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

    [[nodiscard]] auto operator==(const ReplayInfo &other) const noexcept -> bool = default;
};

template<typename StepDataType> struct ReplayDataTemplate
{
    using step_type = StepDataType;
    ReplayInfo header;
    std::vector<StepDataType> data;

    [[nodiscard]] auto getReplayHash() noexcept -> std::string & { return header.replayHash; }
    [[nodiscard]] auto getReplayHash() const noexcept -> const std::string & { return header.replayHash; }
    [[nodiscard]] auto getPlayerId() noexcept -> std::uint32_t & { return header.playerId; }
    [[nodiscard]] auto getPlayerId() const noexcept -> std::uint32_t { return header.playerId; }
};

template<IsSoAType StepDataSoAType> struct ReplayDataTemplateSoA
{
    using struct_type = ReplayDataTemplate<typename StepDataSoAType::struct_type>;
    using step_type = StepDataSoAType;
    ReplayInfo header;
    StepDataSoAType data;

    [[nodiscard]] auto getReplayHash() noexcept -> std::string & { return header.replayHash; }
    [[nodiscard]] auto getReplayHash() const noexcept -> const std::string & { return header.replayHash; }
    [[nodiscard]] auto getPlayerId() noexcept -> std::uint32_t & { return header.playerId; }
    [[nodiscard]] auto getPlayerId() const noexcept -> std::uint32_t { return header.playerId; }

    [[nodiscard]] auto operator[](std::size_t idx) const noexcept -> StepDataSoAType::struct_type { return data[idx]; }
};

using ReplayData2NoUnitsMiniMap = ReplayDataTemplate<StepDataNoUnitsMiniMap>;
using ReplayData2NoUnits = ReplayDataTemplate<StepDataNoUnits>;
using ReplayData2 = ReplayDataTemplate<StepData>;

using ReplayData2SoANoUnitsMiniMap = ReplayDataTemplateSoA<StepDataSoANoUnitsMiniMap>;
using ReplayData2SoANoUnits = ReplayDataTemplateSoA<StepDataSoANoUnits>;
using ReplayData2SoA = ReplayDataTemplateSoA<StepDataSoA>;

static_assert(std::same_as<ReplayData2SoANoUnitsMiniMap::struct_type, ReplayData2NoUnitsMiniMap>);
static_assert(std::same_as<ReplayData2SoANoUnits::struct_type, ReplayData2NoUnits>);
static_assert(std::same_as<ReplayData2SoA::struct_type, ReplayData2>);

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

// -----------------------------------------------------------------
// FLATTENING VERSION 1
// -----------------------------------------------------------------

/**
 * @brief Flattened units in SoA form with associated step index
 * @tparam UnitSoAT Type of flattened unit
 */
template<typename UnitSoAT> struct FlattenedUnits
{
    UnitSoAT units;
    std::vector<std::uint32_t> indices;
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

    // Significantly better compressibility when sorted by unit (and implicitly time)
    std::ranges::stable_sort(
        unitStepFlatten, [](const UnitStepT &a, const UnitStepT &b) { return a.second.id < b.second.id; });

    // Create flattened SoA
    UnitSoAT unitsSoA = cvt::AoStoSoA(std::views::values(unitStepFlatten));

    // Create accompanying step indices for reconstruction
    std::vector<uint32_t> indices;
    indices.resize(unitStepFlatten.size());
    std::ranges::copy(std::views::keys(unitStepFlatten), indices.begin());

    return { unitsSoA, indices };
}

template<IsSoAType UnitSoAT>
[[nodiscard]] auto recoverFlattenedSortedUnits(const FlattenedUnits<UnitSoAT> &flattenedUnits) noexcept
    -> std::vector<std::vector<typename UnitSoAT::struct_type>>
{
    // Create outer dimension with the maximum game step index
    std::vector<std::vector<typename UnitSoAT::struct_type>> replayUnits;
    const std::size_t maxStepIdx = std::ranges::max(flattenedUnits.indices);
    replayUnits.resize(maxStepIdx + 1ull);

    // Copy units to correct timestep
    const auto &indices = flattenedUnits.indices;
    for (std::size_t idx = 0; idx < indices.size(); ++idx) {
        replayUnits[indices[idx]].emplace_back(flattenedUnits.units[idx]);
    }
    return replayUnits;
}


// -----------------------------------------------------------------
// FLATTENING VERSION 2
// THIS METHOD ASSUMES UNIT IDS AND THEIR OBSERVATION IS CONTIGUOUS, BUT THIS DOES NOT HOLD
// -----------------------------------------------------------------

/**
 * @brief Flattened units in SoA form with associated step index
 * @tparam UnitSoAT Type of flattened unit
 */
template<typename UnitSoAT> struct FlattenedUnits2
{
    UnitSoAT units;
    std::vector<std::uint32_t> indices;
    std::uint32_t max_step;
};

template<IsSoAType UnitSoAT>
[[nodiscard, deprecated("Assumptions made with this method don't hold")]] auto flattenAndSortUnits2(
    const std::vector<std::vector<typename UnitSoAT::struct_type>> &replayUnits) noexcept -> FlattenedUnits2<UnitSoAT>
{
    using UnitT = UnitSoAT::struct_type;
    using UnitStepT = std::pair<std::uint32_t, UnitT>;
    std::vector<UnitStepT> unitStepFlatten;
    for (std::size_t idx = 0; idx < replayUnits.size(); ++idx) {
        std::ranges::transform(
            replayUnits[idx], std::back_inserter(unitStepFlatten), [=](UnitT u) { return std::make_pair(idx, u); });
    }

    // Significantly better compressibility when sorted by unit (and implicitly time)
    std::ranges::stable_sort(
        unitStepFlatten, [](const UnitStepT &a, const UnitStepT &b) { return a.second.id < b.second.id; });

    // Create flattened SoA
    FlattenedUnits2<UnitSoAT> result;
    result.max_step = replayUnits.size();
    result.units = cvt::AoStoSoA(std::views::values(unitStepFlatten));

    // Create accompanying first step seen for reconstruction

    // Views impl
    // for (auto &&same_unit_view : std::views::chunk_by(unitStepFlatten,
    //          [](const UnitStepT &prev, const UnitStepT &next) { return prev.second.id == next.second.id; })) {
    //     result.indices.emplace_back(same_unit_view.front().first);
    // }

    // Iterator impl of chunk-by
    auto start = unitStepFlatten.begin();
    for (auto end = unitStepFlatten.begin(); end != unitStepFlatten.end(); ++end) {
        // Check if we've passed the end of our chunk
        if (start->second.id != end->second.id) {
            result.indices.emplace_back(start->first);
            start = end;// set start to next chunk
        }
    }
    result.indices.emplace_back(start->first);// Handle last chunk

    return result;
}

template<IsSoAType UnitSoAT>
[[nodiscard, deprecated("Assumptions made with this method don't hold")]] auto recoverFlattenedSortedUnits2(
    const FlattenedUnits2<UnitSoAT> &flattenedUnits) noexcept
    -> std::vector<std::vector<typename UnitSoAT::struct_type>>
{
    // Create outer dimension with the maximum game step index
    std::vector<std::vector<typename UnitSoAT::struct_type>> replayUnits;
    replayUnits.resize(flattenedUnits.max_step);

    // Copy units to correct timestep
    const std::size_t num_units = flattenedUnits.units.id.size();
    auto base = flattenedUnits.indices.begin();
    std::size_t offset = 0;
    const auto &unit_ids = flattenedUnits.units.id;
    auto unit_id = unit_ids[0];
    for (std::size_t unit_idx = 0; unit_idx < num_units; ++unit_idx) {
        replayUnits[*base + offset].emplace_back(flattenedUnits.units[unit_idx]);
        // Increment the base and reset the offset if there is a new unit next
        const auto next_id = unit_ids[unit_idx + 1];
        if (unit_idx < num_units - 1 && unit_id != next_id) {
            base++;
            offset = 0;
            unit_id = next_id;
        } else {
            offset++;
        }
    }
    return replayUnits;
}

// -----------------------------------------------------------------
// FLATTENING VERSION 3
// -----------------------------------------------------------------


struct IotaRange
{
    std::uint32_t start;
    std::uint32_t num;
};

/**
 * @brief Flattened units in SoA form with associated step index
 * @tparam UnitSoAT Type of flattened unit
 */
template<typename UnitSoAT> struct FlattenedUnits3
{
    UnitSoAT units;
    std::vector<IotaRange> step_count;
    std::uint32_t max_step;
};

template<IsSoAType UnitSoAT>
[[nodiscard]] auto flattenAndSortUnits3(
    const std::vector<std::vector<typename UnitSoAT::struct_type>> &replayUnits) noexcept -> FlattenedUnits3<UnitSoAT>
{
    using UnitT = UnitSoAT::struct_type;
    using UnitStepT = std::pair<std::uint32_t, UnitT>;
    std::vector<UnitStepT> unitStepFlatten;
    for (std::size_t idx = 0; idx < replayUnits.size(); ++idx) {
        std::ranges::transform(
            replayUnits[idx], std::back_inserter(unitStepFlatten), [=](UnitT u) { return std::make_pair(idx, u); });
    }

    // Significantly better compressibility when sorted by unit (and implicitly time)
    std::ranges::stable_sort(
        unitStepFlatten, [](const UnitStepT &a, const UnitStepT &b) { return a.second.id < b.second.id; });

    // Create flattened SoA
    FlattenedUnits3<UnitSoAT> result;
    result.max_step = replayUnits.size();
    result.units = cvt::AoStoSoA(std::views::values(unitStepFlatten));

    // Create accompanying first step seen for reconstruction
    // std::vector<IotaRange> test;
    // for (auto &&iota_steps : std::views::keys(unitStepFlatten)
    //                              | std::views::chunk_by([](std::uint32_t p, std::uint32_t n) { return p + 1 == n; }))
    //                              {
    //     test.emplace_back(iota_steps.front(), static_cast<std::uint32_t>(std::ranges::size(iota_steps)));
    // }

    // Iterator impl of chunk-by
    auto start = unitStepFlatten.begin();
    auto end = unitStepFlatten.begin();
    for (; end != std::prev(unitStepFlatten.end()); ++end) {
        // Check if we're at the end the end of our chunk
        const auto next = std::next(end, 1);
        if (end->first + 1 != next->first) {
            result.step_count.emplace_back(start->first, static_cast<std::uint32_t>(std::distance(start, end) + 1));
            start = next;// set start to next chunk
        }
    }
    result.step_count.emplace_back(
        start->first, static_cast<std::uint32_t>(std::distance(start, end) + 1));// Handle last chunk

    return result;
}

template<IsSoAType UnitSoAT>
[[nodiscard]] auto recoverFlattenedSortedUnits3(const FlattenedUnits3<UnitSoAT> &flattenedUnits) noexcept
    -> std::vector<std::vector<typename UnitSoAT::struct_type>>
{
    // Create outer dimension with the maximum game step index
    std::vector<std::vector<typename UnitSoAT::struct_type>> replayUnits;
    replayUnits.resize(flattenedUnits.max_step);

    // Copy units to correct timestep
    const std::size_t num_units = flattenedUnits.units.id.size();
    std::uint32_t offset = 0;
    const auto &unit_ids = flattenedUnits.units.id;
    auto step_count = flattenedUnits.step_count.begin();
    for (std::size_t unit_idx = 0; unit_idx < num_units; ++unit_idx) {
        replayUnits[step_count->start + offset].emplace_back(flattenedUnits.units[unit_idx]);
        // Increment the base and reset the offset if there is a new unit next
        if (offset < step_count->num - 1) {
            offset++;
        } else {
            step_count++;
            offset = 0;
        }
    }
    return replayUnits;
}

}// namespace cvt
