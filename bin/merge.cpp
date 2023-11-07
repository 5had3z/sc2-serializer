#include "database.hpp"

#include <cxxopts.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

enum class Stratergy { Replace, Append, Merge };

[[nodiscard]] auto getUserChoice() -> Stratergy
{
    using namespace std::string_literals;
    fmt::print("Output file already exists, would you like to [m]erge, [a]ppend or [r]eplace existing file?");
    char opt{};
    while (!"mar"s.contains(opt)) { std::cin >> opt; }
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

int main(int argc, char *argv[])
{
    cxxopts::Options cliParser("SC2 DB Merger",
      "Merge a list of DB paritions into a single DB. A --folder that contains"
      " .SC2Replays will be merged into a single file specified by --output");
    // clang-format off
    cliParser.add_options()
        ("f,folder", "Folder with partitions to merge", cxxopts::value<std::string>())
        ("o,output", "Ouput .SC2Replays file", cxxopts::value<std::string>())
        ("a,append", "Append to existing without prompting (fast but may result in duplicate data)", cxxopts::value<bool>()->default_value("false"))
        ("r,replace", "Replace existing without prompting", cxxopts::value<bool>()->default_value("false"))
        ("m,merge", "Merge with existing without prompting (slow but prevents duplicate data)", cxxopts::value<bool>()->default_value("true"))
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

    cvt::ReplayDatabase replayDb{};
    std::unordered_set<std::string> knownHashes{};
    switch (strat) {
    case Stratergy::Replace:
        fs::remove(outFile);
        break;
    case Stratergy::Append:
        replayDb.open(outFile);
    case Stratergy::Merge:
        knownHashes = replayDb.getHashes();
    }

    fmt::print("Strat: {}", static_cast<int>(strat));

    return 0;
}