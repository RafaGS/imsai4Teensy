#include "simport.h"

#include <Arduino.h>

void sleep_for_us(unsigned long time)
{
  delayMicroseconds((uint32_t)time);
}

void sleep_for_ms(unsigned time)
{
  delay((uint32_t)time);
}

uint64_t get_clock_us(void)
{
  return (uint64_t)micros();
}
