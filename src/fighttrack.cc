/**
 * \file fighttrack.cc
 * \brief FightTrack definition.
 */

#include "fighttrack/fighttrack.h"

#include <cstdio>
#include <cstring>
#include <string>

#include "fighttrack/game_client.h"

/**************************************************************************************/

namespace fighttrack {

int FightTrack::Run(int argc, const char* argv[])
{
    if (argc < 3 || argc > 4) {
        fprintf(stderr,
                "Wrong number of arguments!\n"
                "Arguments: <server|client> <port|address:port> [player name]\n");
        return -1;
    }

    if (strcmp(argv[1], "server") == 0) {
        int port = std::stoi(argv[3]);
        if (port < 0 || port > UINT16_MAX) {
            fprintf(stderr, "Invalid port number!\n");
            return -1;
        }
        // return GameServer().Run(port);
    }
    else if (strcmp(argv[1], "client") == 0) {
        int port;
        unsigned char ip[4];
        if (sscanf(argv[2], "%hhu.%hhu.%hhu.%hhu:%d", &ip[0], &ip[1], &ip[2], &ip[3],
                   &port) != 5) {
            fprintf(stderr, "Invalid server address!\n");
            return -1;
        }
        if (port < 0 || port > UINT16_MAX) {
            fprintf(stderr, "Invalid port number!\n");
            return -1;
        }
        return GameClient(argv[3]).Run(
            { argv[2], std::string(argv[2]).find_first_of(':') }, (uint16_t) port);
    }
    else {
        fprintf(stderr, "Invalid game side!\n");
        return -1;
    }

    return 1;
}

} /* namespace fighttrack */