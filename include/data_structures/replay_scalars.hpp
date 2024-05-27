/**
 * @file replay_scalars.hpp
 * @author Bryce Ferenczi
 * @brief Replay observation data that only contains scalar (score and economy) data. Is compatible with
 * reading ReplayData and ReplayDataNoUnits as the ordering of the data is the same, we just stop reading before minimap
 * data.
 * @version 0.1
 * @date 2024-05-27
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include "../serialize.hpp"
#include "replay_interface.hpp"

#include <ranges>

namespace cvt {

/**
 * @brief Replay step data that only contains scalar data
 */
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

/**
 * @brief SoA representation of an array of StepDataNoUnitsMiniMap
 */
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

    /**
     * @brief Gather step data from each array to make structure of data at step.
     * @param idx time index of replay to gather.
     * @return Gathered step data.
     */
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

/**
 * @brief ReplayData with minimap and scalar data
 */
using ReplayDataNoUnitsMiniMap = ReplayDataTemplate<StepDataNoUnitsMiniMap>;

/**
 * @brief ReplayData as SoA with minimap and scalar data
 */
using ReplayDataSoANoUnitsMiniMap = ReplayDataTemplateSoA<StepDataSoANoUnitsMiniMap>;
static_assert(std::same_as<ReplayDataSoANoUnitsMiniMap::struct_type, ReplayDataNoUnitsMiniMap>);

/**
 * @brief Database interface implementation for ReplayDataSoANoUnitsMiniMap
 */
template<> struct DatabaseInterface<ReplayDataSoANoUnitsMiniMap>
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
