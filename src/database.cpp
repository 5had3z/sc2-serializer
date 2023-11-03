#include "database.hpp"

#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

namespace cvt {

ReplayDatabase::ReplayDatabase(const std::filesystem::path &dbPath) noexcept { this->open(dbPath); }

auto ReplayDatabase::open(std::filesystem::path dbPath) noexcept -> bool
{
    if (dbPath.string() == "") {
        SPDLOG_ERROR("Path to database not set");
        return false;
    }

    dbPath_ = std::move(dbPath);
    bool ok = true;
    if (fs::exists(dbPath_)) {
        ok = this->load();
        if (ok) {
            SPDLOG_INFO("Loaded Existing Database {}", dbPath_.string());
        } else {
            SPDLOG_ERROR("Failed to Load Existing Database {}", dbPath_.string());
        }
    } else {
        ok = this->create();
        if (ok) {
            SPDLOG_INFO("Created New Database: {}", dbPath_.string());
        } else {
            SPDLOG_ERROR("Failed to Create New Database: {}", dbPath_.string());
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

bool ReplayDatabase::addEntry(const ReplayData &data)
{
    // First ensure that the db is not at the maximum 1M entries
    if (!fs::exists(dbPath_)) {
        SPDLOG_ERROR("Database \"{}\" doesn't exist", dbPath_.string());
        return false;
    }
    if (this->isFull()) {
        SPDLOG_ERROR("Database {} is full", dbPath_.string());
        return false;
    }

    // Get the current endPos of the file
    std::ofstream dbStream(dbPath_, std::ios::binary | std::ios::ate | std::ios::in);
    dbStream.seekp(0, std::ios::end);
    entryPtr_.push_back(dbStream.tellp());

    // Write the data
    serialize(data, dbStream);

    // Go to the db index and write new entry
    const std::size_t nEntries = entryPtr_.size();
    dbStream.seekp(0, std::ios::beg);
    dbStream.write(reinterpret_cast<const char *>(&nEntries), sizeof(std::size_t));
    // Write Offset (index) is nEntries - 1 + sizeof(nEntries)
    dbStream.seekp((nEntries - 1) * sizeof(std::streampos) + sizeof(std::size_t), std::ios::beg);
    dbStream.write(reinterpret_cast<const char *>(&entryPtr_.back()), sizeof(std::streampos));
    return true;
}

auto ReplayDatabase::getEntry(std::size_t index) const -> ReplayData
{
    // Check if valid index
    if (index >= entryPtr_.size()) {
        throw std::out_of_range(fmt::format("Index {} exceeds database size {}", index, entryPtr_.size()));
    }

    // Open database and seek to desired position
    std::ifstream dbStream(dbPath_, std::ios::binary);
    dbStream.seekg(entryPtr_[index]);

    // Load and return the data
    ReplayData data;
    deserialize(data, dbStream);
    return data;
}

}// namespace cvt