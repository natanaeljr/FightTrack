/**
 * \file player_states.cc
 * \brief Player States definition.
 */

#include "fighttrack/player.h"
#include "fighttrack/player_states.h"

/**************************************************************************************/

namespace fighttrack {

PlayerState::Standing PlayerState::States::standing{};
PlayerState::Walking PlayerState::States::walking{
    PlayerState::Walking::Direction::RIGHT
};
PlayerState::Jumping PlayerState::States::jumping{};
PlayerState::Hit PlayerState::States::hit{};
PlayerState::Dying PlayerState::States::dying{};
PlayerState::Dead PlayerState::States::dead{};

/**************************************************************************************/

PlayerState* PlayerState::Standing::HandleInput(Player& player, int input)
{
    switch (input) {
        case KEY_UP: {
            player.StartJump();
            return this;
        }
        case KEY_RIGHT: return &(States::walking = Walking(Walking::RIGHT));
        case KEY_LEFT: return &(States::walking = Walking(Walking::LEFT));
    }
    return this;
}

void PlayerState::Standing::Update(Player& player)
{
    static const AsciiArt kArtStanding{ {
        " o ",
        "/|\\",
        "/ \\",
    } };
    player.SetGraphics(kArtStanding);
}

/**************************************************************************************/

PlayerState* PlayerState::Walking::HandleInput(Player& player, int input)
{
    switch (input) {
        case KEY_UP: {
            player.StartJump();
            return this;
        }
        case KEY_RIGHT: {
            if (direction_ == Direction::RIGHT)
                return this;
            return &(States::standing = Standing());
        }
        case KEY_LEFT: {
            if (direction_ == Direction::LEFT)
                return this;
            return &(States::standing = Standing());
        }
    }
    return this;
}

void PlayerState::Walking::Update(Player& player)
{
    threshold_++;
    if (threshold_ >= 2) {
        player.SetPosX(player.GetPosX() + static_cast<int>(direction_));
        printf("player pos walked to %dx%d\n", player.GetPosX(), player.GetPosY());
        threshold_ = 0;
    }
}

/**************************************************************************************/

PlayerState* PlayerState::Jumping::HandleInput(Player& player, int input)
{
    return Standing().HandleInput(player, input);
}

void PlayerState::Jumping::Update(Player& player)
{
    if (!jumped_) {
        player.StartJump();
        jumped_ = true;
    }
}

/**************************************************************************************/

PlayerState* PlayerState::Hit::HandleInput(Player& player, int input)
{
    return this;
}

void PlayerState::Hit::Update(Player& player)
{
}

/**************************************************************************************/

PlayerState* PlayerState::Dying::HandleInput(Player& player, int input)
{
    return this;
}

void PlayerState::Dying::Update(Player& player)
{
}

/**************************************************************************************/

PlayerState* PlayerState::Dead::HandleInput(Player& player, int input)
{
    return this;
}

void PlayerState::Dead::Update(Player& player)
{
}

} /* namespace fighttrack */
