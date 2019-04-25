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
#include <chrono>
#include <gsl/gsl>

using namespace std::chrono_literals;

/**************************************************************************************/

/** Helper function to transform an integer sequence into a initializer list */
template<class T, T... I>
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
      notify_fd_{ 0 },
      listener_thread_{},
      mutex_{},
      rx_queue_{},
      client_ids_{},
      clients_{}
{
}

int Server::Initialize(uint16_t port)
{
    int ret = 0;
    int err = 0;

    /* Create master socket */
    listen_sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock_ == -1) {
        perror("Failed to create socket");
        return ret = -1;
    }
    auto _close_listen_socket = gsl::finally([&] {
        if (ret != 0)
            close(listen_sock_);
    });

    { /* Make file descriptor non-blocking */
        int flags = fcntl(listen_sock_, F_GETFL);
        flags |= O_NONBLOCK;
        err = fcntl(listen_sock_, F_SETFL, flags);
        if (err == -1) {
            perror("Failed to set socket control flags");
            return ret = -1;
        }
    }

    { /* Set socket to reuse address */
        bool val = true;
        err = setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int));
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

    err = bind(listen_sock_, (struct sockaddr*) &server_addr, sizeof(server_addr));
    if (err == -1) {
        perror("Failed to bind socket");
        return ret = -1;
    }

    err = listen(listen_sock_, kMaxClients);
    if (err == -1) {
        perror("Failed to listen socket");
        return ret = -1;
    }

    /* Create a event file descriptor for threads communication */
    notify_fd_ = eventfd(0, EFD_NONBLOCK);
    if (notify_fd_ == -1) {
        perror("Failed to create thread event fd");
        return ret = -1;
    }

    /* Create the event poll */
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1) {
        perror("Failed to create epoll");
        return ret = -1;
    }
    auto _close_epoll_fd = gsl::finally([&] {
        if (ret != 0)
            close(epoll_fd_);
    });

    { /* Add master socket to event poll */
        struct epoll_event event;
        event.events = EPOLLIN;
        event.data.fd = listen_sock_;
        err = epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_sock_, &event);
        if (err == -1) {
            perror("Failed to add socket to epoll");
            return ret = -1;
        }
    }

    { /* Add Thread Event FD to event poll */
        struct epoll_event event;
        event.events = EPOLLIN;
        event.data.fd = notify_fd_;
        err = epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, notify_fd_, &event);
        if (err == -1) {
            perror("Failed to add thread event fd to epoll");
            return ret = -1;
        }
    }

    /* Reset deque of client IDs */
    client_ids_ = make_initializer_list(std::make_integer_sequence<int, kMaxClients>{});
    /* Create event listener thread */
    listener_thread_ = std::thread(&Server::EventHandler, this);

    /* Server socket initialized */
    initialized_ = true;
    printf("Server: terminated\n");

    return ret = 0;
}

/**************************************************************************************/

void Server::Terminate()
{
    if (!initialized_)
        return;

    /* Trigger the listener thread to terminate */
    uint64_t notify = 1;
    if (write(notify_fd_, &notify, sizeof(notify)) == -1) {
        perror("Failed to send terminate signal to listener thread");
        fflush(stderr);
    }
    listener_thread_.join();

    /* Close sockets and file descriptors */
    close(notify_fd_);
    for (auto& client : clients_) {
        close(client.second.sock);
    }
    close(listen_sock_);
    close(epoll_fd_);

    /* Clear clients saved data */
    clients_.clear();
    client_ids_.clear();
    decltype(rx_queue_) tmp_queue;
    rx_queue_.swap(tmp_queue);

    initialized_ = false;
    printf("Server: terminated\n");
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
        constexpr int kTimeoutMs = std::chrono::milliseconds(10s).count();
        int event_num = epoll_wait(epoll_fd_, events, sizeof(events), kTimeoutMs);
        if (event_num == -1) {
            perror("Failed polling events");
            return;
        }
        else if (event_num == 0) {
            printf("Server: epoll timeout\n");
            continue;
        }

        for (int e = 0; e < event_num; ++e) {
            /* Thread notifications */
            if (events[e].data.fd == notify_fd_) {
                uint64_t notify;
                int n = read(notify_fd_, &notify, sizeof(notify));
                if (n == -1) {
                    if (errno == EWOULDBLOCK) {
                        printf(
                            "Server: received unknown event on notify_fd, ignoring.\n");
                        continue;
                    }
                    perror("Failed to read thread notification code");
                    return;
                }
                /* Signal to terminate thread */
                if (notify == 1) {
                    printf("Server: request to terminate listener thread\n");
                    return;  // terminate
                }
                fprintf(stderr,
                        "Unknown thread notification code received: %lu, ignoring.\n",
                        notify);
                continue;
            }
            /* New client event */
            else if (events[e].data.fd == listen_sock_) {
                if (AddNewClient() < 0) {
                    fprintf(stderr, "Failed to add new client\n");
                    return;
                }
            }
            /* Clients IO events */
            else {
                if (HandleClientInput(events[e].data.fd) < 0) {
                    fprintf(stderr, "Failed to handle client input\n");
                    return;
                }
            }
        }
    }
}

