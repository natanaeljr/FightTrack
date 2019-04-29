/**
 * \file server_socket.h
 * \brief Server Socket API.
 */

#include <cstdint>
#include <map>
#include <deque>
#include <queue>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <future>
#include <condition_variable>

#include <netinet/in.h>

#include "safe/lockable.h"

/**************************************************************************************/

namespace fighttrack {

class ServerSocket {
   public:
    /**********************************************************************************/
    /* [CON/DES]STRUCTORS */
    /**********************************************************************************/
    /**
     * \brief Construct a new Server object.
     */
    ServerSocket();

    /**
     * \brief Destroy the Server object.
     */
    ~ServerSocket();

    /**********************************************************************************/
    /* CONTROL */
    /**********************************************************************************/
    /**
     * \brief Create and configure the server socket.
     * \param port Server port.
     * \return 0 on sucess, negative if error.
     */
    int Initialize(const uint16_t port);

    /**
     * \brief Close all open connections and clean resources.
     */
    void Terminate();

    /**********************************************************************************/
    /* MESSAGING */
    /**********************************************************************************/
    /**
     * Transmit status
     */
    enum class TxStatus {
        ERROR = -1,   //!< An error ocurred
        SUCCESS = 0,  //!< Operation sucessful
    };

    /**
     * Receive status
     */
    enum class RxStatus {
        NEW_DATA = 0,      //!< New data available
        CONNECTED = 1,     //!< New client connected
        DISCONNECTED = 2,  //!< Client disconnected
    };

    /**
     * Transmit message
     */
    struct TxMessage {
        std::vector<int> client_ids;  //!< Client IDs to send this message.
        std::string buffer;           //!< Message buffer
    };

    /**
     * Received message
     */
    struct RxMessage {
        int client_id;       //!< Client ID.
        RxStatus status;     //!< Receive message status, should check first.
        std::string buffer;  //!< Received data, set when status is NEW_DATA.
    };

    /**
     * \brief  Get queue of received messages from clients.
     * \return Queue of messages.
     */
    std::queue<RxMessage> GetMessages();

    /**
     * \brief  Send data to clients.
     * \param  message Message to send.
     * \return Future trasmission status.
     */
    std::future<TxStatus> Transmit(TxMessage message);

   private:
    /**********************************************************************************/
    /* HELPERS */
    /**********************************************************************************/

    /**
     * \brief Thread runnable; Handle events of new connections and clients rx.
     */
    void RxEventHandler();

    /**
     * \brief Thread runnable; Handle events transmission.
     */
    void TxEventHandler();

    /**
     * \brief Accept a connection from listener socket and add it to the client list.
     * \return 0 on success, positive if failure, negative if fatal error.
     */
    int AddNewClient();

    /**
     * \brief  Handle client socket input. Try to read data and queue it.
     * \param  client_sock  Client socket.
     * \return 0 on success, positive if failure, negative if fatal error.
     */
    int HandleClientInput(int client_sock);

   private:
    /**********************************************************************************/
    /* MEMBER VARIABLES */
    /**********************************************************************************/
    //! Flag indicating if socket is initialized
    bool initialized_;
    //! Master socket which listens for new connections
    int listen_sock_;
    //! Event poll file descriptor
    int epoll_fd_;
    //! File descriptor used for notifying the RX handling thread
    int rx_thread_event_fd_;

    //! RX event handling thread
    std::thread rx_thread_;
    //! TX event handling thread
    std::thread tx_thread_;

    /**
     * Events for TX thread
     */
    enum class TxThreadEvent {
        NONE = 0,
        NEW_DATA = 1,
        TERMINATE = 2,
    };

    /* Client connection information */
    struct ClientInfo {
        int sock;
        struct sockaddr_in addr;
    };

    /** Common thread-shared data.
     * (access by API client thread, RX thread and TX thread) */
    struct CommonData {
        //! Deque of available client IDs (sorted)
        std::deque<int> available_ids;
        //! Map of clients; key: client ID; element: client info
        std::map<int, ClientInfo> clients;
    };
    //! Cached common data, acessed by API client thread, RX thread and TX thread
    safe::Lockable<CommonData, std::shared_timed_mutex> common_data_;

    /** RX thread-shared data.
     * (access by API client thread and RX thread) */
    struct RxData {
        //! Queue of incomming messages or events from clients
        std::queue<RxMessage> rx_queue;
    };
    //! Cached RX data, acessed by API client thread and RX thread
    safe::Lockable<RxData, std::shared_timed_mutex> rx_data_;

    /** TX thread-shared data
     * (access by API client thread and TX thread) */
    struct TxData {
        struct TxFutureMsg {
            std::promise<TxStatus> promise;  //!< Promise is set when message was sent
            TxMessage message;               //!< Message content
        };
        //!< Queues of outgoing messages for clients
        std::queue<TxFutureMsg> tx_queue;
        //! TX thread event
        TxThreadEvent tx_event;
    };
    //! Cached TX data, acessed by API client thread and TX thread
    safe::Lockable<TxData, std::shared_timed_mutex> tx_data_;

    //! TX thread event notifier
    std::condition_variable_any tx_thread_notify_;

    /**********************************************************************************/
    /* ALIASES */
    /**********************************************************************************/
    //! Alias to the Read accessor type
    template<typename ValueType>
    auto ReadAccess(typename safe::Lockable<ValueType, std::shared_timed_mutex>& value)
    {
        return typename safe::Lockable<
            ValueType, std::shared_timed_mutex>::template ReadAccess<std::shared_lock>{
            value
        };
    }
    //! Alias to the Write accessor type
    template<typename ValueType>
    auto WriteAccess(typename safe::Lockable<ValueType, std::shared_timed_mutex>& value)
    {
        return typename safe::Lockable<
            ValueType, std::shared_timed_mutex>::template WriteAccess<std::unique_lock>{
            value
        };
    }
};

} /* namespace fighttrack */
