/**
 * @file example.cpp
 * @author Bryce Ferenczi
 * @brief This file is an example of using the serialization and dataset framework on a novel dataset.
 * @version 0.1
 * @date 2024-08-14
 *
 * @copyright Copyright (c) 2024
 *
 */

#include <cxxopts.hpp>
#include <spdlog/fmt/fmt.h>

#include "database.hpp"
#include "instance_transform.hpp"

#include <algorithm>
#include <random>

struct Robot
{
    int uid;
    uint32_t payloadType;
    uint32_t payloadCount;
    float payloadWeight;
    float x;
    float y;
    float battery;
    float condition;
    bool needsMaintanence;
    bool isTeleoperated;
};

struct RobotSoA
{
    using struct_type = Robot;
    std::vector<int> uid;
    std::vector<uint32_t> payloadType;
    std::vector<uint32_t> payloadCount;
    std::vector<float> payloadWeight;
    std::vector<float> x;
    std::vector<float> y;
    std::vector<float> battery;
    std::vector<float> condition;

    // NOTE - Since a bool vector cannot be serialized, we implicitly convert to char
    std::vector<char> needsMaintanence;
    std::vector<char> isTeleoperated;

    [[nodiscard]] auto size() const noexcept -> std::size_t { return uid.size(); }

    [[nodiscard]] auto operator[](std::size_t index) const noexcept -> struct_type
    {
        return cvt::gatherStructAtIndex(*this, index);
    }
};

struct Pedestrian
{
    int uid;
    float estHeight;
    float estAge;
    float x;
    float y;
    bool isCarryingObject;
    bool hasPPE;
};

struct PedestrianSoA
{
    using struct_type = Pedestrian;

    std::vector<int> uid;
    std::vector<float> estHeight;
    std::vector<float> estAge;
    std::vector<float> x;
    std::vector<float> y;

    // NOTE - Since a bool vector cannot be serialized, we implicitly convert to char
    std::vector<char> isCarryingObject;
    std::vector<char> hasPPE;

    [[nodiscard]] auto size() const noexcept -> std::size_t { return uid.size(); }

    [[nodiscard]] auto operator[](std::size_t index) const noexcept -> struct_type
    {
        return cvt::gatherStructAtIndex(*this, index);
    }
};

struct Observation
{
    std::vector<Pedestrian> peds;
    std::vector<Robot> robs;
};

using ObservationTimeseries = std::vector<Observation>;

struct ObservationTimeseriesSoA
{
    using struct_type = Observation;
    std::vector<std::vector<Pedestrian>> peds;
    std::vector<std::vector<Robot>> robs;

    [[nodiscard]] auto size() const noexcept -> std::size_t { return peds.size(); }

    [[nodiscard]] auto operator[](std::size_t index) const noexcept -> struct_type
    {
        return cvt::gatherStructAtIndex(*this, index);
    }
};

struct DataEntry
{
    using header_type = std::string;

    std::string hash;

    ObservationTimeseriesSoA data;

    [[nodiscard]] auto size() const noexcept -> std::size_t { return data.size(); }

    [[nodiscard]] auto operator[](std::size_t index) const noexcept -> Observation { return data[index]; }
};

template<> struct cvt::DatabaseInterface<DataEntry>
{
    static auto getHeaderImpl(std::istream &dbStream) -> std::string
    {
        std::string hash;
        cvt::deserialize(hash, dbStream);
        return hash;
    }

    // Our "uid" is the same as the header for this simple example
    static auto getEntryUIDImpl(std::istream &dbStream) -> std::string { return getHeaderImpl(dbStream); }

    static auto getEntryImpl(std::istream &dbStream) -> DataEntry
    {
        DataEntry result;
        cvt::deserialize(result.hash, dbStream);
        {
            cvt::FlattenedData2<PedestrianSoA> peds;
            cvt::deserialize(peds, dbStream);
            result.data.peds = cvt::recoverFlattenedSortedData2(peds);
        }
        {
            cvt::FlattenedData2<RobotSoA> robs;
            cvt::deserialize(robs, dbStream);
            result.data.robs = cvt::recoverFlattenedSortedData2(robs);
        }
        return result;
    }

    static auto addEntryImpl(const DataEntry &d, std::ostream &dbStream) noexcept -> bool
    {
        cvt::serialize(d.hash, dbStream);
        // Both peds and robs have same 'uid' property we want to sort by, so we can use the same lambda
        auto cmpFn = [](auto &&a, auto &&b) { return a.second.uid < b.second.uid; };
        cvt::serialize(flattenAndSortData2<PedestrianSoA>(d.data.peds, cmpFn), dbStream);
        cvt::serialize(flattenAndSortData2<RobotSoA>(d.data.robs, cmpFn), dbStream);
        return true;
    }
};

