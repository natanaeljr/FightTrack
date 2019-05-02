/**
 * \file player.h
 * \brief Player class.
 */

#pragma once

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
    Player() : Player("") {}

    /**
     * \brief Construct a new Player object.
     * \param name Player's name.
     */
    Player(std::string name);

    /**
     * \brief Destroy the Player object.
     */
    ~Player();

    /**
     * \brief Set player's name.
     * \param name Name.
     */
    Player& SetName(std::string name)
    {
        name_ = name;
        return *this;
    }

    /**
     * \brief Get player's name.
     */
    const std::string& GetName() const { return name_; }

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
    Player& Damage(int value);

    /**
     * \brief Set the Player Graphics.
     * \param art ASCII Art.
     */
    Player& SetGraphics(AsciiArt art)
    {
        art_ = art;
        return *this;
    }

    /**
     * \brief Get player position.
     * \return Position.
     */
    int GetPosX() const { return pos_x_; }
    int GetPosY() const { return pos_y_; }

    /**
     * \brief Set player position.
     * \param pos Position.
     */
    Player& SetPosX(int pos_x)
    {
        pos_x_ = pos_x;
        dirty_ = true;
        return *this;
    }
    Player& SetPosY(int pos_y)
    {
        pos_y_ = pos_y;
        dirty_ = true;
        return *this;
    }

    /**
     * \brief Start jump animation
     */
    Player& StartJump();

    /**
     * \brief  Check if player is currently jumping
     */
    bool IsJumping() const;

    /**
     * \brief  Check if player is has been modified
     */
    bool Dirty()
    {
        auto ret = dirty_;
        dirty_ = false;
        return ret;
    }

   private:
    std::string name_;    //!< Player name
    PlayerState* state_;  //!< Player's current state
    int heart_;           //!< Player's current life value (range 0~100)
    AsciiArt art_;        //!< Player's current graphics
    int pos_x_, pos_y_;   //!< Player's current position
    int jump_ticks_ = 0;  //!< Current number of ticks jumping
    bool dirty_;          //!< Flag indicating modification
};

} /* namespace fighttrack */