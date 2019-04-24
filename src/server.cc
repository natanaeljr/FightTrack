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
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include <utility>
#include <gsl/gsl>

/**************************************************************************************/

/** Helper function to transform an integer sequence into a initializer list */
template <class T, T... I>
static constexpr auto make_initializer_list(std::integer_sequence<T, I...>)
{
    return std::initializer_list<T>{ I... };
}

/**************************************************************************************/

namespace fighttrack {

static constexpr size_t kMaxClients = 4;

/**************************************************************************************/

Server::Server()
    : initialized_{ false },
      listen_sock_{ 0 },
      epoll_fd_{ 0 },
      thread_event_fd_{ 0 },
      listener_thread_{},
      mutex_{},
      rx_queue_{},
      client_ids_{ make_initializer_list(std::make_integer_sequence<int, kMaxClients>{}) },
      clients_{}
{
}

int Server::Initialize(uint16_t port)
{
    int ret = 0;
    int err = 0;

    /* Create master socket */
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == -1) {
        perror("Failed to create socket");
        return ret = -1;
    }
    auto _close_listen_socket = gsl::finally([&] {
        if (ret != 0)
            close(listen_sock);
    });

    { /* Make file descriptor non-blocking */
        int flags = fcntl(listen_sock, F_GETFL);
        flags |= O_NONBLOCK;
        err = fcntl(listen_sock, F_SETFL, flags);
        if (err == -1) {
            perror("Failed to set socket control flags");
            return ret = -1;
        }
    }

    { /* Set socket to reuse address */
        bool val = true;
        err = setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int));
        if (err == -1) {
            perror("Failed to set socket options");
            return ret = -1;
        }
    }

    /* Configure server socket */
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    err = bind(listen_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (err == -1) {
        perror("Failed to bind socket");
        return ret = -1;
    }

    err = listen(listen_sock, kMaxClients);
    if (err == -1) {
        perror("Failed to listen socket");
        return ret = -1;
    }

    /* Create a event file descriptor for threads communication */
    thread_event_fd_ = eventfd(0, EFD_NONBLOCK);
    if (thread_event_fd_ == -1) {
        perror("Failed to create thread event fd");
        return ret = -1;
    }

    /* Create the event poll */
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("Failed to create epoll");
        return ret = -1;
    }
    auto _close_epoll_fd = gsl::finally([&] {
        if (ret != 0)
            close(epoll_fd);
    });

    { /* Add master socket to event poll */
        struct epoll_event event;
        event.events = EPOLLIN;
        event.data.fd = listen_sock;
        err = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_sock, &event);
        if (err == -1) {
            perror("Failed to add socket to epoll");
            return ret = -1;
        }
    }

    { /* Add Thread Event FD to event poll */
        struct epoll_event event;
        event.events = EPOLLIN;
        event.data.fd = thread_event_fd_;
        err = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, thread_event_fd_, &event);
        if (err == -1) {
            perror("Failed to add thread event fd to epoll");
            return ret = -1;
        }
    }

    /* Server socket initialized */
    initialized_ = true;
    /* Create event listener thread */
    listener_thread_ = std::thread(&Server::EventHandler, this);

    return ret = 0;
}

/**************************************************************************************/

int Server::Terminate()
{
    if (!initialized_)
        return 0;

    /* Trigger the event thread to terminate */
    int event = 1;
    if (write(thread_event_fd_, &event, 1) == -1) {
        perror("Failed to send terminate signal to server event thread");
        fflush(stderr);
    }
    listener_thread_.join();

    /* Close sockets and FDs */
    for (auto& client : clients_) {
        close(client.second.sock);
    }
    close(epoll_fd_);
    close(listen_sock_);

    initialized_ = false;
    return 0;
}

/**************************************************************************************/