/**************************************************************************************/

int Server::AddNewClient()
{
    int ret = 0;
    int err = 0;

    if (clients_.size() >= kMaxClients) {
        printf("Server: dismissing new client, maximum (%zu) reached.\n", kMaxClients);
        // There's no way to refuse directly, so accept and close immediatly
        struct sockaddr client_addr;
        socklen_t client_len;
        int client_sock = accept(listen_sock_, &client_addr, &client_len);
        if (client_sock != -1) {
            close(client_sock);
        }
        return ret = 1;
    }

    const int client_id = client_ids_.front();
    socklen_t client_len;
    ClientInfo client;
    client.sock = accept(listen_sock_, (struct sockaddr*) &client.addr, &client_len);
    if (client.sock == -1) {
        perror("Failed to accept a new connection");
        return ret = 2;
    }

    std::lock_guard<std::mutex> lock{ mutex_ };
    /* Cache new client */
    clients_[client_id] = client;
    client_ids_.pop_front();

    auto _discard_client = gsl::finally([&] {
        if (ret < 0) {
            close(client.sock);
            clients_.erase(client_id);
            client_ids_.push_front(client_id);
        }
    });

    /* Add client fd to event poll */
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = client.sock;
    err = epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client.sock, &event);
    if (err == -1) {
        perror("Failed to add client to epoll");
        return ret = -1;
    }

    auto _remove_from_epoll = gsl::finally([&] {
        if (ret < 0) {
            err = epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client.sock, nullptr);
            if (err == -1) {
                perror("Failed to remove discarded client from epoll");
            }
        }
    });

    printf("Server: got connection from %s port %d\n", inet_ntoa(client.addr.sin_addr),
           ntohs(client.addr.sin_port));

    /* Send Hello message */
    char buffer[100];
    size_t len = snprintf(buffer, sizeof(buffer),
                          "Hello from server, you are the client %d", client_id);
    int n = send(client.sock, buffer, len, 0);
    if (n == -1) {
        perror("Failed to send data to client");
        return ret = -2;
    }

    return 0;
}

/**************************************************************************************/

int Server::Transmit(ClientData client_data)
{
    std::lock_guard<std::mutex> lock{ mutex_ };
    auto client_it = clients_.find(client_data.id);
    if (client_it == clients_.end()) {
        fprintf(stderr, "Failed to send data to client %d: client id not found\n",
                client_data.id);
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

/**************************************************************************************/

int Server::HandleClientInput(int client_sock)
{
    int ret = 0;

    while (true) {
        /* Try to read for incoming data */
        char buffer[2000];
        int n = recv(client_sock, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);

        /* Check for error */
        if (n == -1) {
            /* No more data available */
            if (errno == EWOULDBLOCK) {
                break;
            }
            perror("Failed to read data from client");
            ret = -1;
            break;
        }

        /* Search for client in cache */
        auto client_it = clients_.begin();
        for (; client_it != clients_.end(); ++client_it) {
            if (client_it->second.sock == client_sock) {
                break;
            }
        }
        if (client_it == clients_.end()) {
            fprintf(stderr, "Fatal error, client sock %d not cached\n", client_sock);
            ret = -1;
            break;
        }
        int client_id = client_it->first;

        /* Check for connection closed */
        if (n == 0) {
            { /* Forget client */
                std::lock_guard<std::mutex> lock{ mutex_ };
                int err = epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_sock, nullptr);
                if (err == -1) {
                    perror("Failed to remove closed client from epoll");
                }
                close(client_sock);
                clients_.erase(client_it);
            }
            /* Restore client id */
            client_ids_.emplace_back(client_id);
            std::sort(client_ids_.begin(), client_ids_.end());

            printf("Server: client %d closed connection.\n", client_id);
            break;
        }
        /* That is a new message */
        else {
            buffer[n] = '\0';
            /* Saved it to received data queue */
            std::lock_guard<std::mutex> lock{ mutex_ };
            rx_queue_.emplace(
                ClientData{ .id = client_id, .buffer = { buffer, (size_t) n } });

            printf("Server: received from client (%d): '%s'\n", client_id, buffer);
        }
    }

    return ret;
}

/**************************************************************************************/

std::queue<Server::ClientData> Server::GetRxQueue()
{
    std::lock_guard<std::mutex> lock{ mutex_ };
    // Return rx queued data and clear internal queue
    decltype(rx_queue_) ret;
    rx_queue_.swap(ret);
    return std::move(ret);
}

} /* namespace fighttrack */

int main()
{
    fighttrack::Server server;
    server.Initialize(9124);
    std::this_thread::sleep_for(8s);
    server.Transmit({ .id = 0, .buffer = { "Bye bye client!" } });
    std::this_thread::sleep_for(2s);
    server.Terminate();
}
