#include <stdbool.h>

#include "input_output.h"

#include "timing_debug.h"

#ifdef TIMING_DEBUG

/* Simple loop for sanity check. If this fails, code is broken badly. */
static void send_single_pulse(void)
{
    receive_bits_flush();

    uint32_t tstart = HAL_GetTick();
    int tmo = 10;
    for(int i = 0;;) {
        int now = HAL_GetTick();
        const int th = 40;
        const int tl = 300;
        const int cnt = 16;

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
                //printf("rx - th = %ld - %d = %ld\r\n", rx, th, rx - th);
            }
            //break;
        }
    }
}

static void determine_fixed_latency(void)
{
    int cnt = 100;
    fixed_latency = 0;

    /* Drain any available bits */
    receive_bits_flush();

    raddr_output_schedule(cnt, us_to_tick(5));

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
    int max = 0;
    uint32_t tstart = HAL_GetTick();
#define WAIT_FOR_SINGLE_BIT
#if defined(WAIT_FOR_SINGLE_BIT)
    const int tmo = 100;
    for(int i = 0; i < bits_to_send; i++) {
        for(;;) {
            if (HAL_GetTick() - tstart > tmo) {
                //printf("%d != %d in %d\r\n", a, bits_to_send, tmo);
                receive_bits_flush();
                return i - bits_to_send; //Return a "very-bad-value"
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
    int tmo = 5;
    int old_a = 0;
    for (;;) {
        int a = receive_bits_available();
        if (a >= bits_to_send)
            break;
        if (a != old_a) {
            tstart = HAL_GetTick();
            old_a = a;
            continue;
        }
        if (HAL_GetTick() - tstart > tmo) {
            //printf("%d != %d in %d\r\n", a, bits_to_send, tmo);
            receive_bits_flush();
            return a - bits_to_send; //Return a "very-bad-value"
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
    int tests = 0;
    receive_bits_flush();

    if (0)
        send_single_pulse();

    /* First find the absolute minimum we should stay above. */
    if (0)
        determine_fixed_latency();

    /* Start trying find from some sane values
     * Experiments show that ~21/22 tick is the minimum for + (~1us is the minimum supported width)
     *
     * The jury is still out on the - width.
     *
     * It seems to depend on the number of bits we send in bulk.
     * But also the way we wait for the bits.
     * Awaiting them all seems to need more time per bit then handling them as soon as possible?
     *
     *
     * */
    const int default_pulse_width = us_to_tick(4);
    const int default_low_width = us_to_tick(40);
    const int min_low_width = 400;

    /* max_diff we accepts between pulse_width and rx */
    const int max_diff = 50;

    /* The found maximums */
    int max_pulse_width = 0;
    int max_low_width = 0;

    /* Loops that assumes input is connected to the output */
    if (1)
    for(int bulk = 0; bulk < 2; bulk++) {
    for(int bits_to_send = 1; bits_to_send <= TX_FIFO_SIZE; bits_to_send++)
    {
        int missed = 0;
        bool use_bulk = bulk == 0;
        int pulse_width = default_pulse_width;
        int res1 = 0, res2 = 0;
        for(; pulse_width > fixed_latency + 1; pulse_width--) {
            int res = send_pulses(pulse_width, default_low_width, bits_to_send, use_bulk);
            if (res < 0) {
                missed++;
                break;
            }

            bool ok = res < max_diff;
                if (res > res1)
                    res1 = res;
            if (!ok) {
                break;
            }
        }
        int zero_cnt = default_low_width;
        for(; zero_cnt > min_low_width; zero_cnt--) {
            int res = send_pulses(default_pulse_width, zero_cnt, bits_to_send, use_bulk);
            if (res < 0) {
                missed++;
                break;
            }
            bool ok = res < max_diff;
            if (res > res2)
                res2 = res;
            if (!ok) {
                break;
            }
        }

        /* Previous was okay, this one failed */
        zero_cnt++;
        pulse_width++;
        if (pulse_width > max_pulse_width)
            max_pulse_width = pulse_width;
        if (zero_cnt > max_low_width)
            max_low_width = zero_cnt;

        printf("% 3d % 3ldns / % 3d % 3ldns. Total % 5ldns, Bits: % 2d bulk: %d. Diff %02d %02d. Miss %d\r\n",
                pulse_width, tick_to_ns(pulse_width),
                zero_cnt, tick_to_ns(zero_cnt),
                tick_to_ns(pulse_width + zero_cnt),
                bits_to_send, use_bulk,
                res1, res2, missed);
        tests++;
    }
    } /* use bulk */
    printf("Tests %d. Found %d/%d (%ldnS/%ldnS)\r\n", tests, max_pulse_width, max_low_width, tick_to_ns(max_pulse_width), tick_to_ns(max_low_width));

    /* Keep sending pulses of equal size and see if any fails, if it does. Increment low_width */
    if (1) {
        int okay = 0;
        max_pulse_width = 40;
        max_low_width += 100;
        printf("Starting loop for %d/%d (%ldnS/%ldnS)\r\n", max_pulse_width, max_low_width, tick_to_ns(max_pulse_width), tick_to_ns(max_low_width));
        for(int t = 0; ; t++) {
            int res = send_pulses(max_pulse_width, max_low_width, TX_FIFO_SIZE - 1, true);
            if(res >= 0) {
                if (res > 3) {
                    //printf("Diff %d\r\n", res);
                }
                else {
                    okay++;
                }
                //printf("Worst case %d\r\n", res);
                //break;
            }
            else {
                printf("Missing %d %d/%d %d\r\n", res * -1,okay,t, max_low_width);
                //max_low_width++;
            }
        }
    }

    if (0) {
        /* Send different sizes +pulses to find.
         * If one fails, increment min until we no longer loose any bits. */
        int okay = 0;
        int min = us_to_tick(45) + 5;
        const int start = 30;
        const int inc = 10;
        printf("Starting test with min = %ld. + %d inc = %d \r\n", tick_to_ns(min), start, inc);
        for(int t = 0; ; t++) {
            /* Send on maximum speed */
            int bits_to_send = TX_FIFO_SIZE;
            raddr_output_bulk_begin();
            for(int i = 0; i < bits_to_send; i++) {
                int expect = start + i * inc;
                raddr_output_bulk_schedule(expect, min);
            }
            raddr_output_bulk_end();

            /* Wait until all bits are received */
            uint32_t tstart = HAL_GetTick();
            const int tmo = 40;
            for (;;) {
                int a = receive_bits_available();
                if (a >= bits_to_send) {
                    okay++;
                    break;
                }
                if (HAL_GetTick() - tstart > tmo) {
                    //missed this bit
                    min++;
                    printf("Extending min to %ld\r\n", tick_to_ns(min));

                    break;
                }
            }
            for(int i = 0; receive_bits_available(); i++) {
                int duration = received_bits_read();
                int expect = start + i * inc;
                int diff = duration - expect;
                if (diff < 0)
                    diff *= -1;
                if (diff > 3) {
                    printf("@%d diff = %d. expect/got %d/%d okay/total %d/%d \r\n", i, diff, expect, duration, okay, t);
                    if (diff > inc - 5)
                        i++;
                }
            }
            /* Run once per second instead of full speed */
            if(0) {
                tstart = HAL_GetTick();
                for (;;) {
                    if (HAL_GetTick() - tstart > 1000) {
                        break;
                    }
                }
            }
        }
    }
    for(;;) {
    }
}
#endif

