#ifndef GAME_H
#define GAME_H

#include "stm32g4xx_hal.h"
#include "navigation.h"
#include "mpu6050.h"
#include "audio.h"

/* Game state machine (spec: game state enum). */
typedef enum {
    STATE_INIT = 0,   /* peripherals coming online */
    STATE_CALIBRATE,  /* averaging at-rest IMU samples; placing target */
    STATE_SEEKING,    /* active gameplay: navigate toward target */
    STATE_WIN,        /* target reached: jingle, servo centred & held */
    STATE_HALT,       /* user-requested stop / awaiting reset */
    STATE_ERROR       /* unrecoverable fault; LED blink code active */
} GameState;

/* Wire up peripherals and start in CALIBRATE (or ERROR on sensor fault).
 *   htim_servo -> TIM3 CH1 (PA6),  htim_audio -> TIM2 CH1 (PA0). */
void Game_Init(I2C_HandleTypeDef *hi2c,
               TIM_HandleTypeDef *htim_servo,
               TIM_HandleTypeDef *htim_audio,
               UART_HandleTypeDef *huart,
               RNG_HandleTypeDef *hrng);

/* Self-paced super-loop body; call as fast as possible from main().
 * Internally rate-limits gameplay to LOOP_HZ. O(1) time/space per cycle. */
void Game_Tick(void);

GameState Game_GetState(void);

#endif /* GAME_H */
