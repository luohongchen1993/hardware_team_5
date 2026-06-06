#ifndef AUDIO_H
#define AUDIO_H

#include "stm32g4xx_hal.h"
#include <stdint.h>

/* Proximity cue level returned by Audio_Update(). */
typedef enum {
    AUDIO_CUE_FAR = 0,
    AUDIO_CUE_MID,
    AUDIO_CUE_NEAR,
    AUDIO_CUE_VERY_NEAR
} AudioCue;

/* Speaker driven by PWM square wave on TIM2 CH1 (PA0). */

void Audio_Init(TIM_HandleTypeDef *htim);

/* Set the output tone. freq_hz == 0 silences the output. */
void Audio_SetTone(TIM_HandleTypeDef *htim, uint32_t freq_hz);

void Audio_Off(TIM_HandleTypeDef *htim);

/* Per-tick update: maps distance to pitch + beep cadence (faster/higher as
 * the target nears) and manages the on/off beeping. Returns the cue level. */
AudioCue Audio_Update(TIM_HandleTypeDef *htim, float distance_m);

/* Blocking ascending victory arpeggio (used once on win, when motion stops). */
void Audio_WinJingle(TIM_HandleTypeDef *htim);

#endif /* AUDIO_H */
