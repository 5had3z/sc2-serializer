#pragma once

#include "replay_structures.hpp"
#include "serialize.hpp"

#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <unordered_set>

namespace cvt {

/**
 * @brief Set the Logging Level of the Database Engine
 * @param lvl Level to set
 */
void setReplayDBLoggingLevel(spdlog::level::level_enum lvl) noexcept;

extern std::shared_ptr<spdlog::logger> gLoggerDB;

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

    [[nodiscard]] static auto getEntryImpl(std::istream &dbStream) noexcept -> T;

    [[maybe_unused]] static auto addEntryImpl(const T &d, std::ostream &dbStream) noexcept -> bool;
};

template<> struct DatabaseInterface<ReplayDataSoA>
{
    static auto getHashIdImpl(std::istream &dbStream) -> std::pair<std::string, std::uint32_t>
    {
        std::string replayHash{};
        std::string gameVersion{};
        std::uint32_t playerId{};
        deserialize(replayHash, dbStream);
        deserialize(gameVersion, dbStream);
        deserialize(playerId, dbStream);
        return std::make_pair(replayHash, playerId);
    }

    static auto getEntryImpl(std::istream &dbStream) noexcept -> ReplayDataSoA
    {
        ReplayDataSoA result;
        deserialize(result, dbStream);
        return result;
    }

    static auto addEntryImpl(const ReplayDataSoA &d, std::ostream &dbStream) noexcept -> bool
    {
        serialize(d, dbStream);
        return true;
    }
};

template<> struct DatabaseInterface<ReplayData2SoA>
{
    static auto getHashIdImpl(std::istream &dbStream) -> std::pair<std::string, std::uint32_t>
    {
        ReplayInfo header;
        deserialize(header, dbStream);
        return std::make_pair(header.replayHash, header.playerId);
    }

    static auto getEntryImpl(std::istream &dbStream) noexcept -> ReplayData2SoA
    {
        ReplayData2SoA result;
        deserialize(result.header, dbStream);
        deserialize(result.data.gameStep, dbStream);
        deserialize(result.data.minearals, dbStream);
        deserialize(result.data.vespene, dbStream);
        deserialize(result.data.popMax, dbStream);
        deserialize(result.data.popArmy, dbStream);
        deserialize(result.data.popWorkers, dbStream);
        deserialize(result.data.score, dbStream);
        deserialize(result.data.visibility, dbStream);
        deserialize(result.data.creep, dbStream);
        deserialize(result.data.player_relative, dbStream);
        deserialize(result.data.alerts, dbStream);
        deserialize(result.data.buildable, dbStream);
        deserialize(result.data.pathable, dbStream);
        deserialize(result.data.actions, dbStream);
        {
            FlattenedUnits<UnitSoA> units;
            deserialize(units, dbStream);
            result.data.units = recoverFlattenedSortedUnits(units);
        }
        {
            FlattenedUnits<NeutralUnitSoA> units;
            deserialize(units, dbStream);
            result.data.neutralUnits = recoverFlattenedSortedUnits(units);
        }
        return result;
    }

    static auto addEntryImpl(const ReplayData2SoA &d, std::ostream &dbStream) noexcept -> bool
    {
        serialize(d.header, dbStream);
        serialize(d.data.gameStep, dbStream);
        serialize(d.data.minearals, dbStream);
        serialize(d.data.vespene, dbStream);
        serialize(d.data.popMax, dbStream);
        serialize(d.data.popArmy, dbStream);
        serialize(d.data.popWorkers, dbStream);
        serialize(d.data.score, dbStream);
        serialize(d.data.visibility, dbStream);
        serialize(d.data.creep, dbStream);
        serialize(d.data.player_relative, dbStream);
        serialize(d.data.alerts, dbStream);
        serialize(d.data.buildable, dbStream);
        serialize(d.data.pathable, dbStream);
        serialize(d.data.actions, dbStream);
        serialize(flattenAndSortUnits<UnitSoA>(d.data.units), dbStream);
        serialize(flattenAndSortUnits<NeutralUnitSoA>(d.data.neutralUnits), dbStream);
        return true;
    }
};

template<typename T>
concept HasDBInterface = requires { typename DatabaseInterface<T>; };

/**
 * @brief
 * @tparam T is a DatabaseIO Type that implements interactions with the database
 */
