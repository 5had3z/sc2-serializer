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
#include <fmt/chrono.h>
#include <fmt/format.h>

#include "s2clientprotocol/sc2api.pb.h"

using namespace std::chrono_literals;

namespace beast = boost::beast;
namespace http = beast::http;
namespace ws = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

std::string default_host = "127.0.0.1";
std::string default_port = "5679";

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


void run_test()
{
    net::io_context ioc;
    tcp::resolver resolver{ ioc };
    ws::stream<tcp::socket> ws_instance{ ioc };

    auto const results = resolver.resolve(default_host, default_port);

    auto endpoint = net::connect(ws_instance.next_layer(), results);

    const std::string host = default_host + ":" + default_port;
    ws_instance.set_option(ws::stream_base::decorator([](ws::request_type &req) {
        req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + "test client");
    }));

    ws_instance.handshake(host, "/sc2api");

    auto send_request = [&](SC2APIProtocol::Request req) { ws_instance.write(net::buffer(req.SerializeAsString())); };

    auto get_response = [&]() -> std::optional<SC2APIProtocol::Response> {
        beast::flat_buffer readBuff;
        ws_instance.read(readBuff);
        if (readBuff.size() > 0) {
            SC2APIProtocol::Response response;
            response.ParseFromArray(readBuff.data().data(), readBuff.size());
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

    const std::string replay_path =
        "/home/bryce/SC2/replays/4.9.2/00004e179daa5a5bafeef22c01bc84408f70052a7e056df5c63800aed85099e9.SC2Replay";
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
        startRequestReplay->set_observed_player_id(1);
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
    // Can't be const because of execve
    std::vector<std::string> cli_args = {
        "/home/bryce/SC2/game/4.9.2/Versions/Base74741/SC2_x64",
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
        // "-dataVersion",
        // "",
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

    const auto start = std::chrono::high_resolution_clock::now();
    run_test();
    const auto duration = std::chrono::high_resolution_clock::now() - start;

    fmt::println("Finished Replay, took {}", std::chrono::duration_cast<std::chrono::duration<float>>(duration));

    if (kill(process_id, SIGTERM) == -1) { fmt::println("Couldn't end process"); }
    int status = 0;
    waitpid(process_id, &status, 0);
    if (!WIFEXITED(status)) { fmt::println("Process terminated with signal: {}", status); }

    return 0;
}
