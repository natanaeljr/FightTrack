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

        if (elapsed <= kMsPerUpdate) {
            std::this_thread::sleep_for(kMsPerUpdate / 4);
            continue;
        }

        if (ProcessNetworkInput() != 0) {
            fprintf(stderr, "Game: error processing network input\n");
            return -1;
        }

        lag += elapsed;
        {
            int64_t times = (lag / kMsPerUpdate);
            if (times > 1) {
                // printf("Running behind. Calling %lux Update() to keep up\n", times);
            }
        }
        while (lag >= kMsPerUpdate) {
            Update();
            lag -= kMsPerUpdate;
        }

        if (TransmitUpdates() != 0) {
            fprintf(stderr, "Game: failed to transmit game updates to clients\n");
            return -1;
        }

        previous = current;
    }

    return 0;
}

/**************************************************************************************/
void GameServer::Update()
{
    for (auto& player_it : players_) {
        auto& player = player_it.second;
        player.Update();
    }
}

/**************************************************************************************/
int GameServer::ProcessNetworkInput()
{
    std::queue<ServerSocket::RxMessage> rx_msgs = server_sock_.GetMessages();

    while (!rx_msgs.empty()) {
        auto& msg = rx_msgs.front();
        switch (msg.status) {
            case ServerSocket::RxStatus::CONNECTED: {
                printf("Game: new client connected: %d\n", msg.client_id);
                players_[msg.client_id].SetPosX(2).SetPosY(18);
                break;
            }
            case ServerSocket::RxStatus::DISCONNECTED: {
                auto player_it = players_.find(msg.client_id);
                if (player_it == players_.end()) {
                    fprintf(
                        stderr,
                        "Game: something went wrong. Can't remove unknown client %d\n",
                        msg.client_id);
                    return -1;
                }
                printf("Game: erasing player '%s'\n",
                       player_it->second.GetName().c_str());
                players_.erase(player_it);

                printf("Game: client %d disconnected\n", msg.client_id);
                break;
            }
            case ServerSocket::RxStatus::NEW_DATA: {
                printf("Game: client %d sent: '%s'\n", msg.client_id, msg.buffer.c_str());
                if (ProcessPacket(msg.client_id, msg.buffer) != 0) {
                    fprintf(stderr, "Game: failed to process message from client %d\n",
                            msg.client_id);
                    return -1;
                }
                break;
            }
        }
        rx_msgs.pop();
    }

    return 0;
}

/**************************************************************************************/
int GameServer::ProcessPacket(int client_id, const std::string& packet)
{
    constexpr char kPlayerNameTag = '1';
    constexpr char kPlayerKeyPressTag = '2';

    size_t pos = 0;
    while (pos < packet.length()) {
        size_t len = packet.find_first_of('\n', pos);
        len = (len == std::string::npos) ? packet.length() : len;
        const std::string data{ &packet[pos], len };

        if (data[1] != ':') {
            fprintf(stderr,
                    "Game: network message format not matched from client %d: (%s)\n",
                    client_id, data.c_str());
        }

        switch (data[0]) {
            case kPlayerNameTag: {
                auto& player = players_[client_id];
                player.SetName(std::string{ &data[2], len });
                printf("Game: player '%s' is online\n", player.GetName().c_str());
                // player.SetPosX(1).SetPosY(18);
                break;
            }
            case kPlayerKeyPressTag: {
                auto& player = players_[client_id];
                int key;
                sscanf(&data[2], "%d", &key);
                printf("Game: client %d press key %d\n", client_id, key);
                player.HandleInput(key);
                break;
            }
            default:
                fprintf(stderr, "Game: unknown message from client %d: (%s)\n", client_id,
                        data.c_str());
        }

        pos = len + 1;
    }

    return 0;
}

int GameServer::TransmitUpdates()
{
    constexpr char kPlayerPositionTag = '3';

    for (auto& player_it : players_) {
        auto& player = player_it.second;
        bool dirty = player.Dirty();
        printf("player %s is dirty? %d\n", player.GetName().c_str(), dirty);
        if (dirty) {
            std::string buffer = { kPlayerPositionTag + std::string(":") +
                                   std::to_string(player.GetPosX()) + ',' +
                                   std::to_string(player.GetPosY()) + "\n" };
            server_sock_.Transmit({
                .client_ids = { player_it.first },
                .buffer = buffer,
            });
            printf("Transmitted '%s' to player '%s'\n", buffer.c_str(),
                   player.GetName().c_str());
        }
    }

    return 0;
}

}  // namespace fighttrack
