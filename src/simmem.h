#ifndef SIMMEM_INC
#define SIMMEM_INC

#include "sim.h"
#include "simdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

extern BYTE memory[64 << 10];

void init_memory(void);
void reset_memory(void);
void set_rom_region(WORD start, WORD end);

BYTE memrdr(WORD addr);
void memwrt(WORD addr, BYTE data);
BYTE getmem(WORD addr);
void putmem(WORD addr, BYTE data);
BYTE dma_read(WORD addr);
void dma_write(WORD addr, BYTE data);

#ifdef __cplusplus
}
#endif

#endif
