#include <cxxopts.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <ranges>

// Observer must be included before Windows.h since it #define's max/min which fucks with std::ranges::{min|max}
#include "data_structures/replay_all.hpp"
#include "observer.hpp"

#if defined(_WIN32)
#include <WinSock2.h>
#include <Windows.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

#include <StormLib.h>

namespace fs = std::filesystem;
using namespace std::string_literals;
using clk = std::chrono::high_resolution_clock;
using json = nlohmann::json;

/**
 * @brief Add known bad replays from file to converter's knownHashes
 * @param badFile Path to file containing known bad replays
 * @param converter Converter engine to add replayHash to
 */
template<typename T> void registerKnownBadReplays(const fs::path &badFile, cvt::BaseConverter<T> *converter)
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
auto getReplaysFromFolder(std::string_view folder) noexcept -> std::vector<std::string>
{
    SPDLOG_INFO("Searching replays in {}", folder);
    std::vector<std::string> replays;
    constexpr std::string_view targetExtension(".SC2Replay");
    std::ranges::transform(fs::directory_iterator{ folder } | std::views::filter([&targetExtension](const auto &entry) {
        return fs::is_regular_file(entry) && entry.path().extension() == targetExtension;
    }),
        std::back_inserter(replays),
        [](const auto &entry) { return entry.path().stem().string(); });
    return replays;
}

std::string getExecutablePath()
{
    std::string executablePath;

#if defined(_WIN32)
    char buffer[MAX_PATH];
    GetModuleFileName(NULL, buffer, MAX_PATH);
    executablePath = std::filesystem::path(buffer).parent_path().string();
#elif defined(__linux__)
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1) {
        buffer[len] = '\0';
        executablePath = std::filesystem::path(buffer).parent_path().string();
    }
#endif

    return executablePath;
}

auto extractLocaleFromMPQ(HANDLE mpqArchive, const SFILE_FIND_DATA &fileData) -> std::optional<std::string>
{
    HANDLE openedLocaleFile;
    if (!SFileOpenFileEx(mpqArchive, fileData.cFileName, SFILE_OPEN_FROM_MPQ, &openedLocaleFile)) {
        SPDLOG_ERROR("Couldn't open file within MPQ Archive");
        return std::nullopt;
    }

    std::string localeFileBuffer(fileData.dwFileSize, '\0');
    DWORD bytesRead;
    if (!SFileReadFile(openedLocaleFile, localeFileBuffer.data(), localeFileBuffer.size(), &bytesRead, nullptr)) {
        SPDLOG_ERROR("Failed to read file inside MPQ Archive");
        return std::nullopt;
    }

    SFileCloseFile(openedLocaleFile);
    return localeFileBuffer;
}


auto getDataVersion(
    const fs::path &replayPath) noexcept -> std::optional<std::tuple<std::string, std::string, std::string>>
{
    std::string replayPathStr = replayPath.string();// Need to make copy that is char as windows is wchar_t

    HANDLE MPQArchive;
    if (!SFileOpenArchive(replayPathStr.c_str(), 0, 0, &MPQArchive)) {
        SPDLOG_WARN("Failed to open file");
        return std::nullopt;
    }

    SFILE_FIND_DATA foundLocaleFileData;
    HANDLE searchHandle = SFileFindFirstFile(MPQArchive, "replay.gamemetadata.json", &foundLocaleFileData, nullptr);
    if (searchHandle == nullptr) {
        SPDLOG_WARN("Failed to find replay.gamemetadata.json in MPQ archive");
        SFileCloseArchive(MPQArchive);
        return std::nullopt;
    }

    auto serialData = extractLocaleFromMPQ(MPQArchive, foundLocaleFileData);
    if (!serialData.has_value()) {
        SPDLOG_WARN("Unable to extract from MPQ");
        SFileCloseFile(searchHandle);
        SFileCloseArchive(MPQArchive);
        return std::nullopt;
    }
    json data = json::parse(serialData.value());

    auto gameVersion = data["GameVersion"].template get<std::string>();
    auto lastSection = std::ranges::find_last(gameVersion, '.');
    gameVersion.resize(
        gameVersion.size() - std::distance(lastSection.begin(), lastSection.end()));// Truncate last section from string
    auto dataVersion = data["DataVersion"].template get<std::string>();
    auto buildVersion = data["BaseBuild"].template get<std::string>().substr(4);

    SFileCloseFile(searchHandle);
    SFileCloseArchive(MPQArchive);

    return std::make_tuple(gameVersion, dataVersion, buildVersion);
}

