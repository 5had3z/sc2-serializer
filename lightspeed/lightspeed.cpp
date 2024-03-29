/**
 * What is the fastest we can run the SC2 Game at???
 */

#include <array>
#include <chrono>
#include <execution>
#include <format>
#include <iostream>
#include <optional>
#include <ranges>
#include <thread>
#include <vector>

#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <cxxopts.hpp>
#include <fmt/chrono.h>
#include <fmt/format.h>

#include "s2clientprotocol/sc2api.pb.h"

using namespace std::chrono_literals;

namespace beast = boost::beast;
namespace http = beast::http;
namespace ws = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

const std::string default_host = "127.0.0.1";

template<typename T, std::size_t N> class CircularBuffer
{
  public:
    void append(T value)
    {
        buffer[endIdx++] = value;
        if (endIdx == N) {
            isFull = true;// Latch on
            endIdx = 0;
        }
    }

    template<std::invocable<T, T> F> [[nodiscard]] auto reduce(T init, F binaryOp) const noexcept -> T
    {
        if (isFull) { return std::reduce(std::execution::unseq, buffer.begin(), buffer.end(), init, binaryOp); }
        return std::reduce(std::execution::unseq, buffer.begin(), std::next(buffer.begin(), endIdx), init, binaryOp);
    }

    [[nodiscard]] auto size() const noexcept -> std::size_t
    {
        if (isFull) { return N; }
        return endIdx;
    }

    [[nodiscard]] auto full() const noexcept -> bool { return isFull; }

  private:
    bool isFull{ false };
    std::size_t endIdx{ 0 };
    std::array<T, N> buffer;
};

class FrequencyTimer
{
  public:
    std::chrono::seconds displayPeriod;

    // cppcheck-suppress noExplicitConstructor
    FrequencyTimer(std::string name, std::chrono::seconds displayPeriod_ = std::chrono::minutes(1))
        : timerName(std::move(name)), displayPeriod(displayPeriod_)
    {}

    void step(std::optional<std::string_view> printExtra) noexcept
    {
        const auto currentTime = std::chrono::steady_clock::now();
        // If very first step just set current time and return
        if (lastStep == std::chrono::steady_clock::time_point{}) {
            lastStep = currentTime;
            return;
        }

        period.append(currentTime - lastStep);
        lastStep = currentTime;

        if (currentTime - lastPrint > displayPeriod && period.full()) {
            const auto meanStep = period.reduce(std::chrono::seconds(0), std::plus<>()) / period.size();
            const auto frequency = std::chrono::seconds(1) / meanStep;
            fmt::println("{} Frequency: {:.1f}Hz - {}", timerName, frequency, printExtra.value_or("No Extra Info"));
            lastPrint = currentTime;
        }
    }

  private:
    CircularBuffer<std::chrono::duration<float>, 100> period{};
    std::string timerName;
    std::chrono::steady_clock::time_point lastStep{};
    std::chrono::steady_clock::time_point lastPrint{};
};


