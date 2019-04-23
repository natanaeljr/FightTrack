/**
 * \file client.cc
 * \brief Client
 */

#include "fighttrack/client.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <gsl/gsl>

/**************************************************************************************/

namespace fighttrack {

int client(const std::string server_address, uint16_t port)
{
    int err = 0;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Failed to create socket");
        return -1;
    }
    auto _close_socket = gsl::finally([&] { close(sock); });

    struct hostent* server;
    server = gethostbyname(server_address.data());
    if (server == nullptr) {
        fprintf(stderr, "Failed to get host %s", server_address.data());
        return -1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    bcopy((char*)server->h_addr, (char*)&server_addr.sin_addr.s_addr, server->h_length);
    server_addr.sin_port = htons(port);

    err = connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (err == -1) {
        perror("Failed to connect to server");
        return -1;
    }

    int n = write(sock, "Hello from client", 17);
    if (n == -1) {
        perror("Failed to write to server");
        return -1;
    }

    char buffer[256] = { 0 };
    n = read(sock, buffer, sizeof(buffer) - 1);
    if (n == -1) {
        perror("Failed to read from server");
        return -1;
    } else if (n == 0) {
        printf("Server closed connection");
    } else {
        buffer[n] = '\0';
        printf("Received '%s'\n", buffer);
    }

    printf("Client finished\n");
    return 0;
}

} /* namespace fighttrack */

int main()
{
    return fighttrack::client("127.0.0.1", 9124);
}