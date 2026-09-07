#ifndef COMMS_H
#define COMMS_H

#include "main.h"

/* Call once at startup, after MX_USART1_UART_Init(). */
void comms_init(void);

/* Queue a NUL-terminated string for transmission and return IMMEDIATELY.
   The bytes go out later, driven by the TX-complete interrupt. If the queue
   is full the remainder of the string is dropped rather than blocking --
   losing a telemetry line is always better than stalling the control loop. */
void comms_puts(const char *s);

/* 1 while bytes are still going out. Rarely needed; useful in tests. */
uint8_t comms_tx_busy(void);

#endif /* COMMS_H */