template<HasDBInterface EntryType> class ReplayDatabase
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
    explicit ReplayDatabase(const std::filesystem::path &dbPath) noexcept { this->open(dbPath); }

    /**
     * @brief Adds an entry to the replay database.
     * @param data The replay data to be added.
     * @return True if the entry was successfully added, false otherwise.
     */
    [[maybe_unused]] auto addEntry(const EntryType &data) -> bool
    {
        namespace fs = std::filesystem;
        namespace bio = boost::iostreams;

        // First ensure that the db is not at the maximum 1M entries
        if (!fs::exists(dbPath_)) {
            SPDLOG_LOGGER_ERROR(gLoggerDB, "Database \"{}\" doesn't exist", dbPath_.string());
            return false;
        }
        if (this->isFull()) {
            SPDLOG_LOGGER_ERROR(gLoggerDB, "Database \"{}\" is full", dbPath_.string());
            return false;
        }

        // Get the current endPos of the file
        std::ofstream dbStream(dbPath_, std::ios::binary | std::ios::ate | std::ios::in);
        entryPtr_.push_back(dbStream.tellp());

        // Write compressed data to the end of the file
        try {
            bio::filtering_ostream filterStream{};
            filterStream.push(bio::zlib_compressor(bio::zlib::best_compression));
            filterStream.push(dbStream);
            DatabaseInterface<EntryType>::addEntryImpl(data, filterStream);
            if (filterStream.bad()) {
                SPDLOG_LOGGER_ERROR(gLoggerDB, "Error Serializing Replay Data");
                return false;
            }
            filterStream.flush();
            filterStream.reset();
        } catch (const std::bad_alloc &e) {
            SPDLOG_LOGGER_CRITICAL(gLoggerDB, "Failed to write replay, got error: {}", e.what());
            return false;
        }

        // Go to the db index and write new entry
        const std::size_t nEntries = entryPtr_.size();
        dbStream.seekp(0, std::ios::beg);
        dbStream.write(reinterpret_cast<const char *>(&nEntries), sizeof(std::size_t));
        // Write Offset (index) is nEntries - 1 + sizeof(nEntries)
        dbStream.seekp((nEntries - 1) * sizeof(std::streampos) + sizeof(std::size_t), std::ios::beg);
        dbStream.write(reinterpret_cast<const char *>(&entryPtr_.back()), sizeof(std::streampos));
        if (dbStream.bad()) {
            SPDLOG_LOGGER_ERROR(gLoggerDB, "Error Writing Db Offset Entry");
            return false;
        }

        SPDLOG_LOGGER_INFO(gLoggerDB, "Saved Replay: {}, PlayerID: {}", data.getReplayHash(), data.getPlayerId());

        return true;
    }

    /**
     * Retrieves the replay data at the specified index from the database.
     * @param index The index of the replay data entry to retrieve.
     * @return The replay data at the specified index.
     */
    [[nodiscard]] auto getEntry(std::size_t index) const -> EntryType
    {
        namespace bio = boost::iostreams;
        // using clock = std::chrono::high_resolution_clock;
        // const auto start = clock::now();
        // Check if valid index
        if (index >= entryPtr_.size()) {
            throw std::out_of_range(fmt::format("Index {} exceeds database size {}", index, entryPtr_.size()));
        }

        // Open database and seek to desired position
        std::ifstream dbStream(dbPath_, std::ios::binary);
        dbStream.seekg(entryPtr_[index]);

        bio::filtering_istream filterStream{};
        filterStream.push(bio::zlib_decompressor());
        filterStream.push(dbStream);

        // Load and return the data
        EntryType data;
        try {
            data = DatabaseInterface<EntryType>::getEntryImpl(filterStream);
        } catch (const std::bad_alloc &e) {
            SPDLOG_LOGGER_CRITICAL(
                gLoggerDB, "Failed to load from {} at index {}, got error: {}", dbPath_.string(), index, e.what());
            throw e;
        }
        filterStream.reset();
        // fmt::print("Time Taken to Load: {}ms, Replay Size: {}\n",
        //     std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count(),
        //     data.gameStep.size());
        return data;
    }

    /**
     * @brief Retrieves the hash and player id at the specified index.
     * @param index The index of the data to retrieve.
     * @return A pair containing the replay hash and player id.
     */
    [[nodiscard]] auto getHashId(std::size_t index) const -> std::pair<std::string, std::uint32_t>
    {
        namespace bio = boost::iostreams;
        // Check if valid index
        if (index >= entryPtr_.size()) {
            throw std::out_of_range(fmt::format("Index {} exceeds database size {}", index, entryPtr_.size()));
        }
        std::ifstream dbStream(dbPath_, std::ios::binary);
        dbStream.seekg(entryPtr_[index]);

        // Maybe see if I can reuse the filter and seek
        bio::filtering_istream filterStream{};
        filterStream.push(bio::zlib_decompressor());
        filterStream.push(dbStream);
        auto result = DatabaseInterface<EntryType>::getHashIdImpl(filterStream);
        filterStream.reset();
        return result;
    }

    /**
     * @brief Return an set of hash+playerId entries in the database
     * @return Unordered set of std::string of concatenated hash and playerId
     */
    [[nodiscard]] auto getHashes() const noexcept -> std::unordered_set<std::string>
    {
        std::unordered_set<std::string> replayHashes{};
        for (auto &&idx : std::views::iota(this->size())) {
            auto [hash, id] = this->getHashId(idx);
            replayHashes.insert(hash + std::to_string(id));
        }
        return replayHashes;
    }

    /**
     * @brief Opens a replay database at the specified path.
     * @param dbPath The path to the replay database.
     * @return True if the database was successfully opened, false otherwise.
     */
    [[maybe_unused]] auto open(std::filesystem::path dbPath) noexcept -> bool
    {
        namespace fs = std::filesystem;
        if (dbPath.string().empty()) {
            SPDLOG_LOGGER_ERROR(gLoggerDB, "Path to database not set");
            return false;
        }

        dbPath_ = std::move(dbPath);
        bool ok;
        if (fs::exists(dbPath_)) {
            ok = this->loadIndexTable();
            if (ok) {
                SPDLOG_LOGGER_INFO(gLoggerDB, "Loaded Existing Database {}", dbPath_.string());
            } else {
                SPDLOG_LOGGER_ERROR(gLoggerDB, "Failed to Load Existing Database {}", dbPath_.string());
            }
        } else {
            ok = this->createNewDatabaseFile();
            if (ok) {
                SPDLOG_LOGGER_INFO(gLoggerDB, "Created New Database: {}", dbPath_.string());
            } else {
                SPDLOG_LOGGER_ERROR(gLoggerDB, "Failed to Create New Database: {}", dbPath_.string());
            }
        }
        return ok;
    }

    /**
     * @brief Get the number of entries in the database
     * @return Number of entries in the database
     */
    [[nodiscard]] auto size() const noexcept -> std::size_t { return entryPtr_.size(); }

    /**
     * @brief Checks if the replay database is full.
     * @return true if the replay database is full, false otherwise.
     */
    [[nodiscard]] auto isFull() const noexcept -> bool { return entryPtr_.size() >= maxEntries; }

    /**
     * @brief Get the path of the database instance
     * @return path to database
     */
    [[nodiscard]] auto path() const noexcept -> const std::filesystem::path & { return dbPath_; }

  private:
    /**
     * @brief Loads the entryPtr_ look up table from an existing database on disk.
     * @return true if the replay database is successfully loaded, false otherwise.
     */
    [[maybe_unused]] auto loadIndexTable() noexcept -> bool
    {
        std::ifstream dbStream(dbPath_, std::ios::binary);
        deserialize(entryPtr_, dbStream);
        return !dbStream.bad();
    }

    /**
     * @brief Creates a new empty database.
     * @return true if the database was successfully created, false otherwise.
     */
    [[maybe_unused]] auto createNewDatabaseFile() noexcept -> bool
    {
        entryPtr_.clear();// Clear existing LUT data

        // Write buffer of zeros in 4k chunks (OS block alignment)
        std::array<char, 4096> zeros;
        std::ranges::fill(zeros, 0);

        // Number of chunks of 4k bytes to write (+1 to ceil div)
        constexpr std::size_t nChunks = (maxEntries * sizeof(std::streampos) + sizeof(std::size_t)) / zeros.size() + 1;

        // Write chunks
        std::ofstream dbStream(dbPath_, std::ios::binary);
        for (std::size_t i = 0; i < nChunks; ++i) { dbStream.write(zeros.data(), sizeof(zeros)); }
        return !dbStream.bad();
    }

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