#ifdef _WIN32
// WARNING: Currently doesn't work
[[no_discard]] auto find_available_port(int start_port) -> std::optional<int>
{
    WSADATA wsaData;

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        SPDLOG_ERROR("WSAStartup failed.");
        return std::nullopt;
    }

    // Create a socket
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        SPDLOG_ERROR("Socket creation failed.");
        WSACleanup();
        return std::nullopt;
    }

    auto testPort = [&](int port) -> bool {
        // Set up sockaddr_in structure
        sockaddr_in server;
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = INADDR_ANY;
        server.sin_port = htons(port);
        return bind(sock, reinterpret_cast<sockaddr *>(&server), sizeof(server)) != SOCKET_ERROR;
    };

    // Linear probe from start port
    std::optional<int> port;
    for (std::size_t attempt = 0; attempt < 64; ++attempt) {
        if (testPort(start_port + attempt)) {
            port = start_port + attempt;
            SPDLOG_INFO("Found available game port: {}", *port);
            break;
        }
    }

    // Clean up
    closesocket(sock);
    WSACleanup();

    return port;
}
#else
[[nodiscard]] auto find_available_port(int start_port) -> std::optional<int>
{
    SPDLOG_WARN("Find available port not currently implemented for non-windows platform");
    return start_port;
}
#endif

/**
 * @brief Run over a vector of replay hashes that are contained in a replay folder and convert them.
 * @tparam T Underlying replay data type of conversion engine
 * @param replayFolder folder that contains the .SC2Replays listed in replayHashes
 * @param replayHashes vector of replay hashes to convert, this should only be the stem of the file path (no extension)
 * @param gamePath path to the "Versions" folder of the game directory
 * @param converter pointer to conversion engine instance, must be initialized outside and not be null
 * @param badFile optional file to log bad replays encountered
 * @param perfPath optional file to log time taken to convert a replay
 * @param port port to run the game api server at
 */
