#include "data_structures/replay_old.hpp"
#include "database.hpp"

#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <spdlog/spdlog.h>

#include <absl/strings/str_format.h>
#include <gtest/gtest.h>
#include <numeric>

namespace fs = std::filesystem;

auto createReplay(int seed) -> cvt::ReplayDataSoA
{
    cvt::ReplayData replay_;

    // Make some random action data
    replay_.stepData.resize(1);
    for (int i = 0; i < 3; ++i) {
        cvt::Action action = { .unit_ids = { 1, 2, static_cast<cvt::UID>(seed) },
            .ability_id = seed + i,
            .target_type = cvt::Action::TargetType::OtherUnit,
            .target = cvt::Action::Target(static_cast<cvt::UID>(3)) };
        replay_.stepData[0].actions.emplace_back(std::move(action));
    }
    for (int i = 0; i < 3; ++i) {
        cvt::Action action = { .unit_ids = { 1, static_cast<cvt::UID>(i) },
            .ability_id = seed * i,
            .target_type = cvt::Action::TargetType::Position,
            .target = cvt::Action::Target(cvt::Point2d(i, 2)) };
        replay_.stepData[0].actions.emplace_back(std::move(action));
    }

    // Make some random unit data
    for (int i = 0; i < 3; ++i) {
        cvt::Unit unit = { .id = static_cast<cvt::UID>(i),
            .unitType = 2,
            .health = 1.f * seed,
            .shield = 4,
            .energy = 5.f * i,
            .pos = { 1.1f, 2.2f * i, 3.3f } };
        replay_.stepData[0].units.emplace_back(std::move(unit));
    }

    // Fill all fields of unit with some data
    {
        cvt::Unit unit{};
        auto *unitPtr = reinterpret_cast<char *>(&unit);
        std::vector<char> unitData(unitPtr, unitPtr + sizeof(cvt::Unit));
        std::iota(unitData.begin(), unitData.end(), seed % 10);
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
    return cvt::AoStoSoA<cvt::ReplayData, cvt::ReplayDataSoA>(replay_);
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

TEST_F(DatabaseTest, ReadDB)
{
    ASSERT_EQ(replayDb_.size(), 2);
    ASSERT_EQ(replayDb_.getEntry(0), createReplay(1));
    ASSERT_EQ(replayDb_.getEntry(1), createReplay(123));
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


/**
 * @brief Hash based on Unit ID
 * @tparam T Unit Type
 */
template<typename T> struct HashID
{
    [[nodiscard]] constexpr auto operator()(const T &data) const noexcept -> std::size_t { return data.id; }
};

template<typename UnitT>
auto fuzzyEquality(std::vector<std::vector<UnitT>> expectedReplay, std::vector<std::vector<UnitT>> actualReplay)
{
    using UnitSet = std::unordered_set<UnitT, HashID<UnitT>>;
    for (auto &&[idx, expectedUnits, actualUnits] :
        std::views::zip(std::views::iota(0), expectedReplay, actualReplay)) {
        UnitSet expectedSet(expectedUnits.begin(), expectedUnits.end());
        UnitSet actualSet(actualUnits.begin(), actualUnits.end());
        UnitSet missing;
        for (const auto &elem : expectedSet) {
            if (!actualSet.contains(elem)) { missing.insert(elem); }
        }
        UnitSet extras;
        for (const auto &elem : actualSet) {
            if (!expectedSet.contains(elem)) { extras.insert(elem); }
        }
        ASSERT_EQ(missing, extras) << fmt::format(
            "Failed at step {}, got {} missing and {} extras", idx, missing.size(), extras.size());
    }
}

TEST(UnitSoA, DISABLED_ConversionToAndFrom)
{
    auto dbPath = std::getenv("SC2_TEST_DB");
    ASSERT_NE(dbPath, nullptr);
    cvt::ReplayDatabase<cvt::ReplayDataSoA> db(dbPath);
    const auto replayData = db.getEntry(0);
    {
        const auto flattened = cvt::flattenAndSortUnits<cvt::UnitSoA>(replayData.units);
        const auto recovered = cvt::recoverFlattenedSortedUnits<cvt::UnitSoA>(flattened);
        fuzzyEquality(replayData.units, recovered);
    }
    {
        const auto flattened = cvt::flattenAndSortUnits<cvt::NeutralUnitSoA>(replayData.neutralUnits);
        const auto recovered = cvt::recoverFlattenedSortedUnits<cvt::NeutralUnitSoA>(flattened);
        fuzzyEquality(replayData.neutralUnits, recovered);
    }
}

// Assumptions do not hold
TEST(UnitSoA, DISABLED_ConversionToAndFrom2)
{
    auto dbPath = std::getenv("SC2_TEST_DB");
    ASSERT_NE(dbPath, nullptr);
    cvt::ReplayDatabase<cvt::ReplayDataSoA> db(dbPath);
    const auto replayData = db.getEntry(0);
    {
        const auto flattened = cvt::flattenAndSortUnits2<cvt::UnitSoA>(replayData.units);
        const auto recovered = cvt::recoverFlattenedSortedUnits2<cvt::UnitSoA>(flattened);
        fuzzyEquality(replayData.units, recovered);
    }
    {
        const auto flattened = cvt::flattenAndSortUnits2<cvt::NeutralUnitSoA>(replayData.neutralUnits);
        const auto recovered = cvt::recoverFlattenedSortedUnits2<cvt::NeutralUnitSoA>(flattened);
        fuzzyEquality(replayData.neutralUnits, recovered);
    }
}

TEST(UnitSoA, DISABLED_ConversionToAndFrom3)
{
    auto dbPath = std::getenv("SC2_TEST_DB");
    ASSERT_NE(dbPath, nullptr);
    cvt::ReplayDatabase<cvt::ReplayDataSoA> db(dbPath);
    const auto replayData = db.getEntry(0);
    {
        const auto flattened = cvt::flattenAndSortUnits3<cvt::UnitSoA>(replayData.units);
        const auto recovered = cvt::recoverFlattenedSortedUnits3<cvt::UnitSoA>(flattened);
        fuzzyEquality(replayData.units, recovered);
    }
    {
        const auto flattened = cvt::flattenAndSortUnits3<cvt::NeutralUnitSoA>(replayData.neutralUnits);
        const auto recovered = cvt::recoverFlattenedSortedUnits3<cvt::NeutralUnitSoA>(flattened);
        fuzzyEquality(replayData.neutralUnits, recovered);
    }
}
