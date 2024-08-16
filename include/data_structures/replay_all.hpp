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

#include "../database.hpp"
#include "../instance_transform.hpp"
#include "replay_interface.hpp"
#include "units.hpp"

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
        return gatherStructAtIndex(*this, idx);
    }

    /**
     * @brief Number of game steps in the Structure-of-Arrays
     *
     * @return std::size_t
     */
    [[nodiscard]] auto size() const noexcept -> std::size_t { return gameStep.size(); }
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

    static auto getEntryUIDImpl(std::istream &dbStream) -> std::string
    {
        const auto replayInfo = DatabaseInterface::getHeaderImpl(dbStream);
        return replayInfo.replayHash + std::to_string(replayInfo.playerId);
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
            FlattenedData2<UnitSoA> units;
            deserialize(units, dbStream);
            result.data.units = recoverFlattenedSortedData2(units);
        }
        {
            FlattenedData2<NeutralUnitSoA> units;
            deserialize(units, dbStream);
            result.data.neutralUnits = recoverFlattenedSortedData2(units);
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

        auto cmp = [](auto &&a, auto &&b) { return a.second.id < b.second.id; };
        serialize(flattenAndSortData2<UnitSoA>(d.data.units, cmp), dbStream);
        serialize(flattenAndSortData2<NeutralUnitSoA>(d.data.neutralUnits, cmp), dbStream);
        return true;
    }
};

}// namespace cvt
