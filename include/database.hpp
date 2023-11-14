#pragma once

#include "data.hpp"
#include <filesystem>
#include <unordered_set>

namespace cvt {

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

    // Check if db is full
    [[nodiscard]] auto isFull() const noexcept -> bool;

    // Load existing or create new based on existence
    [[maybe_unused]] auto open(std::filesystem::path dbPath) noexcept -> bool;

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
