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

class ReplayDatabase
{
  public:
    static const std::size_t maxEntries = 1'000'000;

    ReplayDatabase() {}

    explicit ReplayDatabase(const std::filesystem::path &dbPath) noexcept;

    // Add ReplayData to the database
    [[maybe_unused]] bool addEntry(const ReplayDataSoA &data);

    // Get ReplayData from the database
    [[nodiscard]] auto getEntry(std::size_t index) const -> ReplayDataSoA;

    [[nodiscard]] auto getHashId(std::size_t index) const -> std::pair<std::string, std::uint32_t>;
    [[nodiscard]] auto getHashIdEntry(std::ifstream &dbStream, std::streampos entry) const
        -> std::pair<std::string, std::uint32_t>;// Check if db is full
    [[nodiscard]] auto isFull() const noexcept -> bool;

    // Load existing or create new based on existence
    [[maybe_unused]] auto open(std::filesystem::path dbPath) noexcept -> bool;

    /**
     * @brief Get the number of entries in the database
     * @return Number of entries in the database
     */
    [[nodiscard]] auto size() const noexcept -> std::size_t;

    // Get hashes present in the db
    [[nodiscard]] auto getHashes() const noexcept -> std::unordered_set<std::string>;

  private:
    // Load Existing database
    [[maybe_unused]] auto load() noexcept -> bool;

    // Create new database, clears currently held data
    [[maybe_unused]] auto create() noexcept -> bool;

    std::filesystem::path dbPath_{ "" };
    std::vector<std::streampos> entryPtr_{};
};

}// namespace cvt
