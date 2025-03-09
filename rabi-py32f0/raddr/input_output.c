#include <stdio.h>
#include <stdbool.h>
#include <py32f0xx_hal.h>
#include "wolf.h"
#include "input_output.h"


#define T1US_IN_TICKS   us_to_tick(1)
#define T4US_IN_TICKS   us_to_tick(4)
#define TRESET_TICKS    us_to_tick(TRESET)
#define T0H_TICKS       us_to_tick(T0H)
#define T0L_TICKS       us_to_tick(T0L)
#define T1H_TICKS       us_to_tick(T1H)
#define T1L_TICKS       us_to_tick(T1L)

_Static_assert((RX_FIFO_SIZE & (RX_FIFO_SIZE - 1)) == 0 , "RX_FIFO_SIZE needs to be a power of 2 to make access FAST");
_Static_assert((TX_FIFO_SIZE & (TX_FIFO_SIZE - 1)) == 0 , "TX_FIFO_SIZE needs to be a power of 2 to make access FAST");
_Static_assert(RX_FIFO_SIZE >= TX_FIFO_SIZE, "RX must be larger than TX, otherwise we will overflow our neighbour");

/* The fifo to hold the barks'n'howls we received */
static struct {
    uint8_t write;
    uint8_t read;
    volatile uint32_t size;
    uint32_t buf[RX_FIFO_SIZE];
} rxfifo = {.size = 0};

static bool rxfifo_is_full(void)
{
    return rxfifo.size >= RX_FIFO_SIZE;
}

/* To be used by the main thread (or any other single thread).
 * Returns the number of bits received */
uint32_t receive_bits_available(void)
{
    return rxfifo.size;
}

/* Extract data from the fifo.
 * This extracts raw timing information from the fifo.
 *
 * For the converted version use receive_bit.
 */
uint32_t received_bits_read(void)
{
    uint32_t idx = rxfifo.read;
    uint32_t d = rxfifo.buf[idx];

    /* Updating size must be atomic! Against TIM1 ISR(s) */
    __disable_irq();
    rxfifo.size--;
    __enable_irq();

    rxfifo.read = (idx + 1) % RX_FIFO_SIZE;

    /* Handle timer wrapping @64k */
    uint32_t end = (d >> 16) & 0xFFFF;
    uint32_t begin = d & 0xFFFF;
    if (begin > end) {
        //printf("Too big %ld > %ld ==> %ld \r\n", begin, end, end + (1<<16) - begin);
        end += 1<<16;
    }

    return end - begin;
}

/* Read and discard all received bits */
void receive_bits_flush(void)
{
    while(receive_bits_available())
        received_bits_read();
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
        case T0H_TICKS - T1US_IN_TICKS ... T0H_TICKS + T4US_IN_TICKS:
            return 0;
        case T1H_TICKS - T1US_IN_TICKS ... T1H_TICKS + T4US_IN_TICKS:
            return 1;
        case (TRESET_TICKS - 3 * T1US_IN_TICKS ) ... (TRESET_TICKS + 3 * T1US_IN_TICKS):
            return -1; //Reset
        default:
#if defined(RADDR_INPUT_DEBUG)
            printf("Unknown pulse length %d\r\n", t);
#endif
            return -2; //panic, unknown byte length?

    }
}

/* Only to be used by the timer ISR! */
static inline void rxfifo_write(uint32_t tmo)
{
    uint32_t idx = rxfifo.write;
    if (rxfifo_is_full()) {
        //TODO send reset?
        //This should never happen, programmers error.
        return;
    }
    rxfifo.buf[idx] = tmo;
    rxfifo.write = (idx + 1) % RX_FIFO_SIZE;

    /* Updating size must be atomic! Against main thread */
    rxfifo.size++;
}

/* The fifo to hold our rabi barks'n'howls */
static struct {
    uint8_t write;
    uint8_t read;
    uint8_t bulk_size;
    volatile uint32_t size;
    uint32_t buf[TX_FIFO_SIZE];
} txfifo = {.size = 0};

