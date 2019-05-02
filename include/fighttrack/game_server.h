/**
 * \file game_server.h
 * \brief Game Server.
 */

#pragma once

#include <vector>
#include <map>

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

    /**
     * \brief Process messages received in Server Socket.
     * \return 0 on sucess, negative on error.
     */
    int ProcessNetworkInput();

    /**
     * \brief Process data packets received from client players.
     * \param client_id Client ID.
     * \param packet    Data packet / message.
     * \return 0 on sucess, negative on error.
     */
    int ProcessPacket(int client_id, const std::string& packet);

    /**
     * \brief  Transmit updates to client players.
     * \return 0 on sucess, negative on error.
     */
    int TransmitUpdates();

   private:
    //! Game loop running flag
    bool running_;
    //! World map
    Map map_;
    //! Map of connected players; key: client ID; element: Player object
    std::map<int, Player> players_;
    //! High-level server socket API
    ServerSocket server_sock_;
};

} /* namespace fighttrack */