/**
 * \file   main.cc
 * \brief  Main file.
 */
#include "fighttrack/game.h"

int main(int argc, const char* argv[])
{
    fighttrack::Game game;
    int ret = game.Run(argc, argv);
    return ret;
}