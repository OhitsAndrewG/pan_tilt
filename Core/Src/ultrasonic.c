#include "ultrasonic.h"

/* TIM4 is created by CubeMX in main.c; we borrow the handle. */
extern TIM_HandleTypeDef htim4;

/* ------------------------------------------------------------------------
 * Timing constants
 *
 * TIM4 runs at 1 MHz (16 MHz / (Prescaler 15 + 1)), so ONE TIMER TICK IS
 * ONE MICROSECOND. That is what makes all of this arithmetic readable.
 * ---------------------------------------------------------------------- */

/* The datasheet wants TRIG high for at least 10 us. 12 gives margin without
   being long enough to matter. */
#define TRIG_PULSE_US       12u

/* The HC-SR04 needs >= 60 ms between pings so the tail of the previous burst
   is not mistaken for this one's echo. 100 ms gives 10 readings/second. */
#define MEASURE_PERIOD_MS   100u

/* If ECHO has not completed this long after triggering, give up. The module
   itself gives up around 38 ms when nothing reflects. */
#define ECHO_TIMEOUT_MS     50u

/* 4 m round trip is ~23300 us. Anything longer is out of range, not a
   distance, so we reject it rather than reporting nonsense. */
#define MAX_ECHO_US         25000u

/* Sound is 0.0343 cm/us. Halve it for the round trip and you divide by 58.3.
   Integer 58 is plenty -- the sensor is +/-3 mm at best. */
#define US_PER_CM           58u

typedef enum
{
  US_IDLE = 0,     /* between measurements */
  US_WAIT_RISE,    /* triggered; waiting for ECHO to go high */
  US_WAIT_FALL     /* ECHO is high; waiting for it to fall */
} us_state_t;

/* Shared with the capture interrupt, so volatile. */
static volatile us_state_t state = US_IDLE;
static volatile uint16_t   echo_start = 0;   /* capture value on the rising edge */
static volatile uint16_t   last_cm = 0;
static volatile uint8_t    fresh = 0;        /* set by ISR, consumed by main loop */

static uint32_t last_trigger_ms = 0;         /* main-loop only, no volatile needed */

/* ------------------------------------------------------------------------
 * A microsecond delay built from TIM4's own counter.
 *
 * HAL_Delay() only does milliseconds -- 1000x too coarse for a 10 us pulse.
 * Rather than set up DWT or burn another peripheral, we reuse the timer that
 * is already running at 1 MHz for the capture.
 *
 * The cast to uint16_t before comparing is what makes this survive the
 * counter wrapping from 65535 back to 0 mid-pulse: unsigned subtraction gives
 * the true elapsed count either way. Same trick as HAL_GetTick().
 *
 * This IS a busy-wait, and it is the one in the whole design. 12 us out of a
 * 100 ms measurement cycle is 0.012% of the time -- a deliberate trade
 * against spending a second timer on it.
 * ---------------------------------------------------------------------- */
static void delay_us(uint16_t us)
{
  uint16_t start = (uint16_t)__HAL_TIM_GET_COUNTER(&htim4);
  while ((uint16_t)((uint16_t)__HAL_TIM_GET_COUNTER(&htim4) - start) < us)
  {
    /* spin */
  }
}

void ultrasonic_init(void)
{
  HAL_GPIO_WritePin(US_TRIG_GPIO_Port, US_TRIG_Pin, GPIO_PIN_RESET);

  /* Starts the timer counting AND enables the capture interrupt on CH1.
     Without the _IT variant the capture register would still update, but
     nothing would tell us it had. */
  HAL_TIM_IC_Start_IT(&htim4, TIM_CHANNEL_1);

  state = US_IDLE;
}

void ultrasonic_task(void)
{
  uint32_t now = HAL_GetTick();

  if (state == US_IDLE)
  {
    /* Subtract-then-compare, never add-then-compare: this stays correct when
       the 32-bit millisecond tick wraps after ~49 days. */
    if ((now - last_trigger_ms) >= MEASURE_PERIOD_MS)
    {
      last_trigger_ms = now;

      /* Arm before pulsing, not after. If we set the state afterwards, a very
         close target could echo back before we were ready to record it. */
      state = US_WAIT_RISE;

      HAL_GPIO_WritePin(US_TRIG_GPIO_Port, US_TRIG_Pin, GPIO_PIN_SET);
      delay_us(TRIG_PULSE_US);
      HAL_GPIO_WritePin(US_TRIG_GPIO_Port, US_TRIG_Pin, GPIO_PIN_RESET);
    }
  }
  else
  {
    /* A measurement is in flight. If the echo never completes -- soft target,
       out of range, sensor unplugged -- we must recover rather than sit in
       US_WAIT_* forever and never ping again. */
    if ((now - last_trigger_ms) >= ECHO_TIMEOUT_MS)
    {
      __HAL_TIM_SET_CAPTUREPOLARITY(&htim4, TIM_CHANNEL_1,
                                    TIM_INPUTCHANNELPOLARITY_RISING);
      state = US_IDLE;
      /* last_cm is deliberately left alone: the old reading stays available
         and `fresh` stays clear, so the caller can tell nothing new arrived. */
    }
  }
}

uint16_t ultrasonic_get_cm(void)
{
  return last_cm;
}

uint8_t ultrasonic_take_reading(void)
{
  if (fresh)
  {
    fresh = 0;
    return 1;
  }
  return 0;
}

/**
  * @brief Input-capture interrupt: one edge of the ECHO pulse.
  *
  * Overrides the HAL's __weak version, same mechanism as the UART callback.
  * Chain: TIM4 hardware -> TIM4_IRQHandler() -> HAL_TIM_IRQHandler() -> here.
  *
  * The channel is configured for ONE polarity at a time, so we flip it inside
  * the interrupt: catch the rising edge, switch to falling, catch that, switch
  * back. That is how you measure a pulse width with a single capture channel.
  */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
  uint16_t captured;

  if (htim->Instance != TIM4)
  {
    return;
  }
  if (htim->Channel != HAL_TIM_ACTIVE_CHANNEL_1)
  {
    return;
  }

  captured = (uint16_t)HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);

  if (state == US_WAIT_RISE)
  {
    echo_start = captured;

    __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_1,
                                  TIM_INPUTCHANNELPOLARITY_FALLING);
    state = US_WAIT_FALL;
  }
  else if (state == US_WAIT_FALL)
  {
    /* The whole point of 16-bit unsigned subtraction: if the counter wrapped
       past 65535 between the two edges, this still yields the true width. No
       special case needed. */
    uint16_t width_us = (uint16_t)(captured - echo_start);

    __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_1,
                                  TIM_INPUTCHANNELPOLARITY_RISING);
    state = US_IDLE;

    if (width_us < MAX_ECHO_US)
    {
      last_cm = (uint16_t)(width_us / US_PER_CM);
      fresh = 1;
    }
    /* Too long: out of range. Drop it rather than report a bogus distance. */
  }
  else
  {
    /* Edge arrived while idle -- noise, or a late echo from a previous ping.
       Ignore it and make sure we are back on rising. */
    __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_1,
                                  TIM_INPUTCHANNELPOLARITY_RISING);
  }
}
