#include <cxxopts.hpp>
#include <spdlog/fmt/fmt.h>

#include "database.hpp"

#include <filesystem>

namespace fs = std::filesystem;

int main(int argc, char *argv[])
{
    cxxopts::Options cliParser("SC2 Replay Database Format Conversion",
        "Converts one serialized format to another, significantly faster than resimulating replays if all information "
        "for the target format is available in the original source format.");
    // clang-format off
    cliParser.add_options()
        ("i,input", "Source database to convert from", cxxopts::value<std::string>())
        ("o,output", "Destination database", cxxopts::value<std::string>())
        ("h,help", "This help");
    // clang-format on 
    const auto cliOpts = cliParser.parse(argc, argv);

    if (cliOpts.count("help")){
        fmt::print("{}\n", cliParser.help());
        return 0;
    }

    const fs::path sourcePath = cliOpts["input"].as<std::string>();
    if (!fs::exists(sourcePath)) {
        fmt::print("ERROR: Source Database doesn't exist: {}\n", sourcePath.string());
        return -1;
    }
    cvt::ReplayDatabase<cvt::ReplayDataSoA> source(sourcePath);

    const fs::path destPath = cliOpts["output"].as<std::string>();
    if (!fs::exists(destPath.parent_path())) {
        fmt::print("ERROR: Path to destination doesn't exist: {}\n", destPath.parent_path().string());
        return -1;
    }
    cvt::ReplayDatabase<cvt::ReplayData2SoA> dest(destPath);

    return 0;
}
