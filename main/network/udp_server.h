#pragma once

#include <cstdint>
#include <functional>
#include <lwip/sockets.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <vector>
#include <esp_timer.h>


enum class MessageType : uint8_t {
    DATA = 0,
    DISCONNECT = 1
};

struct MessageHeader {
    MessageType type;
    uint8_t reserved[3];
};

struct ClientInfo {
    sockaddr_in addr;
    
    ClientInfo(const sockaddr_in& a) : addr(a) {}
};

class UDPServer {
public:
    using DataCallback = std::function<void(const uint8_t* data, size_t len, const sockaddr_in& client_addr)>;

    static UDPServer& GetInstance();

    // Delete copy constructor and assignment operator
    UDPServer(const UDPServer&) = delete;
    UDPServer& operator=(const UDPServer&) = delete;

    bool Initialize(uint16_t port);
    void Deinitialize();

    bool HasClients() const { return !clients_.empty(); }

    bool SendToAllClients(const uint8_t* data, size_t len);

    bool SendTo(const uint8_t* data, size_t len, const sockaddr_in& dest_addr);
    
    void SetReceiveCallback(DataCallback callback) { data_callback_ = callback; }

private:
    UDPServer() = default;
    ~UDPServer();

    static void HandleUDPTask(void* arg);
    
    void HandleMessage(const uint8_t* data, size_t len, const sockaddr_in& client_addr);
    
    void RemoveClient(const sockaddr_in& addr);

    int socket_fd_ = -1;
    uint16_t port_ = 0;
    bool should_stop_ = false;
    TaskHandle_t udp_task_ = nullptr;

    std::vector<ClientInfo> clients_;
    
    static UDPServer* instance_;
    DataCallback data_callback_;
}; 