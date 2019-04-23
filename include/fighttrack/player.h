/**
 * \file player.h
 * \brief Player class.
 */

#include <string>
#include <ncurses.h>
#include <gsl/gsl>

#include "fighttrack/ascii_art.h"

/**************************************************************************************/

namespace fighttrack {

class PlayerState;

class Player {
   public:
    /**
     * \brief Construct a new Player object.
     */
    Player(std::string name);

    /**
     * \brief Destroy the Player object.
     */
    ~Player();

    /**
     * \brief  Process input from user.
     * \param input Input Key.
     */
    void HandleInput(int input);

    /**
     * \brief Update the Player object.
     * \return 0 if player is alive, -1 if player died.
     */
    int Update();

    /**
     * \brief Draw the Player object.
     */
    void Draw(WINDOW* win);

    /**
     * \brief Damage the player
     * \param value Value to damage (range 1~100)
     */
    void Damage(int value);

    /**
     * \brief Set the Player Graphics.
     * \param art ASCII Art.
     */
    void SetGraphics(AsciiArt art) { art_ = art; }

    /**
     * \brief Get player position.
     * \return Position.
     */
    int GetPosX() { return pos_x_; }
    int GetPosY() { return pos_y_; }

    /**
     * \brief Set player position.
     * \param pos Position.
     */
    void SetPosX(int pos_x) { pos_x_ = pos_x; }
    void SetPosY(int pos_y) { pos_y_ = pos_y; }

    /**
     * \brief Start jump animation
     */
    void StartJump();

    /**
     * \brief  Check if player is currently jumping
     */
    bool IsJumping();

   private:
    std::string name_;    //!< Player name
    PlayerState* state_;  //!< Player's current state
    int heart_;           //!< Player's current life value (range 0~100)
    AsciiArt art_;        //!< Player's current graphics
    int pos_x_, pos_y_;   //!< Player's current position
    int jump_ticks_ = 0;       //!< Current number of ticks jumping
};

} /* namespace fighttrack */