#ifndef SERIAL_LOG_H
#define SERIAL_LOG_H

#include "stm32g4xx_hal.h"
#include "health_monitor.h"

void SerialLog_Init(UART_HandleTypeDef *huart);
void SerialLog_PrintStatus(const HealthData_t *d);
void SerialLog_Print(const char *msg);

#endif /* SERIAL_LOG_H */
