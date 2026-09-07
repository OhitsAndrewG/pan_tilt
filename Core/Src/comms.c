#include "comms.h"

extern UART_HandleTypeDef huart1;

/* Power of two so the wrap can be a mask instead of a modulo -- integer
   division is slow on Cortex-M4 and this runs inside an interrupt. */
#define TX_SIZE  256u
#define TX_MASK  (TX_SIZE - 1u)

static volatile char     tx_buf[TX_SIZE];
static volatile uint16_t tx_head = 0;   /* written by main loop only */
static volatile uint16_t tx_tail = 0;   /* written by the ISR only    */
static volatile uint8_t  tx_busy = 0;
static uint8_t           tx_byte = 0;   /* the byte currently in flight */

/* ------------------------------------------------------------------------
 * Hand the next queued byte to the UART.
 *
 * MUST be called either from the TX interrupt, or from the main loop with
 * interrupts disabled. See the critical section in comms_puts() for why.
 *
 * The byte is copied into tx_byte rather than pointing the HAL at the ring
 * directly: the HAL holds that pointer until the transfer finishes, and the
 * ring position could be overwritten in the meantime.
 * ---------------------------------------------------------------------- */
static void tx_kick(void)
{
  if (tx_head == tx_tail)
  {
    tx_busy = 0;        /* queue drained -- go idle */
    return;
  }

  tx_byte = (uint8_t)tx_buf[tx_tail];
  tx_tail = (uint16_t)((tx_tail + 1u) & TX_MASK);
  tx_busy = 1;

  HAL_UART_Transmit_IT(&huart1, &tx_byte, 1);
}

void comms_init(void)
{
  tx_head = 0;
  tx_tail = 0;
  tx_busy = 0;
}

void comms_puts(const char *s)
{
  while (*s != '\0')
  {
    uint16_t next = (uint16_t)((tx_head + 1u) & TX_MASK);

    if (next == tx_tail)
    {
      break;            /* full -- drop the rest, never block */
    }

    tx_buf[tx_head] = *s++;

    /* Advance head only AFTER the byte is stored. The ISR reads head to
       decide whether data exists; publishing the index last means it can
       never see a slot that has not been written yet. */
    tx_head = next;
  }

  /* CRITICAL SECTION.
     Without it: we read tx_busy as 1 and skip the kick, but the final
     TX-complete interrupt fires right then, finds the queue empty (we had not
     advanced head yet) and sets tx_busy = 0. Nobody restarts the pump, and
     everything we just queued sits there forever.
     Masking interrupts for these few instructions closes that window. */
  __disable_irq();
  if (!tx_busy)
  {
    tx_kick();
  }
  __enable_irq();
}

uint8_t comms_tx_busy(void)
{
  return tx_busy;
}

/**
  * @brief One byte finished going out; send the next.
  *
  * Overrides the HAL's __weak version -- same mechanism as the RX callback.
  * Chain: USART1 TXE/TC interrupt -> USART1_IRQHandler() ->
  *        HAL_UART_IRQHandler() -> here.
  */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    tx_kick();
  }
}
