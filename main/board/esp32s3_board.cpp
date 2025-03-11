#include "esp32s3_board.h"
#include "i2s_codec.h"
#include <esp_log.h>
#include <esp_random.h>
#include <esp_heap_caps.h>
#include <esp_flash.h>
#include <sstream>
#include <iomanip>
#include <cJSON.h>

static const char* TAG = "ESP32S3Board";

ESP32S3Board::ESP32S3Board() {
    GenerateUuid();
    ESP_LOGI(TAG, "ESP32S3 Board initialized, UUID: %s", uuid_.c_str());
}

ESP32S3Board& ESP32S3Board::GetInstance() {
    static ESP32S3Board instance;
    return instance;
}

void ESP32S3Board::GenerateUuid() {
    uint32_t random_values[4];
    for (int i = 0; i < 4; i++) {
        random_values[i] = esp_random();
    }

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    ss << std::setw(8) << random_values[0] << "-";
    ss << std::setw(4) << ((random_values[1] >> 16) & 0xFFFF) << "-";
    ss << std::setw(4) << (random_values[1] & 0xFFFF) << "-";
    ss << std::setw(4) << ((random_values[2] >> 16) & 0xFFFF) << "-";
    ss << std::setw(8) << (random_values[2] & 0xFFFF) << std::setw(4) << random_values[3];

    uuid_ = ss.str();
}

size_t ESP32S3Board::GetFlashSize() {
    uint32_t flash_size;
    esp_flash_get_size(nullptr, &flash_size);
    return flash_size;
}

size_t ESP32S3Board::GetMinimumFreeHeapSize() {
    return heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
}

I2SCodec* ESP32S3Board::GetAudioCodec() {
    if (!audio_codec_) {
        audio_codec_ = new I2SCodec(AUDIO_SAMPLE_RATE,
            AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
    }
    return audio_codec_;
}

std::string ESP32S3Board::GetJson() const {
    cJSON* root = cJSON_CreateObject();
    
    cJSON_AddStringToObject(root, "board_type", GetBoardType().c_str());
    cJSON_AddStringToObject(root, "uuid", GetUUID().c_str());
    cJSON_AddNumberToObject(root, "flash_size_bytes", GetFlashSize());
    cJSON_AddNumberToObject(root, "free_heap_bytes", GetMinimumFreeHeapSize());
    
    size_t psram_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t psram_largest_free_block = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    cJSON_AddNumberToObject(root, "psram_total_bytes", psram_size);
    cJSON_AddNumberToObject(root, "psram_free_bytes", psram_free);
    cJSON_AddNumberToObject(root, "psram_largest_free_block_bytes", psram_largest_free_block);

    char* json_str = cJSON_Print(root);
    std::string result(json_str);
    
    cJSON_free(json_str);
    cJSON_Delete(root);
    
    return result;
}
