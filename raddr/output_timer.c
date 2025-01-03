#include <stdio.h>
#include <stdbool.h>
#include <py32f0xx_hal.h>
#include "wolf.h"
#include "output_timer.h"
//
//TODO get these from a header file
#define LED_PIN     GPIO_PIN_4

//We need to pick the tick_per_clock as low as reasonably possible.
//But is also defines the upper time:
// CLK  Divider     Single      Max
// 24M  /1          41,66ns     2730ns
// 12M  /2          83,33ns     5416ns
//  8M  /3            125ns     8192ns
//  1M  /24          1000ns     65535ms

#define FIFO_SIZE 16 //Must be a power of 2 and at least capable of handling a full K message

_Static_assert(FIFO_SIZE > 2*K, "FIFO_SIZE needs to be able to contain at least a full K of barks");
_Static_assert((FIFO_SIZE & (FIFO_SIZE - 1)) == 0 , "FIFO_SIZE needs to be a power of 2 to make access FAST");

/* The length the pulse is actually larger the specified.
 * We compensate by reducing the pulse length.
 * Note that is CPU cycles.
 * If the prescaler is not 1 then this number should be divided by the prescaler value! .*/
#define FIXED_LATENCY (72)

/* The fifo to hold our rabi barks'n'howls */
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

static uint32_t fifo_read(void)
{
    uint32_t idx = fifo.read;
    uint32_t d = fifo.buf[idx];
    fifo.read = (idx + 1) % FIFO_SIZE;
    return d;
}

static void fifo_write(bool bit, uint16_t tmo)
{
    uint32_t isr_latency = FIXED_LATENCY; //Number of cpu cycles spend in ISR
    uint32_t d = (tmo - isr_latency) | bit << 31;
    uint32_t idx = fifo.write;
    fifo.buf[fifo.write] = d;
    fifo.write = (idx + 1) % FIFO_SIZE;
}

static int bulk_size;
void raddr_output_bulk_begin(void)
{
    bulk_size = 0;
}

void raddr_output_bulk_schedule(bool bit, uint16_t tmo)
{
    fifo_write(bit, tmo);
    bulk_size++;
}

void raddr_output_bulk_end(void) {
    /* Incrementing the fifo must be atomic against the TIM16 ISR.
     * Since Cortex-M0 does not have STREX/LDREX nor SWP instruction we need to rely on libatomic.
     * Which for reasons beyond me is not implemented for arm-gcc-none-eabi and friends.
     * So lets stick to disabling the TIM16 interrupt for now */
    NVIC_DisableIRQ(TIM16_IRQn);
    fifo.size += bulk_size;
    NVIC_EnableIRQ(TIM16_IRQn);

    /* If ISR not enabled the ISR is not running. Start it */
    if (!(TIM16->DIER & TIM_DIER_UIE)) {
        /* Generate event to wakeup the ISR */
        TIM16->EGR = TIM_EGR_UG;

        /* Enable the timer interrupt, this will take us to the ISR instantly */
        TIM16->DIER = TIM_DIER_UIE;
    }
}

/* Supports a single writer only!
 *  A few things to note here:
 *
 *  If there is no more work to be done the GPIO is set to OFF
 *
 *  If queueing a very low tmo the ISR may finish before the next fifo write.
 *  If this happens there will be a low pulse!
 *
 *  The whole ISR routing adds about ~2.2us (56 cycles on 24Mhz) extra.
 *  Having >6us makes it work reliable: the fifo will not empty in between.
 * */
void raddr_output_schedule(bool bit, uint16_t tmo)
{
    if (fifo_is_full()) {
        //printf("Fifo full!\r\n");
        return; //Drop it, sorry. Programmer error
    }

    fifo_write(bit, tmo);

    /* Incrementing the fifo must be atomic against the TIM16 ISR.
     * Since Cortex-M0 does not have STREX/LDREX nor SWP instruction we need to rely on libatomic.
     * Which for reasons beyond me is not implemented for arm-gcc-none-eabi and friends.
     * So lets stick to disabling the TIM16 interrupt for now */
    NVIC_DisableIRQ(TIM16_IRQn);
    fifo.size++;
    NVIC_EnableIRQ(TIM16_IRQn);

    /* If ISR not enabled the ISR is not running. Start it */
    if (!(TIM16->DIER & TIM_DIER_UIE)) {
        /* Generate event to wakeup the ISR */
        TIM16->EGR = TIM_EGR_UG;

        /* Enable the timer interrupt, this will take us to the ISR instantly */
        TIM16->DIER = TIM_DIER_UIE;
    }
}

