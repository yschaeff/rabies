#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"

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
    return sinf(((float)x)/scale*2*M_PI - phase)*127.0 + 127;
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

void gpio_callback(uint gpio, uint32_t events)
{
    static absolute_time_t t_fall, t_rise;
    if (gpio != 3) return;

    if (events & 0x8) {
        //rising edge
        t_rise = get_absolute_time();

        if (t_rise - t_fall > 80000) printf("\n");

    } else if (events & 0x4) {
        //falling edge
        t_fall = get_absolute_time();

        if (t_fall - t_rise < 4500) {
            printf("0");
        } else {
            printf("1");
        }
    }
}

int main()
{
    uint prg_addr;
    stdio_init_all();

    /*prg_addr = pio_add_program(pio0, &ws2812_program);*/
    /*ws2812_program_init(pio0, 0, prg_addr, LED_OUT_PIN, 800000, IS_RGBW);*/

    prg_addr = pio_add_program(pio0, &rabi_trigger_program);
    rabi_trigger_program_init(pio0, 0, prg_addr, KEY_OUT_PIN, 1);

    prg_addr = pio_add_program(pio1, &rabi_program);
    rabi_program_init(pio1, 0, prg_addr, KEY_IN_PIN, 500);

    /*gpio_set_irq_enabled_with_callback(3, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);*/

    printf("hello. sleeping\n");
    sleep_ms(2000);
    printf("hello\n");

    unsigned int t = 0;
    absolute_time_t t_next = 0;

    while (1) {
        absolute_time_t t_now = get_absolute_time();
        /*if (t_next < t_now) {*/
            /*t_next = t_now + 50000;*/
            /*pattern(W, t++);*/
        /*}*/
        if (t_next < t_now) {
            printf("\nput %d\n", t++);
            t_next = t_now + 3000000; //each second tickle trigger
            pio_sm_put_blocking(pio0, 0, 1);
        }
        if (pio_sm_is_rx_fifo_empty(pio1, 0)) continue;
        uint32_t b = pio_sm_get_blocking(pio1, 0);
        printf("%d", b&1);
    }
    printf("bye\n");
}
