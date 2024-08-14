/**
 * @file test_database.cpp
 * @author Bryce Ferenczi
 * @brief Set of basic tests to ensure reading/writing to database works correctly.
 * @version 0.1
 * @date 2024-05-28
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "data_structures/replay_all.hpp"
#include "database.hpp"

#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <spdlog/spdlog.h>

#include <absl/strings/str_format.h>
#include <gtest/gtest.h>
#include <numeric>

namespace fs = std::filesystem;

/**
 * @brief Hash based on Unit ID
 * @tparam T Unit Type
 */
template<typename T> struct HashID
{
    [[nodiscard]] constexpr auto operator()(const T &data) const noexcept -> std::size_t { return data.id; }
};

template<typename UnitT>
void UnitSetEquality(const std::vector<UnitT> &expectedUnits, const std::vector<UnitT> &actualUnits)
{
    using UnitSet = std::unordered_set<UnitT, HashID<UnitT>>;
    const UnitSet expectedSet(expectedUnits.begin(), expectedUnits.end());
    const UnitSet actualSet(actualUnits.begin(), actualUnits.end());
    UnitSet missing;
    for (const auto &elem : expectedSet) {
        if (!actualSet.contains(elem)) { missing.insert(elem); }
    }
    UnitSet extras;
    for (const auto &elem : actualSet) {
        if (!expectedSet.contains(elem)) { extras.insert(elem); }
    }
    ASSERT_EQ(missing, extras) << fmt::format(
        "Failed Unit Set Comparison got {} missing and {} extras", missing.size(), extras.size());
}

template<typename UnitT>
void UnitSetEqualityVec(const std::vector<std::vector<UnitT>> &expectedReplay,
    const std::vector<std::vector<UnitT>> &actualReplay)
{
    for (auto &&[expectedUnits, actualUnits] : std::views::zip(expectedReplay, actualReplay)) {
        UnitSetEquality(expectedUnits, actualUnits);
    }
}

auto createReplay(int seed) -> cvt::ReplayDataSoA
{
    cvt::ReplayData replay_;

    // Make some random action data
    replay_.data.resize(1);
    for (int i = 0; i < 3; ++i) {
        cvt::Action action = { .unit_ids = { 1, 2, static_cast<cvt::UID>(seed) },
            .ability_id = seed + i,
            .target_type = cvt::Action::TargetType::OtherUnit,
            .target = cvt::Action::Target(static_cast<cvt::UID>(3)) };
        replay_.data[0].actions.emplace_back(std::move(action));
    }
    for (int i = 0; i < 3; ++i) {
        cvt::Action action = { .unit_ids = { 1, static_cast<cvt::UID>(i) },
            .ability_id = seed * i,
            .target_type = cvt::Action::TargetType::Position,
            .target = cvt::Action::Target(cvt::Point2d(i, 2)) };
        replay_.data[0].actions.emplace_back(std::move(action));
    }

    // Make some random unit data
    for (int i = 0; i < 3; ++i) {
        cvt::Unit unit = { .id = static_cast<cvt::UID>(i),
            .unitType = 2,
            .health = 1.f * seed,
            .shield = 4,
            .energy = 5.f * i,
            .pos = { 1.1f, 2.2f * i, 3.3f } };
        replay_.data[0].units.emplace_back(std::move(unit));
    }

    // Fill all fields of unit with some data
    {
        cvt::Unit unit{};
        auto *unitPtr = reinterpret_cast<char *>(&unit);
        std::vector<char> unitData(unitPtr, unitPtr + sizeof(cvt::Unit));
        std::iota(unitData.begin(), unitData.end(), seed % 10);
        replay_.data[0].units.emplace_back(std::move(unit));
    }

    // Duplicate entry
    replay_.data.push_back(replay_.data.back());
    // Change one thing about action and unit
    replay_.data.back().actions.back().ability_id += seed;
    replay_.data.back().units.back().energy += 123;

    // Add some "image" data, let it overflow and wrap
    cvt::Image<std::uint8_t> heightMap;
    heightMap.resize(256, 256);
    std::iota(heightMap.data(), heightMap.data() + heightMap.nelem(), 1);
    replay_.header.heightMap = std::move(heightMap);

    // Add a hash "name" to it
    replay_.header.replayHash = "FooBarBaz";
    return cvt::AoStoSoA<cvt::ReplayDataSoA, cvt::ReplayData>(replay_);
}

class DatabaseTest : public testing::Test
{
  protected:
    void SetUp() override
    {
        spdlog::set_level(spdlog::level::warn);
        // Always want to create db fresh
        if (fs::exists(dbPath_)) { fs::remove(dbPath_); }

        replayDb_.open(dbPath_);
        replayDb_.addEntry(createReplay(1));
        replayDb_.addEntry(createReplay(123));
    }

    void TearDown() override { fs::remove(dbPath_); }

    // Defaults
    fs::path dbPath_ = "testdb.sc2db";
    cvt::ReplayDatabase<cvt::ReplayDataSoA> replayDb_;
};

