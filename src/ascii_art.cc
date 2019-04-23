/**
 * \file ascii_art.cc
 * \brief
 */

#include "fighttrack/ascii_art.h"

/**************************************************************************************/

namespace fighttrack {

AsciiArt::AsciiArt(std::vector<std::string> chars)
    : matrix_{ chars }, max_x_{ 0 }, max_y_{ 0 }
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
    for (size_t y = 0; y < matrix_.size(); ++y) {
        const auto& chars = matrix_[y];

        for (size_t x = 0; x < chars.length();) {
            size_t x_end = chars.find_first_of(' ', x);
            x_end = (x_end == std::string::npos ? chars.length() : x_end);

            size_t length = x_end - x;
            if (length > 0) {
                mvwaddnstr(win, pos_y + y, pos_x + x, &chars[x], length);
            }

            x += length + 1;
        }
    }
}

/**************************************************************************************/

char AsciiArt::GetChar(int pos_x, int pos_y)
{
    /* Check boundaries */
    if (pos_y >= matrix_.size() || pos_x >= matrix_[pos_y].length()) {
        return '\0';
    }
    return matrix_[pos_y][pos_x];
}

} /* namespace fighttrack */
