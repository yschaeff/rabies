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
#include "bsp/board.h"
#include "tusb.h"
#include "usb_descriptors.h"

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

uint8_t key_mapping[W] = { HID_KEY_A, HID_KEY_B, HID_KEY_C, HID_KEY_D, HID_KEY_E, HID_KEY_F, HID_KEY_P, HID_KEY_I, HID_KEY_O };

#define WATCH_DOG_PATIENCE 100000  /* uS after which the watchdog intervenes */

void hid_task(absolute_time_t t_now);

uint8_t led_states[W];

//Two buffers holding the key state. Once a full message is received
//we flip the buffers so we do not get spurious key toggles while receiving
//a message. Later we might compare the 2 to get key up and down events.
uint8_t key_states_a[W];
uint8_t key_states_b[W];
uint8_t *key_states_read = key_states_a;
uint8_t *key_states_write = key_states_b;
uint8_t key_states_events[W];

bool caps_lock = false;

uint8_t clamp(float in, uint8_t min, uint8_t max)
{
    if (in < min)
        return min;
    else if (in > max)
        return max;
    else
        return (uint8_t) in;
}

float gauss(float x, float mu, float sigma)
{
    return pow(M_E, (-pow(x-mu, 2) / (2*pow(sigma, 2))) );
}

uint8_t knight_rider(uint8_t i, uint8_t n, absolute_time_t now)
{
#define STEPS 500
#define ANIM_TIME 5000000
#define STEP_TIME (ANIM_TIME/STEPS)
#define SIGMA 0.1 // 68% width of bell curve
    //get a value between [0, 1] for peak (mean in gauss function)
    //dependent on time
    float mu = ( (now/STEP_TIME)%STEPS ) / (float)STEPS ; //mean
    //stretch out to [-1, 1] on fold over to get [0,1] again
    //this will give an oscillating effect
    mu = fabs(mu*2-1);

    float y = gauss(i/(float)n, mu, SIGMA);
    return clamp(y*255, 0, 255);
#undef STEPS
#undef ANIM_TIME
#undef STEP_TIME
#undef SIGMA
}

// Do full brightness on key down. Fade out on key up.
void update_leds(uint n, absolute_time_t now)
{
    for (uint i = 0; i < n; ++i) {
        if (key_states_read[i]) {
            led_states[i] = 0xFF;
        } else {
            led_states[i] >>= 1;
        }
    }
    for (uint i = 0; i < n; ++i) {
        uint8_t r = 0;
        if (caps_lock)
            r = knight_rider(i, n, now);
        else
            r = 0xFF;
        uint8_t g = 5;
        uint8_t b = led_states[i];

        uint32_t grba = (r<<16) | (g<<24) | (b<<8);
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
    for (int i = 0; i < W; i++) {
        key_states_events[i] = key_states_a[i]^key_states_b[i];
    }
    memset(key_states_write, 0, W);
}

void setup()
{
    stdio_init_all();
    board_init(); //something for tinyUSB
    tusb_init();

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

        tud_task(); // tinyusb device task
        hid_task(t_now);

        if (t_next_led < t_now) {
            t_next_led = t_now + 50000;
            update_leds(W, t_now);
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

// Invoked when device is mounted
void tud_mount_cb(void) { }
// Invoked when device is unmounted
void tud_umount_cb(void) { }
// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en) { }
// Invoked when usb bus is resumed
void tud_resume_cb(void) { }


void hid_task(absolute_time_t t_now)
{
    static absolute_time_t t_next = 0;
    if ( t_now < t_next) return;
    t_next = t_now + 10;

    if ( !tud_hid_ready() ) return;

    uint8_t pressed_keys[6] = { 0 };

    for (int i = 0, j = 0; i < W; i++) {
        if (key_states_read[i]) {
            pressed_keys[j++] = key_mapping[i];
        }
        if (j >= 6) break;
    }

    uint8_t mods = 0;
    if (caps_lock)
        mods |= KEYBOARD_MODIFIER_LEFTSHIFT;
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, pressed_keys);
}

// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len)
{
  (void) instance;
  (void) report;
  (void) len;
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  // TODO not Implemented
  (void) instance;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
    (void) instance;

    if (report_type != HID_REPORT_TYPE_OUTPUT) return;
    if (report_id != REPORT_ID_KEYBOARD) return;
    if ( bufsize < 1 ) return; // bufsize should be (at least) 1

    uint8_t const kbd_leds = buffer[0];
    caps_lock = kbd_leds & KEYBOARD_LED_CAPSLOCK;
}
