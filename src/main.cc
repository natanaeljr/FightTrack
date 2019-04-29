/**
 * \file   main.cc
 * \brief  Main file.
 */
#include "fighttrack/fighttrack.h"

int main(int argc, const char* argv[])
{
    fighttrack::FightTrack game;
    int ret = game.Run(argc, argv);
    return ret;
}