using CustomDatabase = cvt::ReplayDatabase<DataEntry>;

void printStepMeta(const Observation &obs)
{
    const auto humansCarrying = std::ranges::count_if(obs.peds, [](auto &&p) { return p.isCarryingObject; });
    fmt::println("Observation with {} of {} humans carrying objects", humansCarrying, obs.peds.size());
}

[[nodiscard]] auto makeRandomObservation() -> Observation
{
    Observation obs;
    std::random_device dev;
    std::mt19937 gen(dev());
    std::uniform_int_distribution<> dist(5, 10);
    std::normal_distribution<float> norm(0, 1);

    for (auto i : std::ranges::iota_view{ 0uz, static_cast<std::size_t>(dist(gen)) }) {
        obs.peds.emplace_back(i, norm(gen), 1.f + static_cast<float>(i), 2.f, 3.f + norm(gen), i % 3 == 0, false);
    }

    for (auto i : std::ranges::iota_view{ 0uz, static_cast<std::size_t>(dist(gen)) }) {
        obs.robs.emplace_back(i, i % 4 + 6, i % 3 + 2, norm(gen) * 30, norm(gen) + i, i, 1.f, 1.f, !(i % 4), false);
    }

    return obs;
}

[[nodiscard]] auto generateRandomData(std::size_t duration) -> ObservationTimeseries
{
    ObservationTimeseries data;
    for (auto it : std::ranges::iota_view{ 0uz, duration }) {
        fmt::println("Generating Step {}", it);
        auto obs = makeRandomObservation();
        printStepMeta(obs);
        data.emplace_back(std::move(obs));
    }
    return data;
}

int main(int argc, char *argv[])
{
    cxxopts::Options cliParser(
        "Custom Dataset Example", "Reads at an index from custom database or writes some fake data of some duration");

    // clang-format off
    cliParser.add_options()
        ("f,file", "Database Filename", cxxopts::value<std::string>())
        ("i,index", "Read from database at this index", cxxopts::value<std::size_t>())
        ("d,duration", "Write a new entry of this duration", cxxopts::value<std::size_t>())
        ("h,help", "This Help");
    // clang-format on
    const auto opts = cliParser.parse(argc, argv);

    if (opts.count("help")) {
        fmt::println("{}", cliParser.help());
        return 0;
    }

    if (!opts.count("file")) {
        fmt::println("Need a file argument");
        return -1;
    }

    if (opts.count("index") == opts.count("duration")) {
        fmt::println("Need to specify index OR duration");
        return -1;
    }

    auto database = CustomDatabase(opts["file"].as<std::string>());
    fmt::println("Database has {} entries", database.size());
    if (opts.count("index")) {
        auto read_data = database.getEntry(opts["index"].as<std::size_t>());
        for (auto idx : std::ranges::iota_view{ 0uz, read_data.size() }) {
            fmt::println("Reading Step {}", idx);
            printStepMeta(read_data[idx]);
        }
    } else if (opts.count("duration")) {
        auto aos = generateRandomData(opts["duration"].as<std::size_t>());
        auto soa = cvt::AoStoSoA<ObservationTimeseriesSoA>(aos);
        database.addEntry(DataEntry{ "lkasdfkljh", soa });
    }

    return 0;
}
