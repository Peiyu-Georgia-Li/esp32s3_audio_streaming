#include "udp_server.h"
#include <esp_log.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "UDPServer";

UDPServer* UDPServer::instance_ = nullptr;

UDPServer& UDPServer::GetInstance() {
    if (!instance_) {
        instance_ = new UDPServer();
    }
    return *instance_;
}

UDPServer::~UDPServer() {
    Deinitialize();
}

bool UDPServer::Initialize(uint16_t port) {
    port_ = port;

    socket_fd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_fd_ < 0) {
        ESP_LOGE(TAG, "Failed to create socket: errno %d", errno);
        return false;
    }

    sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind socket: errno %d", errno);
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    // create receive task
    should_stop_ = false;
    if (xTaskCreate(HandleUDPTask, "udp_task", 4096, this, 5, &udp_task_) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UDP task");
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    ESP_LOGI(TAG, "UDP server initialized on port %d", port_);
    return true;
}

void UDPServer::Deinitialize() {
    if (socket_fd_ >= 0) {
        should_stop_ = true;
        if (udp_task_) {
            vTaskDelete(udp_task_);
            udp_task_ = nullptr;
        }
        close(socket_fd_);
        socket_fd_ = -1;
        clients_.clear();
    }
}

bool UDPServer::SendToAllClients(const uint8_t* data, size_t len) {
    if (!data || len == 0) {
        return false;
    }

    // each packet has 4 bytes header
    size_t total_len = sizeof(MessageHeader) + len;
    std::vector<uint8_t> buffer(total_len);
    MessageHeader* header = reinterpret_cast<MessageHeader*>(buffer.data());
    header->type = MessageType::DATA;
    memcpy(buffer.data() + sizeof(MessageHeader), data, len);

    bool success = true;
    std::vector<sockaddr_in> failed_clients;

    for (const auto& client : clients_) {
        if (!SendTo(buffer.data(), total_len, client.addr)) {
            failed_clients.push_back(client.addr);
            success = false;
            ESP_LOGW(TAG, "Failed to send data to client %s:%d",
                     inet_ntoa(client.addr.sin_addr), ntohs(client.addr.sin_port));
        }
    }

    // remove failed clients
    for (const auto& addr : failed_clients) {
        RemoveClient(addr);
    }

    return success;
}

bool UDPServer::SendTo(const uint8_t* data, size_t len, const sockaddr_in& dest_addr) {
    if (socket_fd_ < 0 || !data || len == 0) {
        return false;
    }

    ssize_t sent = sendto(socket_fd_, data, len, 0,
                         (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (sent < 0) {
        ESP_LOGE(TAG, "Failed to send data: errno %d", errno);
        return false;
    }
    return true;
}

void UDPServer::RemoveClient(const sockaddr_in& addr) {
    auto it = clients_.begin();
    while (it != clients_.end()) {
        if (it->addr.sin_addr.s_addr == addr.sin_addr.s_addr &&
            it->addr.sin_port == addr.sin_port) {
            ESP_LOGI(TAG, "Client %s:%d disconnected",
                     inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
            it = clients_.erase(it);
            return;
        }
        ++it;
    }
}

void UDPServer::HandleMessage(const uint8_t* data, size_t len, const sockaddr_in& client_addr) {
    if (len < sizeof(MessageHeader)) {
        return;
    }

    const MessageHeader* header = reinterpret_cast<const MessageHeader*>(data);
    const uint8_t* payload = data + sizeof(MessageHeader);
    size_t payload_len = len - sizeof(MessageHeader);

    switch (header->type) {
        case MessageType::DISCONNECT:
            ESP_LOGI(TAG, "Received disconnect message from %s:%d",
                     inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            RemoveClient(client_addr);
            break;

        case MessageType::DATA:
            if (payload_len > 0 && data_callback_) {
                data_callback_(payload, payload_len, client_addr);
            }
            break;
    }
}

void UDPServer::HandleUDPTask(void* arg) {
    UDPServer* server = static_cast<UDPServer*>(arg);
    constexpr size_t BUFFER_SIZE = 1024;
    uint8_t rx_buffer[BUFFER_SIZE];
    
    while (!server->should_stop_) {
        sockaddr_in client_addr = {};
        socklen_t addr_len = sizeof(client_addr);
        
        // receive data
        ssize_t len = recvfrom(server->socket_fd_, rx_buffer, sizeof(rx_buffer), 0,
                              (struct sockaddr*)&client_addr, &addr_len);
                              
        if (len < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // update or add client
        bool is_new_client = true;
        for (const auto& client : server->clients_) {
            if (client.addr.sin_addr.s_addr == client_addr.sin_addr.s_addr &&
                client.addr.sin_port == client_addr.sin_port) {
                is_new_client = false;
                break;
            }
        }

        if (is_new_client) {
            ESP_LOGI(TAG, "New client connected from %s:%d",
                     inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            ClientInfo new_client(client_addr);
            server->clients_.push_back(new_client);
        }

        server->HandleMessage(rx_buffer, len, client_addr);
    }

    vTaskDelete(NULL);
} 