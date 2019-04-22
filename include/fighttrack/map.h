/**
 * \file map.h
 * \brief Map declearation.
 */

#include "fighttrack/ascii_art.h"

/**************************************************************************************/

namespace fighttrack {

class Map {
   public:
    /**
     * \brief Construct a new Map object
     * \param art Map graphics.
     */
    Map(AsciiArt art);

    /**
     * \brief Destroy the Map object
     */
    ~Map() = default;

    /**
     * \brief  Check if a given position a ground.
     * \param x  X postion.
     * \param y  Y position.
     */
    bool IsGround(int x, int y);

   private:
    AsciiArt art_;  //!< Map graphics
};

} /* namespace fighttrack */
