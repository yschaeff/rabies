#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"
#include "rabi.pio.h"

#define IS_RGBW false
#define W 6
#define LED_OUT_PIN 2
#define KEY_IN_PIN  3
#define KEY_OUT_PIN 4

static inline void
put_pixel(uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t grba = ((uint32_t)g<<24) | ((uint32_t)r<<16) | (uint32_t)b<<8;
    pio_sm_put_blocking(pio0, 0, grba);
}

uint8_t
sin_xsp(uint x, uint scale, float phase)
{
    /*return sinf(((float)x)/scale*2*M_PI - phase)*127.0 + 127;*/
    return sinf(((float)x)/scale*2*M_PI - phase)*12.0 + 12;
}

void pattern(uint n, uint t)
{
    for (uint i = 0; i < n; ++i) {
        uint8_t r = sin_xsp(t, 10, M_PI*2*(i+0)/n);
        uint8_t g = sin_xsp(t, 10, M_PI*2*(i+2)/n);
        uint8_t b = sin_xsp(t, 10, M_PI*2*(i+4)/n);

        put_pixel(r,g,b);
    }
}

int main()
{
    stdio_init_all();

    //PIO 0 handles WS2812
    uint ws_addr = pio_add_program(pio0, &ws2812_program);
    ws2812_program_init(pio0, 0, ws_addr, LED_OUT_PIN, 800000, IS_RGBW);

    //PIO 1 handles the rabies
    //statemachine 0 listen to howls and pushed to RX queue
    uint rb_listen_addr = pio_add_program(pio1, &howl_listen_program);
    howl_listen_init(pio1, 0, rb_listen_addr, KEY_IN_PIN, 500);
    //statemachine 1 initiates cry when data in TX queue
    uint rb_howl_addr = pio_add_program(pio1, &howl_start_program);
    howl_start_program_init(pio1, 1, rb_howl_addr, KEY_OUT_PIN, 500);

    unsigned int t = 0;
    absolute_time_t t_next_led = 0;
    absolute_time_t t_next_trigger = 0;

    while (1) {
        absolute_time_t t_now = get_absolute_time();
        if (t_next_led < t_now) {
            t_next_led = t_now + 50000;
            pattern(W, t++);
        }
        if (t_next_trigger < t_now) {
            printf("\nput %d\n", t++);
            t_next_trigger = t_now + 1000000; //each second tickle trigger
            pio_sm_put_blocking(pio1, 1, 1);
        }
        if (pio_sm_is_rx_fifo_empty(pio1, 0)) continue;
        uint32_t b = pio_sm_get_blocking(pio1, 0);
        printf("%d", b&1);
    }
    printf("bye\n");
}
