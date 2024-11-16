/***
 * Demo: LED Toggle
 *
 * PA0   ------> LED+
 * GND   ------> LED-
 */
//#include "py32f0xx_bsp_printf.h"
#include "py32f0xx_bsp_clock.h"

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

int main(void)
{
  HAL_Init();
  BSP_HSI_24MHzClockConfig();
  APP_GPIO_Config();
  //BSP_USART_Config();
  //printf("PY32F0xx LED Toggle Demo\r\nSystem Clock: %ld\r\n", SystemCoreClock);

  while (1)
  {
    HAL_Delay(1000);
    HAL_GPIO_TogglePin(GPIOA, LED_PIN);
    //printf("echo\r\n");
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
