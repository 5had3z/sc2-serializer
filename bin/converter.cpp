#include <cxxopts.hpp>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>
#include <sstream>

#include "observer.hpp"

auto getReplaysFile(const std::string &partitionFile) noexcept -> std::vector<std::string>
{
    SPDLOG_INFO("Loading replays from {}", partitionFile);
    std::vector<std::string> replays;
    std::ifstream partStream(partitionFile);
    std::istream_iterator<std::string> fileIt(partStream);
    std::copy(fileIt, {}, std::back_inserter(replays));
}

auto getReplaysFolder(const std::string_view folder) noexcept -> std::vector<std::string>
{
    SPDLOG_INFO("Searching replays in {}", folder);
    std::vector<std::string> replays;
    std::ranges::transform(std::filesystem::directory_iterator{ folder }, std::back_inserter(replays), [](auto &&e) {
        return e.path().stem().string();
    });
}

void loopReplayFiles(sc2::Coordinator &coordinator,
  const std::string_view replayFolder,
  const std::vector<std::string> &replayFiles,
  cvt::Converter &converter)
{
    std::size_t nComplete = 0;
    for (auto &&replayFile : replayFiles) {
        auto fullPath = std::filesystem::path(replayFolder) / replayFile;
        if (!std::filesystem::exists(fullPath)) {
            SPDLOG_ERROR("Replay file doesn't exist {}", fullPath.string());
            continue;
        }
        converter.setReplayInfo(replayFile, 0);
        coordinator.SetReplayPath(fullPath.string());
        coordinator.SetReplayPerspective(0);

        while (coordinator.Update()) {}
        SPDLOG_INFO("Completed {} of {} replays", nComplete++, replayFiles.size());
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

    auto replayFolder = result["replays"].as<std::string>();
    if (!std::filesystem::exists(replayFolder)) {
        SPDLOG_ERROR("Replay folder doesn't exist: {}", replayFolder);
        return -1;
    }

    auto gamePath = result["game"].as<std::string>();
    if (!std::filesystem::exists(gamePath)) {
        SPDLOG_ERROR("Game path doesn't exist: {}", gamePath);
        return -1;
    }

    auto dbPath = result["output"].as<std::string>();
    cvt::Converter converter;
    if (!converter.loadDB(dbPath)) {
        SPDLOG_ERROR("Unable to load replay db: {}", dbPath);
        return -1;
    }

    std::vector<std::string> replays;
    if (result["partition"].count()) {
        const auto partitionFile = result["partition"].as<std::string>();
        if (!std::filesystem::exists(partitionFile)) {
            SPDLOG_ERROR("Partition file doesn't exist: {}", partitionFile);
            return -1;
        }
        replays = getReplaysFile(partitionFile);
    } else {
        replays = getReplaysFolder(replayFolder);
    }

    if (replays.empty()) {
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

    return 0;
}