/**
 * @file replay_interface.hpp
 * @author Bryce Ferenczi
 * @brief Common interface and utilities for replay data structures.
 * @version 0.1
 * @date 2024-05-27
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include "../soa.hpp"
#include "common.hpp"

namespace cvt {

/**
 * @brief Replay has scalar data
 *
 * @tparam T replay data type
 */
template<typename T>
concept HasScalarData = requires { requires std::same_as<typename T::has_scalar_data, std::true_type>; };

/**
 * @brief Replay has minimap data
 *
 * @tparam T replay data type
 */
template<typename T>
concept HasMinimapData = requires { requires std::same_as<typename T::has_minimap_data, std::true_type>; };

/**
 * @brief Replay has unit data
 *
 * @tparam T replay data type
 */
template<typename T>
concept HasUnitData = requires { requires std::same_as<typename T::has_unit_data, std::true_type>; };

/**
 * @brief Type T matches one of the U types
 *
 * @tparam T test type
 * @tparam U candidate types
 */
template<typename T, typename... U>
concept IsAnyOf = (std::same_as<T, U> || ...);

/**
 * @brief Concept that replay has action data
 *
 * @tparam T replay data type
 */
template<typename T>
concept HasActionData =
    requires(T t) { requires IsAnyOf<decltype(t.actions), std::vector<std::vector<Action>>, std::vector<Action>>; };


/**
 * @brief General replay metadata common to all replay data structures.
 */
struct ReplayInfo
{
    /**
     * @brief Unique hash/identifier associated with a game.
     */
    std::string replayHash{};

    /**
     * @brief Version of the game played as a string i.e. 4.9.2.12345.
     */
    std::string gameVersion{};

    /**
     * @brief The POV Player Id, i.e. Player 1 or 2.
     */
    std::uint32_t playerId{};

    /**
     * @brief Number of gameSteps in the replay.
     */
    std::uint32_t durationSteps{};

    /**
     * @brief Race of the POV Player.
     */
    Race playerRace{ Race::Random };

    /**
     * @brief Result of the POV Player.
     */
    Result playerResult{ Result::Undecided };

    /**
     * @brief MMR of the POV Player.
     */
    int playerMMR{};

    /**
     * @brief APM of the POV Player for this game.
     */
    int playerAPM{};

    /**
     * @brief Map width in game units
     */
    int mapWidth{};

    /**
     * @brief Map height in game units
     */
    int mapHeight{};

    /**
     * @brief Image of map topography [0-255], needs to be normalized (shift+divide) to meters after the fact.
     */
    Image<std::uint8_t> heightMap{};

    /**
     * @brief Default equality operator between two replay headers.
     *
     * @param other Replay to compare against.
     * @return True if both are identical.
     */
    [[nodiscard]] auto operator==(const ReplayInfo &other) const noexcept -> bool = default;
};

/**
 * @brief Structure that contains ReplayInfo and replay data stored as an Array-of-Structures, where the array dimension
 * is time.
 * @tparam StepDataType structure type of observation data at each timestep.
 */
template<typename StepDataType> struct ReplayDataTemplate
{
    /**
     * @brief struct type of observation data at each step
     */
    using step_type = StepDataType;

    /**
     * @brief Metadata of the replay
     */
    ReplayInfo header;

    /**
     * @brief Observation data from each sample step in time.
     */
    std::vector<StepDataType> data;

    /**
     * @brief Check equality between replays
     * @param other comparator
     * @return True if both replays are equal
     */
    [[nodiscard]] auto operator==(const ReplayDataTemplate &other) const noexcept -> bool
    {
        return header == other.header && data == other.data;
    }

    /**
     * @brief Number of observed steps in the replay data
     *
     * @return std::size_t
     */
    [[nodiscard]] auto size() const noexcept -> std::size_t { return data.size(); }

    /**
     * @brief Get constant reference to replay data at step index
     *
     * @param index index of replay observations
     * @return const StepDataType&
     */
    [[nodiscard]] auto operator[](std::size_t index) const noexcept -> const StepDataType & { return data[index]; }

    /**
     * @brief Get reference to replay data at step index
     *
     * @param index index of replay observations
     * @return const StepDataType&
     */
    [[nodiscard]] auto operator[](std::size_t index) noexcept -> StepDataType & { return data[index]; }

    /**
     * @brief Get the unique hash ID of the replay
     * @return Mutable reference of string hash of the replay
     */
    [[nodiscard]] auto getReplayHash() noexcept -> std::string & { return header.replayHash; }

    /**
     * @brief Get the unique hash ID of the replay
     * @return Constant reference to string hash of the replay
     */
    [[nodiscard]] auto getReplayHash() const noexcept -> const std::string & { return header.replayHash; }

    /**
     * @brief Get the player perspective Id of the replay
     * @return Mutable reference to Player Id of the replay
     */
    [[nodiscard]] auto getPlayerId() noexcept -> std::uint32_t & { return header.playerId; }

    /**
     * @brief Get the player perspective Id of the replay
     * @return Constant reference to Player Id of the replay
     */
    [[nodiscard]] auto getPlayerId() const noexcept -> std::uint32_t { return header.playerId; }
};


/**
 * @brief Structure that contains ReplayInfo and replay data stored as a Structure-of-Arrays.
 * @tparam StepDataSoAType structure of arrays type of replay data
 */
template<IsSoAType StepDataSoAType> struct ReplayDataTemplateSoA
{
    /**
     * @brief struct type of observation data at each step
     */
    using step_type = StepDataSoAType::struct_type;

    /**
     * @brief AoS equivalent struct type
     */
    using struct_type = ReplayDataTemplate<typename StepDataSoAType::struct_type>;

    /**
     * @brief Header that contains metadata about the replay.
     */
    using header_type = ReplayInfo;

    /**
     * @brief Replay metadata
     */
    ReplayInfo header;

    /**
     * @brief Observation data from each sample step in time in SoA form.
     */
    StepDataSoAType data;

    /**
     * @brief Check equality between replays
     * @param other comparator
     * @return True if both replays are equal
     */
    [[nodiscard]] auto operator==(const ReplayDataTemplateSoA &other) const noexcept -> bool
    {
        return header == other.header && data == other.data;
    }

    /**
     * @brief Number of observed steps in the replay data
     *
     * @return std::size_t
     */
    [[nodiscard]] auto size() const noexcept -> std::size_t { return data.size(); }

    /**
     * @brief Get replay data at step index
     *
     * @param index index of replay observations
     * @return StepDataType
     */
    [[nodiscard]] auto operator[](std::size_t index) const noexcept -> step_type { return data[index]; }

    /**
     * @brief Get the unique hash ID of the replay
     * @return Mutable reference of string hash of the replay
     */
    [[nodiscard]] auto getReplayHash() noexcept -> std::string & { return header.replayHash; }

    /**
     * @brief Get the unique hash ID of the replay
     * @return Constant reference to string hash of the replay
     */
    [[nodiscard]] auto getReplayHash() const noexcept -> const std::string & { return header.replayHash; }

    /**
     * @brief Get the player perspective Id of the replay
     * @return Mutable reference to Player Id of the replay
     */
    [[nodiscard]] auto getPlayerId() noexcept -> std::uint32_t & { return header.playerId; }

    /**
     * @brief Get the player perspective Id of the replay
     * @return Constant reference to Player Id of the replay
     */
    [[nodiscard]] auto getPlayerId() const noexcept -> std::uint32_t { return header.playerId; }
};

}// namespace cvt
