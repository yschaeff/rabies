/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"

#define IS_RGBW false
#define W 6
#define LED_OUT_PIN 2

static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

static inline uint32_t
urgb_u32(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)g<<16) | ((uint32_t)r<<8) | (uint32_t)(b);
}

void pattern(uint n, uint t)
{
    for (uint i = 0; i < n; ++i) {
        uint8_t r = (t+0)%256;
        uint8_t g = (t+80)%256;
        uint8_t b = (t+160)%256;
        if (t%n == i)
            put_pixel(urgb_u32(r,g,b));
        else
            put_pixel(0);
    }
}

int main()
{
    stdio_init_all();

    uint offset = pio_add_program(pio0, &ws2812_program);
    ws2812_program_init(pio0, 0, offset, LED_OUT_PIN, 800000, IS_RGBW);

    int t = 0;
    while (1) {
        pattern(W, t);
        sleep_ms(20);
        t++;
    }
}
