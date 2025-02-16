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
#define W 25            /* Number of RABIs */

#define KEYMAP_LEN 26
static const uint8_t key_mapping[KEYMAP_LEN] = {
    HID_KEY_A, HID_KEY_B, HID_KEY_C, HID_KEY_D, HID_KEY_E,
    HID_KEY_F, HID_KEY_G, HID_KEY_H, HID_KEY_I, HID_KEY_J,
    HID_KEY_K, HID_KEY_L, HID_KEY_M, HID_KEY_N, HID_KEY_O,
    HID_KEY_P, HID_KEY_Q, HID_KEY_R, HID_KEY_S, HID_KEY_T,
    HID_KEY_U, HID_KEY_V, HID_KEY_W, HID_KEY_X, HID_KEY_Y,
    HID_KEY_Z
};

void hid_task(absolute_time_t t_now_us);

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

uint8_t pulse(uint8_t i, uint8_t n, absolute_time_t now)
{
    return clamp((sinf((float)(now/4000.0))*128)+128, 0, 255);
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
        uint8_t g = 0;
        uint8_t b = 0;

        if (!caps_lock) {
            r = knight_rider(i, n, now);
            g = 5;
        } else {
            r = pulse(i, n, now);
            g = pulse(i, n, now+1333);
        }
        b = led_states[i];

        uint32_t grba = (r<<16) | (g<<24) | (b<<8);
        pio_sm_put_blocking(WS_PIO, WS_SM, grba);
    }
}

void set_leds_uniform(uint32_t grba)
{
    for (uint i = 0; i < W; ++i) {
        pio_sm_put_blocking(WS_PIO, WS_SM, grba);
    }
}
void set_leds_red()   { set_leds_uniform(0x00FF0000); }
void set_leds_blue()  { set_leds_uniform(0x0000FF00); }
void set_leds_green() { set_leds_uniform(0xFF000000); }
void set_leds_yellow(){ set_leds_uniform(0xFF00FF00); }

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

/* Frequency of the input counter. Keep above 24Mhz to be on par with Rabi.
 * Also keep an integer multiple of 125Mhz (FCPU) */
#define FREQ_RB_COUNT   (25 * 1000 * 1000)

static void setup()
{
    stdio_init_all();
    board_init(); //something for tinyUSB
    tusb_init();

    //PIO 0 handles WS2812
    uint ws_addr = pio_add_program(WS_PIO, &ws2812_program);
    ws2812_program_init(WS_PIO, WS_SM, ws_addr, LED_OUT_PIN, 800000, false);

    //PIO 1 handles the rabies
    //25_000_000 Hz => 40ns
    uint rb_count_addr = pio_add_program(RB_PIO, &howl_count_program);
    howl_count_init(RB_PIO, RB_LISTEN_SM, rb_count_addr, KEY_IN_PIN, FREQ_RB_COUNT);

    //statemachine 1 initiates cry when data in TX queue
    uint rb_howl_addr = pio_add_program(RB_PIO, &howl_start_program);
    howl_start_program_init(RB_PIO, RB_HOWL_SM, rb_howl_addr, KEY_OUT_PIN, 1000 * 1000);

}

//return:
//-1 error, reset me!
// 0 still busy, expecting more data. Feed me!
// 1 done. Seen n rabies
int statemachine(bool bit, bool reset, int *n_rabies)
{
    static uint input_id, wolf_id, state = 0;

    /*
      " Everyone knows that debugging is twice as hard as writing
        a program in the first place. So if you're as clever as
        you can be when you write it, how will you ever debug it? "
                                               -- Brian Kernighan
    */

    if (reset) {
        state = 0;
        return 0;
    }

    switch (state) {
        case 0:                     //wait for wakeup
            wolf_id = -1;
            if (bit) {
                state = 1;
            } else {
                //something wrong. we should reset.
                state = 0;
                return -1;
            }
            return 0;
        case 1:                     //recv next header
            if (bit) {
                state = 0;
                *n_rabies = wolf_id+1;
                return 1; //return number of rabies spotted
            }
            state = 2;
            input_id = K;           //we expect to rcv K bits from neighbor
            wolf_id++;
            return 0;
        case 2:                     //read 1 of K bits
            key_states_write[wolf_id] <<= 1;
            key_states_write[wolf_id] |= bit;
            if (--input_id) {
                return 0; //stay in this state
            } else {
                state = 1; //go listen for next rabi
                return 0;
            }
    }
}


#define OPS_PER_TICK 2 //depends on howl_count program
#define us_to_tick(_us)   ((uint32_t)(_us * (1e-6 / ((float)OPS_PER_TICK / FREQ_RB_COUNT))))
#define tick_to_ns(_ti)   ((uint32_t)(_ti * (OPS_PER_TICK * 1000 / ( FREQ_RB_COUNT / 1000000))))

#define T1US_IN_TICKS   us_to_tick(1)
#define TRESET_TICKS    us_to_tick(TRESET * 2)
#define T0H_TICKS       us_to_tick(T0H)
#define T0L_TICKS       us_to_tick(T0L)
#define T1H_TICKS       us_to_tick(T1H)
#define T1L_TICKS       us_to_tick(T1L)
/* Only to be called after receive_bits_available returned true!
 * Returns:
 *  -2 for error
 *  -1 for reset
 *   0 for a zero bit
 *   1 for a one bit
 */
