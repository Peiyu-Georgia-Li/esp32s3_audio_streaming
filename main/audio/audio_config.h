#pragma once

#include <driver/gpio.h>
#include <driver/i2s_std.h>

// Audio sample rate
#define AUDIO_SAMPLE_RATE 16000

#define I2S_PORT_NUM     I2S_NUM_0
#define CHANNEL_NUM      1                          // mono

#define AUDIO_I2S_METHOD_SIMPLEX

#ifdef AUDIO_I2S_METHOD_SIMPLEX
#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_4    // L/R clock
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_5    // Serial clock
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_6    // Serial data
#else
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_4
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_5
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_6
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_7
#endif
