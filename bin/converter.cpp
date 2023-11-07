#include <cxxopts.hpp>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>

#include "observer.hpp"

/**
 * @brief Get Replay hashes from a file
 * @param partitionFile File that contains newline separated hashes
 * @return Vector of Hash Strings
 */
auto getReplaysFile(const std::string &partitionFile) noexcept -> std::vector<std::string>
{
    SPDLOG_INFO("Loading replays from {}", partitionFile);
    std::vector<std::string> replays;
    std::ifstream partStream(partitionFile);
    std::istream_iterator<std::string> fileIt(partStream);
    std::copy(fileIt, {}, std::back_inserter(replays));
    return replays;
}


/**
 * @brief Get replay hashes from folder of replays
 * @param folder Folder that contains replays
 * @return Vector of Hash Strings
 */
auto getReplaysFolder(const std::string_view folder) noexcept -> std::vector<std::string>
{
    SPDLOG_INFO("Searching replays in {}", folder);
    std::vector<std::string> replays;
    std::ranges::transform(std::filesystem::directory_iterator{ folder }, std::back_inserter(replays), [](auto &&e) {
        return e.path().stem().string();
    });
    return replays;
}

void loopReplayFiles(const std::filesystem::path &replayFolder,
  const std::vector<std::string> &replayHashes,
  sc2::Coordinator &coordinator,
  cvt::BaseConverter *converter)
{
    std::size_t nComplete = 0;
    for (auto &&replayHash : replayHashes) {
        const auto replayPath = (replayFolder / replayHash).replace_extension(".SC2Replay");
        if (!std::filesystem::exists(replayPath)) {
            SPDLOG_ERROR("Replay file doesn't exist {}", replayPath.string());
            continue;
        }

        for (uint32_t playerId = 1; playerId < 3; ++playerId) {
            // Setup Replay with Player
            converter->setReplayInfo(replayHash, playerId);
            coordinator.SetReplayPath(replayPath.string());
            coordinator.SetReplayPerspective(playerId);
            // Run Replay
            while (coordinator.Update()) {}
        }

        SPDLOG_INFO("Completed {} of {} replays", ++nComplete, replayHashes.size());
    }
}


auto main(int argc, char *argv[]) -> int
{
    cxxopts::Options cliParser("SC2 Replay", "Run a folder of replays and see if it works");
    // clang-format off
    cliParser.add_options()
      ("r,replays", "path to folder of replays", cxxopts::value<std::string>())
      ("p,partition", "partition file to select replays", cxxopts::value<std::string>())
      ("o,output", "output directory for replays", cxxopts::value<std::string>())
      ("c,converter", "type of converter to use 'action' or 'full'", cxxopts::value<std::string>())
      ("g,game", "path to game execuatable", cxxopts::value<std::string>())
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

    const auto gamePath = cliOpts["game"].as<std::string>();
    if (!std::filesystem::exists(gamePath)) {
        SPDLOG_ERROR("Game path doesn't exist: {}", gamePath);
        return -1;
    }

    const auto dbPath = cliOpts["output"].as<std::string>();
    const auto dbPathParent = std::filesystem::path(dbPath).parent_path();
    if (!std::filesystem::exists(dbPathParent)) {
        SPDLOG_INFO("Creating Output Directory: {}", dbPathParent.string());
        if (!std::filesystem::create_directories(dbPathParent)) {
            SPDLOG_ERROR("Unable to Create Output Directory");
            return -1;
        }
    }

    auto converter = [](const std::string &cvtType) -> std::unique_ptr<cvt::BaseConverter> {
        if (cvtType == "full") { return std::make_unique<cvt::FullConverter>(); }
        if (cvtType == "action") { return std::make_unique<cvt::ActionConverter>(); }
        SPDLOG_ERROR("Got invalid --converter='{}', require 'action' or 'full'", cvtType);
        return nullptr;
    }(cliOpts["converter"].as<std::string>());

    if (converter.get() == nullptr) { return -1; }
    if (!converter->loadDB(dbPath)) {
        SPDLOG_ERROR("Unable to load/create replay db: {}", dbPath);
        return -1;
    }

    const auto replayFiles = [&]() -> std::vector<std::string> {
        if (cliOpts["partition"].count()) {
            const auto partitionFile = cliOpts["partition"].as<std::string>();
            if (!std::filesystem::exists(partitionFile)) {
                SPDLOG_ERROR("Partition file doesn't exist: {}", partitionFile);
                return {};
            }
            return getReplaysFile(partitionFile);
        } else {
            return getReplaysFolder(replayFolder);
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

    loopReplayFiles(replayFolder, replayFiles, coordinator, converter.get());

    return 0;
}