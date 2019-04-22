/**
 * \file   main.cc
 * \brief  Main file.
 */
#include "fighttrack/game.h"

int main(int argc, const char* args[])
{
    fighttrack::Game game;
    int ret = game.Run(argc, args);
    return ret;
}