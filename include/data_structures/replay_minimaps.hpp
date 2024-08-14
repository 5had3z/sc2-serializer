/**
 * @file replay_minimaps.hpp
 * @author Bryce Ferenczi
 * @brief Replay observation data that only contains scalar (score and economy) and minimap data. Is compatible with
 * reading ReplayData as the ordering of the data is the same, we just stop reading before Unit data.
 * @version 0.1
 * @date 2024-05-27
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include "../database.hpp"
#include "replay_interface.hpp"

#include <ranges>

namespace cvt {

/**
 * @brief Replay step data that contains scalar data and minimap data
 */
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

/**
 * @brief SoA representation of an array of StepDataNoUnits
 */
struct StepDataNoUnitsSoA
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

    [[nodiscard]] auto operator==(const StepDataNoUnitsSoA &other) const noexcept -> bool = default;

    /**
     * @brief Gather step data from each array to make structure of data at step.
     * @param idx time index of replay to gather.
     * @return Gathered step data.
     */
    [[nodiscard]] auto operator[](std::size_t idx) const noexcept -> struct_type
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

static_assert(HasScalarData<StepDataNoUnitsSoA> && HasMinimapData<StepDataNoUnitsSoA> && IsSoAType<StepDataNoUnitsSoA>);

/**
 * @brief ReplayData with minimap and scalar data
 */
using ReplayDataNoUnits = ReplayDataTemplate<StepDataNoUnits>;

/**
 * @brief ReplayData as SoA with minimap and scalar data
 */
using ReplayDataSoANoUnits = ReplayDataTemplateSoA<StepDataNoUnitsSoA>;

static_assert(std::same_as<ReplayDataSoANoUnits::struct_type, ReplayDataNoUnits>);

/**
 * @brief Database interface implementation for ReplayDataSoANoUnits
 */
template<> struct DatabaseInterface<ReplayDataSoANoUnits>
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
