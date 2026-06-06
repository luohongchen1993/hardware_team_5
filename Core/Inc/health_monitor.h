#ifndef HEALTH_MONITOR_H
#define HEALTH_MONITOR_H

#include "stm32g4xx_hal.h"
#include <stdint.h>

typedef enum {
    STATE_NOMINAL = 0,
    STATE_VIBRATION_WARN,
    STATE_THERMAL_WARN,
    STATE_SAFETY_BREACH,
    STATE_CRITICAL
} HealthState_t;

typedef struct {
    float       temp_c;
    uint32_t    dist_mm;
    float       vib_rms;
    float       ax, ay, az;
    float       gx, gy, gz;
    HealthState_t state;
} HealthData_t;

void          HealthMonitor_Init(I2C_HandleTypeDef *hi2c,
                                 TIM_HandleTypeDef *htim_servo,
                                 TIM_HandleTypeDef *htim_us,
                                 TIM_HandleTypeDef *htim_spk,
                                 UART_HandleTypeDef *huart);
void          HealthMonitor_Update(void);
HealthState_t HealthMonitor_GetState(void);
HealthData_t *HealthMonitor_GetData(void);

#endif /* HEALTH_MONITOR_H */
