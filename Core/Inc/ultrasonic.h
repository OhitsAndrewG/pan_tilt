#ifndef ULTRASONIC_H
#define ULTRASONIC_H

#include "main.h"

/* Set up the module and start TIM4's input capture. Call once, after
   MX_TIM4_Init() and MX_GPIO_Init() have run. */
void ultrasonic_init(void);

/* Call this every pass through the main loop. It paces the triggering and
   enforces the echo timeout. It never blocks for more than the ~12 us of the
   trigger pulse itself. */
void ultrasonic_task(void);

/* Most recent good reading in centimetres. 0 means "nothing measured yet". */
uint16_t ultrasonic_get_cm(void);

/* Returns 1 exactly once per new reading, then clears itself -- so the main
   loop can tell a fresh measurement from a repeat of the old one. */
uint8_t ultrasonic_take_reading(void);

#endif /* ULTRASONIC_H */
