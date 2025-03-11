#pragma once

#include <string>
#include <esp_wifi.h>
#include <esp_event.h>

class WiFiManager {
public:
    static WiFiManager& GetInstance();

    bool Initialize(const char* ap_ssid, const char* ap_password, uint8_t max_connections = 1);
    void Deinitialize();

    bool IsActive() const { return is_active_; }
    std::string GetIP() const;
    std::string GetSSID() const { return ssid_; }

private:
    WiFiManager() = default;
    ~WiFiManager();
    WiFiManager(const WiFiManager&) = delete;
    WiFiManager& operator=(const WiFiManager&) = delete;

    static void EventHandler(void* arg, esp_event_base_t event_base,
                           int32_t event_id, void* event_data);

    std::string ssid_;
    std::string password_;
    bool is_active_ = false;
}; 