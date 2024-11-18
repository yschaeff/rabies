#include <py32f0xx_hal.h>
#include <py32f0xx_hal_rcc.h>

static RCC_OscInitTypeDef RCC_OscInitStruct = {
  .OscillatorType = RCC_OSCILLATORTYPE_HSI,
  .HSIState = RCC_HSI_ON,                            /* HSI ON */
  //.HSICalibrationValue = RCC_HSICALIBRATION_24MHz,   /* Set HSI clock 24MHz */
  .HSIDiv = RCC_HSI_DIV1,                            /* No division */
  //.HSEState = RCC_HSE_OFF,                           /* OFF */
  //.LSIState = RCC_LSI_OFF,                           /* OFF */
  //#if defined(RCC_LSE_SUPPORT)
  //.LSEState = RCC_LSE_OFF,                           /* OFF */
  //#endif
  //#if defined(RCC_PLL_SUPPORT)
  //.PLL.PLLState = RCC_PLL_OFF,                       /* OFF */
  //#endif
};

static RCC_ClkInitTypeDef RCC_ClkInitStruct = {
  /* Reinitialize AHB,APB bus clock */
  .ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1,
  .SYSCLKSource = RCC_SYSCLKSOURCE_HSI,              /* Select HSI as SYSCLK source */
  .AHBCLKDivider = RCC_SYSCLK_DIV1,                  /* APH clock, no division */
  .APB1CLKDivider = RCC_HCLK_DIV1,                   /* APB clock, no division */
};

/* Setup the clocking infrastructure to use:
 *  internal oscillator @ 24Mhz
 *  AHB: 24Mhz
 *  APB: 24Mhz
 * */
void BSP_HSI_24MHzClockConfig(void) {
  /* Sigh, this reads internal flash and that makes it non-const */
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_24MHz;   /* Set HSI clock 24MHz */
  /* Setup for 24Mhz */
  HAL_RCC_OscConfig(&RCC_OscInitStruct);
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0);
}


