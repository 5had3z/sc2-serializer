#pragma once

#include "data.hpp"

#include <spdlog/common.h>

#include <filesystem>
#include <unordered_set>

namespace cvt {

/**
 * @brief Set the Logging Level of the Database Engine
 * @param lvl Level to set
 */
void setReplayDBLoggingLevel(spdlog::level::level_enum lvl) noexcept;

/**
 * @brief Flattened units in SoA form with associated step index
 * @tparam UnitSoAT Type of flattened unit
 */
template<typename UnitSoAT> struct FlattenedUnits
{
    UnitSoAT units;
    std::vector<std::uint32_t> indicies;
};

template<typename UnitT, typename UnitSoAT>
[[nodiscard]] constexpr auto flattenAndSortUnits(const std::vector<std::vector<UnitT>> &replayUnits) noexcept
    -> FlattenedUnits<UnitSoAT>
{
    using UnitStepT = std::pair<std::uint32_t, UnitT>;
    std::vector<UnitStepT> unitStepFlatten;
    for (auto &&[idx, units] : std::views::enumerate(replayUnits)) {
        std::ranges::transform(
            units, std::back_inserter(unitStepFlatten), [=](UnitT u) { return std::make_pair(idx, u); });
    }

    // Significantly better compressibility when sorted by unit (and implicity time)
    std::ranges::stable_sort(
        unitStepFlatten, [](const UnitStepT &a, const UnitStepT &b) { return a.second.id < b.second.id; });

    // Create flattened SoA
    UnitSoAT unitsSoA = cvt::AoStoSoA(std::views::values(unitStepFlatten));

    // Create accompanying step indicies for reconstruction
    std::vector<uint32_t> indicies;
    indicies.resize(unitStepFlatten.size());
    std::ranges::copy(std::views::keys(unitStepFlatten), indicies.begin());

    return { unitsSoA, indicies };
}

template<typename UnitT, typename UnitSoAT>
[[nodiscard]] constexpr auto recoverFlattenedSortedUnits(const FlattenedUnits<UnitSoAT> &flattenedUnits) noexcept
    -> std::vector<std::vector<UnitT>>
{
    // Create outer dimension with the maximum game step index
    std::vector<std::vector<UnitT>> replayUnits;
    replayUnits.resize(std::ranges::max(flattenedUnits.indicies) + 1);

    // Copy units to correct timestep
    for (auto &&[unitIdx, stepIdx] : std::views::enumerate(flattenedUnits.indicies)) {
        replayUnits[stepIdx].emplace_back(flattenedUnits.units[unitIdx]);
    }

    return replayUnits;
}

class ReplayDatabase
{
  public:
    /**
     * @brief Maximum number of allowed entries in a replay database. This is the maximum lookup table size on disk.
     */
    static const std::size_t maxEntries = 1'000'000;

    /**
     * @brief Create an empty replay database with no associated path
     */
    ReplayDatabase() {}

    /**
     * @brief Constructs a ReplayDatabase and create or load from db path.
     * @param dbPath The path to the database.
     */
    explicit ReplayDatabase(const std::filesystem::path &dbPath) noexcept;

    /**
     * @brief Adds an entry to the replay database.
     * @param data The replay data to be added.
     * @return True if the entry was successfully added, false otherwise.
     */
    [[maybe_unused]] bool addEntry(const ReplayDataSoA &data);

    /**
     * Retrieves the replay data at the specified index from the database.
     * @param index The index of the replay data entry to retrieve.
     * @return The replay data at the specified index.
     */
    [[nodiscard]] auto getEntry(std::size_t index) const -> ReplayDataSoA;

    /**
     * @brief Retrieves the hash ID at the specified index.
     * @param index The index of the hash ID to retrieve.
     * @return A pair containing the hash ID as a string and its associated 32-bit unsigned integer.
     */
    [[nodiscard]] auto getHashId(std::size_t index) const -> std::pair<std::string, std::uint32_t>;

    /**
     * @brief Retrieves the hash ID entry from the database stream at the specified entry position.
     * @param dbStream The input file stream of the database.
     * @param entry The position of the entry in the database stream.
     * @return A pair containing the hash ID string and the associated 32-bit unsigned integer.
     */
    [[nodiscard]] auto getHashIdEntry(std::ifstream &dbStream, std::streampos entry) const
        -> std::pair<std::string, std::uint32_t>;// Check if db is full

    /**
     * @brief Checks if the replay database is full.
     * @return true if the replay database is full, false otherwise.
     */
    [[nodiscard]] auto isFull() const noexcept -> bool;


    /**
     * @brief Opens a replay database at the specified path.
     * @param dbPath The path to the replay database.
     * @return True if the database was successfully opened, false otherwise.
     */
    [[maybe_unused]] auto open(std::filesystem::path dbPath) noexcept -> bool;

    /**
     * @brief Get the number of entries in the database
     * @return Number of entries in the database
     */
    [[nodiscard]] auto size() const noexcept -> std::size_t;

    /**
     * @brief Return an set of hash+playerId entries in the database
     * @return Unordered set of std::string of concatenated hash and playerId
     */
    [[nodiscard]] auto getHashes() const noexcept -> std::unordered_set<std::string>;

    /**
     * @brief Get the path of the database instance
     * @return path to database
     */
    [[nodiscard]] auto path() const noexcept -> const std::filesystem::path &;

  private:
    /**
     * @brief Loads the entryPtr_ look up table from an existing database on disk.
     * @return true if the replay database is successfully loaded, false otherwise.
     */
    [[maybe_unused]] auto load() noexcept -> bool;

    /**
     * @brief Creates a new empty database.
     * @return true if the database was successfully created, false otherwise.
     */
    [[maybe_unused]] auto create() noexcept -> bool;

    /**
     * @brief path to database on disk
     */
    std::filesystem::path dbPath_{ "" };

    /**
     * @brief Look up table for database entries
     */
    std::vector<std::streampos> entryPtr_{};
};

}// namespace cvt
