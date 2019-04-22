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

bool Map::IsGround(int x, int y)
{
    return art_.GetChar(x, y) == '▓';
}

} /* namespace fighttrack */
