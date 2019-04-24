/**
 * \file server.h
 * \brief Server declearation.
 */

#include <cstdint>
#include <map>
#include <deque>
#include <thread>
#include <mutex>
#include <queue>

#include <netinet/in.h>

/**************************************************************************************/

namespace fighttrack {

class Server {
   public:
    /**
     * \brief Construct a new Server object>
     */
    Server();

    /**
     * \brief Destroy the Server object.
     */
    ~Server() = default;

    /**
     * \brief Create and configure the server socket.
     * \param port Server port.
     * \return 0 on sucess, negative if error.
     */
    int Initialize(const uint16_t port);

    /**
     * \brief Close all open connections and clean resources.
     * \return 0 on sucess, negative if error.
     */
    int Terminate();

    /* Client's received data */
    struct ClientData {
        int id;
        std::string buffer;
    };

    /**
     * \brief  Get queue of received messages from clients.
     * \return Queue of messages. Might be empty if there are no new messages.
     */
    std::queue<ClientData> GetRxQueue();

    /**
     * \brief  Send data to clients.
     * \param  client
     * \return 0 on succes, negative if error.
     */
    int Transmit(ClientData client_data);

   private:
    /**
     * \brief Thread runnable; Handle events of new connections and clients rx/tx.
     */
    void EventHandler();

    /* TODO */
    void AddNewClient();
    void Received();

    /* Client connection information */
    struct ClientInfo {
        int sock;
        struct sockaddr_in addr;
    };

    //! Flag indicating if socket is initialized
    bool initialized_;
    //! Master socket which listens for new connections
    int listen_sock_;
    //! Event poll file descriptor
    int epoll_fd_;
    //! File descriptor used for notifying the event handling thread
    int thread_event_fd_;
    //! Event handling thread
    std::thread listener_thread_;
    //! Mutex to protect read/write from/to private members
    std::mutex mutex_;
    //! Queue of incomming messages from clients
    std::queue<ClientData> rx_queue_;

    //! Deque of available client IDs (sorted)
    std::deque<int> client_ids_;
    //! Map of clients. Key: client ID, Element: client Info
    std::map<int, ClientInfo> clients_;
};

} /* namespace fighttrack */