#if defined(RADDR_OUTPUT_DEBUG)
static int cnt = 0;
void debug_print(void)
{
    printf("Cnt %d / %ld\r\n", cnt, TIM16->CNT);
}
#endif
bool huge_difference = false;

void TIM16_IRQHandler(void)
{
    /* This code needs to FAAAAST.
     * But like REALLY fast, like cut corners AND circles to be fast.
     *
     * That means no crappy bloated HAL code here!
     * Just old skool register writing */

    /* First to the fifo things */
    uint32_t size = fifo.size;
    uint32_t t = 0;
    uint32_t bit = 0; //Default to OFF/LOW
    if (size > 0) {
        fifo.size = --size;
        t = fifo_read();
        bit = !!(t & 1u<<31);
    }

    uint32_t tmo = t; //MEGA hack, just write all 32 bit to ARR.

    /* Second part is writing the GPIO */

    /* BSRR => Bit Set Reset Register.
     * Lower 16 bit: Write 1 to set I/O
     * Upper 16 bit: Write 1 to clear I/O */
    GPIOA->BSRR = bit ? LED_PIN : LED_PIN << 16;

    /* Last part is writing the timer registers */

    /* Disable the interrupt,
     * to force not getting here again.
     * It also signal to the writer we are done */
    if (tmo == 0) {
        TIM16->DIER = 0;
    }

#if 0
    static uint16_t last_tmo = 0;
    uint32_t old_arr = TIM16->CNT - last_tmo;
    if (last_tmo && old_arr > 10) {
        huge_difference = true;
    }
    last_tmo = tmo;
#endif

    /* Update reload register with new timeout */
    TIM16->ARR = tmo;

#if defined(RADDR_OUTPUT_DEBUG)
    //This sort-of indicates the number CPU before we reach this point.
    //That sort-of indicates the time needed in the ISR
    cnt = TIM16->CNT;
#endif
    /* Reload timer to force pickup new ARR. */
    /* TODO find a better solution for this.
     * This does ensure the pulse is at least as long as specified. */
    TIM16->EGR = TIM_EGR_UG;

    /* Acknowledge the interrupt */
    TIM16->SR = ~TIM_IT_UPDATE;
}

void raddr_output_init(void)
{
    uint32_t tmpcr1;

    /* First enable the clocking towards TIM16 */
    __HAL_RCC_TIM16_CLK_ENABLE();

    /* Set the Autoreload value to max */
    //TIM16->ARR = ~0;

    /* Set the Prescaler value to the one calculated in the headerfile. */
    TIM16->PSC = TIMER_DIVIDER - 1;

    /* Setup timer for simple upcounting (no repeat, not autoreload etc) */
    tmpcr1 = 0;
    tmpcr1 |= TIM_COUNTERMODE_UP; //0
    tmpcr1 |= TIM_CLOCKDIVISION_DIV1; //0
    /* Enable the timer. */
    tmpcr1 |= TIM_CR1_CEN;

    TIM16->CR1 = tmpcr1;

    /* We need to be the highest priority, to ensure rock solid jitter free output! */
    HAL_NVIC_SetPriority(TIM16_IRQn, PRIORITY_HIGHEST, 0);

    /* Enable our interrupt */
    HAL_NVIC_EnableIRQ(TIM16_IRQn);

#if defined(RADDR_OUTPUT_DEBUG)
    printf("HSI Clock: %ld, divider %ld\r\n", HSI_VALUE, TIMER_DIVIDER);
    printf("100us: %d, 10us %d\r\n", us_to_timer_tick(100), us_to_timer_tick(1));
#endif
}

