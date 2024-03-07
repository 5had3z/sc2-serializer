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

    [[nodiscard]] auto getReplayHash() noexcept -> std::string & { return header.replayHash; }
    [[nodiscard]] auto getReplayHash() const noexcept -> const std::string & { return header.replayHash; }
    [[nodiscard]] auto getPlayerId() noexcept -> std::uint32_t & { return header.playerId; }
    [[nodiscard]] auto getPlayerId() const noexcept -> std::uint32_t { return header.playerId; }

    [[nodiscard]] auto operator[](std::size_t idx) const noexcept -> StepDataSoAType::struct_type { return data[idx]; }
};

template<typename T> struct DatabaseInterface
{
    using value_type = T;

    /**
     * @brief Retrieves the hash ID entry from the database stream at the specified entry position.
     * @param dbStream The input file stream of the database.
     * @param entry The position of the entry in the database stream.
     * @return A pair containing the hash ID string and the associated 32-bit unsigned integer.
     */
    [[nodiscard]] static auto getHashIdImpl(std::istream &dbStream) -> std::pair<std::string, std::uint32_t>;

    [[nodiscard]] static auto getHeaderImpl(std::istream &dbStream) -> ReplayInfo;

    [[nodiscard]] static auto getEntryImpl(std::istream &dbStream) -> T;

    [[maybe_unused]] static auto addEntryImpl(const T &d, std::ostream &dbStream) noexcept -> bool;
};

template<typename T>
concept HasDBInterface = requires { typename DatabaseInterface<T>; };

}// namespace cvt
