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
    "                                         ▓▓▓▓▓▓▓   ▓▓▓▓▓▓▓▓▓▓               ",
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
                int x = 2 + msg.client_id * 10;
                players_[msg.client_id].SetPosX(x).SetPosY(18);
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
                std::vector<int> client_ids;
                client_ids.resize(players_.size() - 1);
                for (auto& player : players_) {
                    if (player.first != msg.client_id)
                        client_ids.push_back(player.first);
                }
                std::string name = player_it->second.GetName();
                if (!client_ids.empty()) {
                    server_sock_.Transmit(
                        { .client_ids = client_ids, .buffer = "4:" + name + "\n" });
                }

                printf("Game: erasing player '%s'\n", name.c_str());
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
                player.SetName(std::string{ &data[2], len - 2 });
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

    std::vector<int> client_ids;
    client_ids.resize(players_.size());

    std::string message;

    for (auto& player_it : players_) {
        client_ids.push_back(player_it.first);
        auto& player = player_it.second;
        // bool dirty = player.Dirty();
        // if (dirty) {
        char buffer[100];
        snprintf(buffer, sizeof(buffer), "%c:%s:%d,%d\n", kPlayerPositionTag,
                 player.GetName().c_str(), player.GetPosX(), player.GetPosY());
        message += buffer;
        // }
    }

    if (!client_ids.empty() && !message.empty()) {
        printf("Transmitting '%s' to clients\n", message.c_str());
        server_sock_.Transmit({
            .client_ids = std::move(client_ids),
            .buffer = std::move(message),
        });
    }

    return 0;
}

}  // namespace fighttrack
