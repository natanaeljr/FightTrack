/**
 * \file player.cc
 * \brief
 */

#include "fighttrack/player.h"
#include "fighttrack/player_states.h"

/**************************************************************************************/

namespace fighttrack {

Player::Player(std::string name)
    : name_{ name },
      state_{ &PlayerState::States::standing },
      heart_{ 100 },
      art_{},
      pos_x_{ 0 },
      pos_y_{ 0 }
{
}

/**************************************************************************************/

Player::~Player()
{
}

/**************************************************************************************/

void Player::HandleInput(int input)
{
    state_ = state_->HandleInput(*this, input);
}

/**************************************************************************************/

int Player::Update()
{
    state_->Update(*this);

    if (jump_ticks_ > 0) {
        if (jump_ticks_ > 3) {
            SetPosY(GetPosY() - 1);
        }
        else {
            SetPosY(GetPosY() + 1);
        }
        jump_ticks_--;
    }

    return 0;
}

/**************************************************************************************/

void Player::Draw(WINDOW* win)
{
    art_.Draw(pos_y_, pos_x_, win);
}

/**************************************************************************************/

void Player::Damage(int value)
{
    if (value < 1 || value > 100) {
        fprintf(stderr, "Player: Invalid damage value requested: %d", value);
        return;
    }

    heart_ -= value;
    heart_ = heart_ < 0 ? 0 : heart_;
}

/**************************************************************************************/

void Player::StartJump()
{
    if (!IsJumping())
        jump_ticks_ = 6;
}

/**************************************************************************************/

bool Player::IsJumping()
{
    return jump_ticks_ > 0;
}

} /* namespace fighttrack */
