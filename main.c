#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"

#define IS_RGBW false
#define W 6
#define LED_OUT_PIN 2

static inline void
put_pixel(uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t grba = ((uint32_t)g<<24) | ((uint32_t)r<<16) | (uint32_t)b<<8;
    pio_sm_put_blocking(pio0, 0, grba);
}

uint8_t
sin_xsp(uint x, uint scale float phase)
{
    return sinf(((float)x)/scale2*M_PI - phase)*127.0 + 127;
}

void pattern(uint n, uint t)
{
    for (uint i = 0; i < n; ++i) {
        uint8_t r = sin_xsp(t, 95, M_PI*2*(i+0)/n);
        uint8_t g = sin_xsp(t, 100, M_PI*2*(i+2)/n);
        uint8_t b = sin_xsp(t, 105, M_PI*2*(i+4)/n);

        put_pixel(r,g,b);
    }
}

int main()
{
    stdio_init_all();

    uint offset = pio_add_program(pio0, &ws2812_program);
    ws2812_program_init(pio0, 0, offset, LED_OUT_PIN, 800000, IS_RGBW);

    unsigned int t = 0;
    while (1) {
        pattern(W, t);
        sleep_ms(10);
        t++;
    }
}
