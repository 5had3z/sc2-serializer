#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include "replay_structures.hpp"
#include "serialize.hpp"

#include <filesystem>
#include <fstream>
#include <numeric>
#include <ranges>

class ReplayDataTest : public testing::Test
{
  protected:
    void SetUp() override
    {
        spdlog::set_level(spdlog::level::warn);

        // Make some random action data
        replay_.stepData.resize(1);
        for (int i = 0; i < 3; ++i) {
            cvt::Action action = { .unit_ids = { 1, 2, 3 },
                .ability_id = 6,
                .target_type = cvt::Action::TargetType::OtherUnit,
                .target = cvt::Action::Target(static_cast<cvt::UID>(3)) };
            replay_.stepData[0].actions.emplace_back(std::move(action));
        }
        for (int i = 0; i < 3; ++i) {
            cvt::Action action = { .unit_ids = { 1, static_cast<cvt::UID>(i) },
                .ability_id = 1,
                .target_type = cvt::Action::TargetType::Position,
                .target = cvt::Action::Target(cvt::Point2d(i, 2)) };
            replay_.stepData[0].actions.emplace_back(std::move(action));
        }

        // Make some random unit data
        for (int i = 0; i < 3; ++i) {
            cvt::Unit unit = { .id = static_cast<cvt::UID>(i),
                .unitType = 2,
                .health = 3,
                .shield = 4,
                .energy = 5.f * i,
                .pos = { 1.1f, 2.2f * i, 3.3f } };
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

TEST(AutoVectorTest, NeutralUnit)
{
    cvt::NeutralUnit unit;
    unit.id = 1;
    unit.pos = { 1.1, 1.2, 1.3 };
    unit.observation = cvt::Visibility::Snapshot;
    unit.contents = 12098;
    unit.heading = 3.1f;
    unit.health = 12312;
    unit.health_max = 123098;
    {
        const auto vec = cvt::vectorize<float>(unit, false);
        ASSERT_EQ(static_cast<float>(unit.id), vec[0]);
        ASSERT_EQ(static_cast<float>(unit.health), vec[2]);
        ASSERT_EQ(static_cast<float>(unit.pos.x), vec[4]);
        ASSERT_EQ(static_cast<float>(unit.pos.y), vec[5]);
        ASSERT_EQ(static_cast<float>(unit.pos.z), vec[6]);
        ASSERT_EQ(static_cast<float>(unit.observation), vec[10]);
    }
    {
        const auto vec = cvt::vectorize<float>(unit, true);
        ASSERT_EQ(static_cast<float>(unit.id), vec[0]);
        ASSERT_EQ(static_cast<float>(unit.health), vec[2]);
        ASSERT_EQ(static_cast<float>(unit.pos.x), vec[4]);
        ASSERT_EQ(static_cast<float>(unit.pos.y), vec[5]);
        ASSERT_EQ(static_cast<float>(unit.pos.z), vec[6]);
        ASSERT_EQ(0.f, vec[10]);// visible
        ASSERT_EQ(1.f, vec[11]);// snapshot
        ASSERT_EQ(0.f, vec[12]);// hidden
    }
}

TEST(AutoVectorTest, Unit)
{
    cvt::Unit unit;
    unit.id = 98712;
    unit.observation = cvt::Visibility::Visible;
    unit.alliance = cvt::Alliance::Self;
    unit.cloak_state = cvt::CloakState::Detected;
    unit.energy = 100;
    unit.pos = { 1.1, 1.2, 1.3 };
    unit.heading = 1.3f;
    unit.build_progress = 1.f;
    unit.is_flying = true;

    {
        const auto vec = cvt::vectorize<float>(unit, false);
        ASSERT_EQ(unit.id, vec[0]);
        ASSERT_EQ(static_cast<float>(unit.alliance), vec[3]);
        ASSERT_EQ(static_cast<float>(unit.cloak_state), vec[4]);
        ASSERT_EQ(unit.energy, vec[11]);
        ASSERT_EQ(unit.pos.x, vec[16]);
        ASSERT_EQ(unit.pos.y, vec[17]);
        ASSERT_EQ(unit.pos.z, vec[18]);
        ASSERT_EQ(unit.heading, vec[19]);
        ASSERT_EQ(unit.build_progress, vec[21]);
        ASSERT_EQ(unit.is_flying, vec[27]);
    }
    {
        const auto vec = cvt::vectorize<float>(unit, true);
        const int obsOff = 2;
        const int aliOff = 3 + obsOff;
        const int clkOff = 4 + aliOff;
        const int addOff = 2 + clkOff;
        ASSERT_EQ(unit.id, vec[0]);
        // alliance
        ASSERT_EQ(1.f, vec[3 + obsOff + 0]);// self
        ASSERT_EQ(0.f, vec[3 + obsOff + 1]);// ally
        ASSERT_EQ(0.f, vec[3 + obsOff + 2]);// neutral
        ASSERT_EQ(0.f, vec[3 + obsOff + 3]);// enemy
        // cloak state
        ASSERT_EQ(0.f, vec[4 + aliOff + 0]);// unknown
        ASSERT_EQ(0.f, vec[4 + aliOff + 1]);// cloaked
        ASSERT_EQ(1.f, vec[4 + aliOff + 2]);// detected
        ASSERT_EQ(0.f, vec[4 + aliOff + 3]);// uncloaked
        ASSERT_EQ(0.f, vec[4 + aliOff + 4]);// allied
        ASSERT_EQ(unit.energy, vec[11 + addOff]);
        ASSERT_EQ(unit.pos.x, vec[16 + addOff]);
        ASSERT_EQ(unit.pos.y, vec[17 + addOff]);
        ASSERT_EQ(unit.pos.z, vec[18 + addOff]);
        ASSERT_EQ(unit.heading, vec[19 + addOff]);
        ASSERT_EQ(unit.build_progress, vec[21 + addOff]);
        ASSERT_EQ(unit.is_flying, vec[27 + addOff]);
    }
}
