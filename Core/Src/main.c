/* main.c — Health Monitor super-loop for STM32G474RE
 *
 * Peripheral setup is done by CubeMX-generated MX_*_Init() functions.
 * Required peripheral configuration (set in CubeMX .ioc):
 *
 *   I2C1   — 400 kHz fast mode, SDA=PB9, SCL=PB8
 *   TIM3   — PWM CH1, PA6; PSC=169, ARR=19999 (50 Hz for SG90)
 *   TIM4   — PWM CH1, PB6; PSC=169, ARR dynamic (audio tones for STEMMA Speaker)
 *   TIM2   — 1-µs free-running counter; PSC=169, ARR=0xFFFFFFFF (HC-SR04 + 1-Wire)
 *   USART2 — 115200 8N1, PA2 TX / PA3 RX (ST-Link USB-CDC)
 *   GPIOB  — PB13/PB14/PB15 output push-pull (LEDs)
 *   GPIOA  — PA9 output push-pull (HC-SR04 TRIG), PA8 input (ECHO)
 *   GPIOA  — PA5 open-drain (DS18B20 1-Wire, shared with LED_LD2 — reassign if needed)
 *   SysTick — 1 kHz (HAL default)
 */

#include "main.h"
#include "stm32g4xx_hal.h"
#include "health_monitor.h"
#include "serial_log.h"

/* ---- HAL peripheral handles (populated by MX_*_Init) ---- */
I2C_HandleTypeDef  hi2c1;
TIM_HandleTypeDef  htim2;   /* 1-µs free-running: HC-SR04 + DS18B20 */
TIM_HandleTypeDef  htim3;   /* 50 Hz PWM: SG90 servo */
TIM_HandleTypeDef  htim4;   /* audio PWM: STEMMA Speaker */
UART_HandleTypeDef huart2;

/* ---- Forward declarations (CubeMX-generated init stubs) ---- */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);
static void MX_USART2_UART_Init(void);

/* ================================================================
 * main
 * ================================================================ */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_TIM2_Init();
    MX_TIM3_Init();
    MX_TIM4_Init();
    MX_USART2_UART_Init();

    /* Start the 1-µs free-running counter for timing */
    HAL_TIM_Base_Start(&htim2);

    HealthMonitor_Init(&hi2c1, &htim3, &htim2, &htim4, &huart2);

    SerialLog_Print("=== Health Monitor Boot OK ===\r\n");

    while (1) {
        HealthMonitor_Update();
        /* Nothing else in the loop — all work is inside HealthMonitor_Update */
    }
}

/* ================================================================
 * Peripheral initialisation — mirrors what CubeMX generates.
 * Replace with auto-generated versions if using CubeIDE.
 * ================================================================ */

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* Enable HSE, configure PLL for 170 MHz */
    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState            = RCC_HSE_ON;
    osc.PLL.PLLState        = RCC_PLL_ON;
    osc.PLL.PLLSource       = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM            = RCC_PLLM_DIV2;  /* 8 MHz / 2 = 4 MHz */
    osc.PLL.PLLN            = 85;              /* 4 * 85 = 340 MHz VCO */
    osc.PLL.PLLP            = RCC_PLLP_DIV2;  /* unused */
    osc.PLL.PLLQ            = RCC_PLLQ_DIV2;
    osc.PLL.PLLR            = RCC_PLLR_DIV2;  /* 340/2 = 170 MHz SYSCLK */
    HAL_RCC_OscConfig(&osc);

    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                         RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4);
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* LEDs: PB13 (green), PB14 (amber), PB15 (red) — push-pull */
    g.Pin   = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &g);

    /* HC-SR04 TRIG: PA9 output */
    g.Pin   = GPIO_PIN_9;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &g);

    /* HC-SR04 ECHO: PA8 input */
    g.Pin   = GPIO_PIN_8;
    g.Mode  = GPIO_MODE_INPUT;
    g.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &g);

    /* DS18B20 1-Wire: PA5 open-drain (no internal pull-up — external 4.7 kΩ) */
    g.Pin   = GPIO_PIN_5;
    g.Mode  = GPIO_MODE_OUTPUT_OD;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &g);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
}

static void MX_I2C1_Init(void)
{
    __HAL_RCC_I2C1_CLK_ENABLE();

    hi2c1.Instance             = I2C1;
    hi2c1.Init.Timing          = 0x00702991;  /* 400 kHz @ 170 MHz (from CubeMX) */
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2     = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hi2c1);
}

static void MX_TIM2_Init(void)
{
    /* 1-µs free-running counter for HC-SR04 echo and DS18B20 1-Wire */
    __HAL_RCC_TIM2_CLK_ENABLE();

    htim2.Instance               = TIM2;
    htim2.Init.Prescaler         = 169;        /* 170 MHz / 170 = 1 MHz */
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = 0xFFFFFFFF;  /* TIM2 is 32-bit */
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_Base_Init(&htim2);
}

static void MX_TIM3_Init(void)
{
    /* 50 Hz PWM for SG90 servo on PA6 (TIM3_CH1) */
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* AF mapping: PA6 → TIM3_CH1 (AF2) */
    GPIO_InitTypeDef g = {0};
    g.Pin       = GPIO_PIN_6;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_LOW;
    g.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOA, &g);

    htim3.Instance               = TIM3;
    htim3.Init.Prescaler         = 169;         /* 1 MHz tick */
    htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3.Init.Period            = 19999;        /* 20 ms → 50 Hz */
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim3);

    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode       = TIM_OCMODE_PWM1;
    oc.Pulse        = 1000;    /* 1 ms = 0° */
    oc.OCPolarity   = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode   = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_1);
}

static void MX_TIM4_Init(void)
{
    /* Audio PWM for STEMMA Speaker on PB6 (TIM4_CH1, AF2).
     * PSC=169 → 1 MHz tick; ARR is set dynamically per tone in speaker.c. */
    __HAL_RCC_TIM4_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin       = GPIO_PIN_6;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF2_TIM4;
    HAL_GPIO_Init(GPIOB, &g);

    htim4.Instance               = TIM4;
    htim4.Init.Prescaler         = 169;           /* 1 MHz tick */
    htim4.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim4.Init.Period            = 1135;           /* ~880 Hz default; updated per tone */
    htim4.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim4);

    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode     = TIM_OCMODE_PWM1;
    oc.Pulse      = 0;            /* silent until speaker driver sets it */
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim4, &oc, TIM_CHANNEL_1);
}

static void MX_USART2_UART_Init(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA2 = TX, PA3 = RX → AF7 */
    GPIO_InitTypeDef g = {0};
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
    HAL_UART_Init(&huart2);
}

/* ================================================================
 * HAL MSP hooks — called by HAL_*_Init
 * ================================================================ */

void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}

/* Required for HAL SysTick handler */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* Required by HAL — minimal error handler */
void Error_Handler(void)
{
    __disable_irq();
    LED_Off(LED_ALL);
    /* Blink red forever to signal hard fault */
    while (1) {
        HAL_GPIO_TogglePin(LED_RED_PORT, LED_RED_PIN);
        /* Busy-wait: HAL_Delay is SysTick-dependent and may be dead here */
        for (volatile uint32_t i = 0; i < 1000000U; i++) {}
    }
}
