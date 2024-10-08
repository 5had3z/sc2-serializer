/**
 * @file format_compare.cpp
 * @author Bryce Ferenczi
 * @brief This program is used for analysing and benchmarking file sizes. One function writes each of the individual
 * data components of a SC2 replay to separate files so we can see the contribution of units, minimaps, scores etc.
 * Another function this performs is to write the unit data in various forms. From this we can check which
 * rearrangement of the unit data structure reults in the smallest file. This is also timed so we can check for
 * associated computational overhead.
 * @version 0.1
 * @date 2024-05-30
 *
 * @copyright Copyright (c) 2024
 *
 */
// External
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <cxxopts.hpp>
#include <spdlog/fmt/bundled/chrono.h>
#include <spdlog/fmt/bundled/ranges.h>
#include <spdlog/fmt/fmt.h>

// Internal
#include <data_structures/replay_all.hpp>
#include <database.hpp>
#include <serialize.hpp>

#include <chrono>
#include <concepts>
#include <execution>
#include <filesystem>
#include <numeric>
#include <ranges>

namespace fs = std::filesystem;

/**
 * @brief Append data to a file with zlib compression
 * @tparam T type of data to write
 * @param data data to write
 * @param outPath filepath to write
 * @param append should append to file, default = true
 */
template<typename T> void writeData(T data, const std::filesystem::path &outPath, bool append = true)
{
    auto mode = std::ios::binary;
    if (append) { mode |= std::ios::app; }

    namespace bio = boost::iostreams;
    bio::filtering_ostream filterStream{};
    filterStream.push(bio::zlib_compressor(bio::zlib::best_compression));
    filterStream.push(bio::file_sink(outPath.string(), mode));
    cvt::serialize(data, filterStream);
    if (filterStream.bad()) { fmt::print("Error Serializing Data to {}\n", outPath.string()); }
    filterStream.flush();
    filterStream.reset();
}

template<typename T> [[nodiscard]] auto readData(const std::filesystem::path &readPath) -> T
{
    namespace bio = boost::iostreams;
    bio::filtering_istream filterStream{};
    filterStream.push(bio::zlib_decompressor());
    filterStream.push(bio::file_source(readPath.string(), std::ios::binary));
    T data;
    cvt::deserialize(data, filterStream);
    filterStream.reset();
    return data;
}

/**
 * @brief Write each component of the replay data to a separate file
 * @param data replay data to be written to file
 * @param outDir directory to write each component.bin
 */
void writeComponents(const cvt::StepDataSoA &data, const fs::path &outDir)
{
    writeData(data.gameStep, outDir / "gameStep.bin");
    writeData(data.minearals, outDir / "minerals.bin");
    writeData(data.vespene, outDir / "vespene.bin");
    writeData(data.popMax, outDir / "popMax.bin");
    writeData(data.popArmy, outDir / "popArmy.bin");
    writeData(data.popWorkers, outDir / "popWorkers.bin");
    writeData(data.score, outDir / "score.bin");
    writeData(data.visibility, outDir / "visibility.bin");
    writeData(data.creep, outDir / "creep.bin");
    writeData(data.player_relative, outDir / "player_relative.bin");
    writeData(data.alerts, outDir / "alerts.bin");
    writeData(data.buildable, outDir / "buildable.bin");
    writeData(data.pathable, outDir / "pathable.bin");
    writeData(data.actions, outDir / "actions.bin");
    writeData(data.units, outDir / "units.bin");
    writeData(data.neutralUnits, outDir / "neutralUnits.bin");
}

/**
 * @brief Write replay data in AoS and SoA forms for comparison
 * @param data  replay data to be written
 * @param outDir directory to write replay_(soa|aos).bin
 */
void writeReplayStructures(const cvt::ReplayDataSoA &data, const fs::path &outDir)
{
    writeData(data, outDir / "replay_soa.bin");
    writeData(cvt::SoAtoAoS<cvt::ReplayDataSoA, cvt::ReplayData>(data), outDir / "replay_aos.bin");
}

/**
 * @brief Write units to disk in various forms
 * @tparam UnitT Basic type of unit
 * @tparam SoA Structure of arrays variant of unit
 * @tparam Transform Function to convert AoS to SoA
 * @param unitData replay data for units (n timesteps * n units per timestep)
 * @param outDir output directory to write data
 * @param prefix prefix for files
 * @param AoStoSoA instance of aos to soa transform function
 */
