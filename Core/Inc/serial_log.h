#ifndef SERIAL_LOG_H
#define SERIAL_LOG_H

#include "stm32g4xx_hal.h"
#include "navigation.h"
#include "audio.h"
#include "game.h"

void SerialLog_Init(UART_HandleTypeDef *huart);

/* Raw string out (blocking, short timeout). */
void SerialLog_Print(const char *msg);

/* One formatted telemetry line: state, position, heading, distance, bearing. */
void SerialLog_PrintNav(const NavState *n, GameState state, AudioCue cue);

#endif /* SERIAL_LOG_H */
