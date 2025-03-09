#include <stdbool.h>

#include "input_output.h"

#include "timing_debug.h"

#ifdef TIMING_DEBUG

/* Simple loop for sanity check. If this fails, code is broken badly. */
static void send_single_pulse(void)
{
    receive_bits_flush();

    uint32_t tstart = HAL_GetTick();
    int tmo = 1000;
    for(int i = 0;;) {
        int now = HAL_GetTick();
        int th = 25;
        int tl = 100;
        int cnt = 10;
        if (now - tstart > tmo) {
            tstart = now;
            i++;
            raddr_output_bulk_begin();
            for(int j = 0; j < cnt; j++) {
                raddr_output_bulk_schedule(th, tl);
            }
            raddr_output_bulk_end();
        }
        int a = receive_bits_available();
        if (a >= cnt) {
            for(;a > 0; a--) {
                uint32_t rx = received_bits_read();
                printf("rx - th = %d - %ld = %ld\r\n", th, rx, rx - th);
            }
            break;
        }
    }
}

static void determine_fixed_latency(void)
{
    int cnt = 100;
    fixed_latency = 0;

    /* Drain any available bits */
    receive_bits_flush();

    raddr_output_schedule(cnt, us_to_timer_tick(5));

    for (;0 == receive_bits_available();) {
    }

    uint32_t t = received_bits_read() & 0xFFFF;

    fixed_latency = t - cnt;
    printf("latency = %d %d, %ld\r\n", fixed_latency, cnt, t);
}

/* Send pulses and receive them, returning the value with the most 'diff' */
static int send_pulses(uint16_t pulse_width, uint16_t low_width, uint8_t bits_to_send, bool use_bulk)
{
    if(receive_bits_available()) {
        printf("Huh bits before we start? %ld\r\n", receive_bits_available());
        receive_bits_flush();
    }

    if (use_bulk) {
        raddr_output_bulk_begin();
        for(int i = 0; i < bits_to_send; i++) {
            raddr_output_bulk_schedule(pulse_width, low_width);
        }
        raddr_output_bulk_end();
    } else
    {
        for(int i = 0; i < bits_to_send; i++) {
            raddr_output_schedule(pulse_width, low_width);
        }
    }
    const int tmo = 100;
    int max = 0;
    uint32_t tstart = HAL_GetTick();
#if defined(WAIT_FOR_SINGLE_BIT)
    for(int i = 0; i < bits_to_send; i++) {
        for(;;) {
            if (HAL_GetTick() - tstart > tmo) {
                //printf("%d != %d in %d\r\n", a, bits_to_send, tmo);
                return 666666; //Return a "very-bad-value"
            }
            if(receive_bits_available()) {
                uint32_t t = received_bits_read() & 0xFFFF;

                int diff = t - pulse_width;
                //printf("diff %d, %d\r\n", diff, pulse_width);
                if (diff < 0)
                    diff *= -1;
                if (diff >= max) {
                    max = diff;
                }
                break;
            }
        }
    }
    return max;
#else /* Wait for all bits before processing them */
    /* Wait for all the transmitted bits to be received by the RX ISR */
    for (;;) {
        int a = receive_bits_available();
        if (a >= bits_to_send)
            break;
        if (HAL_GetTick() - tstart > tmo) {
            //printf("%d != %d in %d\r\n", a, bits_to_send, tmo);
            max = 666666; //Return a "very-bad-value"
            break;
        }
    }

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
#endif
}

void debug_find_lowest_values(void)
{
    if (0)
        send_single_pulse();

    /* First find the absolute minimum we should stay above. */
    determine_fixed_latency();

    /* Start trying find from some sane values */
    const int default_pulse_width = 40; //~1650ns
    const int default_low_width = 100; //~4100ns

    /* max_diff we accepts between pulse_width and rx */
    const int max_diff = 3;

    /* The found maximums */
    int max_pulse_width = 0;
    int max_low_width = 0;

    /* Loops that assumes input is connected to the output */
    for(int bulk = 0; bulk < 2; bulk++) {
    for(int bits_to_send = 1; bits_to_send < TX_FIFO_SIZE; bits_to_send++)
    {
        bool use_bulk = bulk == 0;
        int pulse_width = default_pulse_width;
        int res1 = 0, res2 = 0;
        for(; pulse_width > fixed_latency + 1; pulse_width--) {
            int res = send_pulses(pulse_width, default_low_width, bits_to_send, use_bulk);
            bool ok = res < max_diff;
            if (!ok) {
                break;
            }
            if (res > res1)
                res1 = res;
        }
        int zero_cnt = default_low_width;
        for(; zero_cnt > fixed_latency + 1; zero_cnt--) {
            int res = send_pulses(default_pulse_width, zero_cnt, bits_to_send, use_bulk);
            bool ok = res < max_diff;
            if (!ok) {
                break;
            }
            if (res > res2)
                res2 = res;
        }

        /* Previous was okay, this one failed */
        zero_cnt++;
        pulse_width++;
        if (pulse_width > max_pulse_width)
            max_pulse_width = pulse_width;
        if (zero_cnt > max_low_width)
            max_low_width = zero_cnt;

        printf("% 3d % 3ldns / % 3d % 3ldns. Total % 5ldns, Bits: % 2d bulk: %d. Diff %02d %02d\r\n",
                pulse_width, tick_to_ns(pulse_width),
                zero_cnt, tick_to_ns(zero_cnt),
                tick_to_ns(pulse_width + zero_cnt),
                bits_to_send, use_bulk,
                res1, res2);
    }
    } /* use bulk */

    printf("Starting loop for %d/%d\r\n", max_pulse_width, max_low_width);
    for(;;) {
        /* Send on maximum speed */
        int res = send_pulses(max_pulse_width, max_low_width, TX_FIFO_SIZE - 1, true);
        if(res > 0) {
            printf("Worst case %d\r\n", res);
            break;
        }
    }
    for(;;) {
    }
}
#endif

