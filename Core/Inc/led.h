#ifndef LED_H
#define LED_H

#include <stdint.h>

#define LED_GREEN   (1U << 0)
#define LED_AMBER   (1U << 1)
#define LED_RED     (1U << 2)
#define LED_ALL     (LED_GREEN | LED_AMBER | LED_RED)

void LED_Init(void);
void LED_Set(uint8_t mask, uint8_t on);      /* on: 1=on, 0=off */
void LED_Toggle(uint8_t mask);
void LED_Off(uint8_t mask);

#endif /* LED_H */
