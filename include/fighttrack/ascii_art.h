/**
 * \file ascii_art.h
 * \brief ASCII Art declaration.
 */

#include <vector>
#include <string>
#include <ncurses.h>

/**************************************************************************************/

namespace fighttrack {

class AsciiArt {
   public:
    /**
     * \brief Construct a new Ascii Art object
     */
    AsciiArt(std::vector<std::string> art);

    /**
     * \brief Destroy the Ascii Art object
     */
    ~AsciiArt() = default;

    /**
     * \brief  Draw the art to the window.
     * \param pos_y  Y position.
     * \param pos_x  X position.
     * \param win  Ncurses Window.
     */
    void Draw(int pos_x, int pos_y, WINDOW* win);

    /**
     * \brief Retrive the charecter at given position
     * \param pos_x  X position relative to the art's origin.
     * \param pos_y  Y position relative to the art's origin.
     * \return return '\0' if position is not valid.
     */
    char GetChar(int pos_x, int pos_y);

   private:
    int max_x_, max_y_;                //!< Maximum length of the art
    std::vector<std::string> matrix_;  //!< Art char matrix
};

} /* namespace fighttrack */
