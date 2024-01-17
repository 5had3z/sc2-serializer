#include <cxxopts.hpp>
#include <spdlog/fmt/fmt.h>

#include "database.hpp"

#include <filesystem>
#include <ranges>

namespace fs = std::filesystem;

[[nodiscard]] auto read_hash_steps_file(const fs::path &path) noexcept -> std::unordered_map<std::string, std::uint32_t>
{
    std::unordered_map<std::string, std::uint32_t> hash_steps;
    std::ifstream csv_file(path);
    std::string row;
    while (std::getline(csv_file, row)) {
        std::array<std::string, 2> hash_step_pair;
        for (auto &&[idx, elem] :
            std::views::split(row, ',') | std::views::take(hash_step_pair.size()) | std::views::enumerate) {
            hash_step_pair[idx] = std::string(elem.begin(), elem.end());
        }
        constexpr std::size_t trim_size = std::string(".SC2Replays").size();
        const auto hash = hash_step_pair[0].substr(0, hash_step_pair[0].size() - trim_size + 1);
        hash_steps[hash] = std::stoi(hash_step_pair[1]);
    }
    return hash_steps;
}

int main(int argc, char *argv[])
{
    cxxopts::Options cliParser("SC2 Replay Database Format Conversion",
        "Converts one serialized format to another, significantly faster than resimulating replays if all information "
        "for the target format is available in the original source format.");
    // clang-format off
    cliParser.add_options()
        ("i,input", "Source database to convert from", cxxopts::value<std::string>())
        ("o,output", "Destination database, if folder then use source filename", cxxopts::value<std::string>())
        ("steps-file", "Contains hash-gamestep pairs", cxxopts::value<std::string>())
        ("h,help", "This help");
    // clang-format on
    const auto cliOpts = cliParser.parse(argc, argv);

    if (cliOpts.count("help")) {
        fmt::print("{}\n", cliParser.help());
        return 0;
    }

    fs::path sourcePath = cliOpts["input"].as<std::string>();

    const auto *tmp = std::getenv("POD_NAME");
    std::optional<std::string> podIndex;
    if (tmp == nullptr) {
        SPDLOG_INFO("POD_NAME not in ENV, not appending index suffix");
    } else {
        std::string_view s(tmp);
        // Extract the substring from the last delimiter to the end
        podIndex = s.substr(s.find_last_of('-') + 1);

        SPDLOG_INFO("POD_NAME found, using index suffix: {}", podIndex.value());

        sourcePath /= "db_" + podIndex.value() + ".SC2Replays";
    }

    if (!fs::exists(sourcePath)) {
        fmt::print("ERROR: Source Database doesn't exist: {}\n", sourcePath.string());
        return -1;
    }
    cvt::ReplayDatabase<cvt::ReplayDataSoA> source(sourcePath);

    fs::path destPath = cliOpts["output"].as<std::string>();
    if (fs::is_directory(destPath)) {
        destPath /= sourcePath.filename();
    } else if (!fs::exists(destPath.parent_path())) {
        fmt::print("ERROR: Path to destination doesn't exist: {}\n", destPath.parent_path().string());
        return -1;
    }
    if (destPath == sourcePath) {
        fmt::print("ERROR: Source and Destination path match!: {}", sourcePath.string());
        return -1;
    }
    cvt::ReplayDatabase<cvt::ReplayData2SoA> dest(destPath);

    const fs::path hashStepFile = cliOpts["steps-file"].as<std::string>();
    if (!fs::exists(hashStepFile)) { fmt::print("ERROR: Hash-Step file doesn't exist: {}\n", hashStepFile.string()); }
    const auto hash_steps = read_hash_steps_file(hashStepFile);

    auto already_converted = dest.getHashes();
    const auto print_modulo = source.size() / 10;
    for (std::size_t idx = 0; idx < source.size(); ++idx) {

        cvt::ReplayDataSoA old_data;
        try {
            old_data = source.getEntry(idx);
        } catch (const std::bad_alloc &e) {
            fmt::print("Skipping as failed to read...\n");
            continue;
        }

        const auto old_hash = old_data.replayHash + std::to_string(old_data.playerId);
        if (already_converted.contains(old_hash)) { continue; }
        cvt::ReplayData2SoA new_data;
        auto &header = new_data.header;
        header.durationSteps = hash_steps.at(old_data.replayHash);
        header.replayHash = old_data.replayHash;
        header.gameVersion = old_data.gameVersion;
        header.playerId = old_data.playerId;
        header.playerRace = old_data.playerRace;
        header.playerResult = old_data.playerResult;
        header.playerMMR = old_data.playerMMR;
        header.playerAPM = old_data.playerAPM;
        header.mapWidth = old_data.mapWidth;
        header.mapHeight = old_data.mapHeight;
        header.heightMap = old_data.heightMap;

        auto &stepData = new_data.data;
        stepData.gameStep = old_data.gameStep;
        stepData.minearals = old_data.minearals;
        stepData.vespene = old_data.vespene;
        stepData.popMax = old_data.popMax;
        stepData.popArmy = old_data.popArmy;
        stepData.popWorkers = old_data.popWorkers;
        stepData.score = old_data.score;
        stepData.visibility = old_data.visibility;
        stepData.creep = old_data.creep;
        stepData.player_relative = old_data.player_relative;
        stepData.alerts = old_data.alerts;
        stepData.buildable = old_data.buildable;
        stepData.pathable = old_data.pathable;
        stepData.actions = old_data.actions;
        stepData.units = old_data.units;
        stepData.neutralUnits = old_data.neutralUnits;

        dest.addEntry(new_data);
        if (idx % print_modulo == 0) { fmt::print("Converted {} of {} Replays\n", idx + 1, source.size()); }
    }
    fmt::print("DONE - Converted {} of {} Replays\n", dest.size(), source.size());

    return 0;
}
