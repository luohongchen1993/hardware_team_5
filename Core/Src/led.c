#include "led.h"
#include "config.h"
#include "stm32g4xx_hal.h"

void LED_Init(void)
{
    /* GPIO clocks assumed enabled by CubeMX / MX_GPIO_Init().
     * Ensure all LEDs start off. */
    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN,  GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_AMBER_PORT, LED_AMBER_PIN,  GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_RED_PORT,   LED_RED_PIN,    GPIO_PIN_RESET);
}

void LED_Set(uint8_t mask, uint8_t on)
{
    GPIO_PinState s = on ? GPIO_PIN_SET : GPIO_PIN_RESET;
    if (mask & LED_GREEN) HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, s);
    if (mask & LED_AMBER) HAL_GPIO_WritePin(LED_AMBER_PORT, LED_AMBER_PIN, s);
    if (mask & LED_RED)   HAL_GPIO_WritePin(LED_RED_PORT,   LED_RED_PIN,   s);
}

void LED_Toggle(uint8_t mask)
{
    if (mask & LED_GREEN) HAL_GPIO_TogglePin(LED_GREEN_PORT, LED_GREEN_PIN);
    if (mask & LED_AMBER) HAL_GPIO_TogglePin(LED_AMBER_PORT, LED_AMBER_PIN);
    if (mask & LED_RED)   HAL_GPIO_TogglePin(LED_RED_PORT,   LED_RED_PIN);
}

void LED_Off(uint8_t mask)
{
    LED_Set(mask, 0);
}
