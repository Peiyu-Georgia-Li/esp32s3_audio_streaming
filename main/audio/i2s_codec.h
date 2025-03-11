#pragma once

#include "audio_config.h"
#include <driver/gpio.h>
#include <driver/i2s_std.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <mutex>
#include <functional>

// I2S port definitions for TX (speaker) and RX (microphone)
#define I2S_PORT_TX I2S_NUM_0
#define I2S_PORT_RX I2S_NUM_1

class I2SCodec {
public:
    using MicrophoneCallback = std::function<void(const int16_t*, size_t)>;

    I2SCodec(uint32_t sample_rate,
             gpio_num_t mic_sck, gpio_num_t mic_ws, gpio_num_t mic_din);
    ~I2SCodec();

    bool Initialize();
    void Deinitialize();
    void SetSampleRate(uint32_t sample_rate);
    void SetMicrophoneCallback(MicrophoneCallback callback);
    bool ReadAudioData();

    uint32_t microphone_sample_rate() const { return sample_rate_; }
    uint32_t get_audio_read_duration_ms() const { return audio_read_duration_ms_; }
private:
    static void TimerCallback(void* arg);

    // GPIO pins for microphone
    gpio_num_t mic_sck_;
    gpio_num_t mic_ws_;
    gpio_num_t mic_din_;

    // receive audio (microphone) handle
    i2s_chan_handle_t rx_handle_ = nullptr;

    // audio parameters
    uint32_t sample_rate_;
    size_t input_channels_ = 1;

    /* rate = 16000, dma buffer = 6 * 240 = 1440, frame_size = 32bit
       per dma filling time = 240 / 16000 = 15ms
       total dma filling time = 15 * 6 = 90ms */
    uint32_t audio_read_duration_ms_ = 30;

    /* periodically read audio data from dma buffer every 30ms, convert it, 
       write to the audio_processor's ring buffer, and send it to the server via udp */
    esp_timer_handle_t timer_handle_ = nullptr;

    /* the audio callback_ is invoked by the audio_processor, via SetMicrophoneCallback,
       it writes the updated audio data to the ring buffer */
    std::mutex callback_mutex_;
    MicrophoneCallback audio_callback_;
}; 