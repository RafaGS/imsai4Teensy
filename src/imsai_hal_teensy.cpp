#include "imsai_hal_teensy.h"

#include <Arduino.h>
#include <string.h>

#include "disk_image_provider.h"
#include "disk_fif_minimal.h"

namespace {
constexpr uint16_t RX_CAP = 512;
volatile uint16_t rx_head = 0;
volatile uint16_t rx_tail = 0;
uint8_t rx_buf[RX_CAP];

constexpr bool FRIENDLY_LINE_MODE = true;
constexpr uint16_t LINE_CAP = 128;
uint8_t line_buf[LINE_CAP];
uint16_t line_len = 0;
bool friendly_armed = false;
bool disk_boot_intercept_enabled = true;
bool disk_boot_armed = false;
volatile bool soft_reset_requested = false;

inline uint16_t next_idx(uint16_t v)
{
  return (uint16_t)((v + 1u) % RX_CAP);
}

inline bool rx_push(uint8_t b)
{
  uint16_t next = next_idx(rx_head);
  if (next == rx_tail) {
    return false;
  }
  rx_buf[rx_head] = b;
  rx_head = next;
  return true;
}

inline int rx_pop(void)
{
  if (rx_head == rx_tail) {
    return -1;
  }
  uint8_t b = rx_buf[rx_tail];
  rx_tail = next_idx(rx_tail);
  return b;
}

inline void echo_backspace(void)
{
  Serial.write('\b');
  Serial.write(' ');
  Serial.write('\b');
}

inline void commit_line_to_rx(void)
{
  uint16_t i;

  for (i = 0; i < line_len; i++) {
    (void)rx_push(line_buf[i]);
  }
  (void)rx_push((uint8_t)'\r');
  line_len = 0;
}

inline void queue_monitor_boot_command(void)
{
  /* Return to raw mode while monitor executes boot sequence. */
  friendly_armed = false;
  (void)rx_push((uint8_t)'B');
  (void)rx_push((uint8_t)'\r');
}

inline int unit_from_letter(int c)
{
  if (c >= 'a' && c <= 'z') {
    c -= ('a' - 'A');
  }
  if (c < 'A' || c > 'D') {
    return -1;
  }
  return (c - 'A') + 1;
}

inline bool handle_local_disk_command(void)
{
  char line[LINE_CAP + 1];
  char *p;
  uint16_t i;
  int unit;

  for (i = 0; i < line_len && i < LINE_CAP; i++) {
    line[i] = (char)line_buf[i];
  }
  line[i] = '\0';

  p = line;
  while (*p == ' ') {
    p++;
  }
  if (*p == '\0') {
    return false;
  }

  if (*p == 'B' || *p == 'b') {
    p++;
    while (*p == ' ') {
      p++;
    }

    if (*p == '\0') {
      unit = 1; /* B => boot A */
    } else {
      unit = unit_from_letter((int)*p);
      if (unit < 0) {
        return false;
      }
      p++;
      while (*p == ' ') {
        p++;
      }
      if (*p != '\0') {
        return false;
      }
    }

    if (!disk_image_provider_is_mounted((uint8_t)unit)) {
      Serial.print("\r\nNO DISK IN UNIT ");
      Serial.print((char)('A' + unit - 1));
      Serial.print("\r\n?");
      line_len = 0;
      return true;
    }

    imsai_fif_minimal_set_boot_unit((uint8_t)unit);
    disk_boot_armed = true;
    Serial.print("\r\n[BOOT ");
    Serial.print((char)('A' + unit - 1));
    Serial.print(": ");
    Serial.print(disk_image_provider_unit_name((uint8_t)unit));
    Serial.print("]\r\n");
    queue_monitor_boot_command();
    line_len = 0;
    return true;
  }

  if (*p == 'L' || *p == 'l') {
    char *name;

    p++;
    while (*p == ' ') {
      p++;
    }
    unit = unit_from_letter((int)*p);
    if (unit < 0) {
      return false;
    }

    p++;
    while (*p == ' ') {
      p++;
    }
    name = p;
    if (*name == '\0') {
      return false;
    }

    if (!disk_image_provider_mount_named_to_unit((uint8_t)unit, name) ||
        !disk_image_provider_is_bootable_unit((uint8_t)unit)) {
      Serial.print("\r\nDISCO NO BOTABLE U");
      Serial.print((char)('A' + unit - 1));
      Serial.print(": ");
      Serial.print(name);
      Serial.print(" (");
      Serial.print(disk_image_provider_last_error());
      Serial.print(")\r\n?");
      line_len = 0;
      return true;
    }

    Serial.print("\r\n[LOADED ");
    Serial.print((char)('A' + unit - 1));
    Serial.print(": ");
    Serial.print(disk_image_provider_unit_name((uint8_t)unit));
    Serial.print("]\r\n?");
    line_len = 0;
    return true;
  }

  if (*p == 'S' || *p == 's') {
    p++;
    while (*p == ' ') {
      p++;
    }
    if (*p != '\0') {
      return false;
    }
    Serial.print("\r\nDISK UNITS:\r\n");
    for (int u = 1; u <= 4; u++) {
      Serial.print("  ");
      Serial.print((char)('A' + u - 1));
      Serial.print(": ");
      if (disk_image_provider_is_mounted((uint8_t)u)) {
        Serial.print(disk_image_provider_unit_name((uint8_t)u));
        if ((uint8_t)u == imsai_fif_minimal_get_boot_unit()) {
          Serial.print(" [BOOT]");
        }
      } else {
        Serial.print("(empty)");
      }
      Serial.print("\r\n");
    }
    Serial.print("?");
    line_len = 0;
    return true;
  }

  return false;
}

inline void poll_friendly_line_mode(void)
{
  while (Serial.available() > 0) {
    int c = Serial.read();
    if (c < 0) {
      break;
    }

    if (c == '\n') {
      continue;
    }

    if (c == 0x12) {
      // Ctrl+R: request monitor soft reset.
      soft_reset_requested = true;
      line_len = 0;
      Serial.print("\r\n[SOFT RESET]\r\n");
      continue;
    }

    if (c == '\r') {
      Serial.write('\r');
      Serial.write('\n');
      if (disk_boot_intercept_enabled && handle_local_disk_command()) {
        continue;
      }
      commit_line_to_rx();
      continue;
    }

    if (c == 0x08 || c == 0x7f) {
      if (line_len > 0) {
        line_len--;
        echo_backspace();
      }
      continue;
    }

    c &= 0x7F;

    if (line_len < LINE_CAP) {
      line_buf[line_len++] = (uint8_t)c;
      Serial.write((char)c);
    }
  }
}

inline void poll_raw_mode(void)
{
  while (Serial.available() > 0) {
    int c = Serial.read();
    if (c < 0) {
      break;
    }
    if (c == '\n') {
      continue;
    }

    if (c == 0x12) {
      soft_reset_requested = true;
      Serial.print("\r\n[SOFT RESET]\r\n");
      continue;
    }

    c &= 0x7F;
    (void)rx_push((uint8_t)c);
  }
}
}  // namespace

