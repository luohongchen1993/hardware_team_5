#include "temp_sensor.h"
#include "config.h"

/* DS18B20 1-Wire driver.
 * GPIO pin (TEMP_SENSOR_PIN) must be configured as open-drain output
 * with external 4.7 kΩ pull-up to 3.3 V.  The 1-µs-tick free-running
 * timer (htim) is shared with the distance sensor.
 */

/* ---- low-level 1-Wire helpers ---- */

static inline void pin_low(void)
{
    /* Switch to output-low */
    GPIO_InitTypeDef g = {0};
    g.Pin  = TEMP_SENSOR_PIN;
    g.Mode = GPIO_MODE_OUTPUT_OD;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(TEMP_SENSOR_PORT, &g);
    HAL_GPIO_WritePin(TEMP_SENSOR_PORT, TEMP_SENSOR_PIN, GPIO_PIN_RESET);
}

static inline void pin_release(void)
{
    /* Switch to input (pull-up does the work) */
    GPIO_InitTypeDef g = {0};
    g.Pin  = TEMP_SENSOR_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(TEMP_SENSOR_PORT, &g);
}

static inline uint8_t pin_read(void)
{
    return (uint8_t)HAL_GPIO_ReadPin(TEMP_SENSOR_PORT, TEMP_SENSOR_PIN);
}

static inline void delay_us(TIM_HandleTypeDef *htim, uint32_t us)
{
    __HAL_TIM_SET_COUNTER(htim, 0);
    while (__HAL_TIM_GET_COUNTER(htim) < us) {}
}

/* Returns 0 on presence detected, 1 on no device */
static int ow_reset(TIM_HandleTypeDef *htim)
{
    pin_low();
    delay_us(htim, 480);
    pin_release();
    delay_us(htim, 70);
    int presence = (pin_read() == GPIO_PIN_RESET) ? 0 : 1;
    delay_us(htim, 410);
    return presence;
}

static void ow_write_bit(TIM_HandleTypeDef *htim, uint8_t bit)
{
    pin_low();
    if (bit) {
        delay_us(htim, 6);
        pin_release();
        delay_us(htim, 64);
    } else {
        delay_us(htim, 60);
        pin_release();
        delay_us(htim, 10);
    }
}

static uint8_t ow_read_bit(TIM_HandleTypeDef *htim)
{
    pin_low();
    delay_us(htim, 6);
    pin_release();
    delay_us(htim, 9);
    uint8_t bit = pin_read();
    delay_us(htim, 55);
    return bit;
}

static void ow_write_byte(TIM_HandleTypeDef *htim, uint8_t byte)
{
    for (int i = 0; i < 8; i++) {
        ow_write_bit(htim, (byte >> i) & 1U);
    }
}

static uint8_t ow_read_byte(TIM_HandleTypeDef *htim)
{
    uint8_t val = 0;
    for (int i = 0; i < 8; i++) {
        val |= (ow_read_bit(htim) << i);
    }
    return val;
}

/* ---- public API ---- */

float Temp_ReadCelsius(TIM_HandleTypeDef *htim)
{
    /* Reset + presence check */
    if (ow_reset(htim) != 0) return -999.0f;

    /* Skip ROM (single device on bus), start conversion */
    ow_write_byte(htim, 0xCC);  /* SKIP_ROM */
    ow_write_byte(htim, 0x44);  /* CONVERT_T */

    /* Wait up to 750 ms for 12-bit conversion */
    uint32_t t = HAL_GetTick();
    while (!ow_read_bit(htim)) {
        if ((HAL_GetTick() - t) > 800U) return -999.0f;
    }

    /* Read scratchpad */
    if (ow_reset(htim) != 0) return -999.0f;
    ow_write_byte(htim, 0xCC);  /* SKIP_ROM */
    ow_write_byte(htim, 0xBE);  /* READ_SCRATCHPAD */

    uint8_t lo = ow_read_byte(htim);
    uint8_t hi = ow_read_byte(htim);

    /* Cancel remaining bytes */
    ow_reset(htim);

    int16_t raw = (int16_t)((hi << 8) | lo);
    return raw / 16.0f;
}
