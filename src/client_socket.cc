/**
 * \file client_socket.cc
 * \brief Client Socket implementation
 */

#include "fighttrack/client_socket.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <thread>
#include <chrono>
#include <gsl/gsl>

using namespace std::chrono_literals;

/**************************************************************************************/

namespace fighttrack {

ClientSocket::ClientSocket() : initialized_{ false }, socket_{ 0 }
{
}

/**************************************************************************************/
ClientSocket::~ClientSocket()
{
    if (initialized_)
        Terminate();
}

/**************************************************************************************/
int ClientSocket::Initialize(const std::string& server_addr, const uint16_t port)
{
    int ret = 0;
    int err = 0;

    socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ == -1) {
        perror("Client: failed to create socket");
        return ret = -1;
    }
    auto _close_socket = gsl::finally([&] {
        if (ret != 0) {
            close(socket_);
        }
    });

    struct hostent* server_ent;
    server_ent = gethostbyname(server_addr.data());
    if (server_ent == nullptr) {
        fprintf(stderr, "Client: failed to get host %s", server_addr.data());
        return -1;
    }

    struct sockaddr_in server_sockaddr;
    server_sockaddr.sin_family = AF_INET;
    server_sockaddr.sin_port = htons(port);

    bcopy((char*) server_ent->h_addr, (char*) &server_sockaddr.sin_addr.s_addr,
          server_ent->h_length);

    err = connect(socket_, (struct sockaddr*) &server_sockaddr, sizeof(server_sockaddr));
    if (err == -1) {
        perror("Client: failed to connect to server");
        return ret = -1;
    }

    initialized_ = true;
    printf("Client: initialized\n");

    return ret;
}

/**************************************************************************************/
void ClientSocket::Terminate()
{
    if (!initialized_)
        return;

    close(socket_);

    initialized_ = false;
    printf("Client: terminated\n");
}

/**************************************************************************************/
ClientSocket::RecvData ClientSocket::Receive()
{
    RecvData ret = { Status::SUCCESS, {} };

    while (true) {
        char buffer[2000];
        int n = recv(socket_, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);
        if (n == -1) {
            if (errno == EWOULDBLOCK) {
                break;
            }
            perror("Client: failed to read data from socket");
            ret.status = Status::ERROR;
            break;
        }
        if (n == 0) {
            printf("Client: server closed connection\n");
            ret.status = Status::DISCONNECTED;
            break;
        }
        /* There is data available */
        buffer[n] = '\0';
        printf("Client: received: %s\n", buffer);
        ret.queue.emplace(std::string{ buffer, (size_t) n });
    }

    return ret;
}

/**************************************************************************************/
ClientSocket::Status ClientSocket::Transmit(std::string data)
{
    Status ret = Status::SUCCESS;

    size_t length = data.length();
    while (length > 0) {
        const char* buffer = data.c_str() + (data.length() - length);
        int n = send(socket_, buffer, length, 0);
        if (n == -1) {
            perror("Client: failed to send data to server");
            ret = Status::ERROR;
            break;
        }
        length -= n;
    }

    return ret;
}

} /* namespace fighttrack */
