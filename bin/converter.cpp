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

    cvt::Converter converter;
    converter.loadDB(result["output"].as<std::string>());

    return 0;
}