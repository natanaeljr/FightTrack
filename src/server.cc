/**
 * \file server.cc
 * \brief
 */

#include "fighttrack/server.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include <gsl/gsl>

/**************************************************************************************/

namespace fighttrack {

static constexpr size_t kMaxClients = 4;

int server(uint16_t port)
{
    int err = 0;

    int master_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (master_sock == -1) {
        perror("Failed to create socket");
        return -1;
    }
    auto _close_master_socket = gsl::finally([&] { close(master_sock); });

    {
        int flags = fcntl(master_sock, F_GETFL);
        flags |= O_NONBLOCK;
        err = fcntl(master_sock, F_SETFL, flags);
        if (err == -1) {
            perror("Failed to set socket control flags");
            return -1;
        }
    }

    {
        bool val = true;
        err = setsockopt(master_sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int));
        if (err == -1) {
            perror("Failed to set socket options");
            return -1;
        }
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    err = bind(master_sock, (struct sockaddr*) &server_addr, sizeof(server_addr));
    if (err == -1) {
        perror("Failed to bind socket");
        return -1;
    }

    err = listen(master_sock, kMaxClients);
    if (err == -1) {
        perror("Failed to listen socket");
        return -1;
    }

    while (true) {
        printf("Waiting for new client..\n");
        fflush(stdout);

        struct sockaddr_in client_addr;
        socklen_t client_len;
        int client_sock =
            accept(master_sock, (struct sockaddr*) &client_addr, &client_len);
        if (client_sock == -1) {
            if (errno == EWOULDBLOCK) {
                sleep(1);
                continue;
            }
            else {
                perror("Failed to accept connection");
                return -1;
            }
        }
        auto _close_client_socket = gsl::finally([&] { close(client_sock); });

        printf("Server: got connection from %s port %d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        int n = send(client_sock, "Hello from server", 17, 0);
        if (n == -1) {
            perror("Failed to send data to client");
            return -1;
        }

        while (true) {
            printf("Waiting client response..\n");
            char buffer[256] = { 0 };
            n = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (n == -1) {
                if (errno == EWOULDBLOCK) {
                    sleep(1);
                    continue;
                }
                else {
                    perror("Failed to read data from client");
                    return -1;
                }
            }
            else if (n == 0) {
                printf("Connection closed.\n");
                break;
            }
            else {
                buffer[n] = '\0';
                printf("Received: '%s'\n", buffer);
                break;
            }
        }
    }

    printf("Server finished\n");
    return 0;
}

} /* namespace fighttrack */

int main()
{
    return fighttrack::server(9124);
}