void run_test(const std::string &replay_path, const std::string &port, std::uint32_t playerId)
{
    net::io_context ioc;
    tcp::resolver resolver{ ioc };
    ws::stream<tcp::socket> ws_instance{ ioc };

    auto const results = resolver.resolve(default_host, port);

    auto endpoint = net::connect(ws_instance.next_layer(), results);

    const std::string host = default_host + ":" + port;
    ws_instance.set_option(ws::stream_base::decorator([](ws::request_type &req) {
        req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + "test client");
    }));

    ws_instance.handshake(host, "/sc2api");

    std::string requestBuffer;
    auto send_request = [&](SC2APIProtocol::Request req) {
        req.SerializeToString(&requestBuffer);
        ws_instance.write(net::buffer(requestBuffer));
    };

    beast::flat_buffer readBuff;
    auto get_response = [&]() -> std::optional<SC2APIProtocol::Response> {
        ws_instance.read(readBuff);
        if (readBuff.size() > 0) {
            SC2APIProtocol::Response response;
            response.ParseFromArray(readBuff.data().data(), readBuff.size());
            readBuff.clear();
            return response;
        }
        return {};
    };

    auto get_ping = [&]() {
        SC2APIProtocol::Request request;
        request.mutable_ping();
        send_request(request);
        return get_response();
    };

    {
        auto response = get_ping();
        if (response.has_value()) {
            if (response->has_status()) {
                fmt::println("Got status: {}", static_cast<int>(response->status()));
            } else {
                fmt::println("Got no status?");
            }
            if (response->has_ping()) {
                fmt::println("Got ping info: Game: {}, Data: {}",
                    response->ping().game_version(),
                    response->ping().data_version());
            }
        } else {
            fmt::println("No response from ping");
            return;
        }
    }

    SC2APIProtocol::ResponseReplayInfo replay_info;
    {
        SC2APIProtocol::Request req;
        auto replayInfoReq = req.mutable_replay_info();
        replayInfoReq->set_replay_path(replay_path);
        replayInfoReq->set_download_data(true);
        send_request(req);
        auto resp = get_response();
        if (resp.has_value()) {
            replay_info = resp->replay_info();
        } else {
            fmt::println("Unable to get replay info");
            return;
        }
    }

    {
        SC2APIProtocol::Request request;
        auto startRequestReplay = request.mutable_start_replay();
        startRequestReplay->set_replay_path(replay_path);
        startRequestReplay->set_observed_player_id(playerId);
        startRequestReplay->set_realtime(false);

        auto replayOpts = startRequestReplay->mutable_options();
        replayOpts->set_raw(true);
        replayOpts->set_score(true);
        auto featureLayerOpts = replayOpts->mutable_feature_layer();
        featureLayerOpts->set_width(24.f);
        auto worldViewOpts = featureLayerOpts->mutable_resolution();
        constexpr int32_t viewSize = 64;
        worldViewOpts->set_x(viewSize);
        worldViewOpts->set_y(viewSize);
        auto minimapOpts = featureLayerOpts->mutable_minimap_resolution();
        constexpr int32_t minimapSize = 128;
        minimapOpts->set_x(minimapSize);
        minimapOpts->set_y(minimapSize);

        fmt::println("Sent replay request");
        send_request(request);

        auto response = get_response();
        if (response.has_value()) {
            if (response->has_start_replay()) {
                if (response->start_replay().has_error()) {
                    fmt::println("Can't start replay: {}", response->start_replay().error_details());
                } else {
                    fmt::println("Started replay!");
                }
            }
        } else {
            fmt::println("No response from replay start");
            return;
        }
    }


    {
        auto status = get_ping()->status();
        FrequencyTimer timer("Stepping", std::chrono::seconds(10));
        while (status == SC2APIProtocol::Status::in_replay) {
            // fmt::println("Sending step request");
            SC2APIProtocol::Request request;
            auto step = request.mutable_step();
            step->set_count(1);
            send_request(request);

            // fmt::println("Waiting for step response");
            auto response = get_response();
            if (!response.has_value() || !response->has_step() || response->error_size() > 0) {
                fmt::println("Failed to step");
                return;
            }

            request.Clear();// Reuse request
            auto observation_req = request.mutable_observation();
            // fmt::println("Sending observation request");
            send_request(request);
            response = get_response();
            std::string printInfo;
            if (response.has_value() && response->has_observation()) {
                auto obs = response->observation().observation();
                printInfo += fmt::format("Step {} of {}", obs.game_loop(), replay_info.game_duration_loops());
                if (obs.has_raw_data()) {
                    printInfo += fmt::format(" - I can see {} units", obs.raw_data().units_size());
                } else {
                    printInfo += " - Missing raw_data";
                }
                if (obs.has_feature_layer_data() && obs.feature_layer_data().has_minimap_renders()) {
                    auto renders = obs.feature_layer_data().minimap_renders();
                    printInfo += " - Minimap data: ";
                    if (renders.has_height_map()) { printInfo += "height, "; }
                    if (renders.has_visibility_map()) { printInfo += "visibility, "; }
                    if (renders.has_pathable()) { printInfo += "pathable, "; }
                } else {
                    printInfo += " - Missing minimap renders";
                }
            } else {
                fmt::println("Failed to get observation");
                return;
            }
            timer.step(printInfo);

            status = response->status();
        }
    }
}


int main(int argc, char *argv[])
{
    cxxopts::Options cliParser("Protobuf Test", "Barebones test to see how fast sc2 can run");
    // clang-format off
    cliParser.add_options()
        ("r,replay", "Path to replay file", cxxopts::value<std::string>())
        ("g,game", "Path to game executable", cxxopts::value<std::string>())
        ("p,port", "Port to listen on the game", cxxopts::value<uint32_t>()->default_value("5679"))
        ("perflog", "Log file for replay time taken", cxxopts::value<std::string>());
    // clang-format on
    const auto args = cliParser.parse(argc, argv);
    const auto default_port = std::to_string(args["port"].as<uint32_t>());
    const auto game_path = args["game"].as<std::string>();
    const auto replay_path = args["replay"].as<std::string>();

    // Can't be const because of execve
    std::vector<std::string> cli_args = {
        game_path,
        "-listen",
        default_host,
        "-port",
        default_port,
        "-displayMode",
        "0",
        "-windowwidth",
        "256",
        "-windowheight",
        "256",
    };

    std::vector<char *> char_args;
    std::ranges::for_each(
        cli_args, [&](std::string &c) { return char_args.emplace_back(const_cast<char *>(c.c_str())); });
    char_args.emplace_back(nullptr);

    // Start the process.
    pid_t process_id = fork();
    if (process_id == 0) {
        if (execve(char_args[0], &char_args[0], nullptr) == -1) {
            fmt::println("Failed to execute process {}, error: {}", char_args[0], strerror(errno));
            exit(-1);
        }
        fmt::println("Forked thread started process!");
        std::this_thread::sleep_for(1s);
    }

    std::this_thread::sleep_for(5s);// Wait a bit to connect

    for (std::uint32_t playerId = 1; playerId < 3; ++playerId) {
        const auto start = std::chrono::high_resolution_clock::now();
        run_test(replay_path, default_port, playerId);
        const auto duration =
            std::chrono::duration_cast<std::chrono::duration<float>>(std::chrono::high_resolution_clock::now() - start);
        fmt::println("Finished Replay, P{} took {}", playerId, duration);
        if (args.count("perflog")) {
            std::ofstream logFile(args["perflog"].as<std::string>(), std::ios::app);
            logFile << fmt::format("{},p{},{}\n", replay_path, playerId, duration.count());
        }
    }

    if (kill(process_id, SIGTERM) == -1) { fmt::println("Couldn't end process"); }
    int status = 0;
    waitpid(process_id, &status, 0);
    if (!WIFEXITED(status)) { fmt::println("Process terminated with signal: {}", status); }

    return 0;
}