TEST(BoostZlib, WriteRead)
{
    const fs::path testFile = "test.zlib";
    if (fs::exists(testFile)) { fs::remove(testFile); }
    std::vector<int> writeData(8192, 0);

    // Add some uncompressed header padding
    const std::size_t skip = 293;
    {
        std::ofstream output(testFile, std::ios::binary);
        output.write(reinterpret_cast<const char *>(writeData.data()), skip);
    }

    std::iota(writeData.begin(), writeData.end(), 0);
    {
        boost::iostreams::filtering_ostream output;
        output.push(boost::iostreams::zlib_compressor(boost::iostreams::zlib::best_compression));
        output.push(boost::iostreams::file_sink(testFile.string(), std::ios::binary | std::ios::app));
        output.write(reinterpret_cast<const char *>(writeData.data()), writeData.size() * sizeof(int));
        output.reset();
    }

    std::vector<int> readData;
    readData.resize(writeData.size());
    {
        std::ifstream file(testFile, std::ios::binary);
        file.seekg(skip);
        boost::iostreams::filtering_istream input;
        input.push(boost::iostreams::zlib_decompressor());
        input.push(file);
        input.read(reinterpret_cast<char *>(readData.data()), readData.size() * sizeof(int));
        ASSERT_EQ(input.bad(), false);
    }

    ASSERT_EQ(writeData, readData);

    if (fs::exists(testFile)) { fs::remove(testFile); }
}

TEST_F(DatabaseTest, CreateDB)
{
    fs::path tempPath = "testdb1.sc2db";
    // Remove if it already exists
    if (fs::exists(tempPath)) { fs::remove(tempPath); }

    cvt::ReplayDatabase<cvt::ReplayDataSoA> tempDb(tempPath);
    ASSERT_EQ(fs::exists(tempPath), true);
    fs::remove(tempPath);
}

void testReplayEquality(const cvt::ReplayDataSoA &a, const cvt::ReplayDataSoA &b)
{
    ASSERT_EQ(a.header, b.header);
    for (auto idx = 0uz; idx < a.size(); ++idx) {
        auto a_ = a.data[idx];
        auto b_ = b.data[idx];
        if (a_ != b_) {
            if (a_.units != b_.units) {
                UnitSetEquality(a_.units, b_.units);
                a_.units.clear();
                b_.units.clear();
            }
            if (a_.neutralUnits != b_.neutralUnits) {
                UnitSetEquality(a_.neutralUnits, b_.neutralUnits);
                a_.neutralUnits.clear();
                b_.neutralUnits.clear();
            }
            ASSERT_EQ(a_, b_) << fmt::format("Failed at step {}", idx);
        }
    }
}

TEST_F(DatabaseTest, ReadDB)
{
    ASSERT_EQ(replayDb_.size(), 2);
    testReplayEquality(replayDb_.getEntry(0), createReplay(1));
    testReplayEquality(replayDb_.getEntry(1), createReplay(123));
    ASSERT_NE(replayDb_.getEntry(1), createReplay(120));
}

TEST_F(DatabaseTest, LoadDB)
{
    cvt::ReplayDatabase<cvt::ReplayDataSoA> loadDB(dbPath_);
    ASSERT_EQ(replayDb_.size(), loadDB.size());
    for (std::size_t i = 0; i < replayDb_.size(); ++i) { ASSERT_EQ(replayDb_.getEntry(i), loadDB.getEntry(i)); }
}

namespace cvt {

template<typename Sink> void AbslStringify(Sink &sink, Unit unit) { absl::Format(&sink, "%s", std::string(unit)); }

template<typename Sink> void AbslStringify(Sink &sink, NeutralUnit unit)
{
    absl::Format(&sink, "%s", std::string(unit));
}

}// namespace cvt

template<typename T>
[[nodiscard]] auto sortByUnitId(const std::pair<std::uint32_t, T> &a, const std::pair<std::uint32_t, T> &b) -> bool
{
    return a.second.id < b.second.id;
}

TEST(UnitSoA, ConversionToAndFrom)
{
    auto dbPath = std::getenv("SC2_TEST_DB");
    if (dbPath == nullptr) { GTEST_SKIP(); }
    cvt::ReplayDatabase<cvt::ReplayDataSoA> db(dbPath);
    const auto replayData = db.getEntry(0);
    {
        const auto flattened = cvt::flattenAndSortData<cvt::UnitSoA>(replayData.data.units, sortByUnitId<cvt::Unit>);
        const auto recovered = cvt::recoverFlattenedSortedData<cvt::UnitSoA>(flattened);
        UnitSetEqualityVec(replayData.data.units, recovered);
    }
    {
        const auto flattened =
            cvt::flattenAndSortData<cvt::NeutralUnitSoA>(replayData.data.neutralUnits, sortByUnitId<cvt::NeutralUnit>);
        const auto recovered = cvt::recoverFlattenedSortedData<cvt::NeutralUnitSoA>(flattened);
        UnitSetEqualityVec(replayData.data.neutralUnits, recovered);
    }
}

TEST(UnitSoA, ConversionToAndFrom2)
{
    auto dbPath = std::getenv("SC2_TEST_DB");
    if (dbPath == nullptr) { GTEST_SKIP(); }
    cvt::ReplayDatabase<cvt::ReplayDataSoA> db(dbPath);
    const auto replayData = db.getEntry(0);
    {
        const auto flattened = cvt::flattenAndSortData2<cvt::UnitSoA>(replayData.data.units, sortByUnitId<cvt::Unit>);
        const auto recovered = cvt::recoverFlattenedSortedData2<cvt::UnitSoA>(flattened);
        UnitSetEqualityVec(replayData.data.units, recovered);
    }
    {
        const auto flattened =
            cvt::flattenAndSortData2<cvt::NeutralUnitSoA>(replayData.data.neutralUnits, sortByUnitId<cvt::NeutralUnit>);
        const auto recovered = cvt::recoverFlattenedSortedData2<cvt::NeutralUnitSoA>(flattened);
        UnitSetEqualityVec(replayData.data.neutralUnits, recovered);
    }
}
