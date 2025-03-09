#pragma once

#ifdef USE_SEMIHOSTING
#undef TIMING_DEBUG
extern uint8_t fixed_latency;
void debug_find_lowest_values(void);
#endif
