#include <stdio.h>
#include <stdbool.h>
#include <py32f0xx_hal.h>
#include "wolf.h"
#include "input_capture.h"

//For 24Mhz this is 41ns
#define INPUT_TIMER_ACTUAL_TIME_PER_TICK (1.0 * INPUT_TIMER_DIVIDER / HSI_VALUE)
#define us_to_tick(_us)   ((uint32_t)(_us * (1e-6 / INPUT_TIMER_ACTUAL_TIME_PER_TICK)))
//Multiply by 1million to ensure we have enough range. HSI is normally 24Mhz.
#define tick_to_ns(_ti)   ((1000000 * _ti)/ (((HSI_VALUE) / INPUT_TIMER_DIVIDER) / 1000))

#define TRESET_TICKS    us_to_tick(TRESET)
#define T0H_TICKS       us_to_tick(T0H)
#define T0L_TICKS       us_to_tick(T0L)
#define T1H_TICKS       us_to_tick(T1H)
#define T1L_TICKS       us_to_tick(T1L)

//#define MAX(_a,_b) (_a > _b ? _a : _b)
//#define MIN(_a,_b) (_a < _b ? _a : _b)
//#define UPPER_LIMIT ((uint32_t)(MAX(T0L_TICKS * 1.2, T0H_TICKS * 1.2)))
//#define LOWER_LIMIT ((uint32_t)(MIN(T0L_TICKS * 0.8, T0H_TICKS * 0.8)))

#define FIFO_SIZE 16 //Must be a power of 2
_Static_assert((FIFO_SIZE & (FIFO_SIZE - 1)) == 0 , "FIFO_SIZE needs to be a power of 2 to make access FAST");

/* The fifo to hold the barks'n'howls we received */
static struct {
    uint8_t write;
    uint8_t read;
    volatile uint32_t size;
    uint32_t buf[FIFO_SIZE];
} fifo = {.size = 0};

static bool fifo_is_full(void)
{
    return fifo.size >= FIFO_SIZE;
}

/* To be used by the main thread (or any other single thread).
 * Returns the number of bits received */
uint32_t receive_bits_available(void)
{
    return fifo.size;
}

/* Extract data from the fifo */
uint32_t received_bits_read(void)
{
    uint32_t idx = fifo.read;
    uint32_t d = fifo.buf[idx];
    fifo.read = (idx + 1) % FIFO_SIZE;

    /* Updating size must be atomic! Against TIM1 ISR(s) */
    HAL_NVIC_DisableIRQ(TIM1_CC_IRQn);
    fifo.size--;
    HAL_NVIC_EnableIRQ(TIM1_CC_IRQn);

    return d;
}


/* Only to be called after receive_bits_available returned true!
 * Returns:
 *  -2 for error
 *  -1 for reset
 *   0 for a zero bit
 *   1 for a one bit
 */
int receive_bit(void)
{
    uint32_t d = received_bits_read();
    uint16_t t = d & 0xFFFF;
#if defined(RADDR_INPUT_DEBUG)
    printf("t %d (%ldnS) "
            "%ld %ld %ld" "\r\n",
            t, tick_to_ns(t),
            T1H_TICKS,
            T0H_TICKS,
            TRESET_TICKS
            );
#endif

    /* TODO define a proper margin. 10 ticks seems to work, more is better I guess */
    switch (t) {
        /* TO*/
        case T0H_TICKS - 10 ... T0H_TICKS + 10:
            return 0;
        case T1H_TICKS - 10 ... T1H_TICKS + 10:
            return 1;
        case TRESET_TICKS - 10 ... TRESET_TICKS + 10:
            return -1; //Reset
        default:
#if defined(RADDR_INPUT_DEBUG)
            printf("Unknown pulse length %d\r\n", t);
#endif
            return -2; //panic, unknown byte length?

    }
}

/* Only to be used by the ISR! */
static inline void fifo_write(uint32_t tmo)
{
    uint32_t idx = fifo.write;
    if (fifo_is_full()) {
        //TODO send reset?
        //This should never happen, programmers error.
        return;
    }
    fifo.buf[idx] = tmo;
    fifo.write = (idx + 1) % FIFO_SIZE;

    /* Updating size must be atomic! Against main thread */
    fifo.size++;
}

