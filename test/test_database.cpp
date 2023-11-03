#include "database.hpp"
#include <gtest/gtest.h>
#include <numeric>

namespace fs = std::filesystem;

auto createReplay(int seed) -> cvt::ReplayData
{
    cvt::ReplayData replay_;

    // Make some random action data
    replay_.stepData.resize(1);
    for (int i = 0; i < 3; ++i) {
        cvt::Action action = { .unit_ids = { 1, 2, static_cast<cvt::UID>(seed) },
            .ability_id = seed + i,
            .target_type = cvt::Action::Target_Type::OtherUnit,
            .target = 3 };
        replay_.stepData[0].actions.emplace_back(std::move(action));
    }
    for (int i = 0; i < 3; ++i) {
        cvt::Action action = { .unit_ids = { 1, static_cast<cvt::UID>(i) },
            .ability_id = seed * i,
            .target_type = cvt::Action::Target_Type::Position,
            .target = cvt::Point2d(i, 2) };
        replay_.stepData[0].actions.emplace_back(std::move(action));
    }

    // Make some random unit data
    for (int i = 0; i < 3; ++i) {
        cvt::Unit unit = {
            .id = i, .unitType = 2, .health = seed, .shield = 4, .energy = 5 * i, .pos = { 1.1f, 2.2f * i, 3.3f }
        };
        replay_.stepData[0].units.emplace_back(std::move(unit));
    }

    // Duplicate entry
    replay_.stepData.push_back(replay_.stepData.back());
    // Change one thing about action and unit
    replay_.stepData.back().actions.back().ability_id += seed;
    replay_.stepData.back().units.back().energy += 123;

    // Add some "image" data, let it overflow and wrap
    cvt::Image<std::uint8_t> heightMap;
    heightMap.resize(256, 256);
    std::iota(heightMap.data(), heightMap.data() + heightMap.nelem(), 1);
    replay_.heightMap = std::move(heightMap);

    // Add a hash "name" to it
    replay_.replayHash = "FooBarBaz";
    return replay_;
}

class DatabaseTest : public testing::Test
{
  protected:
    void SetUp() override
    {
        // Always want to create db fresh
        if (fs::exists(dbPath_)) { fs::remove(dbPath_); }

        replayDb_.open(dbPath_);
        replayDb_.addEntry(createReplay(1));
        replayDb_.addEntry(createReplay(123));
    }

    void TearDown() override { fs::remove(dbPath_); }

    // Defaults
    fs::path dbPath_ = "testdb.sc2db";
    cvt::ReplayDatabase replayDb_ = cvt::ReplayDatabase();
};

TEST_F(DatabaseTest, CreateDB)
{
    fs::path tempPath = "testdb1.sc2db";
    // Remove if it already exists
    if (fs::exists(tempPath)) { fs::remove(tempPath); }

    cvt::ReplayDatabase tempDb(tempPath);
    ASSERT_EQ(fs::exists(tempPath), true);
    fs::remove(tempPath);
}

TEST_F(DatabaseTest, ReadDB)
{
    ASSERT_EQ(replayDb_.size(), 2);
    ASSERT_EQ(replayDb_.getEntry(0), createReplay(1));
    ASSERT_EQ(replayDb_.getEntry(1), createReplay(123));
    ASSERT_NE(replayDb_.getEntry(1), createReplay(120));
}

TEST_F(DatabaseTest, LoadDB)
{
    cvt::ReplayDatabase loadDB(dbPath_);
    ASSERT_EQ(replayDb_.size(), loadDB.size());
    ASSERT_EQ(replayDb_.getEntry(0), loadDB.getEntry(0));
}
