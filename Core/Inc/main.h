#ifndef MAIN_H
#define MAIN_H

#include "stm32g4xx_hal.h"
#include "config.h"

/* Fatal, unrecoverable fault: disables IRQs and fast-strobes the red LED. */
void Error_Handler(void);

#endif /* MAIN_H */
