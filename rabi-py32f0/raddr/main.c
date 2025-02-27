#include <py32f0xx_hal.h>
#include "clk_config.h"

#include "uid.h"
#include "output_timer.h"
#include "input_capture.h"
#include "wolf.h"
#include "pack.h"

/*  A wolf is:
 *  Pin     Port(s)         PCB function    SPI1        I2C     UART1       TIM1        Alternate functions
 *  Pin 8   GND             GND             -
 *  Pin 7   PA1             switch          SCK/MOSI            RTS         CH4 / CH2N
 *  Pin 6   PA2/PF2         reset           SCK/MOSI    SDA     TX                      COMP2_OUT (PF2: RESET/MCO)
 *  Pin 5   PA13            SWD             MISO                RX          CH2         SWDIO
 *  Pin 4   PA14/PB3        SWC             SCK                 TX / RTS    CH2         SWCLK / MCO
 *  Pin 3   PA3             KEY Data inp    MOSI        SCL     RX          CH1
 *  Pin 2   PA4/PA10        KEY Data out    NSS         SDA/SCL CK /RX /TX  CH3
 *  Pin 1   VCC             VCC             -
 * */

#define SWC_PIN     GPIO_PIN_1
#define KEY_IN_PIN  GPIO_PIN_3
#define KEY_OUT_PIN GPIO_PIN_4

/**
 * These need to be implemented by us, They are used
 * by the pack.c code
 */
bool K_BINARY_INPUTS[K] = {0};



static void cfg_pin(uint32_t pin, uint32_t mode, uint32_t pull)
{
    GPIO_InitTypeDef pin_cfg;

    pin_cfg.Pin   = pin;
    pin_cfg.Mode  = mode;
    pin_cfg.Pull  = pull;
    pin_cfg.Speed = GPIO_SPEED_FREQ_HIGH;
    //MEGA HACK SMELLY CODE.
    //We only specify mode = Alternate Function for one pin. and so this works.
    //Do not do try this at home.
    pin_cfg.Alternate = GPIO_AF13_TIM1;
    HAL_GPIO_Init(GPIOA, &pin_cfg);
}

static void cfg_gpio(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    cfg_pin(SWC_PIN,     GPIO_MODE_IT_RISING_FALLING, GPIO_PULLUP);
    cfg_pin(KEY_IN_PIN,  GPIO_MODE_AF_OD, GPIO_PULLUP);
    cfg_pin(KEY_OUT_PIN, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL);

    /* EXTI interrupt init*/
    HAL_NVIC_SetPriority(EXTI0_1_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(EXTI0_1_IRQn);

}

/**
 * Switch interrupt handler
 * Rising and falling edges
 */
void EXTI0_1_IRQHandler(void)
{
    //I think this causes bounce issues
    K_BINARY_INPUTS[0] = !HAL_GPIO_ReadPin(GPIOA, SWC_PIN);
    __HAL_GPIO_EXTI_CLEAR_IT(SWC_PIN);
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
    raddr_output_init();
    raddr_input_capture_init();

    uint32_t t_bounce = 0;

#ifdef WOUTER_DEBUG
    /* Loop that assumes input is connected to the output and then
     * print the difference with expected received value.
     * For a whole range of timings */
    uint32_t t_last_tx = 0;
    uint32_t t_last_tx_duration = 1000;
    bool use_bulk = true;

    /* Number of bits to measure */
    const int bits_to_send = 3;

    while (1) {
        uint32_t now = HAL_GetTick();
        /* Do test once every second */
        if (now - t_last_tx > 1000) {
            //uint32_t tmo = us_to_timer_tick(T1H);
            //printf("\r\n====\r\nOutputting %ld\r\n", t_last_tx_duration);
            t_last_tx_duration += 10;
            if (t_last_tx_duration > 480) {
                t_last_tx_duration = 240;
            }
            if (use_bulk) {
                raddr_output_bulk_begin();
                for(int i = 0; i < bits_to_send; i++) {
                    raddr_output_bulk_schedule(1, t_last_tx_duration);
                    raddr_output_bulk_schedule(0, t_last_tx_duration);
                }
                raddr_output_bulk_end();
            }
            else {
                for(int i = 0; i < bits_to_send; i++) {
                    raddr_output_schedule(1, t_last_tx_duration);
                    raddr_output_schedule(0, t_last_tx_duration);
                }
            }
            t_last_tx = now;
        }

        /* Collect all the transmitted bits */
        int cnt = receive_bits_available();
        if (cnt != bits_to_send)
            continue;

        for(int i = 0; i < bits_to_send; i++) {
            uint32_t received_bits_read(void);
            uint32_t t = received_bits_read() & 0xFFFF;

            int diff = t - t_last_tx_duration;
            if (diff < 0)
                diff *= -1;
            if (diff > 3)
                printf("%ld - %ld = %d\r\n", t, t_last_tx_duration, diff);
        }
    }
#endif

    /* Main loop */
    while (1) {
        uint32_t now = HAL_GetTick();

        if (!receive_bits_available()) {
            // if no data, take the time to update inputs
            bool input = !HAL_GPIO_ReadPin(GPIOA, SWC_PIN);
            // if input changed only set it when done bouncing
            if (input^K_BINARY_INPUTS[0] && t_bounce < now) {
                K_BINARY_INPUTS[0] = input;
                t_bounce = now + 2; // only accept change in 2ms
            }
            continue;
        }

        int bit = receive_bit();

        switch(bit) {
            case 0 ... 1:
                join_cry(bit, CRY_OKAY);
                break;
            case -1:
                //Reset
                /* The input capture got confused, or a forced reset is applied */
#if defined USE_SEMIHOSTING
                //printf("Received RESET!\r\n");
#endif
                join_cry(!GROWL, CRY_RESET);
                raddr_output_schedule(1, us_to_timer_tick(TRESET));
                raddr_output_schedule(0, us_to_timer_tick(TRESET / 2));
                break;

            default:
#if defined USE_SEMIHOSTING
                printf("Received unknown bit!\r\n");
#endif
                break;
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