static bool txfifo_is_empty(void)
{
    return txfifo.size == 0;
}

static bool txfifo_is_full(void)
{
    return txfifo.size >= TX_FIFO_SIZE;
}

/* Only to be used by the ISR! */
static inline uint32_t txfifo_read(void)
{
    uint32_t idx = txfifo.read;
    uint32_t d = txfifo.buf[idx];
    txfifo.read = (idx + 1) % TX_FIFO_SIZE;
    txfifo.size--;
    return d;
}


/* The length the pulse is actually larger than specified.
 * We compensate by reducing the pulse length.
 * Note that is CPU cycles.
 * If the prescaler is not 1 then this number should be divided by the prescaler value! .*/
#ifdef TIMING_DEBUG
uint8_t fixed_latency = 3;
#else
static const uint8_t fixed_latency = 3;
#endif

static void txfifo_write(uint16_t th, uint16_t tl)
{
    uint32_t isr_latency = fixed_latency;
    uint32_t d = tl;
    d <<= 16;
    d |= (th - isr_latency);

    uint32_t idx = txfifo.write;
    txfifo.buf[idx] = d;
    txfifo.write = (idx + 1) % TX_FIFO_SIZE;
}

static inline void raddr_output_handle_irq(TIM_TypeDef* tim)
{
    if (txfifo_is_empty()) {
        /* Disable our interrupt, to signal we are done */
        tim->DIER &= ~TIM_DIER_CC4IE;
    }
    else
    {
        /* Determine what to do first. */
        uint32_t t = txfifo_read();
        uint32_t th = t & 0xFFFF;
        uint32_t tl = (t>>16) & 0xFFFF;

        /* Now setup as quickly as possible.
         * Mainly setting up CCR3 relative to writing CCMR2.
         * That delay should be as small as reasonably possible */
        tim->CCMR2 = TIM_OCMODE_FORCED_ACTIVE;
        uint32_t base = tim->CNT;

        /* Setup OCR3 to drive output low again */
        tim->CCR3 = base + th;

        /* Setup OCR4 so we get an ISR when we are done */
        tim->CCR4 = base + th + tl;

        /* Set to low on match
         * Note:
         *  we write all other bits to zero. That is fine.
         *  it is defined for CC1 but that is equal to CC3. That is fine.
         *  This also works if the timer wraps, in case we got nothing to send.
         */
        tim->CCMR2 = TIM_OCMODE_INACTIVE;
    }
}

static inline void raddr_input_handle_irq(TIM_TypeDef* tim)
{
    /* Send start and top count in the fifo.
     * Let the otherside figure wrapping etc.
     * That keeps our ISR fast */
    uint32_t begin = tim->CCR1;
    uint32_t end = tim->CCR2;
    rxfifo_write(end << 16 | begin);
}

/* The ISR for TIM1 */
void TIM1_CC_IRQHandler(void)
{
    TIM_TypeDef* tim = TIM1;

    uint16_t flags = tim->SR;

    /* Clear/acknowledge pending interrupts */
    TIM1->SR = 0;

    /* CCR4 tx time has passed. transmit of a bit + delay is done. */
    if (flags & TIM_SR_CC4IF) {
        raddr_output_handle_irq(tim);
    }
    /* CCR2 capture, we received something */
    if (flags & TIM_SR_CC2IF) {
        raddr_input_handle_irq(tim);
    }
}

