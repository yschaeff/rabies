#include <py32f0xx_hal.h>
#include <py32f0xx_hal_rcc.h>
#include "clk_config.h"

#include "uid.h"
#include "wolf.h"
#include "pack.h"

/*  A wolf is:
 *  PA1 = switch            (Pin 7)
 *  PA2 = shared with reset (Pin 6)
 *  PA13/PB3/SWDIO = SWD    (Pin 5)
 *  PA14/PB3/SWCLK = SWC    (Pin 4)
 *  PA3 = KEY Data in       (Pin 3)
 *  PA4 = KEY Data out      (Pin 2)
 *  VCC =                   (Pin 1)
 * */

#define SWC_PIN     GPIO_PIN_1
#define KEY_IN_PIN  GPIO_PIN_3
#define KEY_OUT_PIN GPIO_PIN_4

volatile int data_ready;

/**
 * These functions need to be implemented by us, They are called
 * by the pack.c code
 */
/* start */
int gpio_get(void)
{
    return HAL_GPIO_ReadPin(GPIOA, KEY_IN_PIN);
}

void gpio_set(int bit)
{
    HAL_GPIO_WritePin(GPIOA, KEY_OUT_PIN, bit);
}

void sleep_ns(int delay_ns)
{
    HAL_Delay(delay_ns); //TODO these are ms
}

bool K_BINARY_INPUTS[K] = {0};

void update_input(void)
{
    // We update the in the switch interrupt handler so this is a NO-OP
}
/* end */


void cfg_pin(uint32_t pin, uint32_t mode, uint32_t pull)
{
    GPIO_InitTypeDef pin_cfg;

    pin_cfg.Pin   = pin;
    pin_cfg.Mode  = mode;
    pin_cfg.Pull  = pull;
    pin_cfg.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &pin_cfg);
}

static void cfg_gpio(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();

    cfg_pin(SWC_PIN,     GPIO_MODE_IT_RISING_FALLING, GPIO_PULLUP);
    cfg_pin(KEY_IN_PIN,  GPIO_MODE_IT_RISING, GPIO_PULLUP);
    cfg_pin(KEY_OUT_PIN, GPIO_MODE_OUTPUT_PP,  GPIO_NOPULL);

    /* EXTI interrupt init*/
    HAL_NVIC_SetPriority(EXTI0_1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI0_1_IRQn);

    HAL_NVIC_SetPriority(EXTI2_3_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI2_3_IRQn);
}

/**
 * Switch interrupt handler
 * Rising and falling edges
 */
void EXTI0_1_IRQHandler(void)
{
    K_BINARY_INPUTS[0] = !HAL_GPIO_ReadPin(GPIOA, SWC_PIN);
    __HAL_GPIO_EXTI_CLEAR_IT(SWC_PIN);
}

/**
 * Key in interrupt handler
 * Falling edges
 */
void EXTI2_3_IRQHandler(void)
{
    data_ready = 1;
    __HAL_GPIO_EXTI_CLEAR_IT(KEY_IN_PIN);
}

int main(void)
{
    /* Setup clock BEFORE HAL_Init.
     * And before running anything else */
    BSP_HSI_24MHzClockConfig();
    HAL_Init();
#if defined USE_SEMIHOSTING
    {
        //This 'magic' function needs to be called.
        //libgloss will setup I/O structures to use
        extern void initialise_monitor_handles();
        initialise_monitor_handles();
        printf("PY32F0xx\r\nSystem Clock: %ld\r\n", SystemCoreClock);
        uid_print();
    }
#endif

    cfg_gpio();

    uint32_t t_last_call = 0;

    // if switch pressed at boot become AKELA.
    int akela = !HAL_GPIO_ReadPin(GPIOA, SWC_PIN);

    if (akela) while (1) {
        rally_pack(); //wakeup pack and bark own state
        // Stop Yapping for a while
#define N 3
        sleep_ns((T1H+T1L+STFU)*(N*K+N+3) + 100);
#undef N
    }

    while (1) {
        if (!data_ready) continue;

        sleep_ns((T0H+T1H)/2);
        int bit = gpio_get();

        data_ready = 0;

        // The statemachine needs to know the elapsed time so
        // it can reset when out of sync.
        uint32_t now = HAL_GetTick();
        uint32_t t_elapsed = now - t_last_call;
        t_last_call = now;

        join_cry(bit, t_elapsed);
    }
}


void APP_ErrorHandler(void)
{
    while (1);
}

#ifdef  USE_FULL_ASSERT
/**
 * @brief  Export assert error source and line number
 */
void assert_failed(uint8_t *file, uint32_t line)
{
    /* printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    while (1);
}
#endif /* USE_FULL_ASSERT */
