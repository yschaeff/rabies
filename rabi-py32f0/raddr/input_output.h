#pragma once

#include <py32f0xx_hal.h>
/* Bit timings to store when receiving */
#define RX_FIFO_SIZE (128/4) //Must be a power of 2

/* Pulse we can buffer when queueing output */
#define TX_FIFO_SIZE (128/4) //Must be a power of 2

/* Run at maximum speed */
#define INPUT_TIMER_DESIRED_BASE_TICK     (1.0/HSI_VALUE)
#define INPUT_TIMER_DIVIDER         ((uint32_t)(HSI_VALUE * INPUT_TIMER_DESIRED_BASE_TICK))
//For 24Mhz this is 41ns
#define TIMER_ACTUAL_TIME_PER_TICK (1.0 * INPUT_TIMER_DIVIDER / HSI_VALUE)

#define us_to_tick(tmo) ((uint32_t)((tmo) * (1e-6 / TIMER_ACTUAL_TIME_PER_TICK)))
#define tick_to_ns(_ti) ((uint32_t)((1000 * (_ti))/ (((HSI_VALUE) / INPUT_TIMER_DIVIDER) / 1000000)))

#if defined(USE_SEMIHOSTING)
#define RADDR_INPUT_DEBUG
#endif
#undef RADDR_INPUT_DEBUG
void raddr_input_output_init(void);
int receive_bit(void);
uint16_t received_bits_read(void);
uint32_t receive_bits_available(void);
void receive_bits_flush(void);


void raddr_output_schedule(uint16_t th, uint16_t tl);
void raddr_output_bulk_begin(void);
void raddr_output_bulk_schedule(uint16_t th, uint16_t tl);
void raddr_output_bulk_end(void);
