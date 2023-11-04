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

void loopReplayFiles(sc2::Coordinator &coordinator,
  const std::string_view replayFolder,
  const std::vector<std::string> &replayHashes,
  cvt::Converter &converter)
{
    std::size_t nComplete = 0;
    for (auto &&replayHash : replayHashes) {
        const auto replayPath = (std::filesystem::path(replayFolder) / replayHash).replace_extension(".SC2Replay");
        if (!std::filesystem::exists(replayPath)) {
            SPDLOG_ERROR("Replay file doesn't exist {}", replayPath.string());
            continue;
        }
        converter.setReplayInfo(replayHash, 0);
        coordinator.SetReplayPath(replayPath.string());
        coordinator.SetReplayPerspective(0);

        while (coordinator.Update()) {}
        SPDLOG_INFO("Completed {} of {} replays", nComplete++, replayHashes.size());
    }
}


auto main(int argc, char *argv[]) -> int
{
    cxxopts::Options cliopts("SC2 Replay", "Run a folder of replays and see if it works");
    // clang-format off
    cliopts.add_options()
      ("r,replays", "path to folder of replays", cxxopts::value<std::string>())
      ("p,partition", "partition file to select replays", cxxopts::value<std::string>())
      ("o,output", "output directory for replays", cxxopts::value<std::string>())
      ("g,game", "path to game execuatable", cxxopts::value<std::string>());
    // clang-format on
    auto result = cliopts.parse(argc, argv);

    const auto replayFolder = result["replays"].as<std::string>();
    if (!std::filesystem::exists(replayFolder)) {
        SPDLOG_ERROR("Replay folder doesn't exist: {}", replayFolder);
        return -1;
    }

    const auto gamePath = result["game"].as<std::string>();
    if (!std::filesystem::exists(gamePath)) {
        SPDLOG_ERROR("Game path doesn't exist: {}", gamePath);
        return -1;
    }

    const auto dbPath = result["output"].as<std::string>();
    const auto dbPathParent = std::filesystem::path(dbPath).parent_path();
    if (!std::filesystem::exists(dbPathParent)) {
        SPDLOG_INFO("Creating Output Directory: {}", dbPathParent.string());
        if (!std::filesystem::create_directories(dbPathParent)) {
            SPDLOG_ERROR("Unable to Create Output Directory");
            return -1;
        }
    }

    cvt::Converter converter;
    if (!converter.loadDB(dbPath)) {
        SPDLOG_ERROR("Unable to load/create replay db: {}", dbPath);
        return -1;
    }

    const auto replayFiles = [&]() -> std::vector<std::string> {
        if (result["partition"].count()) {
            const auto partitionFile = result["partition"].as<std::string>();
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
    coordinator.AddReplayObserver(&converter);
    coordinator.SetProcessPath(gamePath);

    loopReplayFiles(coordinator, replayFolder, replayFiles, converter);

    return 0;
}