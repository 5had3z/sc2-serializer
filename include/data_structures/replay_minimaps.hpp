#pragma once

#include "../serialize.hpp"
#include "replay_interface.hpp"

#include <ranges>

namespace cvt {

struct StepDataNoUnits
{
    using has_scalar_data = std::true_type;
    using has_minimap_data = std::true_type;

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

    [[nodiscard]] auto operator==(const StepDataNoUnits &other) const noexcept -> bool = default;
};

static_assert(HasScalarData<StepDataNoUnits> && HasMinimapData<StepDataNoUnits>);

struct StepDataSoANoUnits
{
    using has_scalar_data = std::true_type;
    using has_minimap_data = std::true_type;

    using struct_type = StepDataNoUnits;
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

    [[nodiscard]] auto operator==(const StepDataSoANoUnits &other) const noexcept -> bool = default;

    [[nodiscard]] auto operator[](std::size_t idx) const noexcept -> StepDataNoUnits
    {
        StepDataNoUnits stepData;
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
        return stepData;
    }
};

static_assert(HasScalarData<StepDataSoANoUnits> && HasMinimapData<StepDataSoANoUnits> && IsSoAType<StepDataSoANoUnits>);

using ReplayDataNoUnits = ReplayDataTemplate<StepDataNoUnits>;
using ReplayDataSoANoUnits = ReplayDataTemplateSoA<StepDataSoANoUnits>;
static_assert(std::same_as<ReplayDataSoANoUnits::struct_type, ReplayDataNoUnits>);

template<> struct DatabaseInterface<ReplayDataSoANoUnits>
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

    static auto getEntryImpl(std::istream &dbStream) -> ReplayDataSoANoUnits
    {
        ReplayDataSoANoUnits result;
        deserialize(result, dbStream);
        return result;
    }

    static auto addEntryImpl(const ReplayDataSoANoUnits &d, std::ostream &dbStream) noexcept -> bool
    {
        serialize(d, dbStream);
        return true;
    }
};

}// namespace cvt
