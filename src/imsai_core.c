#include "imsai_core.h"

#include "sim.h"
#include "simdefs.h"
#include "simglb.h"
#include "simcore.h"
#include "simmem.h"
#include "simio.h"

#include "rom_mpu_a_data.h"

static imsai_rom_t selected_rom = IMSAI_ROM_MPU_A;

void imsai_core_select_rom(imsai_rom_t rom)
{
  selected_rom = rom;
}

imsai_rom_t imsai_core_current_rom(void)
{
  return selected_rom;
}

void imsai_core_init(void)
{
  uint32_t i;
  WORD start_pc;

  // Runtime knobs similar to desktop defaults, but unconstrained speed.
  f_value = 0;
  tmax = 100000;
  cpu_needed = false;

  /* Be explicit: MPU-A ROM is for Intel 8080. */
  cpu = I8080;

  init_cpu();       /* initialize CPU registers */
  init_memory();    /* initialize memory system */

  for (i = 0; i < kMpuARomCount; i++) {
    putmem((WORD)kMpuARom[i].addr, (BYTE)kMpuARom[i].data);
  }

  set_rom_region(0xD800, 0xDFFF);

  /*
   * Baseline MPU-A route known to work on Teensy:
   * keep console routed to SIO1A by neutralizing the STA A8F7 in init path
   * and preloading A8F7 once.
   */
  putmem(0xD827, 0x00);
  putmem(0xD828, 0x00);
  putmem(0xD829, 0x00);
  putmem(0xA8F7, 0x04);
  start_pc = 0xD800;

  /* Initialize I/O devices before starting CPU execution */
  init_io();

  /* Match desktop startup: leave registers/stack from init_cpu(), set entry PC only. */
  PC = start_pc;
  cpu_state = ST_STOPPED;
}

void imsai_core_run_slice(uint32_t instruction_budget)
{
  uint32_t i;

  for (i = 0; i < instruction_budget; i++) {
    step_cpu();
    if (cpu_error != NONE) {
      cpu_error = NONE;
    }
  }
}
