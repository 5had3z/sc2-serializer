#include "database.hpp"

#include <cxxopts.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <iostream>
#include <ranges>

namespace fs = std::filesystem;

enum class Stratergy { Replace, Append, Merge };

// Loop until valid user choice
[[nodiscard]] auto getUserChoice() -> Stratergy
{
    using namespace std::string_literals;
    fmt::print("Output file already exists, would you like to [m]erge, [a]ppend or [r]eplace existing file?");
    char opt{};
    bool invalid = true;
    while (invalid) {
        std::cin >> opt;
        invalid = !"mar"s.contains(opt);
        if (invalid) { SPDLOG_ERROR("Invalid character, got {}", opt); }
    }
    switch (opt) {
    case 'm':
        return Stratergy::Merge;
    case 'a':
        return Stratergy::Append;
    case 'r':
        return Stratergy::Replace;
    default:
        SPDLOG_ERROR("How did I get here with {}? Appending anyway...", opt);
        return Stratergy::Append;
    }
}

[[nodiscard]] auto getReplayParts(const fs::path &folder) -> std::vector<fs::path>
{
    auto filteredFiles = fs::directory_iterator{ folder }
                         | std::views::filter([](auto &&e) { return e.path().extension() == ".SC2Replays"; })
                         | std::views::transform([](auto &&file) { return file.path(); });
    std::vector<fs::path> outFiles;
    std::ranges::copy(filteredFiles, std::back_inserter(outFiles));// change to std::ranges::to when available
    return outFiles;
}

auto mergeDb(cvt::ReplayDatabase &target,
    const cvt::ReplayDatabase &source,
    const std::unordered_set<std::string> &knownHashes) -> bool
{
    const std::size_t nItems = source.size();
    for (std::size_t idx = 0; idx < nItems; ++idx) {
        // Deserialize first two entries only!
        auto [hash, id] = source.getHashId(idx);
        if (knownHashes.contains(hash + std::to_string(id))) { continue; }
        auto replayData = source.getEntry(idx);
        bool ok = target.addEntry(replayData);
        if (!ok && target.isFull()) { return false; }
    }
    return true;
}

auto runOverFolder(cvt::ReplayDatabase &mainDb,
    const fs::path &folder,
    const std::unordered_set<std::string> &knownReplays) noexcept -> bool
{
    auto replayFiles = getReplayParts(folder);
    for (auto &&replayFile : replayFiles) {
        cvt::ReplayDatabase partDb(replayFile);
        auto ok = mergeDb(mainDb, partDb, knownReplays);
        if (!ok && mainDb.isFull()) { return false; }
    }
    return true;
}

int main(int argc, char *argv[])
{
    cxxopts::Options cliParser("SC2 DB Merger",
        "Merge a list of DB partitions into a single DB. A --folder that contains"
        " .SC2Replays will be merged into a single file specified by --output");
    // clang-format off
    cliParser.add_options()
        ("f,folder", "Folder with partitions to merge", cxxopts::value<std::string>())
        ("o,output", "Output .SC2Replays file", cxxopts::value<std::string>())
        ("a,append", "Append to existing without prompting (fast but may result in duplicate data)", cxxopts::value<bool>()->default_value("false"))
        ("r,replace", "Replace existing without prompting", cxxopts::value<bool>()->default_value("false"))
        ("m,merge", "Merge with existing without prompting, check for duplicates", cxxopts::value<bool>()->default_value("true"))
        ("h,help", "This help");
    // clang-format on
    const auto cliOpts = cliParser.parse(argc, argv);

    if (cliOpts.count("help")) {
        fmt::print("{}", cliParser.help());
        return 0;
    }

    fs::path partsFolder = cliOpts["folder"].as<std::string>();
    if (!fs::exists(partsFolder)) {
        SPDLOG_ERROR("--folder doesn't exist: {}", partsFolder.string());
        return -1;
    }

    fs::path outFile = cliOpts["output"].as<std::string>();
    // Ensure extension is .SC2Replays
    outFile.replace_extension(".SC2Replays");
    auto preChosen = cliOpts.count("append") + cliOpts.count("replace") + cliOpts.count("merge");
    if (preChosen > 1) {
        SPDLOG_ERROR("Only one of [append|replace|merge] can be used");
        return -1;
    }

    const Stratergy strat = [&]() {
        if (!fs::exists(outFile)) { return Stratergy::Append; }
        if (preChosen == 0) { return getUserChoice(); }
        if (cliOpts.count("append")) { return Stratergy::Append; }
        if (cliOpts.count("replace")) { return Stratergy::Merge; }
        if (cliOpts.count("merge")) { return Stratergy::Merge; }
        SPDLOG_ERROR("How did I get here? Appending anyway I guess.");
        return Stratergy::Append;// Shouldn't get here
    }();
    fmt::print("Strat: {}", static_cast<int>(strat));

    cvt::ReplayDatabase replayDb{};
    std::unordered_set<std::string> knownHashes{};
    if (strat == Stratergy::Replace) { fs::remove(outFile); }
    replayDb.open(outFile);
    if (strat == Stratergy::Merge) { knownHashes = replayDb.getHashes(); }

    const auto ok = runOverFolder(replayDb, partsFolder, knownHashes);
    return ok ? 0 : -1;
}