void raddr_input_output_init(void)
{
    /* Setup timer1, which is the only one with I/O capabilities.
     *
     * The basic idea is capture rising edge on 1 and falling edge on 2.
     * */

    /* Enable the clock towards TIM1 */
    __HAL_RCC_TIM1_CLK_ENABLE();

    uint32_t tmp;

    /* Setup general timer config */
    TIM1->PSC = INPUT_TIMER_DIVIDER - 1;

    /* Setup output related data */
    /* Force output low BEFORE we enable the rx.
     * This helps testing timing if Rx is connected to Tx. */
    TIM1->CCMR2 = TIM_OCMODE_FORCED_INACTIVE;

    /* Setup capture inputs */
    tmp = 0;
    tmp |= TIM_CCMR1_CC1S_0; //CC1 = 01 == Input from TI1
    tmp |= TIM_CCMR1_CC2S_1; //CC2 = 10 == Input from TI1
    //We do not need input filtering for now
    TIM1->CCMR1 = tmp;

    /* Enable capture.
     * Setup input 1 as rising edge (00)
     * Setup input 2 as falling edge (10)
     * Setup output 3 enabled
     * */
    tmp = 0;
    tmp |= TIM_CCER_CC1E; //enable capture inp1
    tmp |= TIM_CCER_CC2E; //enable capture inp2
    tmp |= TIM_CCER_CC2P; //falling edge
    tmp |= TIM_CCER_CC3E; //Enable CC3 as output
    TIM1->CCER = tmp;

    /* Only enable compare 2 interrupt for now */
    TIM1->DIER = TIM_DIER_CC2IE |
                 0;

    /* Enable the master output otherwise there is no output,
     * perhaps idle state, which might proof useful */
    TIM1->BDTR = TIM_BDTR_MOE;

    /* Setup general timer config */
    tmp = 0;
    tmp |= TIM_COUNTERMODE_UP; // == 0
    tmp |= TIM_CLOCKDIVISION_DIV1; // == 00
    /* Disable update on event */
    tmp |= TIM_CR1_UDIS;
    /* Enable the timer. */
    tmp |= TIM_CR1_CEN;
    TIM1->CR1 = tmp;

    /* We need to be the highest priority */
    HAL_NVIC_SetPriority(TIM1_CC_IRQn, PRIORITY_HIGHEST, 0);
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

void raddr_output_bulk_begin(void)
{
    txfifo.bulk_size = 0;
}

void raddr_output_bulk_schedule(uint16_t th, uint16_t tl)
{
    if (txfifo.bulk_size + txfifo.size >= TX_FIFO_SIZE) {
        printf("Bulk fifo full!\r\n");
        return;
    }
    txfifo_write(th, tl);
    txfifo.bulk_size++;
}

static void raddr_output_update_size(uint8_t size)
{
    /* Incrementing the fifo must be atomic against the TIM16 ISR.
     * Since Cortex-M0 does not have STREX/LDREX nor SWP instruction we need to rely on libatomic.
     * Which for reasons beyond me is not implemented for arm-gcc-none-eabi and friends.
     * So we tried disabling the TIM1 interrupt.
     * That makes it atomic, but also sometime runs other ISR, delaying our precious IRQ.
     * That gives upto ~3.5uS of extra latency.
     * So now we are back at good old disable EVERY interrupt to make it atomic&fast. */
    __disable_irq();
    txfifo.size += size;
    __enable_irq();

    /* Use the TIM_DIER_CC4IE interrupt to check if we need to kick the TX */
    if(!(TIM1->DIER & TIM_DIER_CC4IE)) {
        /* Must be atomic! */
        __disable_irq();
        /* Enable our interrupt */
        TIM1->DIER |= TIM_DIER_CC4IE;
        /* Generate the interrupt */
        TIM1->EGR |= TIM_EGR_CC4G;
        __enable_irq();
    }

}

void raddr_output_bulk_end(void)
{
    raddr_output_update_size(txfifo.bulk_size);
    txfifo.bulk_size = 0;
}

/* Supports a single writer only!
 *  A few things to note here:
 *  If there is no more work to be done the output is set to OFF
 */
void raddr_output_schedule(uint16_t th, uint16_t tl)
{
    if (txfifo_is_full()) {
        printf("TX Fifo full!\r\n");
        return; //Drop it, sorry. Programmer error
    }

    txfifo_write(th, tl);
    raddr_output_update_size(1);
}

