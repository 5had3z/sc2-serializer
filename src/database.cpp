#include "database.hpp"
#include "serialize.hpp"

#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <fstream>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;
namespace bio = boost::iostreams;

static auto gLogger = spdlog::stdout_color_mt("ReplayDatabase");


namespace cvt {

/**
 * @brief Constructs a ReplayDatabase object.
 *
 * @param dbPath The path to the database.
 */
ReplayDatabase::ReplayDatabase(const std::filesystem::path &dbPath) noexcept { this->open(dbPath); }

/**
 * Opens a replay database at the specified path.
 *
 * @param dbPath The path to the replay database.
 * @return True if the database was successfully opened, false otherwise.
 */
auto ReplayDatabase::open(std::filesystem::path dbPath) noexcept -> bool
{
    if (dbPath.string().empty()) {
        SPDLOG_LOGGER_ERROR(gLogger, "Path to database not set");
        return false;
    }

    dbPath_ = std::move(dbPath);
    bool ok;
    if (fs::exists(dbPath_)) {
        ok = this->load();
        if (ok) {
            SPDLOG_LOGGER_INFO(gLogger, "Loaded Existing Database {}", dbPath_.string());
        } else {
            SPDLOG_LOGGER_ERROR(gLogger, "Failed to Load Existing Database {}", dbPath_.string());
        }
    } else {
        ok = this->create();
        if (ok) {
            SPDLOG_LOGGER_INFO(gLogger, "Created New Database: {}", dbPath_.string());
        } else {
            SPDLOG_LOGGER_ERROR(gLogger, "Failed to Create New Database: {}", dbPath_.string());
        }
    }
    return ok;
}

/**
 * @brief Creates a new entry in the replay database.
 *
 * @return true if the entry was successfully created, false otherwise.
 */
auto ReplayDatabase::create() noexcept -> bool
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
 * @brief Loads the replay database.
 *
 * @return true if the replay database is successfully loaded, false otherwise.
 */
auto ReplayDatabase::load() noexcept -> bool
{
    std::ifstream dbStream(dbPath_, std::ios::binary);
    deserialize(entryPtr_, dbStream);
    return !dbStream.bad();
}

/**
 * Checks if the replay database is full.
 *
 * @return true if the replay database is full, false otherwise.
 */
auto ReplayDatabase::isFull() const noexcept -> bool { return entryPtr_.size() >= maxEntries; }

/**
 * Returns the number of entries in the replay database.
 *
 * @return The number of entries in the replay database.
 */
auto ReplayDatabase::size() const noexcept -> std::size_t { return entryPtr_.size(); }

/**
 * @brief Retrieves the set of hashes stored in the replay database.
 *
 * @return The set of hashes as an unordered set of strings.
 */
auto ReplayDatabase::getHashes() const noexcept -> std::unordered_set<std::string>
{
    std::unordered_set<std::string> replayHashes{};
    std::ifstream dbStream(dbPath_, std::ios::binary);
    for (auto &&entry : entryPtr_) {
        auto [hash, id] = getHashIdEntry(dbStream, entry);
        replayHashes.insert(hash + std::to_string(id));
    }
    return replayHashes;
}

/**
 * @brief Adds an entry to the replay database.
 *
 * @param data The replay data to be added.
 * @return True if the entry was successfully added, false otherwise.
 */
bool ReplayDatabase::addEntry(const ReplayDataSoA &data)
{
    // First ensure that the db is not at the maximum 1M entries
    if (!fs::exists(dbPath_)) {
        SPDLOG_LOGGER_ERROR(gLogger, "Database \"{}\" doesn't exist", dbPath_.string());
        return false;
    }
    if (this->isFull()) {
        SPDLOG_LOGGER_ERROR(gLogger, "Database \"{}\" is full", dbPath_.string());
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
        serialize(data, filterStream);
        if (filterStream.bad()) {
            SPDLOG_LOGGER_ERROR(gLogger, "Error Serializing Replay Data");
            return false;
        }
        filterStream.flush();
        filterStream.reset();
    } catch (const std::bad_alloc &e) {
        SPDLOG_LOGGER_CRITICAL(gLogger, "Failed to write replay, got error: {}", e.what());
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
        SPDLOG_LOGGER_ERROR(gLogger, "Error Writing Db Offset Entry");
        return false;
    }

    SPDLOG_LOGGER_INFO(gLogger, "Saved Replay: {}, PlayerID: {}", data.replayHash, data.playerId);

    return true;
}


/**
 * Retrieves the hash ID entry from the database stream at the specified entry position.
 *
 * @param dbStream The input file stream of the database.
 * @param entry The position of the entry in the database stream.
 * @return A pair containing the hash ID string and the associated 32-bit unsigned integer.
 */
auto ReplayDatabase::getHashIdEntry(std::ifstream &dbStream, std::streampos entry) const
    -> std::pair<std::string, std::uint32_t>
{
    dbStream.seekg(entry);
    // Maybe see if I can reuse the filter and seek
    bio::filtering_istream filterStream{};
    filterStream.push(bio::zlib_decompressor());
    filterStream.push(dbStream);
    std::string replayHash{};
    std::string gameVersion{};
    std::uint32_t playerId{};
    deserialize(replayHash, filterStream);
    deserialize(gameVersion, filterStream);
    deserialize(playerId, filterStream);
    filterStream.reset();
    return std::make_pair(replayHash, playerId);
}

/**
 * @brief Retrieves the hash ID at the specified index.
 *
 * @param index The index of the hash ID to retrieve.
 * @return A pair containing the hash ID as a string and its associated 32-bit unsigned integer.
 */
auto ReplayDatabase::getHashId(std::size_t index) const -> std::pair<std::string, std::uint32_t>
{
    // Check if valid index
    if (index >= entryPtr_.size()) {
        throw std::out_of_range(fmt::format("Index {} exceeds database size {}", index, entryPtr_.size()));
    }

    std::ifstream dbStream(dbPath_, std::ios::binary);
    return getHashIdEntry(dbStream, entryPtr_[index]);
}


/**
 * Retrieves the replay data at the specified index from the database.
 *
 * @param index The index of the replay data entry to retrieve.
 * @return The replay data at the specified index.
 */
auto ReplayDatabase::getEntry(std::size_t index) const -> ReplayDataSoA
{
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
    ReplayDataSoA data;
    try {
        deserialize(data, filterStream);
    } catch (const std::bad_alloc &e) {
        SPDLOG_LOGGER_CRITICAL(
            gLogger, "Failed to load from {} at index {}, got error: {}", dbPath_.string(), index, e.what());
        throw e;
    }
    filterStream.reset();
    // fmt::print("Time Taken to Load: {}ms, Replay Size: {}\n",
    //     std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count(),
    //     data.gameStep.size());
    return data;
}

/**
 * @brief Sets the logging level for the replay database.
 *
 * @param lvl The logging level to set.
 */
void setReplayDBLoggingLevel(spdlog::level::level_enum lvl) noexcept { gLogger->set_level(lvl); }

}// namespace cvt
