#include "data_structures/units.hpp"
#include "vectorize.hpp"

#include <gtest/gtest.h>

TEST(AutoVectorTest, OneHotEnum)
{
    ASSERT_EQ(cvt::numEnumValues<cvt::Alliance>(), 4uz);
    auto e = cvt::Alliance::Ally;
    auto oneHot = cvt::enumToOneHot<float>(e);
    std::vector<float> expected = { 0.f, 1.f, 0.f, 0.f };
    ASSERT_EQ(oneHot, expected);
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
