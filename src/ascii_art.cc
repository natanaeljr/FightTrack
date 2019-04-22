/**
 * \file ascii_art.cc
 * \brief
 */

#include "fighttrack/ascii_art.h"

/**************************************************************************************/

namespace fighttrack {

AsciiArt::AsciiArt(std::vector<std::string> chars) : matrix_{ chars }, max_x_{ 0 }, max_y_{ 0 }
{
    max_y_ = matrix_.size();
    for (const auto& seq : matrix_) {
        if (max_x_ < seq.size()) {
            max_x_ = seq.size();
        }
    }
}

/**************************************************************************************/

void AsciiArt::Draw(int pos_x, int pos_y, WINDOW* win)
{
    mvwaddnstr(win, pos_y + 0, pos_x, " o ", 3);
    mvwaddnstr(win, pos_y + 1, pos_x, "/|\\", 3);
    mvwaddnstr(win, pos_y + 2, pos_x, "/ \\", 3);
}

/**************************************************************************************/

char AsciiArt::GetChar(int pos_x, int pos_y)
{
    return '\0';
}

} /* namespace fighttrack */
