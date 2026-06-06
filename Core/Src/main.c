/* main.c — Navigation-game firmware for STM32G474RE (Nucleo-G474RE).
 *
 * Bare-metal, HAL, 170 MHz, 100 Hz fixed-rate super-loop.
 *
 * Peripheral map (see CLAUDE.md):
 *   I2C1   400 kHz  PB8=SCL PB9=SDA      -> MPU-6050 @ 0x68
 *   TIM3   CH1 PA6  50 Hz PWM            -> servo (CCR = pulse us)
 *   TIM2   CH1 PA0  variable-freq PWM    -> speaker (proximity tones)
 *   USART2 PA2/PA3  115200 8N1           -> ST-Link VCP telemetry
 *   RNG                                   -> deterministic-PRNG seed
 *   GPIOB  PB13/14/15 push-pull          -> status LEDs (green/amber/red)
 *   GPIOC  PC13 input                    -> B1 user button (replay)
 *
 * The peripheral init below mirrors what STM32CubeMX generates; swap in the
 * auto-generated versions if you regenerate the .ioc. */

#include "main.h"
#include "stm32g4xx_hal.h"
#include "game.h"
#include "led.h"

/* ---- HAL peripheral handles ---- */
I2C_HandleTypeDef  hi2c1;
TIM_HandleTypeDef  htim2;   /* audio PWM */
TIM_HandleTypeDef  htim3;   /* servo PWM */
UART_HandleTypeDef huart2;
RNG_HandleTypeDef  hrng;

/* ---- init prototypes ---- */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_RNG_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_TIM2_Init();
    MX_TIM3_Init();
    MX_USART2_UART_Init();
    MX_RNG_Init();

    /* Bring up the game (calibrate, place target) and run the loop. */
    Game_Init(&hi2c1, &htim3, &htim2, &huart2, &hrng);

    /* O(1)-per-iteration super-loop; Game_Tick paces itself to LOOP_HZ. */
    for (;;) {
        Game_Tick();
    }
}

/* ================================================================
 * Clock tree: HSI16 -> PLL -> 170 MHz SYSCLK (Range 1 boost).
 * HSI48 feeds the RNG.  Flash 4 wait states.
 * ================================================================ */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef       osc    = {0};
    RCC_ClkInitTypeDef       clk    = {0};
    RCC_PeriphCLKInitTypeDef periph = {0};

    /* 170 MHz requires Range 1 "boost" voltage scaling. */
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_HSI48;
    osc.HSIState            = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.HSI48State          = RCC_HSI48_ON;          /* RNG clock source */
    osc.PLL.PLLState        = RCC_PLL_ON;
    osc.PLL.PLLSource       = RCC_PLLSOURCE_HSI;     /* 16 MHz */
    osc.PLL.PLLM            = RCC_PLLM_DIV4;          /* 16/4   = 4 MHz   */
    osc.PLL.PLLN            = 85;                     /* 4*85   = 340 MHz */
    osc.PLL.PLLP            = RCC_PLLP_DIV2;
    osc.PLL.PLLQ            = RCC_PLLQ_DIV2;
    osc.PLL.PLLR            = RCC_PLLR_DIV2;          /* 340/2  = 170 MHz */
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();

    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                         RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;            /* 170 MHz */
    clk.APB1CLKDivider = RCC_HCLK_DIV1;              /* 170 MHz (TIM2/3 kernel) */
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4) != HAL_OK) Error_Handler();

    /* RNG kernel clock from HSI48. (I2C1 stays on PCLK1 by reset default;
     * its TIMINGR below is the CubeMX value for that clock.) */
    periph.PeriphClockSelection = RCC_PERIPHCLK_RNG;
    periph.RngClockSelection    = RCC_RNGCLKSOURCE_HSI48;
    if (HAL_RCCEx_PeriphCLKConfig(&periph) != HAL_OK) Error_Handler();
}

