/**
 * \file game_client.h
 * \brief Game Client.
 */

#include <vector>
#include <cstdint>
#include <ncurses.h>

#include "fighttrack/client_socket.h"
#include "fighttrack/player.h"
#include "fighttrack/map.h"

namespace fighttrack {

class GameClient {
   public:
    /**
     * \brief Construct a new Game Client object
     * \param player_name Main player name.
     */
    GameClient(std::string player_name);

    /**
     * \brief Destroy the Game Client object
     */
    ~GameClient();

    /**
     * \brief Run the game loop.
     * \param server_addr Server address, format "0.0.0.0".
     * \param port  Server port.
     * \return 0 on sucess, negative on error.
     */
    int Run(std::string server_addr, uint16_t port);

   private:
    /**
     * \brief Configure the terminal with Ncurses.
     */
    void ConfigureTerminal(WINDOW* win);

    /**
     * \brief Game loop.
     * \return 0 on sucess, negative on error.
     */
    int Loop(WINDOW* win);

    /**
     * \brief Process input from user.
     */
    void ProcessInput(WINDOW* win);

    /**
     * \brief Update all objects.
     */
    void Update();

    /**
     * \brief Render all objects.
     */
    void Render(WINDOW* win);

   private:
    //! Game loop running flag
    bool running_;
    //! World map
    Map map_;
    //! This Player
    Player player_;
    //! Remote players
    std::vector<Player> remote_players_;
    //! High-level client socket API
    ClientSocket client_sock_;
};

} /* namespace fighttrack */
