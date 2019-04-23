/**
 * \file client.h
 * \brief Client
 */

#include <cstdint>
#include <string>

/**************************************************************************************/

namespace fighttrack {

int client(const std::string server_address, const uint16_t port);

} /* namespace fighttrack */