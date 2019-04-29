/**
 * \file client_socket.h
 * \brief Client Socket API.
 */

#include <cstdint>
#include <string>
#include <queue>

/**************************************************************************************/

namespace fighttrack {

class ClientSocket {
   public:
    /**
     * \brief Construct a new Client Socket object
     */
    ClientSocket();

    /**
     * \brief Destroy the Client Socket object
     */
    ~ClientSocket();

    /**
     * \brief Create and configure the socket.
     * \param server_addr Server address.
     * \param port Server port.
     * \return 0 on success, negative if error.
     */
    int Initialize(const std::string& server_addr, const uint16_t port);

    /**
     * \brief Close open connection and clean resources.
     */
    void Terminate();

    /* Connection status */
    enum class Status {
        ERROR = -1,
        SUCCESS = 0,
        DISCONNECTED = 2,
    };

    struct RecvData {
        Status status;
        std::queue<std::string> queue;
    };

    /**
     * \brief Try to read for incoming data. (synchronous)
     * \return Status and a queue of read data, if any.
     */
    RecvData Receive();

    /**
     * \brief Send data to server.
     * \param data Data to send.
     * \return 0 on success, negative if error.
     */
    Status Transmit(std::string data);

   private:
    //! Flag indicating if socket is initialized
    bool initialized_;
    //! Socket connected to server
    int socket_;
};

} /* namespace fighttrack */