#include <stdbool.h>

#include "input_capture.h"

#include "timing_debug.h"

#ifdef TIMING_DEBUG
static void send_single_pulse(void)
{
    uint32_t tstart = HAL_GetTick();
    raddr_output_schedule(us_to_timer_tick(32), us_to_timer_tick(12));
    for(int i = 0;;) {
        int tmo = 1000;
        int now = HAL_GetTick();
        if (now - tstart > tmo) {
            i++;
            printf("Send pulse %d\r\n", i);
            tstart = now;
        }
        if (receive_bits_available())
        {
            //break;
        }
    }
}
static void determine_fixed_latency(void)
{
    int cnt = 100;
    fixed_latency = 0;
    raddr_output2_schedule(100, us_to_timer_tick(10));

    for (;0 == receive_bits_available();) {
    }

    uint32_t t = received_bits_read() & 0xFFFF;

    fixed_latency = t - cnt;
    printf("latency = %d %ld\r\n", fixed_latency, tick_to_ns(fixed_latency));
}

/* Send pulses and receive them, returning the value with the most 'diff' */
static int send_pulses(uint16_t pulse_width, uint16_t low_width, uint8_t bits_to_send, bool use_bulk)
{
    if (use_bulk) {
        raddr_output_bulk_begin();
        for(int i = 0; i < bits_to_send; i++) {
            raddr_output_bulk_schedule(1, pulse_width);
            raddr_output_bulk_schedule(0, low_width);
        }
        raddr_output_bulk_end();
    } else
    {
        for(int i = 0; i < bits_to_send; i++) {
            raddr_output_schedule(1, pulse_width);
            raddr_output_schedule(0, low_width);
        }
    }

    /* Wait for all the transmitted bits to be received by the RX ISR */
    uint32_t tstart = HAL_GetTick();
    for (;;) {
        int a = receive_bits_available();
        if (a >= bits_to_send)
            break;
        int tmo = 100;
        if (HAL_GetTick() - tstart > tmo) {
            //printf("%d != %d in %d\r\n", a, bits_to_send, tmo);
            return 666666; //Return a "very-bad-value"
        }
    }

    int max = 0;
    while (receive_bits_available()) {
        uint32_t t = received_bits_read() & 0xFFFF;

        int diff = t - pulse_width;
        //printf("diff %d, %d\r\n", diff, pulse_width);
        if (diff < 0)
            diff *= -1;
        if (diff >= max) {
            max = diff;
        }
    }
    return max;
}

void debug_find_lowest_values(void)
{
    if (0)
        send_single_pulse();
    /* First find the absolute minimum we should stay above. */
    determine_fixed_latency();
    /* FIXED_LATENCY * 2 seems to be the minimum to make it work reliably */
    const int default_pulse_width = FIXED_LATENCY * 2;
    const int default_low_width = FIXED_LATENCY * 2;
    const int max_diff = us_to_timer_tick(2);

    /* Loops that assumes input is connected to the output */
    for(int bulk = 0; bulk < 2; bulk++) {
    for(int bits_to_send = 1; bits_to_send <= TX_FIFO_SIZE; bits_to_send++)
    {
        bool use_bulk = bulk == 0;
        int pulse_width = default_pulse_width;
        int res1 = 0, res2 = 0;
        for(; pulse_width > fixed_latency + 1; pulse_width--) {
            int res = send_pulses(pulse_width, default_low_width, bits_to_send, use_bulk);
            if (res > res1)
                res1 = res;
            bool ok = res < max_diff;
            if (!ok) {
                break;
            }
        }
        int zero_cnt = default_low_width;
        for(; zero_cnt > fixed_latency + 1; zero_cnt--) {
            int res = send_pulses(default_pulse_width, zero_cnt, bits_to_send, use_bulk);
            if (res > res2)
                res2 = res;
            bool ok = res < max_diff;
            if (!ok) {
                break;
            }
        }
        printf("%03d %03ld / %03d %03ld. Total %05ld, Bits: %d bulk: %d. %02d %02d\r\n",
                pulse_width, tick_to_ns(pulse_width),
                zero_cnt, tick_to_ns(zero_cnt),
                tick_to_ns(pulse_width + zero_cnt),
                bits_to_send, use_bulk,
                res1, res2);
    }
    } /* use bulk */

    /* Try worst case we should support? */
    int res = send_pulses(FIXED_LATENCY + 5, FIXED_LATENCY + 5, 8, false);
    printf("Worst case %d\r\n", res);

    for(;;) {

    }
}
#endif