static int timing_to_bit(uint32_t t)
{
    /* TODO define a proper margin. 10 ticks seems to work, more is better I guess */
    /*printf("pulse length %d %d\r\n", t, tick_to_ns(t));*/
    if (t > 400) return -1;
    if (t > 190) return 1;
    return 0;
    /*switch (t) {//126 313*/
        /*case T0H_TICKS - 20 ... T0H_TICKS + 3 * T1US_IN_TICKS:*/
            /*return 0;*/
        /*case T1H_TICKS - 10 ... T1H_TICKS + 3 * T1US_IN_TICKS:*/
            /*return 1;*/
        /*case (TRESET_TICKS - 3 * T1US_IN_TICKS ) ... (TRESET_TICKS + 3 * T1US_IN_TICKS):*/
            /*return -1; //Reset*/
        /*default:*/
            /*printf("Unknown pulse length %d %d\r\n", t, tick_to_ns(t));*/
            /*return -2; //panic, unknown byte length?*/

    /*}*/
}

#define WATCHDOG_TIMEOUT (100 * 1000)  /* uS after which the watchdog intervenes */

#define RESET_WATCHDOG() t_watch_dog = t_now_us + WATCHDOG_TIMEOUT
#define DATA_READY()     !pio_sm_is_rx_fifo_empty(RB_PIO, RB_LISTEN_SM)
#define READ()           timing_to_bit(pio_sm_get(RB_PIO, RB_LISTEN_SM))
#define WRITE(_bit)      pio_sm_put_blocking(RB_PIO, RB_HOWL_SM, _bit)
#define SEND_RESET()     pio_sm_put_blocking(RB_PIO, RB_HOWL_SM, -1); if (0) printf("RESET\n")
#define RESET_MSG        (-1)
#define ERROR_MSG        (-2)

#define GOTO_RESET() {\
    if (0) set_leds_red();\
    RESET_WATCHDOG();\
    state = STATE_RESET;\
    SEND_RESET();\
    break;\
}
#define GOTO_COOLDOWN() {\
    if (0) set_leds_yellow();\
    RESET_WATCHDOG();\
    state = STATE_COOLDOWN;\
    break;\
}
#define GOTO_GOOD() {\
    if (0) set_leds_green();\
    RESET_WATCHDOG();\
    state = STATE_GOOD;\
    (void)statemachine(0, true, NULL);\
    WRITE(1);\
    WRITE(1);\
    break;\
}

int main()
{
    enum states {STATE_GOOD, STATE_COOLDOWN, STATE_RESET};
    int state;
    int good_cnt = 0;
    absolute_time_t t_watch_dog = 0;
    absolute_time_t t_led_task = 0;

    setup();

    state = STATE_COOLDOWN;

    while (1) {
        tud_task(); // tinyusb device task, do always. otherwise we do not have serial debug
        absolute_time_t t_now_us = get_absolute_time(); //us

        switch (state) {
            // In the GOOD state we are happy.
            // We initiated a new transmission
            // and are now waiting for all data.
            case STATE_GOOD:
                if (t_now_us > t_watch_dog) GOTO_RESET(); //we expect data, but got silence. Do reset.
                if (!DATA_READY()) break;                 //still waiting for data

                uint32_t data = READ();                   //
                if (data == RESET_MSG) GOTO_COOLDOWN();       //unsolicited reset, someone must have panicked
                if (data == ERROR_MSG) GOTO_COOLDOWN();   //now I'm panicking!

                int n;
                int r = statemachine(data, false, &n);  //feed it to our statemachine
                if (r==-1) GOTO_RESET();                //statemachine indicated it is confused.
                good_cnt++;
                if( good_cnt % 10000 == 0){
                    printf("Happy for %d\n", good_cnt);
                }
                if (!r) {
                    RESET_WATCHDOG();                   //data received but not done yet, watchdog takes chillpill
                    break;
                }
                //We are done! we recvd a good cry. Now do other stuff.
                //TODO we never get here...
                //
                /*printf("Seen %d rabies in transmission\n", n);*/
                flip();
                hid_task(t_now_us);
                if (t_now_us > t_led_task) {
                    t_led_task = t_now_us + 20000;
                    update_leds(25, t_now_us);
                }
                //Now we have done stuff do a sanity check and check we
                //did not received any data in the mean time.
                if (DATA_READY()) {
                    GOTO_RESET();                   //shit, something is wrong
                } else {
                    GOTO_GOOD();                    //everybody agrees!
                }

            // Shit has gone sour. Lets wait until we see no more
            // activity at all for at least WDT. Then we reset everyone
            // so we are all on the same page.
            case STATE_COOLDOWN:
                if(good_cnt > 0) {
                    printf("Good count is %d\n", good_cnt);
                    good_cnt = 0;
                }
                if (t_now_us > t_watch_dog) GOTO_RESET(); //No yapping heard. Good. Do reset.
                if (!DATA_READY()) break;                 //Everyone is still silent. Good
                (void)READ();                             //Hush!
                GOTO_COOLDOWN();                          //Someone ruined it, now we all need to wait again.

            // We've send a RESET_MSG. Now listen for a RESET_MSG.
            // If we hear something else we go back to the cooldown.
            case STATE_RESET:
                if (t_now_us > t_watch_dog) GOTO_RESET(); //RESET_MSG not recieved in time, send another
                if (!DATA_READY()) break;                 //I guess we have to wait
                if (READ() != RESET_MSG) GOTO_COOLDOWN();     //Some rabi is still yapping, send him to the icebox!
                GOTO_GOOD();                              //All aboard!
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


void hid_task(absolute_time_t t_now_us)
{
    static absolute_time_t t_next = 0;
    if ( t_now_us < t_next) return;
    t_next = t_now_us + 10000; //every 10 ms

    if ( !tud_hid_ready() ) return;

    uint8_t pressed_keys[6] = { 0 };

    for (int i = 0, j = 0; i < W; i++) {
        if (key_states_read[i]) {
            pressed_keys[j++] = key_mapping[i%KEYMAP_LEN];
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
