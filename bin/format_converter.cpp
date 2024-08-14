/**
 * @file format_converter.cpp
 * @author Bryce Ferenczi
 * @brief This program converts one form of serialized SC2 data to another. This was originally used from some V1 to V2
 * type data structure (V1 is deleted and removed from the repo). This has been kept so it can be reused if necessary
 * without rewriting from scratch.
 * @version 0.1
 * @date 2024-05-30
 *
 * @copyright Copyright (c) 2024
 *
 */
#include <cxxopts.hpp>
#include <spdlog/fmt/fmt.h>

#include "data_structures/replay_all.hpp"
#include "data_structures/replay_minimaps.hpp"
#include "database.hpp"

#include <filesystem>
#include <ranges>

namespace fs = std::filesystem;

using SrcFormat = cvt::ReplayDataSoA;
using DstFormat = cvt::ReplayDataSoANoUnits;

/**
 * @brief Reads hash,steps pair from text file.
 * @note This was previously required when the number of steps wasn't recorded in the original SC2Replay format. Number
 * of steps in replay was gathered by another program and written to a plain-text file for this program to consume and
 * add for the new format.
 * @param path Path to hash-step file.
 * @return Mapping from replay hash to steps in replay.
 */
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
        "for the target format is available in the original source format. This is probably overkill for simple "
        "conversions that could be written in python instead.");
    // clang-format off
    cliParser.add_options()
        ("i,input", "Source database to convert from", cxxopts::value<std::string>())
        ("o,output", "Destination database, if folder then use source filename", cxxopts::value<std::string>())
        // ("steps-file", "Contains hash-gamestep pairs", cxxopts::value<std::string>())
        ("offset", "Offset to apply to partition index", cxxopts::value<int>())
        ("h,help", "This help");
    // clang-format on
    const auto cliOpts = cliParser.parse(argc, argv);

    if (cliOpts.count("help")) {
        fmt::print("{}\n", cliParser.help());
        return 0;
    }

    fs::path sourcePath = cliOpts["input"].as<std::string>();

    const auto *tmp = std::getenv("POD_NAME");
    if (tmp == nullptr) {
        SPDLOG_INFO("POD_NAME not in ENV, not appending index suffix");
    } else {
        const std::string_view s(tmp);
        // Extract the substring from the last delimiter to the end
        std::string podIndex(s.substr(s.find_last_of('-') + 1));
        SPDLOG_INFO("POD_NAME: {}, using index suffix: {}", s, podIndex);
        if (cliOpts["offset"].count()) { podIndex = std::to_string(std::stoi(podIndex) + cliOpts["offset"].as<int>()); }
        sourcePath.replace_filename(sourcePath.stem().string() + "_" + podIndex + ".SC2Replays");
    }

    if (!fs::is_regular_file(sourcePath)) {
        fmt::print("ERROR: Source Database doesn't exist: {}\n", sourcePath.string());
        return -1;
    }
    cvt::ReplayDatabase<SrcFormat> source;
    if (!source.load(sourcePath)) { return -1; }

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
    cvt::ReplayDatabase<DstFormat> dest(destPath);

    // const fs::path hashStepFile = cliOpts["steps-file"].as<std::string>();
    // if (!fs::exists(hashStepFile)) { fmt::print("ERROR: Hash-Step file doesn't exist: {}\n", hashStepFile.string());
    // } const auto hash_steps = read_hash_steps_file(hashStepFile);

    auto already_converted = dest.getAllUIDs();
    const auto print_modulo = source.size() / 10;
    for (std::size_t idx = 0; idx < source.size(); ++idx) {
        SrcFormat oldReplayData;
        try {
            if (already_converted.contains(source.getEntryUID(idx))) { continue; }
            oldReplayData = source.getEntry(idx);
        } catch (const std::bad_alloc &err) {
            SPDLOG_ERROR("Skipping index {}, due to read failure", idx);
            continue;
        }
        DstFormat newReplayData;
        newReplayData.header = oldReplayData.header;

        auto &newStepData = newReplayData.data;
        const auto &oldStepData = oldReplayData.data;
        newStepData.gameStep = oldStepData.gameStep;
        newStepData.minearals = oldStepData.minearals;
        newStepData.vespene = oldStepData.vespene;
        newStepData.popMax = oldStepData.popMax;
        newStepData.popArmy = oldStepData.popArmy;
        newStepData.popWorkers = oldStepData.popWorkers;
        newStepData.score = oldStepData.score;
        newStepData.visibility = oldStepData.visibility;
        newStepData.creep = oldStepData.creep;
        newStepData.player_relative = oldStepData.player_relative;
        newStepData.alerts = oldStepData.alerts;
        newStepData.buildable = oldStepData.buildable;
        newStepData.pathable = oldStepData.pathable;

        dest.addEntry(newReplayData);
        if (idx % print_modulo == 0) { fmt::print("Converted {} of {} Replays\n", idx + 1, source.size()); }
        // Add into already_converted to filter out potential duplicates from the original dataset
        already_converted.insert(newReplayData.header.replayHash + std::to_string(newReplayData.header.playerId));
    }
    fmt::print("DONE - Converted {} of {} Replays\n", dest.size(), source.size());

    return 0;
}