/* ================================================================
 * GPIO: LED outputs (PB13/14/15) and the B1 button input (PC13).
 * ================================================================ */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* Status LEDs (all on GPIOB) */
    HAL_GPIO_WritePin(GPIOB, LED_GREEN_PIN | LED_AMBER_PIN | LED_RED_PIN, GPIO_PIN_RESET);
    g.Pin   = LED_GREEN_PIN | LED_AMBER_PIN | LED_RED_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &g);

    /* B1 user button (Nucleo has an external pull-up; pressed = LOW) */
    g.Pin  = USER_BTN_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(USER_BTN_PORT, &g);
}

/* ================================================================
 * I2C1 — 400 kHz fast mode, PB8/PB9 (AF4), open-drain w/ pull-ups.
 * ================================================================ */
static void MX_I2C1_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    g.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
    g.Mode      = GPIO_MODE_AF_OD;
    g.Pull      = GPIO_PULLUP;
    g.Speed     = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &g);

    __HAL_RCC_I2C1_CLK_ENABLE();
    hi2c1.Instance              = I2C1;
    hi2c1.Init.Timing           = 0x00702991;   /* CubeMX value; regen if comms fail */
    hi2c1.Init.OwnAddress1      = 0;
    hi2c1.Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2      = 0;
    hi2c1.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) Error_Handler();
}

/* ================================================================
 * TIM2 CH1 (PA0, AF1) — audio PWM. 1 MHz tick; ARR set per-tone.
 * ================================================================ */
static void MX_TIM2_Init(void)
{
    GPIO_InitTypeDef    g  = {0};
    TIM_OC_InitTypeDef  oc = {0};

    __HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin       = GPIO_PIN_0;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_LOW;
    g.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(GPIOA, &g);

    htim2.Instance               = TIM2;
    htim2.Init.Prescaler         = 169;          /* 170 MHz / 170 = 1 MHz tick */
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = 999;          /* 1 kHz default; overwritten per tone */
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(&htim2) != HAL_OK) Error_Handler();

    oc.OCMode     = TIM_OCMODE_PWM1;
    oc.Pulse      = 0;                            /* start silent */
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim2, &oc, TIM_CHANNEL_1) != HAL_OK) Error_Handler();
}

/* ================================================================
 * TIM3 CH1 (PA6, AF2) — servo PWM, 50 Hz. CCR = pulse width (us).
 * ================================================================ */
static void MX_TIM3_Init(void)
{
    GPIO_InitTypeDef    g  = {0};
    TIM_OC_InitTypeDef  oc = {0};

    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin       = GPIO_PIN_6;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_LOW;
    g.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOA, &g);

    htim3.Instance               = TIM3;
    htim3.Init.Prescaler         = 169;          /* 1 MHz tick */
    htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3.Init.Period            = 19999;        /* 20 ms -> 50 Hz */
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) Error_Handler();

    oc.OCMode     = TIM_OCMODE_PWM1;
    oc.Pulse      = SERVO_PULSE_MID_US;          /* centre */
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_1) != HAL_OK) Error_Handler();
}

/* ================================================================
 * USART2 (PA2 TX / PA3 RX, AF7) — 115200 8N1 to the ST-Link VCP.
 * ================================================================ */
static void MX_USART2_UART_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &g);

    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 115200;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK) Error_Handler();
}

/* ================================================================
 * RNG — hardware entropy for the deterministic PRNG seed.
 * ================================================================ */
static void MX_RNG_Init(void)
{
    __HAL_RCC_RNG_CLK_ENABLE();
    hrng.Instance = RNG;
    if (HAL_RNG_Init(&hrng) != HAL_OK) Error_Handler();
}

/* ================================================================
 * HAL hooks
 * ================================================================ */
void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* Fatal HAL-init fault: fast-strobe the red LED forever. */
void Error_Handler(void)
{
    __disable_irq();
    for (;;) {
        HAL_GPIO_TogglePin(LED_RED_PORT, LED_RED_PIN);
        for (volatile uint32_t i = 0; i < 1000000U; i++) { __NOP(); }
    }
}
