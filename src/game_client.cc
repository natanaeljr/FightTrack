/**
 * \file game_client.cc
 * \brief  Game Client.
 */

#include "fighttrack/game_client.h"

#include <iostream>
#include <chrono>
#include <unistd.h>
#include <thread>

#include <ncurses.h>
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
GameClient::GameClient(std::string player_name)
    : running_{ false },
      map_{ kMapArt },
      player_{ player_name },
      remote_players_{},
      client_sock_{}
{
}

/**************************************************************************************/
GameClient::~GameClient()
{
}

/**************************************************************************************/
int GameClient::Run(std::string server_addr, uint16_t port)
{
    printf("Launch GameClient...\n");
    fflush(stdout);
    auto _exit_gameclient = gsl::finally([] {
        printf("Exit GameClient...\n");
        fflush(stdout);
    });

    if (client_sock_.Initialize(server_addr, port) != 0) {
        fprintf(stderr, "Failed to initialize client socket!\n");
        return -1;
    }

    /* Set default locale */
    setlocale(LC_ALL, "");

    /* Open a new tty for ncurses */
    FILE* tty = fopen("/dev/tty", "r+");
    if (tty == nullptr) {
        fprintf(stderr, "Failed to open a tty!\n");
        return -1;
    }
    auto _close_tty = gsl::finally([&] { fclose(tty); });

    /* Initialize terminal with ncurses */
    SCREEN* screen = newterm(nullptr, tty, tty);
    if (screen == nullptr) {
        fprintf(stderr, "Failed to initialize ncurses!\n");
        return -1;
    }
    auto _terminate_ncurses = gsl::finally([&] {
        endwin();
        delscreen(screen);
    });

    /* Set terminal to ncurses */
    set_term(screen);

    /* Check terminal size */
    constexpr int kMinLines = 24;
    constexpr int kMinCols = 80;
    if (LINES < kMinLines || COLS < kMinCols) {
        fprintf(stderr, "Terminal size must be at least %dx%d. Current size is %dx%d\n",
                kMinLines, kMinCols, LINES, COLS);
        return -2;
    }
    printf("Terminal window size is %dx%d\n", LINES, COLS);

    ConfigureTerminal(stdscr);

    /* Create Game window */
    WINDOW* game_window = newwin(kMinLines - 2, kMinCols - 2, (LINES - kMinLines) / 2,
                                 (COLS - kMinCols) / 2);
    if (game_window == nullptr) {
        fprintf(stderr, "Failed to create the game window\n");
        return -1;
    }
    auto _del_game_window = gsl::finally([&] { delwin(game_window); });
    ConfigureTerminal(game_window);

    running_ = true;
    return Loop(game_window);
}

/**************************************************************************************/

void GameClient::ConfigureTerminal(WINDOW* win)
{
    /* Make cursor invisible */
    curs_set(0);
    /* Disable line buffering. Get character as soon as it is typed */
    cbreak();
    /* Switch off echoing */
    noecho();
    /* Enable capturing of F1, F2, arrow keys, etc */
    keypad(win, true);
    /* Set timeout for input reading */
    wtimeout(win, 5);
}

/**************************************************************************************/

int GameClient::Loop(WINDOW* win)
{
    using namespace std::chrono_literals;
    auto now_ms = [] {
        return std::chrono::time_point_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now());
    };

    /* Get terminal size */
    int max_x = getmaxx(win) - 1;
    int max_y = getmaxy(win) - 1;
    /* Set player initial position */
    player_.SetPosX(1);
    player_.SetPosY(max_y - 3);

    constexpr auto kFramePerSec = 20;
    constexpr auto kMsPerUpdate = std::chrono::milliseconds(1000 / kFramePerSec);
    auto previous = now_ms();
    std::chrono::milliseconds lag = 0ms;

    if (client_sock_.Transmit("1:" + player_.GetName() + "\n") !=
        ClientSocket::Status::SUCCESS) {
        fprintf(stderr, "Failed to send player name to server\n");
        return -1;
    }

    while (running_) {
        auto current = now_ms();
        auto elapsed = current - previous;

        ProcessInput(win);

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

        Render(win);

        previous = current;
    }

    return 0;
}

/**************************************************************************************/

void GameClient::ProcessInput(WINDOW* win)
{
    int key = getch();
    if (key == -1) {
        return;
    }

    // printf("Key pressed: %d\n", key);
    switch (key) {
        case 27 /* ESC */: {
            running_ = false;
            return;
        }
    }

    printf("Game: sending key %d to server\n", key);
    client_sock_.Transmit("2:" + std::to_string(key) + "\n");
}

/**************************************************************************************/
int GameClient::ProcessNetworkInput()
{
    ClientSocket::RecvData recv_data = client_sock_.Receive();
    switch (recv_data.status) {
        case ClientSocket::Status::DISCONNECTED:
            printf("Disconnected from server\n");
            running_ = false;
            break;
        case ClientSocket::Status::ERROR:
            printf("Error reading from client socket\n");
            return -1;
        case ClientSocket::Status::SUCCESS:
            while (!recv_data.queue.empty()) {
                auto& msg = recv_data.queue.front();
                printf("Server sent: '%s'\n", msg.c_str());
                if (ProcessPacket(recv_data.queue.front()) != 0) {
                    fprintf(stderr, "Game: failed to process server message\n");
                    return -1;
                }
                recv_data.queue.pop();
            }
            break;
    }

    return 0;
}

/**************************************************************************************/
int GameClient::ProcessPacket(const std::string& packet)
{
    constexpr char kPlayerPositionTag = '3';

    size_t pos = 0;
    while (pos < packet.length()) {
        size_t len = packet.find_first_of('\n', pos);
        len = (len == std::string::npos) ? packet.length() : len;
        const std::string data{ &packet[pos], len };

        if (data[1] != ':') {
            fprintf(stderr, "Game: network message format not matched: (%s)\n",
                    data.c_str());
        }

        switch (data[0]) {
            case kPlayerPositionTag: {
                int x, y;
                sscanf(&data[2], "%d,%d", &x, &y);
                player_.SetPosX(x).SetPosY(y);
                printf("Game: received new player position %dx%d\n", player_.GetPosX(),
                       player_.GetPosY());
                break;
            }
            default:
                fprintf(stderr, "Game: unknown message from server: (%s)\n",
                        data.c_str());
        }

        pos = len + 1;
    }

    return 0;
}

/**************************************************************************************/
void GameClient::Update()
{
    player_.Update();
}

/**************************************************************************************/
void GameClient::Render(WINDOW* win)
{
    werase(win);
    box(win, 0, 0);
    map_.Draw(win);
    player_.Draw(win);
    wrefresh(win);
}

} /* namespace fighttrack */
