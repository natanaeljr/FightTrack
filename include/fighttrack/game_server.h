/**
 * \file game_server.h
 * \brief Game Server.
 */

#include <vector>

#include "fighttrack/server_socket.h"
#include "fighttrack/player.h"
#include "fighttrack/map.h"

namespace fighttrack {

class GameServer {
   public:
    /**
     * \brief Construct a new Game Server object
     */
    GameServer();
    /**
     * \brief Destroy the Game Server object
     */
    ~GameServer();

    /**
     * \brief Run the game loop.
     * \param port  Server port.
     * \return 0 on sucess, negative on error.
     */
    int Run(uint16_t port);

   private:
    /**
     * \brief Game loop.
     * \return 0 on sucess, negative on error.
     */
    int Loop();

    /**
     * \brief Update all objects.
     */
    void Update();

   private:
    //! Game loop running flag
    bool running_;
    /* List of connected players */
    std::vector<Player> players_;
    //! World map
    Map map_;
    //! High-level server socket API
    ServerSocket server_sock_;
};

} /* namespace fighttrack */