#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <cxxopts.hpp>
#include <database.hpp>
#include <serialize.hpp>
#include <spdlog/fmt/fmt.h>

#include <filesystem>

namespace fs = std::filesystem;

/**
 * @brief Append data to a file with zlib compression
 * @tparam T type of data to write
 * @param data data to write
 * @param outPath filepath to write
 */
template<typename T> void writeData(T data, std::filesystem::path outPath)
{
    namespace bio = boost::iostreams;
    bio::filtering_ostream filterStream{};
    filterStream.push(bio::zlib_compressor(bio::zlib::best_compression));
    filterStream.push(bio::file_sink(outPath, std::ios::binary | std::ios::app));
    cvt::serialize(data, filterStream);
    if (filterStream.bad()) { fmt::print("Error Serializing Data to {}", outPath.string()); }
    filterStream.flush();
    filterStream.reset();
}

/**
 * @brief Write each component of the replay data to a separate file
 * @param data replay data to be written to file
 * @param outDir directory to write each component.bin
 */
void writeComponents(const cvt::ReplayDataSoA &data, const fs::path &outDir)
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
    writeData(cvt::ReplaySoAtoAoS(data), outDir / "replay_aos.bin");
}


/**
 * @brief Write unit data with different structural methods
 * @param data Replay data to write
 * @param outDir direcory to write unit binary data
 */
void writeUnitStructures(const cvt::ReplayDataSoA &data, const fs::path &outDir)
{
    // Array-of-Array-of-Structures
    writeData(data.units, outDir / "units_aoaos.bin");

    // Array-of-Structure-of-Arrays
    {
        std::vector<cvt::UnitSoA> units;
        std::ranges::transform(data.units, std::back_inserter(units), [](const std::vector<cvt::Unit> &u) {
            return cvt::UnitAoStoSoA(u);
        });
        writeData(units, outDir / "units_aosoa.bin");
    }

    // Structure-of-Flattened-Arrays
    {
        std::vector<cvt::Unit> unitFlatten;
        for (auto &&units : data.units) { std::ranges::copy(units, std::back_inserter(unitFlatten)); }
        // Write sorted by time
        writeData(UnitAoStoSoA(unitFlatten), outDir / "units_sofa.bin");
        // Write sorted by unit id (and time by stable sorting)
        std::ranges::stable_sort(unitFlatten, [](const cvt::Unit &a, const cvt::Unit &b) { return a.id < b.id; });
        writeData(UnitAoStoSoA(unitFlatten), outDir / "units_sorted_sofa.bin");
    }
}

int main(int argc, char *argv[])
{
    cxxopts::Options cliParser("SC2 Format Comparison",
        "Reads replay database and re-writes units and neutralUnit informaiton to disk in different structural "
        "formats");
    // clang-format off
    cliParser.add_options()
        ("i,input", "Database to Read", cxxopts::value<std::string>())
        ("o,output", "Output path to write results", cxxopts::value<std::string>())
        ("unit-struct", "Analyse the size with different unit structures", cxxopts::value<bool>()->default_value("false"))
        ("components", "Break down contribution of each component", cxxopts::value<bool>()->default_value("false"))
        ("replay-meta", "Compare meta-stucture of entire replay SoA vs AoS", cxxopts::value<bool>()->default_value("false"))
        ("h,help", "This Help", cxxopts::value<bool>());
    // clang-format on
    const auto cliOpts = cliParser.parse(argc, argv);

    if (cliOpts.count("help")) {
        fmt::print("{}", cliParser.help());
        return 0;
    }

    const fs::path databasePath = cliOpts["input"].as<std::string>();
    if (!fs::exists(databasePath)) {
        fmt::print("Database does not exist: {}", databasePath.string());
        return -1;
    }

    const fs::path writeFolder = cliOpts["output"].as<std::string>();
    if (!fs::exists(writeFolder)) {
        if (!fs::create_directory(writeFolder)) {
            fmt::print("Unable to create output directory {}", writeFolder.string());
            return -1;
        }
    }

    // Setup flags
    const bool unitFlag = cliOpts["unit-struct"].as<bool>();
    const bool compFlag = cliOpts["components"].as<bool>();
    const bool metaFlag = cliOpts["replay-meta"].as<bool>();

    if (!(unitFlag | compFlag | metaFlag)) {
        fmt::print("No comparison flags set!\n{}", cliParser.help());
        return -1;
    }

    const cvt::ReplayDatabase database(databasePath);
    for (std::size_t idx = 0; idx < database.size(); ++idx) {
        const auto replayData = database.getEntry(idx);
        if (unitFlag) { writeUnitStructures(replayData, writeFolder); }
        if (compFlag) { writeComponents(replayData, writeFolder); }
        if (metaFlag) { writeReplayStructures(replayData, writeFolder); }
        fmt::print("Completed {} of {} Replays", idx, database.size());
    }

    return 0;
}
