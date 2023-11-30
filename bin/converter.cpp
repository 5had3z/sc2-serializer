#include <cxxopts.hpp>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>

#include "observer.hpp"

namespace fs = std::filesystem;


/**
 * @brief Add known bad replays from file to converter's knownHashes
 * @param badFile Path to file containing known bad replays
 * @param converter Converter engine to add replayHash to
 */
void registerKnownBadReplays(const fs::path &badFile, cvt::BaseConverter *converter)
{
    std::ifstream badFileStream(badFile);
    std::string badHash;
    while (std::getline(badFileStream, badHash)) {
        for (auto &&playerId : std::array{ 1, 2 }) { converter->addKnownHash(badHash + std::to_string(playerId)); }
    }
}


/**
 * @brief Get Replay hashes from a partition file
 * @param partitionFile File that contains newline separated replay hashes or filenames
 * @return Vector of Hash Strings
 */
auto getReplaysFromFile(const std::string &partitionFile) noexcept -> std::vector<std::string>
{
    SPDLOG_INFO("Loading replays from {}", partitionFile);
    std::vector<std::string> replays;
    std::ifstream partStream(partitionFile);
    std::istream_iterator<std::string> fileIt(partStream);
    std::copy(fileIt, {}, std::back_inserter(replays));

    // If the file suffix is on the replay hashes then trim them off
    // We assume if the first entry has the suffix, all should have a suffix
    using namespace std::string_literals;
    constexpr std::string_view fileExt(".SC2Replay");
    if (!replays.empty() && replays.front().ends_with(fileExt)) {
        std::ranges::for_each(replays, [extLen = fileExt.length()](std::string &r) { r.erase(r.length() - extLen); });
    }
    return replays;
}


/**
 * @brief Get replay hashes from folder of replays
 * @param folder Folder that contains replays
 * @return Vector of Hash Strings
 */
auto getReplaysFromFolder(const std::string_view folder) noexcept -> std::vector<std::string>
{
    SPDLOG_INFO("Searching replays in {}", folder);
    std::vector<std::string> replays;
    std::ranges::transform(fs::directory_iterator{ folder }, std::back_inserter(replays), [](auto &&e) {
        return e.path().stem().string();
    });
    return replays;
}

void loopReplayFiles(const fs::path &replayFolder,
    const std::vector<std::string> &replayHashes,
    sc2::Coordinator &coordinator,
    cvt::BaseConverter *converter,
    std::optional<fs::path> badFile)
{
    std::size_t nComplete = 0;
    for (auto &&replayHash : replayHashes) {
        const auto replayPath = (replayFolder / replayHash).replace_extension(".SC2Replay");
        if (!fs::exists(replayPath)) {
            SPDLOG_ERROR("Replay file doesn't exist {}", replayPath.string());
            continue;
        }

        for (uint32_t playerId = 1; playerId < 3; ++playerId) {
            const std::string replayHashPlayer = replayHash + std::to_string(playerId);
            if (converter->isKnownHash(replayHashPlayer)) {
                SPDLOG_INFO("Skipping known Replay {}, PlayerID: {}", replayHash, playerId);
                continue;
            }
            // Setup Replay with Player
            converter->setReplayInfo(replayHash, playerId);
            coordinator.SetReplayPath(replayPath.string());
            coordinator.SetReplayPerspective(playerId);
            // Run Replay
            while (coordinator.Update()) {}
            // If update has exited and games haven't ended, there must've been an error
            if (!converter->hasWritten()) {
                SPDLOG_ERROR("Finished Game Without Writing, Must Contain An Error, Skipping");
                if (badFile.has_value()) {
                    SPDLOG_INFO("Adding bad replay to file: {}", replayHash);
                    std::ofstream badFileStream(*badFile, std::ios::app);
                    badFileStream << replayHash << "\n";
                }
                break;
            }
            converter->addKnownHash(replayHashPlayer);
        }

        SPDLOG_INFO("Completed {} of {} replays", ++nComplete, replayHashes.size());
    }
}


