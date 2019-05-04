/**
 * \file server_socket.cc
 * \brief Server Socket implementation.
 */

#include "fighttrack/server_socket.h"

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
ServerSocket::ServerSocket()
    : initialized_{ false },
      listen_sock_{ 0 },
      epoll_fd_{ 0 },
      rx_thread_event_fd_{ 0 },
      rx_thread_{},
      tx_thread_{},
      common_data_{},
      rx_data_{},
      tx_data_{},
      tx_thread_notify_{}
{
}

/**************************************************************************************/
ServerSocket::~ServerSocket()
{
    if (initialized_)
        Terminate();
}

/**************************************************************************************/
int ServerSocket::Initialize(uint16_t port)
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

    /* Create a event file descriptor for communicating with RX thread */
    rx_thread_event_fd_ = eventfd(0, EFD_NONBLOCK);
    if (rx_thread_event_fd_ == -1) {
        perror("Failed to create RX thread event FD");
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
        event.data.fd = rx_thread_event_fd_;
        err = epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, rx_thread_event_fd_, &event);
        if (err == -1) {
            perror("Failed to add thread event fd to epoll");
            return ret = -1;
        }
    }

    /* Server socket initialized */
    initialized_ = true;
    printf("Server: initialized\n");

    /* Reset deque of available client IDs */
    common_data_.unsafe().available_ids =
        make_initializer_list(std::make_integer_sequence<int, kMaxClients>{});
    /* Create event handling threads */
    rx_thread_ = std::thread(&ServerSocket::RxEventHandler, this);
    tx_thread_ = std::thread(&ServerSocket::TxEventHandler, this);

    return ret;
}

/**************************************************************************************/
void ServerSocket::Terminate()
{
    if (!initialized_)
        return;

    /* Trigger the RX thread to terminate */
    uint64_t notify = 1;
    if (write(rx_thread_event_fd_, &notify, sizeof(notify)) <= 0) {
        perror("Failed to send terminate signal to RX thread");
        fflush(stderr);
    }

    /* Trigger the TX thread to terminate */
    {
        auto access = WriteAccess(tx_data_);
        access->tx_event = TxThreadEvent::TERMINATE;
    }
    tx_thread_notify_.notify_one();

    /* Wait for the threads to terminate */
    rx_thread_.join();
    tx_thread_.join();

    /* Clean common resources */
    {
        // we can access directly since there is no other thread running at this point
        auto& access = common_data_.unsafe();

        /* Close sockets and file descriptors */
        close(epoll_fd_);
        close(rx_thread_event_fd_);
        for (auto& client : access.clients) {
            close(client.second.sock);
        }
        close(listen_sock_);

        /* Clear clients saved data */
        access.clients.clear();
        access.available_ids.clear();
    }

    /* Clean RX resources */
    {
        // we can access directly since RX thread running at this point
        auto& access = rx_data_.unsafe();
        /* Clear RX queue */
        decltype(access.rx_queue) tmp_queue;
        access.rx_queue.swap(tmp_queue);
    }

    /* Clean TX resources */
    {
        // we can access directly since TX thread running at this point
        auto& access = tx_data_.unsafe();
        /* Clear TX queue */
        decltype(access.tx_queue) tmp_queue;
        access.tx_queue.swap(tmp_queue);
    }

    initialized_ = false;
    printf("Server: terminated\n");
}

