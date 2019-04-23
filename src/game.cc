/**
 * \file game.cc
 * \brief Game definition.
 */

#include "fighttrack/game.h"

#include <iostream>
#include <chrono>
#include <unistd.h>
#include <linux/input-event-codes.h>

#include <ncurses.h>
#include <gsl/gsl>

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

Game::Game() : player_{ "Main" }, map_{ kMapArt }, running_{ false }
{
}

/**************************************************************************************/

Game::~Game()
{
}

/**************************************************************************************/

int Game::Run(int argc, const char* args[])
{
    /* No arguments for now */
    std::ignore = argc;
    std::ignore = args;

    printf("Launch FightTrack...\n");
    fflush(stdout);
    auto _exit_fighttrack = gsl::finally([] {
        printf("Exit FightTrack...\n");
        fflush(stdout);
    });

    /* Set default locale */
    setlocale(LC_ALL, "");

    /* Open a new tty for ncurses */
    FILE* tty = fopen("/dev/tty", "r+");
    if (tty == nullptr) {
        fprintf(stderr, "Failed to open a tty!");
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

void Game::ConfigureTerminal(WINDOW* win)
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
    wtimeout(win, 10);
}

/**************************************************************************************/

int Game::Loop(WINDOW* win)
{
    using namespace std::chrono_literals;
    auto now_ms = [] {
        return std::chrono::time_point_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now());
    };

    auto previous = now_ms();
    std::chrono::milliseconds lag = 0ms;
    constexpr auto kFramePerSec = 20;
    constexpr auto kMsPerUpdate = std::chrono::milliseconds(1000 / kFramePerSec);

    int max_x = getmaxx(win) - 1;
    int max_y = getmaxy(win) - 1;

    /* Set player initial position */
    player_.SetPosX(1);
    player_.SetPosY(max_y - 3);

    while (running_) {
        auto current = now_ms();
        auto elapsed = current - previous;

        ProcessInput(win);

        if (elapsed <= kMsPerUpdate) {
            continue;
        }

        lag += elapsed;
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

void Game::ProcessInput(WINDOW* win)
{
    int key = getch();
    if (key == -1) {
        return;
    }

    printf("Key pressed: %d\n", key);
    switch (key) {
        case 27 /* ESC */: {
            running_ = false;
            return;
        }
    }

    player_.HandleInput(key);
}

/**************************************************************************************/

void Game::Update()
{
    printf("%s\n", __PRETTY_FUNCTION__);
    player_.Update();
}

/**************************************************************************************/

void Game::Render(WINDOW* win)
{
    printf("%s\n", __PRETTY_FUNCTION__);
    werase(win);
    box(win, 0, 0);
    map_.Draw(win);
    player_.Draw(win);
    wrefresh(win);
}

} /* namespace fighttrack */