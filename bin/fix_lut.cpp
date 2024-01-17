#include <cxxopts.hpp>
#include <spdlog/fmt/fmt.h>

#include "serialize.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

void inplace_convert_lookuptable(const fs::path &path)
{
    fmt::print("Running conversion on {}....\n", path.string());

    std::vector<std::int64_t> int64Table;
    {
        std::ifstream dbStream(path, std::ios::binary);
        const auto startOffset = dbStream.tellg();
        std::vector<std::streampos> streamposTable;
        cvt::deserialize(streamposTable, dbStream);
        int64Table.resize(streamposTable.size());
        std::ranges::transform(streamposTable, int64Table.begin(), [=](std::streampos s) {
            return static_cast<int64_t>(s - startOffset);
        });
    }

    {
        std::ofstream dbStream(path, std::ios::binary | std::ios::ate | std::ios::in);
        dbStream.seekp(0, std::ios::beg);
        cvt::serialize(int64Table, dbStream);
    }

    fmt::print("Finished conversion\n");
}

int main(int argc, char *argv[])
{
    cxxopts::Options cliParser("SC2 Fix LUT Header",
        "Unfortunately it was discovered that Windows and Linux have different std::streampos representations, "
        "therefore using a plain std::int64_t offset is required. This program converts the header and doesn't touch "
        "the data as the header is still a simple std::vector<std::streampos>. Obviously this needs to be done on the "
        "original OS the file was created.");
    // clang-format off
    cliParser.add_options()
        ("i,input", "Database to convert std::streampos to std::int32_t", cxxopts::value<std::string>())
        ("o,output", "Output path if operation is not inplace, if output is a folder then the desination will have same filename as input", cxxopts::value<std::string>())
        ("inplace", "Modify the original database rather than making a copy", cxxopts::value<bool>()->default_value("true"))
        ("h,help", "This Help", cxxopts::value<bool>());
    // clang-format on
    const auto cliOpts = cliParser.parse(argc, argv);

    if (cliOpts.count("help")) {
        fmt::print("{}\n", cliParser.help());
        return 0;
    }

    fs::path sourceDb = cliOpts["input"].as<std::string>();
    if (!fs::exists(sourceDb)) {
        fmt::print("Database doesn't exist: {}\n", sourceDb.string());
        return -1;
    }

    if (cliOpts.count("inplace") == 0) {
        if (cliOpts.count("output") == 0) {
            fmt::print("If not --inplace then an output file or folder must be specified");
            return -1;
        }
        fs::path dest = cliOpts["output"].as<std::string>();
        if (fs::is_directory(dest)) { dest /= sourceDb.filename(); }
        fmt::print("Copying {} to {}\n", sourceDb.string(), dest.string());
        fs::copy_file(sourceDb, dest);
        sourceDb = dest;
    }

    inplace_convert_lookuptable(sourceDb);

    return 0;
}
