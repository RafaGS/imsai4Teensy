#ifndef DISK_FIF_MINIMAL_H
#define DISK_FIF_MINIMAL_H

#include <stdint.h>

#include "sim.h"
#include "simdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

BYTE imsai_fif_minimal_in(void);
void imsai_fif_minimal_out(BYTE data);
void imsai_fif_minimal_reset(void);
void imsai_fif_minimal_set_boot_unit(uint8_t unit);
uint8_t imsai_fif_minimal_get_boot_unit(void);

#ifdef __cplusplus
}
#endif

#endif
