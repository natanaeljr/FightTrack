/**
 * \file game_server.cc
 * \brief
 */

#include "fighttrack/game_server.h"

#include <iostream>
#include <chrono>
#include <unistd.h>

#include <gsl/gsl>

#include "fighttrack/ascii_art.h"

/**************************************************************************************/

namespace fighttrack {

static const AsciiArt kMapArt{ {
    "                                                                            ",
    "                                                                            ",
    "                                                                            ",
    "                                                                            ",
    "                                                                            ",
    "                                                                            ",
    "                                                                            ",
    "                                                                            ",
    "                                                                            ",
    "                                                                            ",
    "                                                                            ",
    "                                                                            ",
    "                                                                            ",
    "                                                                            ",
    "                                                                            ",
    "▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓                 ▓▓▓▓▓▓▓▓▓▓                                 ",
    "                                                                            ",
    "                    ▓▓▓▓▓▓▓                       ▓▓▓▓▓▓▓▓▓▓                ",
    "                                                                            ",
    "         ▓▓▓▓▓▓▓                                                    ▓▓▓▓▓▓▓▓",
} };

/**************************************************************************************/

GameServer::GameServer() : running_{ false }, players_{}, map_{ kMapArt }, server_sock_{}
{
}

/**************************************************************************************/
GameServer::~GameServer()
{
}

/**************************************************************************************/
int GameServer::Run(uint16_t port)
{
    if (server_sock_.Initialize(port) != 0) {
        fprintf(stderr, "Failed to initialize server socket!\n");
        return -1;
    }

    running_ = true;
    return Loop();
}

/**************************************************************************************/
int GameServer::Loop()
{
    using namespace std::chrono_literals;
    auto now_ms = [] {
        return std::chrono::time_point_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now());
    };

    constexpr auto kFramePerSec = 20;
    constexpr auto kMsPerUpdate = std::chrono::milliseconds(1000 / kFramePerSec);
    auto previous = now_ms();
    std::chrono::milliseconds lag = 0ms;

    while (running_) {
        auto current = now_ms();
        auto elapsed = current - previous;

        std::queue<ServerSocket::RxMessage> rx_msgs = server_sock_.GetMessages();

        if (elapsed <= kMsPerUpdate) {
            std::this_thread::sleep_for(kMsPerUpdate / 4);
            continue;
        }

        lag += elapsed;
        {
            int64_t times = (lag / kMsPerUpdate);
            if (times > 1) {
                printf("Running behind. Calling %lux Update() to keep up\n", times);
            }
        }
        while (lag >= kMsPerUpdate) {
            Update();
            lag -= kMsPerUpdate;
        }

        // Send new data

        previous = current;
    }

    return 0;
}

}  // namespace fighttrack
