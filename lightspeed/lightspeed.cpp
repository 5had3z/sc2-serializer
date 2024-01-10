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

#include "s2clientprotocol/sc2api.pb.h"

using namespace std::chrono_literals;

void run_test() {}

int main(int argc, char *argv[])
{
    // Can't be const because of execve
    std::vector<char *> char_list = {
        "/home/bryce/SC2/game/4.9.2/Versions/Base74741/SC2_x64",
        "-listen",
        "127.0.0.1",
        "-port",
        "5679",
        "-displayMode",
        "0",
        "-windowwidth",
        "256",
        "-windowheight",
        "256",
        // "-dataVersion",
        // "",
        nullptr// End for execve
    };

    // Start the process.
    pid_t process_id = fork();
    if (process_id == 0) {
        if (execve(char_list[0], &char_list[0], nullptr) == -1) {
            std::cerr << "Failed to execute process " << char_list[0] << " error: " << strerror(errno) << std::endl;
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