template<typename UnitT, typename SoA>
void implWriteUnitT(const std::vector<std::vector<UnitT>> &unitData, const fs::path &outDir, std::string_view prefix)
    requires std::is_same_v<UnitT, typename SoA::struct_type>
{
    using AoS = std::vector<UnitT>;
    // Array-of-Array-of-Structures
    writeData(unitData, outDir / fmt::format("{}_aoaos.bin", prefix));

    // Array-of-Structure-of-Arrays
    {
        std::vector<SoA> units;
        std::ranges::transform(unitData, std::back_inserter(units), [](const AoS &u) { return cvt::AoStoSoA<SoA>(u); });
        writeData(units, outDir / fmt::format("{}_aosoa.bin", prefix));
    }

    // Structure-of-Flattened-Arrays
    {
        std::vector<UnitT> unitFlatten;
        for (auto &&units : unitData) { std::ranges::copy(units, std::back_inserter(unitFlatten)); }
        // Write sorted by time and observation api
        writeData(cvt::AoStoSoA<SoA>(unitFlatten), outDir / fmt::format("{}_sofa.bin", prefix));

        // Write sorted by unit id (and then time by stable sorting)
        std::ranges::stable_sort(unitFlatten, [](const UnitT &a, const UnitT &b) { return a.id < b.id; });
        writeData(cvt::AoStoSoA<SoA>(unitFlatten), outDir / fmt::format("{}_sorted_sofa.bin", prefix));
    }
    // Structure-of-Flattened-Arrays Index Variants
    {
        auto cmp = [](auto &&a, auto &&b) { return a.second.id < b.second.id; };
        writeData(cvt::flattenAndSortData<SoA>(unitData, cmp), outDir / fmt::format("{}_sorted_sofa1.bin", prefix));
        writeData(cvt::flattenAndSortData2<SoA>(unitData, cmp), outDir / fmt::format("{}_sorted_sofa3.bin", prefix));
    }
}

/**
 * @brief Write unit data with different structural methods
 * @param data Replay data to write
 * @param outDir directory to write unit binary data
 */
void writeUnitStructures(const cvt::StepDataSoA &data, const fs::path &outDir)
{
    implWriteUnitT<cvt::Unit, cvt::UnitSoA>(data.units, outDir, "units");
    implWriteUnitT<cvt::NeutralUnit, cvt::NeutralUnitSoA>(data.neutralUnits, outDir, "neutralUnits");
}


using clk = std::chrono::high_resolution_clock;
struct bench_timing
{
    std::vector<clk::duration> readAoS;
    std::vector<clk::duration> readSoA;
    std::vector<clk::duration> recover;
};

static bench_timing timingUnits{};
static bench_timing timingNeutralUnits{};

template<typename UnitT, typename SoA>
void implBenchmarkUnit(const std::vector<std::vector<UnitT>> &unitData, bench_timing &timing)
{
    const auto tempFile = fs::current_path() / "temp.bin";
    writeData(unitData, tempFile, false);
    {
        const auto begin = clk::now();
        const auto ignore = readData<std::vector<std::vector<UnitT>>>(tempFile);
        timing.readAoS.emplace_back(clk::now() - begin);
    }

    const auto flatten =
        cvt::flattenAndSortData2<SoA>(unitData, [](auto &&a, auto &&b) { return a.second.id < b.second.id; });
    writeData(flatten, tempFile, false);
    {
        auto begin = clk::now();
        const auto tmp = readData<cvt::FlattenedData2<SoA>>(tempFile);
        timing.readSoA.emplace_back(clk::now() - begin);
        begin = clk::now();
        const auto recovered = cvt::recoverFlattenedSortedData2<SoA>(tmp);
        timing.recover.emplace_back(clk::now() - begin);
    }

    fs::remove(tempFile);
}

