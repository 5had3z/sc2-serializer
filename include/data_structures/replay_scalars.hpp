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
struct StepDataNoUnitsMinimap
{
    using has_scalar_data = std::true_type;

    std::uint32_t gameStep{};
    std::uint16_t minearals{};
    std::uint16_t vespene{};
    std::uint16_t popMax{};
    std::uint16_t popArmy{};
    std::uint16_t popWorkers{};
    Score score{};

    [[nodiscard]] auto operator==(const StepDataNoUnitsMinimap &other) const noexcept -> bool = default;
};

static_assert(HasScalarData<StepDataNoUnitsMinimap>);

/**
 * @brief SoA representation of an array of StepDataNoUnitsMinimap
 */
struct StepDataNoUnitsMinimapSoA
{
    using has_scalar_data = std::true_type;

    using struct_type = StepDataNoUnitsMinimap;

    std::vector<std::uint32_t> gameStep{};
    std::vector<std::uint16_t> minearals{};
    std::vector<std::uint16_t> vespene{};
    std::vector<std::uint16_t> popMax{};
    std::vector<std::uint16_t> popArmy{};
    std::vector<std::uint16_t> popWorkers{};
    std::vector<Score> score{};

    [[nodiscard]] auto operator==(const StepDataNoUnitsMinimapSoA &other) const noexcept -> bool = default;

    /**
     * @brief Gather step data from each array to make structure of data at step.
     * @param idx time index of replay to gather.
     * @return Gathered step data.
     */
    [[nodiscard]] auto operator[](std::size_t idx) const noexcept -> StepDataNoUnitsMinimap
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

static_assert(HasScalarData<StepDataNoUnitsMinimapSoA> && IsSoAType<StepDataNoUnitsMinimapSoA>);

/**
 * @brief ReplayData with minimap and scalar data
 */
using ReplayDataNoUnitsMiniMap = ReplayDataTemplate<StepDataNoUnitsMinimap>;

/**
 * @brief ReplayData as SoA with minimap and scalar data
 */
using ReplayDataSoANoUnitsMiniMap = ReplayDataTemplateSoA<StepDataNoUnitsMinimapSoA>;
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
