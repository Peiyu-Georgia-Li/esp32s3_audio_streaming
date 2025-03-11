#include <esp_log.h>
#include <nvs_flash.h>
#include "board/esp32s3_board.h"
#include "audio/audio_processor.h"
#include "network/wifi_manager.h"
#include "network/udp_server.h"
#include <inttypes.h>

static const char* TAG = "main";

/* wifi ap config */
static const char* WIFI_AP_SSID = "ESP32_TEST_SERVER";
static const char* WIFI_AP_PASSWORD = "12345678";
static const uint16_t UDP_PORT = 5001;

/* udp data callback */
void HandleUDPData(const uint8_t* data, size_t len, const sockaddr_in& client_addr) {
    char addr_str[32];
    inet_ntoa_r(client_addr.sin_addr, addr_str, sizeof(addr_str) - 1);
    ESP_LOGI(TAG, "Received %d bytes from %s:%d", len, addr_str, ntohs(client_addr.sin_port));
}

extern "C" void app_main(void)
{
    /* initialize nvs */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* get board instance */
    auto& board = ESP32S3Board::GetInstance();

    /* print basic information */
    ESP_LOGI(TAG, "Board Info: %s", board.GetJson().c_str());
    
    /* initialize audio codec */
    auto* codec = board.GetAudioCodec();
    if (!codec->Initialize()) {
        ESP_LOGE(TAG, "Failed to initialize audio codec");
        return;
    }
    ESP_LOGI(TAG, "Audio microphone sample rate: %" PRIu32 " Hz", codec->microphone_sample_rate());

    /* initialize audio processor */
    auto& audio_processor = AudioProcessor::GetInstance();
    if (!audio_processor.Initialize(codec)) {
        ESP_LOGE(TAG, "Failed to initialize audio processor");
        return;
    }

    /* initialize wifi ap */
    auto& wifi_manager = WiFiManager::GetInstance();
    ESP_LOGI(TAG, "Initializing WiFi AP...");
    if (!wifi_manager.Initialize(WIFI_AP_SSID, WIFI_AP_PASSWORD, 4)) {
        ESP_LOGE(TAG, "Failed to initialize WiFi AP");
        return;
    }
    ESP_LOGI(TAG, "WiFi AP started successfully");
    ESP_LOGI(TAG, "SSID: %s", WIFI_AP_SSID);
    ESP_LOGI(TAG, "Password: %s", WIFI_AP_PASSWORD);
    ESP_LOGI(TAG, "IP Address: %s", wifi_manager.GetIP().c_str());

    /* initialize udp server */
    auto& udp_server = UDPServer::GetInstance();
    if (!udp_server.Initialize(UDP_PORT)) {
        ESP_LOGE(TAG, "Failed to initialize UDP server");
        return;
    }
    ESP_LOGI(TAG, "UDP server listening on port %d", UDP_PORT);
    udp_server.SetReceiveCallback(HandleUDPData);
}