void printStats(const bench_timing &timing, std::string_view prefix)
{
    auto to_ms = [](clk::duration d) { return std::chrono::duration_cast<std::chrono::milliseconds>(d); };

    auto mean_var = [to_ms](std::ranges::range auto vec) {
        const clk::duration sum =
            std::reduce(std::execution::unseq, vec.begin(), vec.end(), clk::duration{}, std::plus<>());
        const clk::duration mean = sum / vec.size();
        auto var_calc = [mean](clk::duration t) {
            auto tmp = (t - mean).count();
            tmp = std::pow(tmp, 2);
            return clk::duration(tmp);
        };
        const clk::duration var =
            std::transform_reduce(
                std::execution::unseq, vec.begin(), vec.end(), clk::duration{}, std::plus<>(), var_calc)
            / vec.size();

        // Cast to millisecond precision, that's about the expected order of magnitude
        return std::make_pair(to_ms(mean), to_ms(var));
    };

    const auto [aos_mean, aos_var] = mean_var(timing.readAoS);
    const auto [soa_mean, soa_var] = mean_var(timing.readSoA);
    const auto [recover_mean, recover_var] = mean_var(timing.recover);

    const auto soaTotal =
        std::views::zip_transform([](auto a, auto b) { return a + b; }, timing.readSoA, timing.recover);
    const auto [total_mean, total_var] = mean_var(soaTotal);

    fmt::println("{} Results:\n AoS: {}\n SoA: {}\n Recover: {}",
        prefix,
        timing.readAoS | std::views::transform(to_ms),
        timing.readSoA | std::views::transform(to_ms),
        timing.recover | std::views::transform(to_ms));

    fmt::println("Summary\n AoS Read: {}({}),\n SoA Read: {}({})\n SoA Decode: {}({})\n SoA Total: {}({})",
        aos_mean,
        aos_var,
        soa_mean,
        soa_var,
        recover_mean,
        recover_var,
        total_mean,
        total_var);
}

void benchmarkUnitFormatting(const cvt::StepDataSoA &data)
{
    implBenchmarkUnit<cvt::Unit, cvt::UnitSoA>(data.units, timingUnits);
    implBenchmarkUnit<cvt::NeutralUnit, cvt::NeutralUnitSoA>(data.neutralUnits, timingNeutralUnits);
}

int main(int argc, char *argv[])
{
    cxxopts::Options cliParser("SC2 Format Comparison", "formats");
    // clang-format off
    cliParser.add_options()
        ("i,input", "Database to Read", cxxopts::value<std::string>())
        ("o,output", "Output path to write results", cxxopts::value<std::string>())
        ("unit-struct", "Analyse the size with different unit structures", cxxopts::value<bool>()->default_value("false"))
        ("components", "Break down contribution of each component", cxxopts::value<bool>()->default_value("false"))
        ("replay-meta", "Compare meta-stucture of entire replay SoA vs AoS", cxxopts::value<bool>()->default_value("false"))
        ("benchmark", "Benchmark flattening and recovering unit data", cxxopts::value<bool>()->default_value("false"))
        ("h,help", "This Help", cxxopts::value<bool>());
    // clang-format on
    const auto cliOpts = cliParser.parse(argc, argv);

    if (cliOpts.count("help")) {
        fmt::println("{}", cliParser.help());
        return 0;
    }

    const fs::path databasePath = cliOpts["input"].as<std::string>();
    if (!fs::exists(databasePath)) {
        fmt::println("Database does not exist: {}", databasePath.string());
        return -1;
    }

    const fs::path writeFolder = cliOpts["output"].as<std::string>();
    if (!fs::exists(writeFolder)) {
        if (!fs::create_directory(writeFolder)) {
            fmt::println("Unable to create output directory {}", writeFolder.string());
            return -1;
        }
    }

    // Setup flags
    const bool unitFlag = cliOpts["unit-struct"].as<bool>();
    const bool compFlag = cliOpts["components"].as<bool>();
    const bool metaFlag = cliOpts["replay-meta"].as<bool>();
    const bool benchFlag = cliOpts["benchmark"].as<bool>();

    if (!(unitFlag | compFlag | metaFlag | benchFlag)) {
        fmt::println("No comparison flags set!\n{}", cliParser.help());
        return -1;
    }

    const cvt::ReplayDatabase<cvt::ReplayDataSoA> database(databasePath);
    for (std::size_t idx = 0; idx < database.size(); ++idx) {
        const auto replayData = database.getEntry(idx);
        if (unitFlag) { writeUnitStructures(replayData.data, writeFolder); }
        if (compFlag) { writeComponents(replayData.data, writeFolder); }
        if (metaFlag) { writeReplayStructures(replayData, writeFolder); }
        if (benchFlag) { benchmarkUnitFormatting(replayData.data); }
        fmt::println("Completed {} of {} Replays", idx + 1, database.size());
    }

    if (benchFlag) {
        printStats(timingUnits, "units");
        printStats(timingNeutralUnits, "neutralUnits");
    }

    return 0;
}
