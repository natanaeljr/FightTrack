/**
 * \file player_states.h
 * \brief Player States declaration.
 */

#pragma once

#include <ncurses.h>
#include <gsl/gsl>

/***************************************************************************************/

namespace fighttrack {

class Player;

/**************************************************************************************/

class PlayerState {
   public:
    virtual ~PlayerState() = default;
    virtual PlayerState* HandleInput(Player& player, int input) = 0;
    virtual void Update(Player& player) = 0;

    /* Pool states to avoid allocation */
    struct States;

    class Standing;
    class Walking;
    class Jumping;
    class Hit;
    class Dying;
    class Dead;
};

/**************************************************************************************/

class PlayerState::Standing : public PlayerState {
   public:
    virtual ~Standing() = default;
    virtual PlayerState* HandleInput(Player& player, int input) override;
    virtual void Update(Player& player) override;
};

/**************************************************************************************/

class PlayerState::Walking : public PlayerState {
   public:
    enum Direction {
        LEFT = -1,
        RIGHT = 1,
    };

    Walking(Direction direction) : direction_{ direction }, threshold_{ 0 } {}
    virtual ~Walking() = default;
    virtual PlayerState* HandleInput(Player& player, int input) override;
    virtual void Update(Player& player) override;

   protected:
    Direction direction_;
    int threshold_;
};

/**************************************************************************************/

class PlayerState::Jumping : public PlayerState {
   public:
    virtual ~Jumping() = default;
    virtual PlayerState* HandleInput(Player& player, int input) override;
    virtual void Update(Player& player) override;

   protected:
    bool jumped_ = false;
};

/**************************************************************************************/

class PlayerState::Hit : public PlayerState {
   public:
    virtual ~Hit() = default;
    virtual PlayerState* HandleInput(Player& player, int input) override;
    virtual void Update(Player& player) override;
};

/**************************************************************************************/

class PlayerState::Dying : public PlayerState {
   public:
    virtual ~Dying() = default;
    virtual PlayerState* HandleInput(Player& player, int input) override;
    virtual void Update(Player& player) override;
};

/**************************************************************************************/

class PlayerState::Dead : public PlayerState {
   public:
    virtual ~Dead() = default;
    virtual PlayerState* HandleInput(Player& player, int input) override;
    virtual void Update(Player& player) override;
};

/**************************************************************************************/

struct PlayerState::States {
    static PlayerState::Standing standing;
    static PlayerState::Walking walking;
    static PlayerState::Jumping jumping;
    static PlayerState::Hit hit;
    static PlayerState::Dying dying;
    static PlayerState::Dead dead;
};

} /* namespace fighttrack */