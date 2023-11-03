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
    std::vector<std::string> replays;
    std::ifstream partStream(partitionFile);
    std::istream_iterator<std::string> fileIt(partStream);
    std::copy(fileIt, {}, std::back_inserter(replays));
}

auto getReplaysFolder(const std::string_view folder) noexcept -> std::vector<std::string>
{
    std::vector<std::string> replays;
    std::ranges::transform(std::filesystem::directory_iterator{ folder }, std::back_inserter(replays), [](auto &&e) {
        return e.path().stem().string();
    });
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
        SPDLOG_ERROR("Folder doesn't exist: {}", replayFolder);
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
        replays = getReplaysFile(result["partition"].as<std::string>());
    } else {
        replays = getReplaysFolder(replayFolder);
    }

    return 0;
}