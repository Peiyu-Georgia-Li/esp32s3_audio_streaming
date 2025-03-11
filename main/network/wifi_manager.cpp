#include "wifi_manager.h"
#include <esp_log.h>
#include <cstring>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <nvs_flash.h>
#include <freertos/event_groups.h>
#include <esp_mac.h>

static const char* TAG = "WiFiManager";

static const int WIFI_AP_STARTED_BIT = BIT0;
static EventGroupHandle_t s_wifi_event_group;

WiFiManager& WiFiManager::GetInstance() {
    static WiFiManager instance;
    return instance;
}

WiFiManager::~WiFiManager() {
    Deinitialize();
}

void WiFiManager::EventHandler(void* arg, esp_event_base_t event_base,
                             int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_AP_START) {
            GetInstance().is_active_ = true;
            xEventGroupSetBits(s_wifi_event_group, WIFI_AP_STARTED_BIT);
            ESP_LOGI(TAG, "WiFi AP started");
        } else if (event_id == WIFI_EVENT_AP_STOP) {
            GetInstance().is_active_ = false;
            ESP_LOGI(TAG, "WiFi AP stopped");
        } else if (event_id == WIFI_EVENT_AP_STACONNECTED) {
            wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
            ESP_LOGI(TAG, "Station %02x:%02x:%02x:%02x:%02x:%02x joined, AID=%d",
                    event->mac[0], event->mac[1], event->mac[2],
                    event->mac[3], event->mac[4], event->mac[5],
                    event->aid);
        } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
            wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
            ESP_LOGI(TAG, "Station %02x:%02x:%02x:%02x:%02x:%02x left, AID=%d",
                    event->mac[0], event->mac[1], event->mac[2],
                    event->mac[3], event->mac[4], event->mac[5],
                    event->aid);
        }
    }
}

bool WiFiManager::Initialize(const char* ap_ssid, const char* ap_password, uint8_t max_connections) {
    ssid_ = ap_ssid;
    password_ = ap_password;

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &EventHandler, nullptr));

    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.ap.ssid, ssid_.c_str(), 32);
    strncpy((char*)wifi_config.ap.password, password_.c_str(), 64);
    wifi_config.ap.ssid_len = strlen(ap_ssid);
    wifi_config.ap.channel = 6;  /* can use channel 1, 6, 11 */
    wifi_config.ap.max_connection = max_connections;
    wifi_config.ap.authmode = strlen(ap_password) > 0 ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    wifi_config.ap.beacon_interval = 100;
    wifi_config.ap.ssid_hidden = 0;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));  /* disable power saving */
    
    wifi_country_t country = {
        .cc = "US",
        .schan = 1,
        .nchan = 11,
        .max_tx_power = 20,
        .policy = WIFI_COUNTRY_POLICY_AUTO,
    };
    ESP_ERROR_CHECK(esp_wifi_set_country(&country));
    
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_AP, 
        WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N));

    ESP_ERROR_CHECK(esp_wifi_start());

    /* wait for the wifi to start */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* set the max tx power */
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(84));  /* 84 = 20dBm */

    ESP_LOGI(TAG, "AP Configuration:");
    ESP_LOGI(TAG, "  SSID: %s", ap_ssid);
    ESP_LOGI(TAG, "  Password: %s", strlen(ap_password) > 0 ? ap_password : "none");
    ESP_LOGI(TAG, "  Channel: %d", wifi_config.ap.channel);
    ESP_LOGI(TAG, "  Auth mode: %d", wifi_config.ap.authmode);
    ESP_LOGI(TAG, "  Hidden: %d", wifi_config.ap.ssid_hidden);
    ESP_LOGI(TAG, "  Max connections: %d", wifi_config.ap.max_connection);
    ESP_LOGI(TAG, "  Beacon interval: %d", wifi_config.ap.beacon_interval);

    /* get and print the mac address */
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_AP, mac);
    ESP_LOGI(TAG, "AP MAC Address: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    /* get and print the current channel */
    uint8_t primary;
    wifi_second_chan_t second;
    esp_wifi_get_channel(&primary, &second);
    ESP_LOGI(TAG, "Current channel: primary=%d, secondary=%d", primary, second);

    /* get and print the current tx power */
    int8_t power;
    esp_wifi_get_max_tx_power(&power);
    ESP_LOGI(TAG, "Current TX power: %d", power);

    /* wait for the ap to start */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                         WIFI_AP_STARTED_BIT,
                                         pdFALSE,
                                         pdFALSE,
                                         pdMS_TO_TICKS(5000));

    return (bits & WIFI_AP_STARTED_BIT) != 0;
}

void WiFiManager::Deinitialize() {
    if (s_wifi_event_group) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = nullptr;
    }

    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &EventHandler);
    esp_wifi_stop();
    esp_wifi_deinit();
    esp_netif_deinit();
    nvs_flash_deinit();
}

std::string WiFiManager::GetIP() const {
    if (!is_active_) {
        return "";
    }

    esp_netif_ip_info_t ip_info;
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) {
        return "";
    }

    char ip_str[16];
    snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
    return ip_str;
} 