/* The ISR for TIM1 compare */
void TIM1_CC_IRQHandler(void)
{
    uint32_t tmo = TIM1->CCR2;

    /* Clear/acknowledge pending interrupts */
    TIM1->SR = 0;

#if defined(RADDR_INPUT_DEBUG)
    /* Add status for debug purpose */
    uint32_t status = TIM1->SR;
    tmo |= status << 16;
#endif
    fifo_write(tmo);

#if 0 && defined(RADDR_INPUT_DEBUG)
    /* Printing in an ISR is very bad idea! */
    printf(
            "CCR1: %ld \r\n"
            "CCR2: %d \r\n"
            "CCR3: %ld \r\n"
            "CCR4: %ld \r\n"
            "isr flags: %x \r\n"
            "",
            TIM1->CCR1,
            tmo,
            TIM1->CCR3,
            TIM1->CCR4,
            status
            );
#endif


}

void raddr_input_capture_init(void)
{
    /* Setup timer1, which is the only one with input capture.
     *
     * The basic idea is to reset the timer using input 1 and get the length of the +pulse at input 2
     * input 1 defines the full length of the bit.
     * */

    /* Enable the clock towards TIM1 */
    __HAL_RCC_TIM1_CLK_ENABLE();

    //TIM1->CCER = 0; //Disable all comparators

    uint32_t tmp;

    /* Setup general timer config */
    TIM1->PSC = INPUT_TIMER_DIVIDER - 1;

    /* Setup capture inputs */
    tmp = 0;
    tmp |= TIM_CCMR1_CC1S_0; //CC1 = 01 == Input from TI1
    tmp |= TIM_CCMR1_CC2S_1; //CC2 = 10 == Input from TI1
    //We do not need input filtering for now
    TIM1->CCMR1 = tmp;

    /* Enable capture.
     * Setup input 1 as rising edge (00)
     * Setup input 2 as falling edge (10)
     * */
    tmp = 0;
    tmp |= TIM_CCER_CC1E;  //enable capture inp1
    tmp |= TIM_CCER_CC2E;  //enable capture inp2
    tmp |= TIM_CCER_CC2P; //falling edge
    TIM1->CCER = tmp;

    /* Reset the count on input detection
     * TS = 101
     * SMS = 100 */
    tmp = 0;
    /* Filtered input 1: TI1FP1 => TS = 101 */
    tmp |= TIM_SMCR_TS_0;
    tmp |= TIM_SMCR_TS_2;

    /* SMS 100: Reset on edge detection on T1 */
    tmp |= TIM_SMCR_SMS_2;
    TIM1->SMCR = tmp;

    /* Only enable compare 2 interrupt for now */
    TIM1->DIER = TIM_DIER_CC2IE |
                 0;
    TIM1->CCR1 = 10000;
    TIM1->CCR3 = 20000;
    TIM1->CCR4 = 30000;

    /* Setup general timer config */
    tmp = 0;
    tmp |= TIM_COUNTERMODE_UP; // == 0
    tmp |= TIM_CLOCKDIVISION_DIV1; // == 00
    /* Disable update on event */
    tmp |= TIM_CR1_UDIS;
    /* Enable the timer. */
    tmp |= TIM_CR1_CEN;
    TIM1->CR1 = tmp;

    /* We need to be the next to highest priority */
    HAL_NVIC_SetPriority(TIM1_CC_IRQn, PRIORITY_HIGH, 0);
    HAL_NVIC_EnableIRQ(TIM1_CC_IRQn);

#if defined(RADDR_INPUT_DEBUG)
    printf("Input divider %ld %ld"
            " %lx"
            " %lx"
            " %lx"
            " %lx"
            "\r\n",
            INPUT_TIMER_DIVIDER, TIM1->CNT,
            TIM1->CR1,
            TIM1->CCER,
            TIM1->SMCR,
            TIM1->DIER
            );
#endif
}
