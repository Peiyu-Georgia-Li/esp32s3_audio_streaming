#include "i2s_codec.h"
#include <esp_log.h>
#include <cstring>
#include <inttypes.h>
#include <vector>

static const char* TAG = "I2SCodec";

I2SCodec::I2SCodec(uint32_t sample_rate,
                   gpio_num_t mic_sck, gpio_num_t mic_ws, gpio_num_t mic_din)
    : mic_sck_(mic_sck), mic_ws_(mic_ws), mic_din_(mic_din),
      sample_rate_(sample_rate) {
}

I2SCodec::~I2SCodec() {
    Deinitialize();
}

bool I2SCodec::Initialize() {
    i2s_chan_config_t rx_chan_cfg = {
        .id = (i2s_port_t)1,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,
        .dma_frame_num = 240,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    
    ESP_ERROR_CHECK(i2s_new_channel(&rx_chan_cfg, nullptr, &rx_handle_));

    i2s_std_config_t rx_std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)sample_rate_,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,  // always initialize this field
            .mclk_multiple = I2S_MCLK_MULTIPLE_256
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_STD_SLOT_LEFT,  // use left channel
            .ws_width = I2S_DATA_BIT_WIDTH_32BIT,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = true,   // left alignment
            .big_endian = false,  // little endian
            .bit_order_lsb = false // msb first
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = mic_sck_,
            .ws = mic_ws_,
            .dout = I2S_GPIO_UNUSED,
            .din = mic_din_,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            }
        }
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &rx_std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle_));

    const esp_timer_create_args_t timer_args = {
        .callback = TimerCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "i2s_read_timer",
        .skip_unhandled_events = true,
    };
    
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle_));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle_, audio_read_duration_ms_ * 1000));

    ESP_LOGI(TAG, "I2S Configuration:");
    ESP_LOGI(TAG, "  Sample Rate: %lu Hz", sample_rate_);
    ESP_LOGI(TAG, "  MCLK Multiple: 256");
    ESP_LOGI(TAG, "  Data Bit Width: 32-bit");
    ESP_LOGI(TAG, "  Slot Bit Width: AUTO");
    ESP_LOGI(TAG, "  WS Width: 32-bit");
    ESP_LOGI(TAG, "  GPIO: SCK=%d, WS=%d, DIN=%d", mic_sck_, mic_ws_, mic_din_);

    ESP_LOGI(TAG, "I2S codec initialized successfully");
    return true;
}

void I2SCodec::Deinitialize() {
    if (timer_handle_) {
        esp_timer_stop(timer_handle_);
        esp_timer_delete(timer_handle_);
        timer_handle_ = nullptr;
    }

    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        audio_callback_ = nullptr;
    }

    if (rx_handle_) {
        i2s_channel_disable(rx_handle_);
        i2s_del_channel(rx_handle_);
        rx_handle_ = nullptr;
    }
}

void I2SCodec::SetSampleRate(uint32_t sample_rate) {
    if (sample_rate == sample_rate_ || !rx_handle_) return;
    
    sample_rate_ = sample_rate;
    
    i2s_channel_disable(rx_handle_);
    i2s_std_clk_config_t clk_cfg = {
        .sample_rate_hz = sample_rate,
        .clk_src = I2S_CLK_SRC_DEFAULT,
        .ext_clk_freq_hz = 0,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256
    };
    
    ESP_ERROR_CHECK(i2s_channel_reconfig_std_clock(rx_handle_, &clk_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle_));
}

void I2SCodec::SetMicrophoneCallback(MicrophoneCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    audio_callback_ = callback;
}

bool I2SCodec::ReadAudioData() {
    if (!rx_handle_) return false;

    // calculate the expected frame size
    size_t expected_samples = (sample_rate_ / 1000) * audio_read_duration_ms_ * 1;
    size_t expected_bytes = expected_samples * sizeof(int32_t);

    std::vector<int32_t> audio_buffer(expected_samples);
    size_t total_bytes_read = 0;

    // read until get enough audio data
    while (total_bytes_read < expected_bytes) {
        size_t current_read = 0;
        esp_err_t err = i2s_channel_read(rx_handle_, 
                                         audio_buffer.data() + (total_bytes_read / sizeof(int32_t)), 
                                         expected_bytes - total_bytes_read, 
                                         &current_read, 
                                         portMAX_DELAY);
        if (err != ESP_OK) {
            if (err != ESP_ERR_TIMEOUT) {
                ESP_LOGE(TAG, "Error reading I2S data: %s", esp_err_to_name(err));
            }
            return false;
        }

        total_bytes_read += current_read;
        if (current_read == 0) {
            ESP_LOGW(TAG, "I2S read returned 0 bytes, possible buffer underrun");
            break;  // possible data stream broken, prevent infinite loop
        }
    }

    if (total_bytes_read == 0) {
        return false;
    }

    size_t samples = total_bytes_read / sizeof(int32_t);
    std::vector<int16_t> converted_data(samples);

    // convert 32-bit pcm to 16-bit pcm
    for (size_t i = 0; i < samples; i++) {
        int32_t value = audio_buffer[i] >> 12;
        converted_data[i] = (value > INT16_MAX) ? INT16_MAX : 
                            (value < -INT16_MAX) ? -INT16_MAX : 
                            static_cast<int16_t>(value);
    }

    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (audio_callback_) {
        audio_callback_(converted_data.data(), samples);
        return true;
    }

    return false;
}


void I2SCodec::TimerCallback(void* arg) {
    static_cast<I2SCodec*>(arg)->ReadAudioData();
}