void Server::EventHandler()
{
    int err = 0;
    constexpr size_t kMaxEvents = 10;
    struct epoll_event events[kMaxEvents];

    /* Listen for client events (new connections and incoming data) */
    while (true) {
        printf("Server: Polling for events..\n");
        fflush(stdout);

        /* Wait for an event from master socket or client sockets */
        constexpr int kTimeoutMs = 1'000;
        int event_num = epoll_wait(epoll_fd_, events, sizeof(events), kTimeoutMs);
        if (event_num == -1) {
            perror("Failed polling events");
            return;
        } else if (event_num == 0) {
            printf("Epoll timeout\n");
            continue;
        }

        for (int e = 0; e < event_num; ++e) {
            std::lock_guard<std::mutex> lock{ mutex_ };
            /* Thread notifications */
            if (events[e].data.fd == thread_event_fd_) {
                int event;
                int n = read(thread_event_fd_, &event, 1);
                if (n != 1) {
                    perror("Failed to read thread event code");
                    return;
                }
                /* Signal to terminate thread */
                if (event == 1) {
                    return;  // terminate
                } else {
                    fprintf(stderr, "Unknown thread event code received: %d, ignoring.\n", event);
                    continue;
                }
            }
            /* New client event */
            else if (events[e].data.fd == listen_sock_) {
                if (clients_.size() >= kMaxClients) {
                    printf("Dismissing new client, max reached.\n");
                    // There's no way to refuse directly, so accept and close immediatly
                    struct sockaddr client_addr;
                    socklen_t client_len;
                    int client_sock = accept(listen_sock_, &client_addr, &client_len);
                    if (client_sock != -1) {
                        close(client_sock);
                    }
                    continue;
                }

                const int client_id = client_ids_.front();
                ClientInfo client;
                socklen_t client_len;
                client.sock = accept(listen_sock_, (struct sockaddr*)&client.addr, &client_len);
                if (client.sock == -1) {
                    perror("Failed to accept connection");
                    return -1;
                }
                /* Cache new client */
                clients_[client_id] = client;
                client_ids_.pop_front();

                /* Add client fd to event poll */
                struct epoll_event event;
                event.events = EPOLLIN;
                event.data.fd = client.sock;
                err = epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client.sock, &event);
                if (err == -1) {
                    perror("Failed to add client to epoll");
                    return -1;
                }

                printf("Server: got connection from %s port %d\n", inet_ntoa(client.addr.sin_addr),
                       ntohs(client.addr.sin_port));

                /* Send Hello message */
                char buffer[100];
                size_t len = snprintf(buffer, sizeof(buffer),
                                      "Hello from server, you are the client %d", client_id + 1);
                err = Transmit({ .id = client_id, .buffer = buffer });
                if (err == -1) {
                    fprintf(stderr, "Failed to send hello-message to client\n");
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
                    auto client_it = clients_.begin();
                    for (; client_it != clients_.end(); ++client_it) {
                        if (client_it->second.sock == client_sock)
                            break;
                    }
                    if (client_it == clients_.end()) {
                        fprintf(stderr, "Fatal error, client sock %d not cached\n", client_sock);
                        return -1;
                    }
                    /* Close socket */
                    close(client_sock);
                    clients_.erase(client_it);
                    /* Restore client id */
                    auto client_id = client_it->first;
                    client_ids_.emplace_back(client_id);
                    std::sort(client_ids_.begin(), client_ids_.end());

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
}

/**************************************************************************************/

std::queue<Server::ClientData> Server::GetRxQueue()
{
    std::lock_guard<std::mutex> lock{ mutex_ };
    // Return rx queued data and clean internal queue
    std::queue<ClientData> ret;
    rx_queue_.swap(ret);
    return std::move(ret);
}

/**************************************************************************************/

int Server::Transmit(ClientData client_data)
{
    std::lock_guard<std::mutex> lock{ mutex_ };
    auto client_it = clients_.find(client_data.id);
    if (client_it == clients_.end()) {
        fprintf(stderr, "Failed to send data to client %d: client id not found\n", client_data.id);
        return -1;
    }

    auto& client_sock = client_it->second.sock;

    int n = send(client_sock, client_data.buffer.data(), client_data.buffer.length(), 0);
    if (n == -1) {
        perror("Failed to send data to client");
        return -1;
    }

    return 0;
}

} /* namespace fighttrack */

int main()
{
    return fighttrack::server(9124);
}
