/**
 * \file fighttrack.h
 * \brief FightTrack class.
 */

/**************************************************************************************/

namespace fighttrack {

class FightTrack {
   public:
    /**
     * \brief Construct a new FightTrack object.
     */
    FightTrack() = default;

    /**
     * \brief Destroy the FightTrack object.
     */
    ~FightTrack() = default;

    /**
     * \brief Run the game loop.
     * \param argc Number of arguments
     * \param argv List of arguments
     * \return 0 on sucess, negative on error.
     */
    int Run(int argc, const char* argv[]);
};

} /* namespace fighttrack */
