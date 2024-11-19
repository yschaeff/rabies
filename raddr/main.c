#include <py32f0xx_hal.h>
#include <py32f0xx_hal_rcc.h>
#include "clk_config.h"
#include "uid.h"

#include "wolf.h"
#include "pack.h"

/*  A wolf is:
 *  PA1 = switch (Pin 7)
 *  PA2 = shared with reset (Pin 6)
 *  PA13/PB3/SWDIO = SWD (Pin 5)
 *  PA14/PB3/SWCLK = SWC (Pin 4)
 *  PA3 = KEY Data in (Pin 3)
 *  PA4 = KEY Data out (Pin 2)
 *  VCC = (Pin 1)
 *
 * */

#define SWC_PIN     GPIO_PIN_1
#define KEY_IN_PIN  GPIO_PIN_3
#define KEY_OUT_PIN GPIO_PIN_4

// if parent send dataframes continuously to trigger child
#define PARENT 0

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
    HAL_NVIC_SetPriority(EXTI0_1_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(EXTI0_1_IRQn);

    HAL_NVIC_SetPriority(EXTI2_3_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(EXTI2_3_IRQn);
}

void gpio_set(int bit)
{
    HAL_GPIO_WritePin(GPIOA, KEY_OUT_PIN, bit);
}

void sleep_ns(int delay)
{
    HAL_Delay(delay); //TODO these are ms
}

int gpio_get(void)
{
    return HAL_GPIO_ReadPin(GPIOA, KEY_IN_PIN);
}

bool SOME_INPUT[K] = {0,0,0,0, 0,0,0,0};

void update_input(void)
{
    SOME_INPUT[0] = !HAL_GPIO_ReadPin(GPIOA, SWC_PIN);
}

//switch
void EXTI0_1_IRQHandler(void)
{
    //maybe read switch state here instead of update_input()?
    __HAL_GPIO_EXTI_CLEAR_IT(SWC_PIN);
}

//key
void EXTI2_3_IRQHandler(void)
{
    if (!PARENT) {
        //trigger statemachine
        receive_bit();
        /*int a = HAL_GPIO_ReadPin(GPIOA, KEY_IN_PIN);*/
        /*HAL_GPIO_WritePin(GPIOA, KEY_OUT_PIN, a);*/
    }
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

    while (1) {
        if (PARENT) {
            statemachine(1);
            HAL_Delay(T1L); //TODO these are ms
            statemachine(1);
            HAL_Delay(T1L); //TODO these are ms
            HAL_Delay(1000); //TODO these are ms
            printf("howl!\r\n");
        }

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
