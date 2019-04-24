/**
 * \file server.cc
 * \brief Server definition.
 */

#include "fighttrack/server.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include <map>
#include <deque>
#include <utility>
#include <gsl/gsl>

/**************************************************************************************/

/** Helper function to transform an interger sequence into a initializer list */
template<class T, T... I>
static constexpr auto make_initializer_list(std::integer_sequence<T, I...>)
{
    return std::initializer_list<T>{ I... };
}

/**************************************************************************************/

namespace fighttrack {

static constexpr size_t kMaxClients = 4;

/**************************************************************************************/

int server(uint16_t port)
{
    int err = 0;

    auto _server_finished = gsl::finally([] { printf("Server finished\n"); });

    /* Create master socket */
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == -1) {
        perror("Failed to create socket");
        return -1;
    }
    auto _close_listen_socket = gsl::finally([&] { close(listen_sock); });

    { /* Make file descriptor non-blocking */
        int flags = fcntl(listen_sock, F_GETFL);
        flags |= O_NONBLOCK;
        err = fcntl(listen_sock, F_SETFL, flags);
        if (err == -1) {
            perror("Failed to set socket control flags");
            return -1;
        }
    }

    { /* Set socket to reuse address */
        bool val = true;
        err = setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int));
        if (err == -1) {
            perror("Failed to set socket options");
            return -1;
        }
    }

    /* Configure server socket */
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    err = bind(listen_sock, (struct sockaddr*) &server_addr, sizeof(server_addr));
    if (err == -1) {
        perror("Failed to bind socket");
        return -1;
    }

    err = listen(listen_sock, kMaxClients);
    if (err == -1) {
        perror("Failed to listen socket");
        return -1;
    }

    /* Create the event poll */
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("Failed to create epoll");
        return -1;
    }
    auto _close_epoll_fd = gsl::finally([&] { close(epoll_fd); });

    constexpr size_t kMaxEvents = 10;
    struct epoll_event events[kMaxEvents];

    { /* Add master socket to event poll */
        struct epoll_event event;
        event.events = EPOLLIN;
        event.data.fd = listen_sock;
        err = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_sock, &event);
        if (err == -1) {
            perror("Failed to add socket to epoll");
            return -1;
        }
    }

    /* Client connection information */
    struct client_info {
        int sock;
        struct sockaddr_in addr;
    };

    //! Deque of available client IDs (sorted)
    std::deque<int> client_ids =
        make_initializer_list(std::make_integer_sequence<int, kMaxClients>{});
    //! Map from ID to client Info
    std::map<int, struct client_info> clients;

    auto _close_client_socks = gsl::finally([&] {
        for (auto& client : clients) {
            close(client.second.sock);
        }
    });

    /* Listen for Client events (new connections and incoming data) */
    while (true) {
        printf("Polling for events..\n");
        fflush(stdout);

        /* Wait for an event from master socket or client sockets */
        constexpr int kTimeoutMs = 10'000;
        int event_num = epoll_wait(epoll_fd, events, sizeof(events), kTimeoutMs);
        if (event_num == -1) {
            printf("Failed polling events");
            return -1;
        }
        else if (event_num == 0) {
            printf("Epoll timeout\n");
            continue;
        }

        for (int e = 0; e < event_num; ++e) {
            /* New client event */
            if (events[e].data.fd == listen_sock) {
                if (clients.size() >= kMaxClients) {
                    printf("Dismissing new client, max reached.\n");
                    // There's no way to refuse directly, so accept and close immediatly
                    struct sockaddr client_addr;
                    socklen_t client_len;
                    int client_sock = accept(listen_sock, &client_addr, &client_len);
                    if (client_sock != -1) {
                        close(client_sock);
                    }
                    continue;
                }

                const int client_id = client_ids.front();
                struct client_info client;
                socklen_t client_len;
                client.sock =
                    accept(listen_sock, (struct sockaddr*) &client.addr, &client_len);
                if (client.sock == -1) {
                    perror("Failed to accept connection");
                    return -1;
                }
                /* Cache new client */
                clients[client_id] = client;
                client_ids.pop_front();

                /* Add client fd to event poll */
                struct epoll_event event;
                event.events = EPOLLIN;
                event.data.fd = client.sock;
                err = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client.sock, &event);
                if (err == -1) {
                    perror("Failed to add client to epoll");
                    return -1;
                }

                printf("Server: got connection from %s port %d\n",
                       inet_ntoa(client.addr.sin_addr), ntohs(client.addr.sin_port));

                /* Send Hello message */
                char buffer[100];
                size_t len =
                    snprintf(buffer, sizeof(buffer),
                             "Hello from server, you are the client %d", client_id + 1);
                int n = send(client.sock, buffer, len, 0);
                if (n == -1) {
                    perror("Failed to send data to client");
                    return -1;
                }
            }
            /* Clients IO events */
            else {
                auto client_sock = events[e].data.fd;

                /* Try to read incoming data */
                char buffer[256] = { 0 };
                int n = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
                /* Check for connection closed */
                if (n == 0) {
                    /* Search for client in cache */
                    auto client_it = clients.begin();
                    for (; client_it != clients.end(); ++client_it) {
                        if (client_it->second.sock == client_sock)
                            break;
                    }
                    if (client_it == clients.end()) {
                        fprintf(stderr, "Fatal error, client sock %d not cached",
                                client_sock);
                        return -1;
                    }
                    /* Close socket */
                    close(client_sock);
                    clients.erase(client_it);
                    /* Restore client id */
                    auto client_id = client_it->first;
                    client_ids.emplace_back(client_id);
                    std::sort(client_ids.begin(), client_ids.end());

                    printf("Client %d connection closed.\n", client_id + 1);
                }
                /* Check for error */
                else if (n == -1) {
                    perror("Failed to read data from client");
                    return -1;
                }
                /* There is a new message */
                else {
                    buffer[n] = '\0';
                    printf("Received: '%s'\n", buffer);
                }
            }
        }
    }

    return 0;
}

} /* namespace fighttrack */

int main()
{
    return fighttrack::server(9124);
}
