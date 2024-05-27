/**
 * @file replay_all.hpp
 * @author Bryce Ferenczi
 * @brief Replay data structure for recording and deserializing all observation data.
 * @version 0.1
 * @date 2024-05-27
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include "../serialize.hpp"
#include "replay_interface.hpp"
#include "units.hpp"

#include <ranges>

namespace cvt {

/**
 * @brief Step data that contains scalar, minimap and unit data. Basically all of it.
 */
struct StepData
{
    using has_scalar_data = std::true_type;
    using has_minimap_data = std::true_type;
    using has_unit_data = std::true_type;

    std::uint32_t gameStep{};
    std::uint16_t minearals{};
    std::uint16_t vespene{};
    std::uint16_t popMax{};
    std::uint16_t popArmy{};
    std::uint16_t popWorkers{};
    Score score{};
    Image<std::uint8_t> visibility{};
    Image<bool> creep{};
    Image<std::uint8_t> player_relative{};
    Image<std::uint8_t> alerts{};
    Image<bool> buildable{};
    Image<bool> pathable{};
    std::vector<Action> actions{};
    std::vector<Unit> units{};
    std::vector<NeutralUnit> neutralUnits{};

    [[nodiscard]] auto operator==(const StepData &other) const noexcept -> bool = default;
};

static_assert(HasScalarData<StepData> && HasMinimapData<StepData> && HasUnitData<StepData>);

/**
 * @brief SoA representation of an array of StepData
 */
struct StepDataSoA
{
    using has_scalar_data = std::true_type;
    using has_minimap_data = std::true_type;
    using has_unit_data = std::true_type;

    using struct_type = StepData;
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

    [[nodiscard]] auto operator==(const StepDataSoA &other) const noexcept -> bool = default;

    /**
     * @brief Gather step data from each array to make structure of data at step.
     * @param idx time index of replay to gather.
     * @return Gathered step data.
     */
    [[nodiscard]] auto operator[](std::size_t idx) const noexcept -> StepData
    {
        StepData stepData;
        stepData.gameStep = gameStep[idx];
        stepData.minearals = minearals[idx];
        stepData.vespene = vespene[idx];
        stepData.popMax = popMax[idx];
        stepData.popArmy = popArmy[idx];
        stepData.popWorkers = popWorkers[idx];
        stepData.score = score[idx];
        stepData.visibility = visibility[idx];
        stepData.creep = creep[idx];
        stepData.player_relative = player_relative[idx];
        stepData.alerts = alerts[idx];
        stepData.buildable = buildable[idx];
        stepData.pathable = pathable[idx];
        stepData.actions = actions[idx];
        stepData.units = units[idx];
        stepData.neutralUnits = neutralUnits[idx];
        return stepData;
    }
};

static_assert(
    HasScalarData<StepDataSoA> && HasMinimapData<StepDataSoA> && HasUnitData<StepDataSoA> && IsSoAType<StepDataSoA>);

/**
 * @brief ReplayData with only scalar data
 */
using ReplayData = ReplayDataTemplate<StepData>;

/**
 * @brief ReplayData as SoA with only scalar data
 */
using ReplayDataSoA = ReplayDataTemplateSoA<StepDataSoA>;

static_assert(std::same_as<ReplayDataSoA::struct_type, ReplayData>);

/**
 * @brief Convert array-of-structures to structure-of-arrays
 *
 * @tparam Range range type of StepData
 * @param aos data to transform
 * @return StepDataSoA transposed data
 */
template<std::ranges::range Range>
    requires std::same_as<std::ranges::range_value_t<Range>, StepData>
auto AoStoSoA(Range &&aos) noexcept -> StepDataSoA
{
    StepDataSoA soa;

    // Preallocate for expected size
    boost::pfr::for_each_field(soa, [&](auto &field) { field.reserve(aos.size()); });

    for (auto &&step : aos) {
        soa.gameStep.emplace_back(step.gameStep);
        soa.minearals.emplace_back(step.minearals);
        soa.vespene.emplace_back(step.vespene);
        soa.popMax.emplace_back(step.popMax);
        soa.popArmy.emplace_back(step.popArmy);
        soa.popWorkers.emplace_back(step.popWorkers);
        soa.score.emplace_back(step.score);
        soa.visibility.emplace_back(step.visibility);
        soa.creep.emplace_back(step.creep);
        soa.player_relative.emplace_back(step.player_relative);
        soa.alerts.emplace_back(step.alerts);
        soa.buildable.emplace_back(step.buildable);
        soa.pathable.emplace_back(step.pathable);
        soa.actions.emplace_back(step.actions);
        soa.units.emplace_back(step.units);
        soa.neutralUnits.emplace_back(step.neutralUnits);
    }
    return soa;
}

/**
 * @brief Database interface implementation for ReplayDataSoA
 */
template<> struct DatabaseInterface<ReplayDataSoA>
{
    static auto getHeaderImpl(std::istream &dbStream) -> ReplayInfo
    {
        ReplayInfo result;
        deserialize(result, dbStream);
        return result;
    }

    static auto getHashIdImpl(std::istream &dbStream) -> std::pair<std::string, std::uint32_t>
    {
        const auto replayInfo = DatabaseInterface::getHeaderImpl(dbStream);
        return std::make_pair(replayInfo.replayHash, replayInfo.playerId);
    }

    static auto getEntryImpl(std::istream &dbStream) -> ReplayDataSoA
    {
        ReplayDataSoA result;
        deserialize(result.header, dbStream);
        deserialize(result.data.gameStep, dbStream);
        deserialize(result.data.minearals, dbStream);
        deserialize(result.data.vespene, dbStream);
        deserialize(result.data.popMax, dbStream);
        deserialize(result.data.popArmy, dbStream);
        deserialize(result.data.popWorkers, dbStream);
        deserialize(result.data.score, dbStream);
        deserialize(result.data.visibility, dbStream);
        deserialize(result.data.creep, dbStream);
        deserialize(result.data.player_relative, dbStream);
        deserialize(result.data.alerts, dbStream);
        deserialize(result.data.buildable, dbStream);
        deserialize(result.data.pathable, dbStream);
        deserialize(result.data.actions, dbStream);
        {
            FlattenedUnits2<UnitSoA> units;
            deserialize(units, dbStream);
            result.data.units = recoverFlattenedSortedUnits2(units);
        }
        {
            FlattenedUnits2<NeutralUnitSoA> units;
            deserialize(units, dbStream);
            result.data.neutralUnits = recoverFlattenedSortedUnits2(units);
        }
        return result;
    }

    static auto addEntryImpl(const ReplayDataSoA &d, std::ostream &dbStream) noexcept -> bool
    {
        serialize(d.header, dbStream);
        serialize(d.data.gameStep, dbStream);
        serialize(d.data.minearals, dbStream);
        serialize(d.data.vespene, dbStream);
        serialize(d.data.popMax, dbStream);
        serialize(d.data.popArmy, dbStream);
        serialize(d.data.popWorkers, dbStream);
        serialize(d.data.score, dbStream);
        serialize(d.data.visibility, dbStream);
        serialize(d.data.creep, dbStream);
        serialize(d.data.player_relative, dbStream);
        serialize(d.data.alerts, dbStream);
        serialize(d.data.buildable, dbStream);
        serialize(d.data.pathable, dbStream);
        serialize(d.data.actions, dbStream);
        serialize(flattenAndSortUnits2<UnitSoA>(d.data.units), dbStream);
        serialize(flattenAndSortUnits2<NeutralUnitSoA>(d.data.neutralUnits), dbStream);
        return true;
    }
};

}// namespace cvt
