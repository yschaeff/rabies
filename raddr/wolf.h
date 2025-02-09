#ifndef WOLF_H
#define WOLF_H

#include <stdbool.h>
#include <stdint.h>
#include "output_timer.h"

/* Times in uS */
#define TRESET 64 //Limit by the pico pi code.
#define TTOTAL 20 //Total duration of every 'bit'
#define T0H 6
#define T1H 10
#define T0L (TTOTAL - T0H)
#define T1L (TTOTAL - T1H)

#define K 1

// Start of transmission
// Will be followed by either a HOWL or a BARK
#define GROWL 1
// End of transmission
// Will be quiet for at least TRES
#define HOWL 1
// Will be followed by a dataframe of K bits
#define BARK (!HOWL)

enum states {
    S_REST,       //do nothing
    S_ALERT,      //listen for howl
    S_HOWL,       //transmit frame
    S_BARK        //copy frame
};

extern bool K_BINARY_INPUTS[K];

/**
 * bark a full bit. This is useful for sending a single bit
 */
static inline void bark_full(int bit)
{
    if(bit) {
        raddr_output_schedule(1, us_to_timer_tick(T1H));
        raddr_output_schedule(0, us_to_timer_tick(T1L));
    } else {
        raddr_output_schedule(1, us_to_timer_tick(T0H));
        raddr_output_schedule(0, us_to_timer_tick(T0L));
    }
}

/**/
static inline void bark_bulk(int bit)
{
    if(bit) {
        raddr_output_bulk_schedule(1, us_to_timer_tick(T1H));
        raddr_output_bulk_schedule(0, us_to_timer_tick(T1L));
    } else {
        raddr_output_bulk_schedule(1, us_to_timer_tick(T0H));
        raddr_output_bulk_schedule(0, us_to_timer_tick(T0L));
    }
}

#endif
