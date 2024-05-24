#pragma once

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
 * @brief General information about the replay.
 */
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
     * @brief type of replay data
     */
    using step_type = StepDataSoAType;

    /**
     * @brief AoS equivalent struct type
     */
    using struct_type = ReplayDataTemplate<typename StepDataSoAType::struct_type>;

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

    /**
     * @brief Gather the step data from struct of arrays at a point in time.
     * @param idx Index of replay to gather data
     * @return Observation data in original struct form
     */
    [[nodiscard]] auto operator[](std::size_t idx) const noexcept -> StepDataSoAType::struct_type { return data[idx]; }
};

/**
 * @brief Interface that tells database how to read/write replay data structure.
 * @tparam ReplayT replay data structure to read/write from database.
 */
template<typename ReplayT> struct DatabaseInterface
{
    /**
     * @brief Datatype of replay
     */
    using value_type = ReplayT;

    /**
     * @brief Retrieves the header information from a replay entry
     * @param dbStream Input file stream of the database
     * @return Struct that contains information about the replay
     */
    [[nodiscard]] static auto getHeaderImpl(std::istream &dbStream) -> ReplayInfo;

    /**
     * @brief Retrieves the hash ID entry from the database stream at the specified entry position.
     * @param dbStream The input file stream of the database.
     * @param entry The position of the entry in the database stream.
     * @return A pair containing the hash ID string and the associated 32-bit unsigned integer.
     */
    [[nodiscard]] static auto getHashIdImpl(std::istream &dbStream) -> std::pair<std::string, std::uint32_t>;

    /**
     * @brief Defines how to read the entry from the database
     * @param dbStream Input stream to read the replay data from
     * @return Replay data read from the
     */
    [[nodiscard]] static auto getEntryImpl(std::istream &dbStream) -> ReplayT;


    /**
     * @brief Defines how to write the replay structure to the file
     * @param replay Replay data structure to write to the stream
     * @param dbStream Output stream to write the replay structure to
     * @return Success flag, true if writing completed without issues.
     */
    [[maybe_unused]] static auto addEntryImpl(const ReplayT &replay, std::ostream &dbStream) noexcept -> bool;
};

/**
 * @brief Concept that checks if a database interface can be constructed from a replay data structure
 *
 * @tparam T data structure of the replay
 */
template<typename T>
concept HasDBInterface = requires { typename DatabaseInterface<T>; };

}// namespace cvt
