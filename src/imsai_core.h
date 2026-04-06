#ifndef IMSAI_CORE_H
#define IMSAI_CORE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	IMSAI_ROM_MPU_A = 0,
	IMSAI_ROM_BASIC8K = 1,
} imsai_rom_t;

void imsai_core_init(void);
void imsai_core_select_rom(imsai_rom_t rom);
imsai_rom_t imsai_core_current_rom(void);
void imsai_core_run_slice(uint32_t instruction_budget);

#ifdef __cplusplus
}
#endif

#endif
