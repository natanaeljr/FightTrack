/**
 * \file ascii_art.h
 * \brief ASCII Art declaration.
 */

#include <ncurses.h>

/**************************************************************************************/

namespace fighttrack {

class AsciiArt {
   public:
    AsciiArt();
    ~AsciiArt() = default;

    void Draw(int pos_y, int pos_x, WINDOW* win);

   private:
    int x, y;
};

} /* namespace fighttrack */
