#include "teensy_bsp.h"

#include <Arduino.h>

void bsp_init(void)
{
  // Reserved for clock, watchdog, and SD init in later phases.
}

uint64_t bsp_clock_us(void)
{
  return (uint64_t)micros();
}

void bsp_sleep_us(uint32_t us)
{
  delayMicroseconds(us);
}

void bsp_sleep_ms(uint32_t ms)
{
  delay(ms);
}
