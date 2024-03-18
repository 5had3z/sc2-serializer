#pragma once

#include "../serialize.hpp"
#include "replay_interface.hpp"

#include <ranges>

namespace cvt {

struct StepDataNoUnitsMiniMap
{
    using has_scalar_data = std::true_type;

    std::uint32_t gameStep{};
    std::uint16_t minearals{};
    std::uint16_t vespene{};
    std::uint16_t popMax{};
    std::uint16_t popArmy{};
    std::uint16_t popWorkers{};
    Score score{};

    [[nodiscard]] auto operator==(const StepDataNoUnitsMiniMap &other) const noexcept -> bool = default;
};

static_assert(HasScalarData<StepDataNoUnitsMiniMap>);

struct StepDataSoANoUnitsMiniMap
{
    using has_scalar_data = std::true_type;

    using struct_type = StepDataNoUnitsMiniMap;

    std::vector<std::uint32_t> gameStep{};
    std::vector<std::uint16_t> minearals{};
    std::vector<std::uint16_t> vespene{};
    std::vector<std::uint16_t> popMax{};
    std::vector<std::uint16_t> popArmy{};
    std::vector<std::uint16_t> popWorkers{};
    std::vector<Score> score{};

    [[nodiscard]] auto operator==(const StepDataSoANoUnitsMiniMap &other) const noexcept -> bool = default;

    [[nodiscard]] auto operator[](std::size_t idx) const noexcept -> StepDataNoUnitsMiniMap
    {
        StepDataNoUnitsMiniMap stepData;
        stepData.gameStep = gameStep[idx];
        stepData.minearals = minearals[idx];
        stepData.vespene = vespene[idx];
        stepData.popMax = popMax[idx];
        stepData.popArmy = popArmy[idx];
        stepData.popWorkers = popWorkers[idx];
        stepData.score = score[idx];
        return stepData;
    }
};

static_assert(HasScalarData<StepDataSoANoUnitsMiniMap> && IsSoAType<StepDataSoANoUnitsMiniMap>);

using ReplayDataNoUnitsMiniMap = ReplayDataTemplate<StepDataNoUnitsMiniMap>;
using ReplayDataSoANoUnitsMiniMap = ReplayDataTemplateSoA<StepDataSoANoUnitsMiniMap>;
static_assert(std::same_as<ReplayDataSoANoUnitsMiniMap::struct_type, ReplayDataNoUnitsMiniMap>);

template<> struct DatabaseInterface<ReplayDataSoANoUnitsMiniMap>
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

    static auto getEntryImpl(std::istream &dbStream) -> ReplayDataSoANoUnitsMiniMap
    {
        ReplayDataSoANoUnitsMiniMap result;
        deserialize(result, dbStream);
        return result;
    }

    static auto addEntryImpl(const ReplayDataSoANoUnitsMiniMap &d, std::ostream &dbStream) noexcept -> bool
    {
        serialize(d.header, dbStream);
        return true;
    }
};


}// namespace cvt
