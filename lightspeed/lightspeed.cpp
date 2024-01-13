/**
 * What is the fastest we can run the SC2 Game at???
 */

#include <array>
#include <chrono>
#include <iostream>
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

#include "s2clientprotocol/sc2api.pb.h"

using namespace std::chrono_literals;

namespace beast = boost::beast;
namespace http = beast::http;
namespace ws = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

std::string default_host = "127.0.0.1";
std::string default_port = "5679";

void run_test()
{
    // net::io_context ioc;
    // tcp::resolver resolver{ ioc };
    // ws::stream<tcp::socket> ws_instance;

    // auto const results = resolver.resolve(default_host, default_port);

    // auto endpoint = net::connect(ws_instance.next_layer(), results);

    // const std::string host = default_host + ":" + default_port;
    // ws_instance.set_option(ws::stream_base::decorator([](ws::request_type &req) {
    //     req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + "test client");
    // }));

    // ws_instance.handshake(host, "/sc2api");
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
            std::cerr << "Failed to execute process " << char_args[0] << " error: " << strerror(errno) << std::endl;
            exit(-1);
        }
        std::cout << "Forked thread started process!\n";
        std::this_thread::sleep_for(1s);
    }

    std::this_thread::sleep_for(5s);// Wait a bit to connect

    run_test();

    std::cout << "Killing process\n";

    if (kill(process_id, SIGTERM) == -1) { std::cerr << "Coudn't end process\n"; }
    int status = 0;
    waitpid(process_id, &status, 0);
    if (!WIFEXITED(status)) { std::cerr << "Process terminated with signal: " << status << "\n"; }

    return 0;
}