auto main(int argc, char *argv[]) -> int
{
    cxxopts::Options cliParser(
        "SC2 Replay Converter", "Convert SC2 Replays into a database which can be sampled for machine learning");
    // clang-format off
    cliParser.add_options()
      ("r,replays", "path to folder of replays", cxxopts::value<std::string>())
      ("p,partition", "partition file to select a subset of replays from the folder", cxxopts::value<std::string>())
      ("o,output", "output filename for replay database", cxxopts::value<std::string>())
      ("c,converter", "type of converter to use [action|full|strided]", cxxopts::value<std::string>())
      ("s,stride", "stride for the strided converter", cxxopts::value<std::size_t>())
      ("g,game", "path to game executable", cxxopts::value<std::string>())
      ("b,badfile", "file that contains a known set of bad replays", cxxopts::value<std::string>())
      ("offset", "Offset to apply to partition index", cxxopts::value<int>())
      ("h,help", "This help");
    // clang-format on
    const auto cliOpts = cliParser.parse(argc, argv);

    if (cliOpts.count("help")) {
        fmt::print("{}", cliParser.help());
        return 0;
    }

    const auto replayFolder = cliOpts["replays"].as<std::string>();
    if (!std::filesystem::exists(replayFolder)) {
        SPDLOG_ERROR("Replay folder doesn't exist: {}", replayFolder);
        return -1;
    }
    SPDLOG_INFO("Found replay folder: {}", replayFolder);

    const auto gamePath = cliOpts["game"].as<std::string>();
    if (!std::filesystem::exists(gamePath)) {
        SPDLOG_ERROR("Game path doesn't exist: {}", gamePath);
        return -1;
    }
    SPDLOG_INFO("Found game path: {}", gamePath);

    const auto *tmp = std::getenv("POD_NAME");
    std::optional<std::string> podIndex;
    if (tmp == nullptr) {
        SPDLOG_INFO("POD_NAME not in ENV, not appending index suffix");
    } else {
        std::string_view s(tmp);
        // Extract the substring from the last delimiter to the end
        podIndex = s.substr(s.find_last_of('-') + 1);
        if (cliOpts["offset"].count()) {// Add offset to pod index if specified
            podIndex = std::to_string(std::stoi(podIndex.value()) + cliOpts["offset"].as<int>());
        }
        SPDLOG_INFO("POD_NAME found, using index suffix: {}", podIndex.value());
    }

    const auto dbPath = [&]() {
        auto ret = fs::path(cliOpts["output"].as<std::string>());
        if (podIndex.has_value()) { ret.replace_filename(ret.stem().string() + "_" + podIndex.value()); }
        ret.replace_extension(".SC2Replays");
        return ret;
    }();
    const auto dbPathParent = dbPath.parent_path();
    if (!fs::exists(dbPathParent)) {
        SPDLOG_INFO("Creating Output Directory: {}", dbPathParent.string());
        if (!std::filesystem::create_directories(dbPathParent)) {
            SPDLOG_ERROR("Unable to Create Output Directory");
            return -1;
        }
    }

    auto converter = [&](const std::string &cvtType) -> std::unique_ptr<cvt::BaseConverter> {
        if (cvtType == "full") { return std::make_unique<cvt::FullConverter>(); }
        if (cvtType == "action") { return std::make_unique<cvt::ActionConverter>(); }
        if (cvtType == "strided") {
            auto converter = std::make_unique<cvt::StridedConverter>();
            if (cliOpts["stride"].count() == 0) {
                SPDLOG_ERROR("Strided converter used but no --stride set");
                return nullptr;
            }
            converter->SetStride(cliOpts["stride"].as<std::size_t>());
            return converter;
        }
        SPDLOG_ERROR("Got invalid --converter='{}', require [full|action|strided]", cvtType);
        return nullptr;
    }(cliOpts["converter"].as<std::string>());

    if (converter.get() == nullptr) { return -1; }
    if (!converter->loadDB(dbPath)) {
        SPDLOG_ERROR("Unable to load/create replay db: {}", dbPath.string());
        return -1;
    }

    std::optional<fs::path> badFile;
    if (cliOpts["badfile"].count()) {
        badFile = fs::path(cliOpts["badfile"].as<std::string>());
        if (podIndex.has_value()) {// Append pod index
            badFile->replace_filename(
                badFile->stem().string() + "_" + podIndex.value() + badFile->extension().string());
        }
        if (!fs::exists(*badFile)) {
            SPDLOG_INFO("Creating new bad replay registry file: {}", badFile->string());
            if (!fs::exists(badFile->parent_path())) {
                const auto folder = badFile->parent_path();
                SPDLOG_INFO("Creating folder for bad replay file registry: {}", folder.string());
                if (!fs::create_directories(folder)) {
                    SPDLOG_ERROR("Error creating directory");
                    return -1;
                }
            }
            std::ofstream temp(*badFile);
        } else {
            registerKnownBadReplays(*badFile, converter.get());
        }
    }

    const auto replayFiles = [&]() -> std::vector<std::string> {
        if (cliOpts["partition"].count()) {
            auto partitionFile = cliOpts["partition"].as<std::string>();
            if (podIndex.has_value()) { partitionFile += "_" + podIndex.value(); }
            if (!std::filesystem::exists(partitionFile)) {
                SPDLOG_ERROR("Partition file doesn't exist: {}", partitionFile);
                return {};
            }
            SPDLOG_INFO("Using partition file: {}", partitionFile);
            return getReplaysFromFile(partitionFile);
        } else {
            return getReplaysFromFolder(replayFolder);
        }
    }();

    if (replayFiles.empty()) {
        SPDLOG_ERROR("No replay files loaded");
        return -1;
    }

    sc2::Coordinator coordinator;
    {
        constexpr int mapSize = 128;
        sc2::FeatureLayerSettings fSettings;
        fSettings.minimap_x = mapSize;
        fSettings.minimap_y = mapSize;
        coordinator.SetFeatureLayers(fSettings);
    }
    coordinator.AddReplayObserver(converter.get());
    coordinator.SetProcessPath(gamePath);
    coordinator.SetTimeoutMS(60'000);

    loopReplayFiles(replayFolder, replayFiles, coordinator, converter.get(), badFile);

    return 0;
}
