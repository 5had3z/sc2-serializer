#pragma once

#include "data.hpp"
#include <filesystem>

namespace cvt {

class ReplayDatabase
{
  public:
    static const std::size_t maxEntries = 1'000'000;

    ReplayDatabase() {}

    explicit ReplayDatabase(const std::filesystem::path &dbPath) noexcept;

    // Add ReplayData to the database
    bool addEntry(const ReplayData &data);

    // Get ReplayData from the database
    auto getEntry(std::size_t index) const -> ReplayData;

    // Check if db is full
    auto isFull() const noexcept -> bool;

    // Load existing or create new based on existance
    auto open(std::filesystem::path dbPath) noexcept -> bool;

    auto size() const noexcept -> std::size_t;

  private:
    // Load Existing database
    auto load() noexcept -> bool;

    // Create new database, clears currently held data
    auto create() noexcept -> bool;

    std::filesystem::path dbPath_{ "" };
    std::vector<std::streampos> entryPtr_{};
};

}// namespace cvt