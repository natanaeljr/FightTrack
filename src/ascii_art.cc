/**
 * \file ascii_art.cc
 * \brief
 */

#include "fighttrack/ascii_art.h"

/**************************************************************************************/

namespace fighttrack {

AsciiArt::AsciiArt()
{
}

/**************************************************************************************/

void AsciiArt::Draw(int pos_y, int pos_x, WINDOW* win)
{
    mvwaddnstr(win, pos_y + 0, pos_x, " o ", 3);
    mvwaddnstr(win, pos_y + 1, pos_x, "/|\\", 3);
    mvwaddnstr(win, pos_y + 2, pos_x, "/ \\", 3);
}

} /* namespace fighttrack */
