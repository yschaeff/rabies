#include <py32f0xx_hal.h>
#include <py32f0xx_hal_rcc.h>
#include "clk_config.h"
#include "uid.h"

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

#define LED_PIN     GPIO_PIN_4

static void APP_GPIO_Config(void);

static void do_debug_print(void) {
#if defined USE_SEMIHOSTING
    static int i = 0;
    switch (i++) {
        case 0: printf("Hello World\r\n"); break;
        case 1: printf("Hello Yuri\r\n"); break;
        case 2: printf("Holw\r\n"); break;
        case 3: printf("Growl\r\n"); break;
        case 4: printf("FULL MOON!\r\n"); break;
        default:
            i = 0;
    }
#endif
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

  APP_GPIO_Config();

  while (1)
  {
    HAL_Delay(1000);
    HAL_GPIO_TogglePin(GPIOA, LED_PIN);
    do_debug_print();
  }
}

static void APP_GPIO_Config(void)
{
  GPIO_InitTypeDef GPIO_InitStruct;

  __HAL_RCC_GPIOA_CLK_ENABLE();
  // PA0
  GPIO_InitStruct.Pin = LED_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
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
