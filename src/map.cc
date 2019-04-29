/**
 * \file map.cc
 * \brief Map definition.
 */

#include "fighttrack/map.h"

/**************************************************************************************/

namespace fighttrack {

Map::Map(AsciiArt art) : art_{ art }
{
}

/**************************************************************************************/

void Map::Draw(WINDOW* win)
{
    art_.Draw(1, 1, win);
}

/**************************************************************************************/

bool Map::IsGround(int x, int y)
{
    // return art_.GetChar(x, y) == '▓';
    return art_.GetChar(x, y) == ' ';
}

} /* namespace fighttrack */
