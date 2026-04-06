#include <string.h>

#include "simmem.h"

BYTE memory[64 << 10];

static WORD rom_start = 0;
static WORD rom_end = 0;

void set_rom_region(WORD start, WORD end)
{
  rom_start = start;
  rom_end = end;
}

void init_memory(void)
{
  memset(memory, 0x00, sizeof(memory));
  rom_start = 0;
  rom_end = 0;
}

void reset_memory(void)
{
  // Keep RAM contents across reset for now.
}

BYTE memrdr(WORD addr)
{
  return memory[addr];
}

void memwrt(WORD addr, BYTE data)
{
  if (addr >= rom_start && addr <= rom_end) {
    return;
  }
  memory[addr] = data;
}

BYTE getmem(WORD addr)
{
  return memory[addr];
}

void putmem(WORD addr, BYTE data)
{
  memory[addr] = data;
}

BYTE dma_read(WORD addr)
{
  return memory[addr];
}

void dma_write(WORD addr, BYTE data)
{
  memwrt(addr, data);
}
