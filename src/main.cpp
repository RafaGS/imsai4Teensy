#include <Arduino.h>

#include "boot_mode.h"
#include "imsai_hal_teensy.h"
#include "imsai_core.h"
#include "teensy_bsp.h"

// Enable linked 8080 core wrapper.
#define IMSAI_CORE_LINKED 1

#ifdef IMSAI_CORE_LINKED
#include <stdint.h>
#endif

static const char *boot_mode_name(void)
{
#if IMSAI_BOOT_MODE == IMSAI_BOOT_MONITOR_ONLY
  return "monitor-only";
#elif IMSAI_BOOT_MODE == IMSAI_BOOT_MONITOR_THEN_DISK
  return "monitor-then-disk";
#elif IMSAI_BOOT_MODE == IMSAI_BOOT_AUTOBOOT_IMDOS
  return "autoboot-imdos";
#else
  return "unknown";
#endif
}

static const char *rom_name(imsai_rom_t rom)
{
  if (rom == IMSAI_ROM_BASIC8K) {
    return "BASIC8K";
  }
  return "MPU-A-ROM";
}

static void print_banner(void)
{
  Serial.println();
  Serial.println("imsai4Teensy: IMSAI 8080 Emulator on Teensy 4.1");
  Serial.println("Serial-only mode, no front panel");
#ifdef IMSAI_CORE_LINKED
  Serial.println("Core: linked");
#else
  Serial.println("Core: not linked yet (HAL loop test)");
#endif
  Serial.print("Boot mode: ");
  Serial.println(boot_mode_name());
  Serial.print("ROM: ");
  Serial.println(rom_name(imsai_core_current_rom()));
  Serial.println("Ctrl+R: soft reset");
  Serial.println();
}

static void drain_serial_input(uint32_t settle_ms)
{
  uint32_t t0 = millis();

  while ((millis() - t0) < settle_ms) {
    while (Serial.available() > 0) {
      (void)Serial.read();
      t0 = millis();
    }
    delay(1);
  }
}

void setup()
{
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  bsp_init();
  imsai_hal_teensy_init();

#ifdef IMSAI_CORE_LINKED
  imsai_rom_t selected_rom = IMSAI_ROM_MPU_A;
  imsai_core_select_rom(selected_rom);
  imsai_hal_teensy_set_line_mode(false);
  imsai_hal_teensy_set_disk_boot_intercept(true);
  print_banner();
  drain_serial_input(120);
  imsai_core_init();
#else
  print_banner();
#endif
}

void loop()
{
#ifdef IMSAI_CORE_LINKED
  imsai_hal_teensy_poll();

  if (imsai_hal_teensy_take_soft_reset_request()) {
    drain_serial_input(60);
    imsai_hal_teensy_set_line_mode(false);
    imsai_core_init();
    return;
  }

  // Run bounded slices to keep USB serial responsive.
  imsai_core_run_slice(2000);
#else
  // Smoke test mode: direct USB serial echo.
  // Keep this path simple and deterministic for terminal validation.
  while (Serial.available() > 0) {
    int c = Serial.read();
    if (c < 0) {
      break;
    }

    if (c == '\r') {
      Serial.write('\r');
      Serial.write('\n');
    } else if (c == '\n') {
      // Ignore LF when host sends CRLF.
    } else {
      Serial.write((char)c);
    }
  }
  Serial.flush();
  yield();
#endif
}
