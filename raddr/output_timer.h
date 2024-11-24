#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "py32f0xx_hal_conf.h"
void raddr_output_init(void);

/* Just run it at maximum speed for maximum resolution */
//#define TIMER_DESIRED_BASE_TICK     (1/24e6)
//#define TIMER_DIVIDER               ((uint32_t)(HSI_VALUE * TIMER_DESIRED_BASE_TICK))
#define TIMER_DIVIDER               (1ul)
_Static_assert(TIMER_DIVIDER < 65535, "Divider too large. Consider lowering input clock");

#define TIMER_ACTUAL_TIME_PER_TICK (1.0 * TIMER_DIVIDER / HSI_VALUE)
//Only feed this constants. Otherwise we drag in floating point code!
//Also note that tmo should be at least 4, otherwise we underflow
static inline uint16_t us_to_timer_tick(uint16_t tmo) {
    // - 3.5 because our ISR takes about so long
    uint16_t t = (tmo) * (1e-6 / TIMER_ACTUAL_TIME_PER_TICK);
    t -= (uint16_t)(3.5 * (1e-6 / TIMER_ACTUAL_TIME_PER_TICK));
    return t;
}

void raddr_output_schedule(bool bit, uint16_t tmo);

