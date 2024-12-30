#pragma once

#include <py32f0xx_hal.h>
#define INPUT_TIMER_DESIRED_BASE_TICK     (1 * 1e-6)
#define INPUT_TIMER_DIVIDER         ((uint32_t)(HSI_VALUE * INPUT_TIMER_DESIRED_BASE_TICK))

#define RADDR_INPUT_DEBUG
void raddr_input_capture_init(void);
