#pragma once

#include <py32f0xx_hal.h>
/* Run at maximum speed */
#define INPUT_TIMER_DESIRED_BASE_TICK     (1.0/HSI_VALUE)
#define INPUT_TIMER_DIVIDER         ((uint32_t)(HSI_VALUE * INPUT_TIMER_DESIRED_BASE_TICK))

#if defined(USE_SEMIHOSTING)
#define RADDR_INPUT_DEBUG
#endif
#undef RADDR_INPUT_DEBUG
void raddr_input_capture_init(void);
int receive_bit(void);
uint32_t receive_bits_available(void);
