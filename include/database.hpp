#pragma once

#include "data_structures/replay_interface.hpp"
#include "serialize.hpp"

#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <ranges>
#include <unordered_set>

namespace cvt {

/**
 * @brief Set the Logging Level of the Database Engine
 * @param lvl Level to set
 */
void setReplayDBLoggingLevel(spdlog::level::level_enum lvl) noexcept;

extern std::shared_ptr<spdlog::logger> gLoggerDB;

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
     * @brief Open a replay database (creates new or loads existing) with the given path
     * @param dbPath The path to the database to open.
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
        const auto previousEnd = dbStream.tellp();

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

        // Calculate offset of start of db entry
        dbStream.seekp(0, std::ios::beg);
        entryPtr_.emplace_back(previousEnd - dbStream.tellp());

        // Write Offset (index) is nEntries - 1 + sizeof(nEntries)
        const std::size_t nEntries = entryPtr_.size();
        constexpr auto elementSize = sizeof(std::ranges::range_value_t<decltype(entryPtr_)>);
        dbStream.seekp((nEntries - 1) * elementSize + sizeof(std::size_t), std::ios::beg);
        dbStream.write(reinterpret_cast<const char *>(&entryPtr_.back()), elementSize);

        // Write Number of Elements in LUT last to confirm the update
        dbStream.seekp(0, std::ios::beg);
        dbStream.write(reinterpret_cast<const char *>(&nEntries), sizeof(nEntries));

