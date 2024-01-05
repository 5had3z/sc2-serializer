#include "database.hpp"
#include "serialize.hpp"

#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <fstream>

namespace fs = std::filesystem;
namespace bio = boost::iostreams;

static auto gLogger = spdlog::stdout_color_mt("ReplayDatabase");


namespace cvt {

void setReplayDBLoggingLevel(spdlog::level::level_enum lvl) noexcept { gLogger->set_level(lvl); }

ReplayDatabase::ReplayDatabase(const std::filesystem::path &dbPath) noexcept { this->open(dbPath); }

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

auto ReplayDatabase::load() noexcept -> bool
{
    std::ifstream dbStream(dbPath_, std::ios::binary);
    deserialize(entryPtr_, dbStream);
    return !dbStream.bad();
}

auto ReplayDatabase::isFull() const noexcept -> bool { return entryPtr_.size() >= maxEntries; }

auto ReplayDatabase::size() const noexcept -> std::size_t { return entryPtr_.size(); }

auto ReplayDatabase::path() const noexcept -> const std::filesystem::path & { return dbPath_; }

template<typename T> auto ReplayDatabase::addEntry(const T &data) -> bool
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
        DatabaseIO<T>::addEntryImpl(data, filterStream);
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

template auto ReplayDatabase::addEntry<ReplayDataSoA>(const ReplayDataSoA &data) -> bool;

template<typename T> auto ReplayDatabase::getHashes() const noexcept -> std::unordered_set<std::string>
{
    std::unordered_set<std::string> replayHashes{};
    for (auto &&idx : std::views::iota(this->size())) {
        auto [hash, id] = this->getHashId<T>(idx);
        replayHashes.insert(hash + std::to_string(id));
    }
    return replayHashes;
}

template auto ReplayDatabase::getHashes<ReplayDataSoA>() const noexcept -> std::unordered_set<std::string>;

template<typename T> auto ReplayDatabase::getHashId(std::size_t index) const -> std::pair<std::string, std::uint32_t>
{
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
    auto result = DatabaseIO<T>::getHashIdImpl(filterStream);
    filterStream.reset();
    return result;
}

template auto ReplayDatabase::getHashId<ReplayDataSoA>(std::size_t index) const
    -> std::pair<std::string, std::uint32_t>;

template<typename T> auto ReplayDatabase::getEntry(std::size_t index) const -> T
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
    T data;
    try {
        data = DatabaseIO<T>::getEntryImpl(filterStream);
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

template auto ReplayDatabase::getEntry<ReplayDataSoA>(std::size_t index) const -> ReplayDataSoA;


template<> struct DatabaseIO<ReplayDataSoA>
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


}// namespace cvt
