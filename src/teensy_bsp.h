#ifndef TEENSY_BSP_H
#define TEENSY_BSP_H

#include <stdint.h>

void bsp_init(void);
uint64_t bsp_clock_us(void);
void bsp_sleep_us(uint32_t us);
void bsp_sleep_ms(uint32_t ms);

#endif
