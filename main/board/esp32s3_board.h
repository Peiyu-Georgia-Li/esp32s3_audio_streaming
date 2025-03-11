#pragma once

#include <string>
#include "i2s_codec.h"
#include "audio_config.h"
#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_random.h>
#include <esp_flash.h>
#include <cJSON.h>

class ESP32S3Board {
public:
    static ESP32S3Board& GetInstance();

    std::string GetBoardType() const { return "ESP32S3"; }
    std::string GetUUID() const { return uuid_; }
    std::string GetJson() const;

    I2SCodec* GetAudioCodec();

    static size_t GetFlashSize();
    static size_t GetMinimumFreeHeapSize();

    bool Initialize();
    void Deinitialize();

private:
    ESP32S3Board();
    ~ESP32S3Board() = default;
    ESP32S3Board(const ESP32S3Board&) = delete;
    ESP32S3Board& operator=(const ESP32S3Board&) = delete;

    void GenerateUuid();
    std::string uuid_;
    I2SCodec* audio_codec_ = nullptr;
}; 