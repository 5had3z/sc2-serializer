#pragma once

#include "common.hpp"

namespace cvt {

template<typename T>
concept HasScalarData = requires { requires std::same_as<typename T::has_scalar_data, std::true_type>; };

template<typename T>
concept HasMinimapData = requires { requires std::same_as<typename T::has_minimap_data, std::true_type>; };

template<typename T>
concept HasUnitData = requires { requires std::same_as<typename T::has_unit_data, std::true_type>; };

template<typename T, typename... U>
concept IsAnyOf = (std::same_as<T, U> || ...);

template<typename T>
concept HasActionData =
    requires(T t) { requires IsAnyOf<decltype(t.actions), std::vector<std::vector<Action>>, std::vector<Action>>; };


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

template<typename StepDataType> struct ReplayDataTemplate
{
    using step_type = StepDataType;
    ReplayInfo header;
    std::vector<StepDataType> data;

    [[nodiscard]] auto operator==(const ReplayDataTemplate &other) const noexcept -> bool
    {
        return header == other.header && data == other.data;
    }

    [[nodiscard]] auto getReplayHash() noexcept -> std::string & { return header.replayHash; }

    [[nodiscard]] auto getReplayHash() const noexcept -> const std::string & { return header.replayHash; }

    [[nodiscard]] auto getPlayerId() noexcept -> std::uint32_t & { return header.playerId; }

    [[nodiscard]] auto getPlayerId() const noexcept -> std::uint32_t { return header.playerId; }
};

template<IsSoAType StepDataSoAType> struct ReplayDataTemplateSoA
{
    using struct_type = ReplayDataTemplate<typename StepDataSoAType::struct_type>;
    using step_type = StepDataSoAType;
    ReplayInfo header;
    StepDataSoAType data;

    [[nodiscard]] auto operator==(const ReplayDataTemplateSoA &other) const noexcept -> bool
    {
        return header == other.header && data == other.data;
    }

    [[nodiscard]] auto getReplayHash() noexcept -> std::string & { return header.replayHash; }

    [[nodiscard]] auto getReplayHash() const noexcept -> const std::string & { return header.replayHash; }

    [[nodiscard]] auto getPlayerId() noexcept -> std::uint32_t & { return header.playerId; }

    [[nodiscard]] auto getPlayerId() const noexcept -> std::uint32_t { return header.playerId; }

    [[nodiscard]] auto operator[](std::size_t idx) const noexcept -> StepDataSoAType::struct_type { return data[idx]; }
};

/**
 * @brief Interface that tells database how to read/write replay data structure.
 * @tparam ReplayT replay data structure to read/write from database.
 */
template<typename ReplayT> struct DatabaseInterface
{
    using value_type = ReplayT;

    /**
     * @brief Retrieves the hash ID entry from the database stream at the specified entry position.
     * @param dbStream The input file stream of the database.
     * @param entry The position of the entry in the database stream.
     * @return A pair containing the hash ID string and the associated 32-bit unsigned integer.
     */
    [[nodiscard]] static auto getHashIdImpl(std::istream &dbStream) -> std::pair<std::string, std::uint32_t>;

    /**
     * @brief Retrieves the header information from a replay entry
     * @param dbStream Input file stream of the database
     * @return Struct that contains information about the replay
     */
    [[nodiscard]] static auto getHeaderImpl(std::istream &dbStream) -> ReplayInfo;


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
