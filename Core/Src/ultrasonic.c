/* This file measures how far away something is.
   It uses sound, the same way a bat does. */

#include "ultrasonic.h"

/* The timer was made in main.c. We borrow it. */
extern TIM_HandleTypeDef htim4;

/* The timer ticks one time every microsecond.
   So one tick means one microsecond. That keeps the math easy. */

/* Hold the trigger pin high this long to start a beep. */
#define TRIG_PULSE_US       12u

/* Wait this long before beeping again. Ten beeps each second. */
#define MEASURE_PERIOD_MS   100u

/* If no echo comes back by now, stop waiting. */
#define ECHO_TIMEOUT_MS     50u

/* Longer than this means nothing was there. Throw it away. */
#define MAX_ECHO_US         25000u

/* Sound takes about 58 microseconds to travel one centimeter and back. */
#define US_PER_CM           58u

typedef enum
{
  US_IDLE = 0,     /* Resting. Not measuring right now. */
  US_WAIT_RISE,    /* We beeped. Waiting for the echo to start. */
  US_WAIT_FALL     /* Echo started. Waiting for it to stop. */
} us_state_t;

/* The interrupt changes these, so we mark them volatile.
   That word tells the compiler they can change at any time. */
static volatile us_state_t state = US_IDLE;
static volatile uint16_t   echo_start = 0;   /* Time the echo began. */
static volatile uint16_t   last_cm = 0;      /* Newest distance we found. */
static volatile uint8_t    fresh = 0;        /* 1 means we have a new answer. */

/* Only the main loop touches this one. */
static uint32_t last_trigger_ms = 0;

/* Wait a few microseconds.
   HAL_Delay only counts milliseconds. That is way too slow here.
   So we watch the timer we already have instead.
   The uint16_t cast keeps this right when the timer rolls over to zero.
   This is the only spot in the whole project that waits. It is 12
   microseconds out of every 100 milliseconds, so it is tiny. */
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

  /* Start the timer and let it tell us when the echo pin changes. */
  HAL_TIM_IC_Start_IT(&htim4, TIM_CHANNEL_1);

  state = US_IDLE;
}

void ultrasonic_task(void)
{
  uint32_t now = HAL_GetTick();

  if (state == US_IDLE)
  {
    /* Subtract, then compare. Never add.
       Adding breaks after 49 days when the clock rolls over. */
    if ((now - last_trigger_ms) >= MEASURE_PERIOD_MS)
    {
      last_trigger_ms = now;

      /* Get ready before we beep, not after.
         A very close wall could echo back before we were listening. */
      state = US_WAIT_RISE;

      HAL_GPIO_WritePin(US_TRIG_GPIO_Port, US_TRIG_Pin, GPIO_PIN_SET);
      delay_us(TRIG_PULSE_US);
      HAL_GPIO_WritePin(US_TRIG_GPIO_Port, US_TRIG_Pin, GPIO_PIN_RESET);
    }
  }
  else
  {
    /* We are waiting for an echo.
       If it never comes we must give up, or we would wait forever. */
    if ((now - last_trigger_ms) >= ECHO_TIMEOUT_MS)
    {
      __HAL_TIM_SET_CAPTUREPOLARITY(&htim4, TIM_CHANNEL_1,
                                    TIM_INPUTCHANNELPOLARITY_RISING);
      state = US_IDLE;
      /* Keep the old distance. Do not set fresh, because it is not new. */
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

/* The chip calls this when the echo pin goes up or down.
   We can only watch one direction at a time.
   So we catch the up edge, then flip and catch the down edge.
   The gap between them is how long the echo lasted. */
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

    /* Now watch for the echo to end. */
    __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_1,
                                  TIM_INPUTCHANNELPOLARITY_FALLING);
    state = US_WAIT_FALL;
  }
  else if (state == US_WAIT_FALL)
  {
    /* Subtracting works even if the timer rolled over. */
    uint16_t width_us = (uint16_t)(captured - echo_start);

    __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_1,
                                  TIM_INPUTCHANNELPOLARITY_RISING);
    state = US_IDLE;

    if (width_us < MAX_ECHO_US)
    {
      last_cm = (uint16_t)(width_us / US_PER_CM);
      fresh = 1;
    }
    /* Too long means too far. We just drop it. */
  }
  else
  {
    /* We were not expecting this. Ignore it and start over. */
    __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_1,
                                  TIM_INPUTCHANNELPOLARITY_RISING);
  }
}