template<typename T>
void loopReplayFiles(const fs::path &replayFolder,
    const std::vector<std::string> &replayHashes,
    const std::string &gamePath,
    cvt::BaseConverter<T> *converter,
    std::optional<fs::path> badFile,
    std::optional<fs::path> perfPath,
    int port)
{
    auto make_coordinator = [&]() {
        auto coordinator = std::make_unique<sc2::Coordinator>();
        {
            constexpr int mapSize = 128;
            sc2::FeatureLayerSettings fSettings;
            fSettings.minimap_x = mapSize;
            fSettings.minimap_y = mapSize;
            coordinator->SetFeatureLayers(fSettings);
        }
        coordinator->AddReplayObserver(converter);
        coordinator->SetProcessPath(gamePath);
        coordinator->SetTimeoutMS(30'000);
        const auto newport = find_available_port(port);
        if (newport.has_value()) { coordinator->SetPortStart(*newport); }
        return coordinator;
    };
    auto coordinator = make_coordinator();

    std::string currVer = "";

    std::size_t nComplete = 0;
    for (auto &&replayHash : replayHashes) {
        const auto replayPath = (replayFolder / replayHash).replace_extension(".SC2Replay");
        if (!fs::exists(replayPath)) {
            SPDLOG_ERROR("Replay file doesn't exist {}", replayPath.string());
            continue;
        }
        SPDLOG_INFO("Starting replay: {}", replayPath.stem().string());

        for (uint32_t playerId = 1; playerId < 3; ++playerId) {
            const std::string replayHashPlayer = replayHash + std::to_string(playerId);
            if (converter->isKnownHash(replayHashPlayer)) {
                SPDLOG_INFO("Skipping known Replay {}, PlayerID: {}", replayHash, playerId);
                continue;
            }
            auto versionResult = getDataVersion(replayPath);
            if (versionResult.has_value()) {
                auto [gameVersion, dataVersion, build_version] = *versionResult;

                auto path = fs::path(gamePath) / ("Base"s + build_version) / "SC2_x64";
#ifdef _WIN32
                path.replace_extension("exe");// Add ".exe" only if compiling for Windows
#endif
                if (!fs::exists(path)) {
                    SPDLOG_WARN(
                        "You do not have the correct StarCraft II Version, you need version {} with Identifier {}",
                        gameVersion,
                        "Base"s + build_version);
                    break;
                }

                coordinator->SetDataVersion(dataVersion, true);
                coordinator->SetProcessPath(path.string(), true);
                // uh oh game version has changed
                if (!currVer.empty() && dataVersion != currVer) { coordinator->Relaunch(); }
                currVer = dataVersion;
            }

            auto runReplay = [&]() {
                // Setup Replay with Player
                converter->clear();
                converter->setReplayInfo(replayHash, playerId);
                coordinator->SetReplayPath(replayPath.string());
                coordinator->SetReplayPerspective(playerId);
                auto startTime = clk::now();
                // Run Replay
                while (coordinator->Update()) {}
                auto durationSec = std::chrono::duration_cast<std::chrono::duration<float>>(clk::now() - startTime);
                if (perfPath.has_value()) {
                    std::ofstream perfFile(perfPath.value(), std::ios::app);
                    perfFile << fmt::format("{},p{},{}\n", replayPath.string(), playerId, durationSec.count());
                }
            };

            constexpr std::size_t maxRetry = 3;
            for (std::size_t retryCount = 0; !converter->hasWritten() && retryCount < maxRetry; ++retryCount) {
                runReplay();
                if (!converter->hasWritten()) {
                    SPDLOG_ERROR(
                        "Failed Converting Replay, Relaunching Coordinator, Attempt {} of {}", retryCount, maxRetry);
                    coordinator->Relaunch();
                }
            }

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
            converter->clear();
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
      ("r,replays", "Path to folder of replays or replay file.", cxxopts::value<std::string>())
      ("p,partition", "Partition file to select a subset of replays a folder.", cxxopts::value<std::string>())
      ("o,output", "Output filepath for replay database.", cxxopts::value<std::string>())
      ("c,converter", "Type of converter to use [action|full|strided].", cxxopts::value<std::string>())
      ("s,stride", "Stride for the strided converter (in game steps).", cxxopts::value<std::size_t>())
      ("save-actions", "Strided converter will also save steps with player actions.", cxxopts::value<bool>())
      ("g,game", "Path to 'Versions' folder of the SC2 game.", cxxopts::value<std::string>())
      ("b,badfile", "File to record a known set of bad replays (to skip).", cxxopts::value<std::string>())
      ("offset", "Offset to apply to partition index", cxxopts::value<int>())
      ("port", "Port for communication with SC2.", cxxopts::value<int>()->default_value("9168"))
      ("perflog", "Path to log time taken for replay observation.", cxxopts::value<std::string>())
      ("h,help", "Show this help.");
    // clang-format on
    const auto cliOpts = cliParser.parse(argc, argv);

    if (cliOpts.count("help")) {
        fmt::print("{}", cliParser.help());
        return 0;
    }

    const auto replayPath = cliOpts["replays"].as<std::string>();
    if (!std::filesystem::exists(replayPath)) {
        SPDLOG_ERROR("Replay path doesn't exist: {}", replayPath);
        return -1;
    }
    SPDLOG_INFO("Found replay path: {}", replayPath);

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

    using ReplayDataType = cvt::ReplayDataSoA;
    auto converter = [&](const std::string &cvtType) -> std::unique_ptr<cvt::BaseConverter<ReplayDataType>> {
        if (cvtType == "full") { return std::make_unique<cvt::FullConverter<ReplayDataType>>(); }
        if (cvtType == "action") { return std::make_unique<cvt::ActionConverter<ReplayDataType>>(); }
        if (cvtType == "strided") {
            auto converter_ = std::make_unique<cvt::StridedConverter<ReplayDataType>>();
            if (cliOpts["stride"].count() == 0) {
                SPDLOG_ERROR("Strided converter used but no --stride set");
                return nullptr;
            }
            converter_->SetStride(cliOpts["stride"].as<std::size_t>());
            converter_->SetActionSaving(cliOpts["save-actions"].count());
            if (converter_->ActionsAreSaved()) { SPDLOG_INFO("Strided Converter is Saving Actions"); }
            return converter_;
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
        } else if (fs::is_directory(replayPath)) {
            return getReplaysFromFolder(replayPath);
        } else {// Is a single replay file
            return std::vector{ fs::path(replayPath).stem().string() };
        }
    }();

    if (replayFiles.empty()) {
        SPDLOG_ERROR("No replay files loaded");
        return -1;
    }

    std::optional<fs::path> perfPath;
    if (cliOpts.count("perflog")) { perfPath = cliOpts["perflog"].as<std::string>(); }

    // Check if the replayPath argument is a 'single' file, hence the replayFolder folder is the parent
    const auto replayFolder = fs::is_directory(replayPath) ? fs::path(replayPath) : fs::path(replayPath).parent_path();
    loopReplayFiles(replayFolder, replayFiles, gamePath, converter.get(), badFile, perfPath, cliOpts["port"].as<int>());

    return 0;
}