extern "C" {

void imsai_hal_teensy_init(void)
{
  rx_head = 0;
  rx_tail = 0;
  line_len = 0;
  friendly_armed = false;
  disk_boot_intercept_enabled = true;
  disk_boot_armed = false;
  soft_reset_requested = false;
}

void imsai_hal_teensy_poll(void)
{
  if (FRIENDLY_LINE_MODE && friendly_armed) {
    poll_friendly_line_mode();
  } else {
    poll_raw_mode();
  }
}

void imsai_hal_teensy_set_line_mode(bool enabled)
{
  friendly_armed = enabled;
  if (!enabled) {
    line_len = 0;
  }
}

void imsai_hal_teensy_set_disk_boot_intercept(bool enabled)
{
  disk_boot_intercept_enabled = enabled;
}

bool imsai_hal_teensy_take_soft_reset_request(void)
{
  bool v = soft_reset_requested;
  soft_reset_requested = false;
  return v;
}

bool imsai_hal_teensy_monitor_prompt_seen(void)
{
  return friendly_armed;
}

bool imsai_hal_teensy_disk_boot_armed(void)
{
  return disk_boot_armed;
}

void hal_reset(void)
{
  /* Reset RX buffer only, preserve line_mode and other HAL settings */
  rx_head = 0;
  rx_tail = 0;
  line_len = 0;
  disk_boot_armed = false;
  soft_reset_requested = false;
}

void hal_status_in(sio_port_t sio, BYTE *stat)
{
  *stat &= (BYTE)(~0x03);
  if (sio == SIO1A) {
    *stat |= 0x01;
    if (rx_head != rx_tail) {
      *stat |= 0x02;
    }
  }
}

int hal_data_in(sio_port_t sio)
{
  if (sio != SIO1A) {
    return -1;
  }
  return rx_pop();
}

void hal_data_out(sio_port_t sio, BYTE data)
{
  BYTE out;

  if (sio != SIO1A) {
    return;
  }
  out = (BYTE)(data & 0x7F);
  if (out == 0) {
    return;
  }

  /* In monitor flow, arm line mode when prompt '?' is displayed. */
  if (disk_boot_intercept_enabled && out == (BYTE)'?') {
    friendly_armed = true;
  }

  Serial.write((char)out);
}

bool hal_carrier_detect(sio_port_t sio)
{
  (void)sio;
  return true;
}

}  // extern "C"
