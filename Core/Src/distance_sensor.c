#include "distance_sensor.h"
#include "config.h"

/* HC-SR04 driver.
 * Uses a 1-µs-tick timer (TIM2 or TIM4 configured as a free-running
 * 16-bit counter at 1 MHz) to measure echo pulse width.
 * Sound speed: 343 m/s → 1 µs ≈ 0.0343 cm → distance = ticks / 58 (mm).
 */

static inline void delay_us(TIM_HandleTypeDef *htim, uint32_t us)
{
    __HAL_TIM_SET_COUNTER(htim, 0);
    while (__HAL_TIM_GET_COUNTER(htim) < us) {}
}

uint32_t Distance_ReadMM(TIM_HandleTypeDef *htim)
{
    /* 10 µs trigger pulse */
    HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_RESET);
    delay_us(htim, 2);
    HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_SET);
    delay_us(htim, 10);
    HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_RESET);

    /* Wait for ECHO to go high */
    uint32_t timeout = HCSR04_TIMEOUT_US;
    while (HAL_GPIO_ReadPin(HCSR04_ECHO_PORT, HCSR04_ECHO_PIN) == GPIO_PIN_RESET) {
        delay_us(htim, 1);
        if (--timeout == 0) return UINT32_MAX;
    }

    /* Measure high duration */
    __HAL_TIM_SET_COUNTER(htim, 0);
    timeout = HCSR04_TIMEOUT_US;
    while (HAL_GPIO_ReadPin(HCSR04_ECHO_PORT, HCSR04_ECHO_PIN) == GPIO_PIN_SET) {
        if (__HAL_TIM_GET_COUNTER(htim) >= HCSR04_TIMEOUT_US) return UINT32_MAX;
    }
    uint32_t ticks = __HAL_TIM_GET_COUNTER(htim);

    /* ticks (µs) / 5.8 ≈ mm; multiply by 10 / 58 */
    return (ticks * 10U) / 58U;
}
