#pragma once

#include <cstdint>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include "i2s_codec.h"
#include "../network/udp_server.h"

class AudioProcessor {
public:
    static AudioProcessor& GetInstance();

    AudioProcessor(const AudioProcessor&) = delete;
    AudioProcessor& operator=(const AudioProcessor&) = delete;

    bool Initialize(I2SCodec* codec);
    void Deinitialize();

    void SendData();

private:
    AudioProcessor() : ring_buffer_(nullptr) {}
    ~AudioProcessor();

    esp_timer_handle_t read_timer_ = nullptr;
    I2SCodec* codec_ = nullptr;

    /* udp server */
    UDPServer& udp_server_ = UDPServer::GetInstance();

    /* ring buffer */
    int16_t* ring_buffer_;
    static constexpr size_t ring_buffer_size_ = 16000 * 4;   /* (16kHz * 0.4 seconds) */
    size_t write_pos_ = 0;
    size_t read_pos_ = 0;
    size_t last_send_pos_ = 0;

    /* read timer callback */
    static void ReadTimerCallback(void* arg);

    /* microphone callback */
    static void MicrophoneCallback(const int16_t* data, size_t len);
    /* write data to the ring buffer */
    void WriteData(const int16_t* data, size_t samples);

    static AudioProcessor* instance_;
};