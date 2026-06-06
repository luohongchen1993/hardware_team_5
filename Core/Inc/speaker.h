#ifndef SPEAKER_H
#define SPEAKER_H

#include "stm32g4xx_hal.h"
#include "health_monitor.h"

void Speaker_Init(TIM_HandleTypeDef *htim);
void Speaker_SetAlarm(HealthState_t state);
void Speaker_Update(void);   /* call every main-loop tick */
void Speaker_Stop(void);

#endif /* SPEAKER_H */