        // Check bad-ness
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
        return this->readFromDatabase(index, DatabaseInterface<EntryType>::getEntryImpl);
    }

    /**
     * @brief Retrieves replay header information from the database
     * @param index Index to read from database
     * @return ReplayInfo at index
     */
    [[nodiscard]] auto getHeader(std::size_t index) const -> ReplayInfo
    {
        return this->readFromDatabase(index, DatabaseInterface<EntryType>::getHeaderImpl);
    }

    /**
     * @brief Retrieves the hash and player id at the specified index.
     * @param index The index of the data to retrieve.
     * @return A pair containing the replay hash and player id.
     */
    [[nodiscard]] auto getHashId(std::size_t index) const -> std::pair<std::string, std::uint32_t>
    {
        return this->readFromDatabase(index, DatabaseInterface<EntryType>::getHashIdImpl);
    }

    /**
     * @brief Return an set of hash+playerId entries in the database
     * @return Unordered set of std::string of concatenated hash and playerId
     */
    [[nodiscard]] auto getHashes() const -> std::unordered_set<std::string>
    {
        std::unordered_set<std::string> replayHashes{};
        for (auto &&idx : std::views::iota(std::size_t{ 0 }, this->size())) {
            auto [hash, id] = this->getHashId(idx);
            replayHashes.insert(hash + std::to_string(id));
        }
        return replayHashes;
    }

    /**
     * @brief Opens a replay database at the specified path, will make a new one if it doesn't already exist.
     * @param dbPath The path to the replay database.
     * @return True if the database was successfully opened, false otherwise.
     */
    [[maybe_unused]] auto open(std::filesystem::path dbPath) -> bool
    {
        namespace fs = std::filesystem;
        if (dbPath.string().empty()) {
            SPDLOG_LOGGER_ERROR(gLoggerDB, "Empty path given to ReplayDatabase::open");
            return false;
        }

        dbPath_ = std::move(dbPath);
        if (fs::exists(dbPath_)) {
            return this->loadIndexTable();
        } else {
            return this->createNewDatabaseFile();
        }
    }

    /**
     * @brief Create new database at specified path
     * @param dbPath path to create database at
     * @return false if there's an error such as database already existing at the path
     */
    [[maybe_unused]] auto create(std::filesystem::path path) -> bool
    {
        namespace fs = std::filesystem;
        if (path.string().empty()) {
            SPDLOG_LOGGER_ERROR(gLoggerDB, "Empty path given to ReplayDatabase::create");
            return false;
        }
        if (fs::exists(path)) {
            SPDLOG_LOGGER_ERROR(gLoggerDB, "Database already exists at path: {}", path.string());
            return false;
        }
        dbPath_ = std::move(path);
        return this->createNewDatabaseFile();
    }

    /**
     * @brief Load existing database at specified path
     * @param path path to load database from
     * @return false if there's an error and a failure to load an existing database
     */
    [[maybe_unused]] auto load(std::filesystem::path path) -> bool
    {
        namespace fs = std::filesystem;
        if (path.string().empty()) {
            SPDLOG_LOGGER_ERROR(gLoggerDB, "Empty path given to ReplayDatabase::load");
            return false;
        }
        if (!fs::exists(path)) {
            SPDLOG_LOGGER_ERROR(gLoggerDB, "Database does not exist at path: {}", path.string());
            return false;
        }
        dbPath_ = std::move(path);
        return this->loadIndexTable();
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
     * @brief Get the current path of the database instance
     * @return path to database
     */
    [[nodiscard]] auto path() const noexcept -> const std::filesystem::path & { return dbPath_; }

  private:
    /**
     * @brief Loads the entryPtr_ look up table from an existing database on disk.
     * @return true if the replay database is successfully loaded, false otherwise.
     */
    [[maybe_unused]] auto loadIndexTable() -> bool
    {
        std::ifstream dbStream(dbPath_, std::ios::binary);
        deserialize(entryPtr_, dbStream);

        const bool ok = !dbStream.bad();
        if (ok) {
            SPDLOG_LOGGER_INFO(gLoggerDB, "Loaded Existing Database Table {}", dbPath_.string());
        } else {
            SPDLOG_LOGGER_ERROR(gLoggerDB, "Failed to Load Existing Database Table {}", dbPath_.string());
        }
        return ok;
    }

    /**
     * @brief Creates a new empty database.
     * @return true if the database was successfully created, false otherwise.
     */
    [[maybe_unused]] auto createNewDatabaseFile() -> bool
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

        const bool ok = !dbStream.bad();
        if (ok) {
            SPDLOG_LOGGER_INFO(gLoggerDB, "Created New Database: {}", dbPath_.string());
        } else {
            SPDLOG_LOGGER_ERROR(gLoggerDB, "Failed to Create New Database: {}", dbPath_.string());
        }
        return ok;
    }

    /**
     * @brief Read from database at index using pointer to reading function
     * @tparam T type returned by the reading function
     * @param index Index of database to read from
     * @param reader pointer to function that knows how to read data
     * @return Struct filled with data read by function pointer if successful, uninitialized if failed ( ͡° ͜ʖ ͡°)
     */
    template<typename T> [[nodiscard]] auto readFromDatabase(std::size_t index, T (*reader)(std::istream &)) const -> T
    {
        namespace bio = boost::iostreams;
        // Check if valid index
        if (index >= entryPtr_.size()) {
            throw std::out_of_range(fmt::format("Index {} exceeds database size {}", index, entryPtr_.size()));
        }

        // Open database and seek to desired position
        std::ifstream dbStream(dbPath_, std::ios::binary);
        dbStream.seekg(entryPtr_[index], std::ios::beg);

        bio::filtering_istream filterStream{};
        filterStream.push(bio::zlib_decompressor());
        filterStream.push(dbStream);

        // Load and return the data
        // using clock = std::chrono::high_resolution_clock;
        // const auto start = clock::now();
        T data;
        try {
            data = reader(filterStream);
        } catch (const std::bad_alloc &e) {
            SPDLOG_LOGGER_CRITICAL(
                gLoggerDB, "Failed to read from {} at index {}, got error: {}", dbPath_.string(), index, e.what());
            throw e;
        }
        filterStream.reset();
        // fmt::print("Time Taken to Load: {}ms, Replay Size: {}\n",
        //     std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count(),
        //     data.gameStep.size());
        return data;
    }

    /**
     * @brief path to database on disk
     */
    std::filesystem::path dbPath_{ "" };

    /**
     * @brief Look up table for database entries
     */
    std::vector<std::int64_t> entryPtr_{};
};


}// namespace cvt
