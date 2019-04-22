#include <iostream>
#include <unistd.h>

#include <gsl/gsl>
#include <ncurses.h>

const char kSlogan[] = R"(
   ██████╗██╗ ██████╗ ██╗  ██╗████████╗  ████████╗██████╗  █████╗  ██████╗██╗  ██╗
   ██╔═══╝██║██╔════╝ ██║  ██║╚══██╔══╝  ╚══██╔══╝██╔══██╗██╔══██╗██╔════╝██╚═██╔╝
   ████╗  ██║██║ ████╗███████║   ██║   ██╗  ██║   ██████╔╝███████║██║     █████╔╝ 
   ██╔═╝  ██║██║   ██║██╔══██║   ██║   ╚═╝  ██║   ██╔══██╗██╔══██║██║     ██╔═██╗ 
   ██║    ██║╚██████╔╝██║  ██║   ██║        ██║   ██║  ██║██║  ██║╚██████╗██║  ██╗
   ╚═╝    ╚═╝ ╚═════╝ ╚═╝  ╚═╝   ╚═╝        ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝╚═╝  ╚═╝
)";

const char kPlayer1[] =
    " o "
    "/|\\"
    "/ \\";

const char kPlayer2[] =
    " o "
    "/|>"
    " >\\";

const char kPlayer3[] =
    " o "
    "<|\\"
    " |>";

void DrawPlayer(int y, int x, gsl::czstring<> player)
{
    mvaddnstr(y + 0, x, &player[0], 3);
    mvaddnstr(y + 1, x, &player[3], 3);
    mvaddnstr(y + 2, x, &player[6], 3);
}

int test()
{
    setlocale(LC_ALL, "");

    WINDOW* window = initscr();
    if (window == nullptr) {
        std::cerr << "Failed to initialize ncurses!" << std::endl;
        return -1;
    }
    auto _auto_terminate_ncurses = gsl::finally([] { endwin(); });

    /* Make cursor invisible */
    curs_set(0);

    /* Disable line buffering. Get character as soon as it is typed */
    cbreak();

    /* Switch off echoing */
    noecho();

    /* Enable capturing of F1, F2, arrow keys, etc */
    keypad(stdscr, true);

    /* Get the number of rows and columns */
    int row = getmaxy(stdscr);
    int col = getmaxx(stdscr);

    addstr(kSlogan);

    refresh();

    for (int i = 0; i < 21; i += 6) {
        clear();
        DrawPlayer(0, i, kPlayer1);
        refresh();
        usleep(150'000);
    }

    char ch = getch();

    addch(ch | A_BOLD | A_UNDERLINE | A_COLOR);

    getch();

    return 0;
}