/**************************************************************************************/
void ServerSocket::RxEventHandler()
{
    int err = 0;
    constexpr size_t kMaxEvents = 10;
    struct epoll_event events[kMaxEvents];

    printf("Server: polling for events..\n");
    fflush(stdout);
    /* Listen for client events (new connections and incoming data) */
    while (true) {
        /* Wait for an event from master socket or client sockets */
        constexpr int kTimeoutMs = std::chrono::milliseconds(30s).count();
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
            if (events[e].data.fd == rx_thread_event_fd_) {
                uint64_t notify;
                int n = read(rx_thread_event_fd_, &notify, sizeof(notify));
                if (n == -1) {
                    if (errno == EWOULDBLOCK) {
                        /* Might've been a write-available event. Ignore. */
                        continue;
                    }
                    perror("Failed to read thread notification code");
                    return;
                }
                /* Signal to terminate thread */
                if (notify == 1) {
                    printf("Server: request to terminate RX thread\n");
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
int ServerSocket::AddNewClient()
{
    int ret = 0;
    int err = 0;

    if (ReadAccess(common_data_)->clients.size() >= kMaxClients) {
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

    // lock access to shared data, we are modifying it
    auto access = WriteAccess(common_data_);

    const int client_id = access->available_ids.front();
    socklen_t client_len;
    ClientInfo client;
    client.sock = accept(listen_sock_, (struct sockaddr*) &client.addr, &client_len);
    if (client.sock == -1) {
        perror("Failed to accept a new connection");
        return ret = 2;
    }

    /* Cache new client */
    access->clients[client_id] = client;
    access->available_ids.pop_front();

    auto _discard_client = gsl::finally([&] {
        if (ret < 0) {
            close(client.sock);
            access->clients.erase(client_id);
            access->available_ids.push_front(client_id);
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

    { /* Notify API client that a new client connected */
        auto access = WriteAccess(rx_data_);
        access->rx_queue.emplace(RxMessage{
            .client_id = client_id,
            .status = RxStatus::CONNECTED,
            .buffer = {},
        });
    }

    printf("Server: got connection from %s port %d\n", inet_ntoa(client.addr.sin_addr),
           ntohs(client.addr.sin_port));

    return 0;
}

/**************************************************************************************/
int ServerSocket::HandleClientInput(int client_sock)
{
    int ret = 0;

    while (true) {
        /* Try to read for incoming data */
        char buffer[4095];
        int n = recv(client_sock, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);

        /* Check for error */
        if (n == -1) {
            /* No more data available */
            if (errno == EWOULDBLOCK) {
                break;
            }
            else if (errno == ECONNRESET) {
                n = 0;  // proceed to connection close below
            }
            else {
                perror("Failed to read data from client");
                ret = -1;
                break;
            }
        }

        int client_id;
        { /* Search for client socket to get client ID */
            auto access = ReadAccess(common_data_);
            /* Search for client in cache */
            auto client_it = access->clients.begin();
            for (; client_it != access->clients.end(); ++client_it) {
                if (client_it->second.sock == client_sock) {
                    break;
                }
            }
            if (client_it == access->clients.end()) {
                fprintf(stderr, "Fatal error, client sock %d not cached\n", client_sock);
                ret = -1;
                break;
            }
            client_id = client_it->first;
        }

        /* Check for connection closed */
        if (n == 0) {
            /* Forget client */
            int err = epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_sock, nullptr);
            if (err == -1) {
                perror("Failed to remove closed client from epoll");
            }
            close(client_sock);

            { /* Remove from list of clients */
                auto access = WriteAccess(common_data_);
                access->clients.erase(client_id);
                /* Restore client id */
                access->available_ids.emplace_back(client_id);
                std::sort(access->available_ids.begin(), access->available_ids.end());
            }

            { /* Notify API client with a message */
                auto access = WriteAccess(rx_data_);
                access->rx_queue.emplace(RxMessage{
                    .client_id = client_id,
                    .status = RxStatus::DISCONNECTED,
                    .buffer = {},
                });
            }

            printf("Server: client %d closed connection.\n", client_id);
            break;
        }
        /* That is a new message */
        {
            {
                auto access = WriteAccess(rx_data_);
                /* Saved it to received data queue */
                access->rx_queue.emplace(RxMessage{
                    .client_id = client_id,
                    .status = RxStatus::NEW_DATA,
                    .buffer = { buffer, (size_t) n },
                });
            }
            printf("Server: received %d bytes from client %d\n", n, client_id);
        }
    }

    return ret;
}

/**************************************************************************************/
std::queue<ServerSocket::RxMessage> ServerSocket::GetMessages()
{
    auto access = WriteAccess(rx_data_);

    // Return rx queued data and clear internal queue
    decltype(access->rx_queue) ret;
    access->rx_queue.swap(ret);
    return std::move(ret);
}

/**************************************************************************************/
void ServerSocket::TxEventHandler()
{
    // Local TX queue
    std::queue<TxData::TxFutureMsg> tx_queue;

    while (true) {
        { /* Poll for new event */
            auto access = WriteAccess(tx_data_);
            // conditional variable
            tx_thread_notify_.wait(
                access.lock, [&] { return access->tx_event != TxThreadEvent::NONE; });
            /* Check for signal to terminate the thread */
            if (access->tx_event == TxThreadEvent::TERMINATE) {
                printf("Server: request to terminate TX thread\n");
                break;  // exit infity loop
            }
            access->tx_queue.swap(tx_queue);
        }

        /* Start sending new data */
        while (!tx_queue.empty()) {
            TxStatus status = TxStatus::SUCCESS;
            TxMessage& message = tx_queue.front().message;
            /* Send this message for all listed clients */
            for (auto client_id : message.client_ids) {
                using Iterator = decltype(std::declval<CommonData>().clients)::iterator;
                /* Search for client ID in map to get client socket */
                int client_sock;
                {
                    auto access = ReadAccess(common_data_);
                    auto client_it = access->clients.find(client_id);
                    if (client_it == access->clients.end()) {
                        fprintf(stderr,
                                "Failed to send data to client %d: client id not found\n",
                                client_id);
                        status = TxStatus::ERROR;
                        break;  // this message failed, skip remaining clients
                    }
                    client_sock = client_it->second.sock;
                }
                /* Send data to socket */
                for (size_t bytes_sent = 0; bytes_sent < message.buffer.length();) {
                    ssize_t n = send(client_sock, message.buffer.data() + bytes_sent,
                                     message.buffer.length() - bytes_sent, 0);
                    if (n == -1) {
                        perror("Failed to send data to client");
                        status = TxStatus::ERROR;
                        break;  // this message failed, skip remaining clients
                    }
                    bytes_sent += n;
                }
                /* If message failed once, skip remaining clients */
                if (status != TxStatus::SUCCESS) {
                    break;
                }
            }
            /* Message finished, set promise */
            tx_queue.front().promise.set_value(status);
            tx_queue.pop();
        }
    }
}

/**************************************************************************************/
std::future<ServerSocket::TxStatus> ServerSocket::Transmit(TxMessage message)
{
    std::promise<TxStatus> promise;
    auto future = promise.get_future();

    if (message.client_ids.empty() || message.buffer.empty()) {
        promise.set_value(TxStatus::ERROR);
        return future;
    }

    /* Enqueue messages and notify TX thread that there is new data to be send */
    {
        auto access = WriteAccess(tx_data_);
        access->tx_queue.emplace(TxData::TxFutureMsg{
            .promise = std::move(promise),
            .message = std::move(message),
        });
        access->tx_event = TxThreadEvent::NEW_DATA;
    }
    tx_thread_notify_.notify_one();

    return future;
}

} /* namespace fighttrack */
