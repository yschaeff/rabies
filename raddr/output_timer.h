#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "py32f0xx_hal.h"

void raddr_output_init(void);

#undef RADDR_OUTPUT_DEBUG
#if defined(USE_SEMIHOSTING)
//#define RADDR_OUTPUT_DEBUG
#endif

/* Just run it at maximum speed for maximum resolution */
#define TIMER_DESIRED_BASE_TICK     (1.0/HSI_VALUE)
#define TIMER_DIVIDER               ((uint32_t)(HSI_VALUE * TIMER_DESIRED_BASE_TICK))
_Static_assert(TIMER_DIVIDER < 65535, "Divider too large. Consider lowering input clock");

#define TIMER_ACTUAL_TIME_PER_TICK (1.0 * TIMER_DIVIDER / HSI_VALUE)
//Only feed this constants. Otherwise we drag in floating point code!
//Also note that tmo should be at least 6, otherwise we underflow
static inline uint16_t us_to_timer_tick(uint32_t tmo) {
    return (tmo) * (1e-6 / TIMER_ACTUAL_TIME_PER_TICK);
}

/* Schedule bit to be output for tmo long.
 * tmo is specified in TIMER_ACTUAL_TIME_PER_TICK */
void raddr_output_schedule(bool bit, uint16_t tmo);


void raddr_output_bulk_begin(void);
void raddr_output_bulk_schedule(bool bit, uint16_t tmo);
void raddr_output_bulk_end(void);

static inline void raddr_output_debug(void)
{
#if defined(RADDR_OUTPUT_DEBUG)
    HAL_Delay(1000);
    uint16_t t = us_to_timer_tick(10);
    for (int i = 0; i < 16/2; i++) //FIFO_SIZE / 2
    {
        raddr_output_schedule(1, t);
        raddr_output_schedule(0, t);
    }
#endif
}

