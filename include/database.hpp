/**
 * @file database.hpp
 * @author Bryce Ferenczi
 * @brief Database framework for reading and writing serialized replay data. ReplayDatabase is templated on any type
 * which fulfills the interface concept HasDBInterface, i.e. a specialization is defined for that type to enable
 * reading/writing the main data and any metadata associated with the replay.
 * @version 0.1
 * @date 2024-05-27
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include "serialize.hpp"

#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <spdlog/fmt/chrono.h>
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

/**
 * @brief static global logger
 */
extern std::shared_ptr<spdlog::logger> gLoggerDB;


/**
 * @brief Interface that tells database how to read/write replay data structure.
 * @tparam ReplayT replay data structure to read/write from database.
 */
template<typename T> struct DatabaseInterface
{
    /**
     * @brief Datatype of replay
     */
    using value_type = T;

    /**
     * @brief Type that contains metadata of an entry
     */
    using header_type = typename T::header_type;

    /**
     * @brief Retrieves the header information from a replay entry
     * @param dbStream Input file stream of the database
     * @return Struct that contains information about the replay
     */
    [[nodiscard]] static auto getHeaderImpl(std::istream &dbStream) -> header_type;

    /**
     * @brief Retrieves a unique identifier of the entry in the database. For example, this could be a concatenation of
     * the the Replay Hash and the POV Player's ID.
     * @param dbStream The input file stream of the database.
     * @param entry The position of the entry in the database stream.
     * @return A string the represents a unique identifier of the entry.
     */
    [[nodiscard]] static auto getEntryUIDImpl(std::istream &dbStream) -> std::string;

    /**
     * @brief Defines how to read the entry from the database
     * @param dbStream Input stream to read the replay data from
     * @return Replay data read from the
     */
    [[nodiscard]] static auto getEntryImpl(std::istream &dbStream) -> value_type;


    /**
     * @brief Defines how to write the replay structure to the file
     * @param replay Replay data structure to write to the stream
     * @param dbStream Output stream to write the replay structure to
     * @return Success flag, true if writing completed without issues.
     */
    [[maybe_unused]] static auto addEntryImpl(const value_type &replay, std::ostream &dbStream) noexcept -> bool;
};

/**
 * @brief Concept that checks if a database interface can be constructed from a replay data structure
 *
 * @tparam T data structure of the replay
 */
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
    [[nodiscard]] auto getHeader(std::size_t index) const
    {
        return this->readFromDatabase(index, DatabaseInterface<EntryType>::getHeaderImpl);
    }

    /**
     * @brief Retrieves the UID of an entry in the database.
     * @param index The index of the data to retrieve.
     * @return A pair containing the replay hash and player id.
     */
    [[nodiscard]] auto getEntryUID(std::size_t index) const -> std::string
    {
        return this->readFromDatabase(index, DatabaseInterface<EntryType>::getEntryUIDImpl);
    }

    /**
     * @brief Return a set of all UIDs in the database.
     * @return Unordered set of std::string
     */
    [[nodiscard]] auto getAllUIDs() const -> std::unordered_set<std::string>
    {
        std::unordered_set<std::string> replayHashes{};
        for (auto &&idx : std::views::iota(std::size_t(0), this->size())) {
            replayHashes.insert(this->getEntryUID(idx));
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
        using clock = std::chrono::high_resolution_clock;

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
        // auto start = clock::now();
        T data;
        // const std::streampos pos_start = dbStream.tellg();
        try {
            data = reader(filterStream);
        } catch (const std::bad_alloc &e) {
            SPDLOG_LOGGER_CRITICAL(
                gLoggerDB, "Failed to read from {} at index {}, got error: {}", dbPath_.string(), index, e.what());
            throw e;
        }
        // const std::streampos pos_end = dbStream.tellg();
        // spdlog::debug("Took {} to load {} bytes",
        //     std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - start),
        //     pos_end - pos_start);
        filterStream.reset();
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
