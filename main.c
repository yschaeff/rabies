#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"
#include "rabi.pio.h"

#define LED_OUT_PIN 2
#define KEY_IN_PIN  3
#define KEY_OUT_PIN 4

#define WS_PIO pio0
#define WS_SM 0

#define RB_PIO pio1
#define RB_LISTEN_SM 0
#define RB_HOWL_SM 1

#define K 1
#define W 9

uint8_t led_states[W];
uint8_t key_states[W];

uint8_t lin_lut[] = { 0, 1, 2, 3, 4, 5, 7, 9,12, 15, 18, 22, 27, 32, 38, 44, 51, 58, 67, 76, 86, 96, 108, 120, 134, 148, 163, 180, 197, 216, 235, 255 };

void update_leds(uint n)
{
    for (uint i = 0; i < n; ++i) {
        if (key_states[i]) {
            led_states[i] = 0xFF;
        }
        /*else {*/
            /*led_states[i] = 0x00;*/

        /*}*/
        uint32_t grba = led_states[i]<<16;
        /*uint32_t grba = lin_lut[led_states[i]]<<16;*/

        pio_sm_put_blocking(WS_PIO, WS_SM, grba);
        /*if (led_states[i]) led_states[i]--;*/
        led_states[i] >>= 1;
    }
}



void setup()
{
    stdio_init_all();

    //PIO 0 handles WS2812
    uint ws_addr = pio_add_program(WS_PIO, &ws2812_program);
    ws2812_program_init(WS_PIO, WS_SM, ws_addr, LED_OUT_PIN, 800000, false);

    //PIO 1 handles the rabies
    //statemachine 0 listen to howls and pushed to RX queue
    uint rb_listen_addr = pio_add_program(RB_PIO, &howl_listen_program);
    howl_listen_init(RB_PIO, RB_LISTEN_SM, rb_listen_addr, KEY_IN_PIN, 500);
    //statemachine 1 initiates cry when data in TX queue
    uint rb_howl_addr = pio_add_program(RB_PIO, &howl_start_program);
    howl_start_program_init(RB_PIO, RB_HOWL_SM, rb_howl_addr, KEY_OUT_PIN, 500);
}

bool statemachine(bool bit, bool reset)
{
    static uint k, w, state = 0;


    /*
      " Everyone knows that debugging is twice as hard as writing
        a program in the first place. So if you're as clever as
        you can be when you write it, how will you ever debug it? "
                                               -- Brian Kernighan
    */

    if (reset) state = 0;
    switch (state) {
        case 0:                     //wait for wakeup
            w = -1;
            state = bit;
            return false;
        case 1:                     //recv next header
            k = K;
            state = bit + 2;
            w++;
            key_states[w] = 0;
            return false;
        case 2:                     //read 1 of K bits
            key_states[w] <<= 1;
            key_states[w] |= bit;
            state -= !--k;
            return false;
        case 3:                     //end ow howl
            state = 0;
            return bit;
    }
}

int main()
{
    unsigned int t = 0;
    absolute_time_t t_next_led = 0;
    absolute_time_t t_last_bit = 0;
    bool howl_done = true;

    setup();

    while (1) {
        absolute_time_t t_now = get_absolute_time();

        if (howl_done && t_next_led < t_now) {
            t_next_led = t_now + 50000;
            update_leds(W);
        }
        if (!howl_done && t_now - t_last_bit > 100000) {
            //1 second silence. lets reset
            (void)statemachine(0, true);
            howl_done = true;
            printf("\nout of sync?");
        }

        if (howl_done && t_now - t_last_bit > 100000) {
            printf("\nhowl %d\n", t++);
            pio_sm_put_blocking(RB_PIO, RB_HOWL_SM, 1);
            t_last_bit = t_now;
            howl_done = false;
            /*update_leds(W);*/
        }

        if (!pio_sm_is_rx_fifo_empty(RB_PIO, RB_LISTEN_SM)) {
            uint32_t b = pio_sm_get(RB_PIO, RB_LISTEN_SM);
            printf("%d", b&1);
            howl_done = statemachine(b&1, false);
            t_last_bit = t_now;
        }
    }
}
