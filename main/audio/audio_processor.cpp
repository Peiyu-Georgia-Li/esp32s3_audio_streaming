#include "audio_processor.h"
#include <esp_log.h>
#include <cstring>
#include <string.h>
#include <algorithm>
#include <inttypes.h>

static const char* TAG = "AudioProcessor";

AudioProcessor* AudioProcessor::instance_ = nullptr;

AudioProcessor& AudioProcessor::GetInstance() {
    if (!instance_) {
        instance_ = new AudioProcessor();
    }
    return *instance_;
}

AudioProcessor::~AudioProcessor() {
    Deinitialize();
}

bool AudioProcessor::Initialize(I2SCodec* codec) {
    if (!codec) {
        ESP_LOGE(TAG, "Invalid codec pointer");
        return false;
    }

    // allocate memory in PSRAM (each sample is 2 bytes)
    ring_buffer_ = (int16_t*)heap_caps_malloc(ring_buffer_size_ * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    if (!ring_buffer_) {
        ESP_LOGE(TAG, "Failed to allocate PSRAM for audio buffer");
        return false;
    }

    codec_ = codec;
    write_pos_ = 0;
    last_send_pos_ = 0;
    
    // create timer for periodic data reading
    const esp_timer_create_args_t timer_args = {
        .callback = ReadTimerCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "audio_read_timer",
        .skip_unhandled_events = true,
    };
    
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &read_timer_));
    ESP_ERROR_CHECK(esp_timer_start_periodic(read_timer_, codec_->get_audio_read_duration_ms() * 1000));

    ESP_LOGI(TAG, "Setting microphone callback");
    codec_->SetMicrophoneCallback(MicrophoneCallback);

    ESP_LOGI(TAG, "Audio processor initialized with PSRAM buffer");
    return true;
}

void AudioProcessor::Deinitialize() {
    if (read_timer_) {
        esp_timer_stop(read_timer_);
        esp_timer_delete(read_timer_);
        read_timer_ = nullptr;
    }

    if (codec_) {
        codec_->SetMicrophoneCallback(nullptr);
        codec_ = nullptr;
    }

    if (ring_buffer_) {
        heap_caps_free(ring_buffer_);
        ring_buffer_ = nullptr;
    }
    
    write_pos_ = 0;
    last_send_pos_ = 0;
}

void AudioProcessor::WriteData(const int16_t* data, size_t samples) {
    if (!data || samples == 0 || !ring_buffer_) {
        return;
    }

    // aggressively write data to the ring buffer
    if (write_pos_ + samples <= ring_buffer_size_) {
        memcpy(&ring_buffer_[write_pos_], data, samples * sizeof(int16_t));
    } else {
        size_t first_part = ring_buffer_size_ - write_pos_;
        memcpy(&ring_buffer_[write_pos_], data, first_part * sizeof(int16_t));
        memcpy(&ring_buffer_[0], data + first_part, (samples - first_part) * sizeof(int16_t));
    }

    write_pos_ = (write_pos_ + samples) % ring_buffer_size_;
}


void AudioProcessor::MicrophoneCallback(const int16_t* data, size_t len) {
    if (instance_) {
        instance_->WriteData(data, len);
    }
}

void AudioProcessor::ReadTimerCallback(void* arg) {
    static_cast<AudioProcessor*>(arg)->SendData();
}

void AudioProcessor::SendData() {
    if (!ring_buffer_) {
        return;
    }

    /* calculate the available data (from the send position to the write position) */
    size_t valid_data_samples = 0;
    if (write_pos_ >= last_send_pos_) {
        valid_data_samples = write_pos_ - last_send_pos_;
    } else {
        valid_data_samples = ring_buffer_size_ - last_send_pos_ + write_pos_;
    }

    /* send data to the server via udp */
    if (valid_data_samples > 0 && udp_server_.HasClients()) {
        const size_t MAX_SAMPLES_PER_PACKET = 480; // 480 samples = 1920 bytes = 30ms
        size_t samples_sent = 0;
        size_t current_send_pos = last_send_pos_;  // track the current send position
        size_t failed_packets = 0;
        const size_t MAX_FAILED_PACKETS = 3;       // allow max consecutive failures
        size_t total_packets_sent = 0;             // for calculating delay

        ESP_LOGI(TAG, "Sending %zu samples from pos %zu", valid_data_samples, current_send_pos);

        while (samples_sent < valid_data_samples) {
            size_t samples_to_send = std::min(MAX_SAMPLES_PER_PACKET, valid_data_samples - samples_sent);
            std::vector<int16_t> send_buffer(samples_to_send);

            // read new data from the ring buffer
            size_t read_pos = (current_send_pos + samples_sent) % ring_buffer_size_;
            if (read_pos + samples_to_send <= ring_buffer_size_) {
                memcpy(send_buffer.data(), &ring_buffer_[read_pos], 
                       samples_to_send * sizeof(int16_t));
            } else {
                size_t first_part = ring_buffer_size_ - read_pos;
                memcpy(send_buffer.data(), &ring_buffer_[read_pos], 
                       first_part * sizeof(int16_t));
                memcpy(send_buffer.data() + first_part, &ring_buffer_[0], 
                       (samples_to_send - first_part) * sizeof(int16_t));
            }

            // send
            if (!udp_server_.SendToAllClients(reinterpret_cast<const uint8_t*>(send_buffer.data()), 
                                            samples_to_send * sizeof(int16_t))) {
                ESP_LOGW(TAG, "Failed to send packet at offset %zu", samples_sent);
                failed_packets++;
                
                if (failed_packets >= MAX_FAILED_PACKETS) {
                    ESP_LOGE(TAG, "Too many consecutive send failures (%zu), skipping this batch", 
                            failed_packets);
                    samples_sent += samples_to_send;  // skip this batch
                    continue;
                }
                
                continue;
            }

            failed_packets = 0;  // reset the failure count
            samples_sent += samples_to_send;
            total_packets_sent++;
        }

        // update the send position
        if (samples_sent > 0) {
            last_send_pos_ = (current_send_pos + samples_sent) % ring_buffer_size_;
            if (samples_sent < valid_data_samples) {
                ESP_LOGW(TAG, "Only sent %zu samples out of %zu valid samples (%zu packets)", 
                         samples_sent, valid_data_samples, total_packets_sent);
            } else {
                ESP_LOGI(TAG, "Successfully sent %zu samples in %zu packets", 
                         samples_sent, total_packets_sent);
            }
        } else {
            ESP_LOGW(TAG, "No data sent, will retry from position %zu next time", last_send_pos_);
        }
    }
} 