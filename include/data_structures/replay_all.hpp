#pragma once

#include "../serialize.hpp"
#include "replay_interface.hpp"
#include "units.hpp"

#include <ranges>

namespace cvt {

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

using ReplayData2 = ReplayDataTemplate<StepData>;
using ReplayData2SoA = ReplayDataTemplateSoA<StepDataSoA>;
static_assert(std::same_as<ReplayData2SoA::struct_type, ReplayData2>);

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

template<> struct DatabaseInterface<ReplayData2SoA>
{
    static auto getHashIdImpl(std::istream &dbStream) -> std::pair<std::string, std::uint32_t>
    {
        ReplayInfo header;
        deserialize(header, dbStream);
        return std::make_pair(header.replayHash, header.playerId);
    }

    static auto getHeaderImpl(std::istream &dbStream) -> ReplayInfo
    {
        ReplayInfo result;
        deserialize(result, dbStream);
        return result;
    }

    static auto getEntryImpl(std::istream &dbStream) -> ReplayData2SoA
    {
        ReplayData2SoA result;
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
            FlattenedUnits3<UnitSoA> units;
            deserialize(units, dbStream);
            result.data.units = recoverFlattenedSortedUnits3(units);
        }
        {
            FlattenedUnits3<NeutralUnitSoA> units;
            deserialize(units, dbStream);
            result.data.neutralUnits = recoverFlattenedSortedUnits3(units);
        }
        return result;
    }

    static auto addEntryImpl(const ReplayData2SoA &d, std::ostream &dbStream) noexcept -> bool
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
        serialize(flattenAndSortUnits3<UnitSoA>(d.data.units), dbStream);
        serialize(flattenAndSortUnits3<NeutralUnitSoA>(d.data.neutralUnits), dbStream);
        return true;
    }
};

}// namespace cvt
