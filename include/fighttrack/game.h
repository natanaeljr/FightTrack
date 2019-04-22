/**
 * \file game.h
 * \brief Game class.
 */

#include <cstdint>
#include <ncurses.h>

#include "fighttrack/player.h"

/**************************************************************************************/

namespace fighttrack {

class Game {
   public:
    /**
     * \brief Construct a new Game object.
     */
    Game();

    /**
     * \brief Destroy the Game object.
     */
    ~Game();

    /**
     * \brief  Run the game loop.
     * \param argc Number of arguments
     * \param args List of arguments
     * \return 0 on sucess, negative on error.
     */
    int Run(int argc, const char* args[]);

   private:
    /**
     * \brief Configure the terminal with Ncurses.
     */
    void ConfigureTerminal(WINDOW* win);

    /**
     * \brief Game loop.
     * \return 0 on sucess, negative on error.
     */
    int Loop(WINDOW* win);

    /**
     * \brief Process input from user.
     */
    void ProcessInput(WINDOW* win);

    /**
     * \brief Update all objects.
     */
    void Update();

    /**
     * \brief Render all objects.
     */
    void Render(WINDOW* win);

   private:
    Player player_;
    bool running_;
};

} /* namespace fighttrack */
