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

#define K 1             /* Number if inputs per RABI */
#define W 9             /* Number of RABIs */

#define WATCH_DOG_PATIENCE 100000  /* uS after which the watchdog intervenes */


uint8_t led_states[W];

//Two buffers holding the key state. Once a full message is received
//we flip the buffers so we do not get spurious key toggles while receiving
//a message. Later we might compare the 2 to get key up and down events.
uint8_t key_states_a[W];
uint8_t key_states_b[W];
uint8_t *key_states_read = key_states_a;
uint8_t *key_states_write = key_states_b;

// Do full brightness on key down. Fade out on key up.
void update_leds(uint n)
{
    for (uint i = 0; i < n; ++i) {
        if (key_states_read[i]) {
            led_states[i] = 0xFF;
        } else {
            led_states[i] >>= 1;
        }
        uint32_t grba = led_states[i]<<16;
        pio_sm_put_blocking(WS_PIO, WS_SM, grba);
    }
}

// Flip read and write buffer
void flip()
{
    if (key_states_read == key_states_a) {
        key_states_read  = key_states_b;
        key_states_write = key_states_a;
    } else {
        key_states_read  = key_states_a;
        key_states_write = key_states_b;
    }
    memset(key_states_write, 0, W);
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
    static uint input_id, wolf_id, state = 0;

    /*
      " Everyone knows that debugging is twice as hard as writing
        a program in the first place. So if you're as clever as
        you can be when you write it, how will you ever debug it? "
                                               -- Brian Kernighan
    */

    if (reset) state = 0;
    switch (state) {
        case 0:                     //wait for wakeup
            wolf_id = -1;
            state = bit;
            return false;
        case 1:                     //recv next header
            input_id = K;           //we expect to rcv K bits from neighbor
            state = bit + 2;
            wolf_id++;
            return false;
        case 2:                     //read 1 of K bits
            key_states_write[wolf_id] <<= 1;
            key_states_write[wolf_id] |= bit;
            state -= !--input_id;
            return false;
        case 3:                     //end ow howl
            state = 0;
            return bit;
    }
}

int main()
{
    absolute_time_t t_next_led = 0;
    absolute_time_t t_watch_dog = 0;
    bool idle = true;

    setup();

    while (1) {
        absolute_time_t t_now = get_absolute_time();

        if (t_next_led < t_now) {
            t_next_led = t_now + 50000;
            update_leds(W);
        }
        if (!idle && t_now > t_watch_dog) {
            // 100ms passed but we are still not done. We are probably
            // terribly confused. Lets reset the statemachine and try again.
            idle = true;
            (void)statemachine(0, true);
            t_watch_dog = t_now + WATCH_DOG_PATIENCE;
            printf("\nout of sync?");
        }

        // process events and initiate new howl
        if (idle) {
            idle = false;
            // for edge detection get diff between buffers before doing the flip.
            flip();
            pio_sm_put_blocking(RB_PIO, RB_HOWL_SM, 1);
            t_watch_dog = t_now + WATCH_DOG_PATIENCE;
            printf("\nhowl ");
        }

        if (!pio_sm_is_rx_fifo_empty(RB_PIO, RB_LISTEN_SM)) {
            uint32_t b = pio_sm_get(RB_PIO, RB_LISTEN_SM);
            printf("%d", b&1);
            idle = statemachine(b&1, false);
            t_watch_dog = t_now + WATCH_DOG_PATIENCE;
        }
    }
}
