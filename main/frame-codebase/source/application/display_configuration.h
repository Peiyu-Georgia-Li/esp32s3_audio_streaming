/*
 * This file is a part of: https://github.com/brilliantlabsAR/frame-codebase
 *
 * Authored by: Raj Nakarja / Brilliant Labs Ltd. (raj@brilliant.xyz)
 *              Rohit Rathnam / Silicon Witchery AB (rohit@siliconwitchery.com)
 *              Uma S. Gupta / Techno Exponent (umasankar@technoexponent.com)
 *
 * ISC Licence
 *
 * Copyright © 2023 Brilliant Labs Ltd.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once
#include <stdint.h>

typedef struct display_config_t
{
    uint8_t address;
    uint8_t value;
} display_config_t;

static const display_config_t display_config[] = {
    {0x00, 0x92}, // Color format, scan direction, clock polarity, power mode
    {0x01, 0x00}, // Temperature sensing, data format
    {0x02, 0x00}, // VCAL output selection, horizontal offset
    {0x03, 0x20}, // Vertical offset
    {0x04, 0x3F},
    {0x05, 0xCB}, // Dithering, luminosity
    {0x06, 0x00},
    {0x07, 0x40},
    {0x08, 0x80}, // Sync polarity, white chromaticity/luminance preset mode
    {0x09, 0x00}, // Capture mode setting
    {0x0A, 0x10}, // 120Hz mode
    {0x0B, 0x00},
    {0x0C, 0x00},
    {0x0D, 0x00},
    {0x0E, 0x00},
    {0x0F, 0x56},
    {0x10, 0x00},
    {0x11, 0x00},
    {0x12, 0x00},
    {0x13, 0x00},
    {0x14, 0x00},
    {0x15, 0x00},
    {0x16, 0x00},
    {0x17, 0x00},
    {0x18, 0x00},
    {0x19, 0x00},
    {0x1A, 0x00},
    {0x1B, 0x00},
    {0x1C, 0x00},
    {0x1D, 0x00},
    {0x1E, 0x00},
    {0x1F, 0x00},
    {0x20, 0x01}, // Contrast adjustment bit 8
    {0x21, 0x00}, // Contrast adjustment bits 7-0
    {0x22, 0x40}, // Red sub-contrast
    {0x23, 0x40}, // Green sub-contrast
    {0x24, 0x40}, // Blue sub-contrast
    {0x25, 0x80}, // Brightness adjustment
    {0x26, 0x40}, // Red sub-brightness
    {0x27, 0x40}, // Green sub-brightness
    {0x28, 0x40}, // Blue sub-brightness
    {0x29, 0x0B},
    {0x2A, 0xBE}, // Manual luminance adjustment
    {0x2B, 0x3C},
    {0x2C, 0x02},
    {0x2D, 0x7A},
    {0x2E, 0x02},
    {0x2F, 0xFA},
    {0x30, 0x26},
    {0x31, 0x01},
    {0x32, 0xB6},
    {0x33, 0x00},
    {0x34, 0x03},
    {0x35, 0x5A},
    {0x36, 0x00},
    {0x37, 0x76},
    {0x38, 0x02},
    {0x39, 0xFE},
    {0x3A, 0x02},
    {0x3B, 0x0D},
    {0x3C, 0x00},
    {0x3D, 0x1B},
    {0x3E, 0x00},
    {0x3F, 0x1C},
    {0x40, 0x01},
    {0x41, 0xF3},
    {0x42, 0x01},
    {0x43, 0xF4},
    {0x44, 0x80},
    {0x45, 0x00},
    {0x46, 0x00},
    {0x47, 0x2D},
    {0x48, 0x08},
    {0x49, 0x01},
    {0x4A, 0x7E},
    {0x4B, 0x08},
    {0x4C, 0x0A},
    {0x4D, 0x04},
    {0x4E, 0x00},
    {0x4F, 0x3A},
    {0x50, 0x01},
    {0x51, 0x58},
    {0x52, 0x01},
    {0x53, 0x2D},
    {0x54, 0x01},
    {0x55, 0x15},
    {0x56, 0x00},
    {0x57, 0x2B},
    {0x58, 0x11},
    {0x59, 0x02},
    {0x5A, 0x11},
    {0x5B, 0x02},
    {0x5C, 0x25},
    {0x5D, 0x04},
    {0x5E, 0x0B},
    {0x5F, 0x00},
    {0x60, 0x23},
    {0x61, 0x02},
    {0x62, 0x1A},
    {0x63, 0x00},
    {0x64, 0x0A},
    {0x65, 0x01},
    {0x66, 0x8C},
    {0x67, 0x30},
    {0x68, 0x00},
    {0x69, 0x00},
    {0x6A, 0x00},
    {0x6B, 0x00},
    {0x6C, 0x00},
    {0x6D, 0x00},
    {0x6E, 0x00},
    {0x6F, 0x60},
    {0x70, 0x00},
    {0x71, 0x00},
    {0x72, 0x00},
    {0x73, 0x00},
    {0x74, 0x00},
    {0x75, 0x00},
    {0x76, 0x00},
    {0x77, 0x00},
    {0x78, 0x00},
    {0x79, 0x68},
    {0x7A, 0x00},
    {0x7B, 0x00},
    {0x7C, 0x00},
    {0x7D, 0x00},
    {0x7E, 0x00},
    {0x7F, 0x00},
    {0x00, 0x93}, // Power on
};
