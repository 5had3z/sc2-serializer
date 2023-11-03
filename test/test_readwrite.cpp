#include <gtest/gtest.h>

#include "data.hpp"
#include <filesystem>
#include <numeric>
#include <ranges>

class ReplayDataTest : public testing::Test
{
  protected:
    void SetUp() override
    {
        // Make some random action data
        replay_.stepData.resize(1);
        for (int i = 0; i < 3; ++i) {
            cvt::Action action = { .unit_ids = { 1, 2, 3 },
                .ability_id = 6,
                .target_type = cvt::Action::Target_Type::OtherUnit,
                .target = 3 };
            replay_.stepData[0].actions.emplace_back(std::move(action));
        }
        for (int i = 0; i < 3; ++i) {
            cvt::Action action = { .unit_ids = { 1, i },
                .ability_id = 1,
                .target_type = cvt::Action::Target_Type::Position,
                .target = cvt::Point2d(i, 2) };
            replay_.stepData[0].actions.emplace_back(std::move(action));
        }

        // Make some random unit data
        for (int i = 0; i < 3; ++i) {
            cvt::Unit unit = {
                .uniqueId = i, .unitType = 2, .health = 3, .shield = 4, .energy = 5 * i, .pos = { 1.1f, 2.2f * i, 3.3f }
            };
            replay_.stepData[0].units.emplace_back(std::move(unit));
        }

        // Duplicate entry
        replay_.stepData.push_back(replay_.stepData.back());
        // Change one thing about action and unit
        replay_.stepData.back().actions.back().ability_id += 10;
        replay_.stepData.back().units.back().energy += 123;

        // Add some "image" data, let it overflow and wrap
        cvt::Image<std::uint8_t> heightMap;
        heightMap.resize(256, 256);
        std::iota(heightMap.data(), heightMap.data() + heightMap.nelem(), 1);
        replay_.heightMap = std::move(heightMap);
    }

    void TearDown() override
    {
        // Remove temp data
        if (std::filesystem::exists(testFilename_)) { std::filesystem::remove(testFilename_); }
    }

    cvt::ReplayData replay_;
    std::string testFilename_ = "testdata.sc2bin";
};

TEST_F(ReplayDataTest, ReadWriteOneUnit)
{
    cvt::Unit writeUnit = replay_.stepData[0].units.front();
    {
        std::ofstream out_stream(testFilename_, std::ios::binary);
        cvt::serialize(writeUnit, out_stream);
    }

    cvt::Unit readUnit;
    {
        std::ifstream in_stream(testFilename_, std::ios::binary);
        cvt::deserialize(readUnit, in_stream);
    }
    ASSERT_EQ(readUnit, writeUnit);
}

TEST_F(ReplayDataTest, ReadWriteManyUnit)
{
    std::vector<cvt::Unit> writeUnits = replay_.stepData.front().units;
    ASSERT_EQ(writeUnits.empty(), false);// Not empty units
    {
        std::ofstream out_stream(testFilename_, std::ios::binary);
        cvt::serialize(writeUnits, out_stream);
    }

    std::vector<cvt::Unit> readUnits;
    {
        std::ifstream in_stream(testFilename_, std::ios::binary);
        cvt::deserialize(readUnits, in_stream);
    }

    // Ensure equal after reading
    ASSERT_EQ(readUnits.size(), writeUnits.size());
    for (auto &&[writeUnit, readUnit] : std::views::zip(writeUnits, readUnits)) { ASSERT_EQ(readUnit, writeUnit); }
}

TEST_F(ReplayDataTest, ReadWriteOneAction)
{
    cvt::Action writeAction = replay_.stepData.front().actions.front();
    {
        std::ofstream out_stream(testFilename_, std::ios::binary);
        cvt::serialize(writeAction, out_stream);
    }

    cvt::Action readAction;
    {
        std::ifstream in_stream(testFilename_, std::ios::binary);
        cvt::deserialize(readAction, in_stream);
    }
    ASSERT_EQ(readAction, writeAction);
}

TEST_F(ReplayDataTest, ReadWriteManyAction)
{
    std::vector<cvt::Action> writeActions = replay_.stepData.front().actions;
    ASSERT_EQ(writeActions.empty(), false);// not empty actions
    {
        std::ofstream out_stream(testFilename_, std::ios::binary);
        cvt::serialize(writeActions, out_stream);
    }

    std::vector<cvt::Action> readActions;
    {
        std::ifstream in_stream(testFilename_, std::ios::binary);
        cvt::deserialize(readActions, in_stream);
    }

    // Ensure equal after reading
    ASSERT_EQ(readActions.size(), writeActions.size());
    for (auto &&[w, r] : std::views::zip(writeActions, readActions)) { ASSERT_EQ(w, r); }
}


TEST_F(ReplayDataTest, ReadWriteOneStep)
{
    cvt::StepData writeStep = replay_.stepData.front();
    {
        std::ofstream out_stream(testFilename_, std::ios::binary);
        cvt::serialize(writeStep, out_stream);
    }

    cvt::StepData readStep;
    {
        std::ifstream in_stream(testFilename_, std::ios::binary);
        cvt::deserialize(readStep, in_stream);
    }
    ASSERT_EQ(writeStep, readStep);
}

TEST_F(ReplayDataTest, ReadWriteManyStep)
{
    std::vector<cvt::StepData> writeSteps = replay_.stepData;
    ASSERT_EQ(writeSteps.empty(), false);
    {
        std::ofstream out_stream(testFilename_, std::ios::binary);
        cvt::serialize(writeSteps, out_stream);
    }

    std::vector<cvt::StepData> readSteps;
    {
        std::ifstream in_stream(testFilename_, std::ios::binary);
        cvt::deserialize(readSteps, in_stream);
    }

    // Ensure equal after reading
    ASSERT_EQ(readSteps.size(), writeSteps.size());
    for (auto &&[w, r] : std::views::zip(writeSteps, readSteps)) { ASSERT_EQ(w, r); }
}

TEST_F(ReplayDataTest, ReadWriteReplay)
{
    cvt::ReplayData writeReplay = replay_;
    {
        std::ofstream out_stream(testFilename_, std::ios::binary);
        cvt::serialize(writeReplay, out_stream);
    }

    cvt::ReplayData readReplay;
    {
        std::ifstream in_stream(testFilename_, std::ios::binary);
        cvt::deserialize(readReplay, in_stream);
    }

    // Ensure equal after reading
    ASSERT_EQ(readReplay, writeReplay);
}