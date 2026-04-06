#ifndef IMSAI_HAL_TEENSY_H
#define IMSAI_HAL_TEENSY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t BYTE;

typedef enum sio_port {
  SIO1A,
  SIO1B,
  SIO2A,
  SIO2B,
  MAX_SIO_PORT
} sio_port_t;

void imsai_hal_teensy_init(void);
void imsai_hal_teensy_poll(void);
void imsai_hal_teensy_set_line_mode(bool enabled);
void imsai_hal_teensy_set_disk_boot_intercept(bool enabled);
bool imsai_hal_teensy_take_soft_reset_request(void);
bool imsai_hal_teensy_monitor_prompt_seen(void);
bool imsai_hal_teensy_disk_boot_armed(void);

// Compatibility layer for iodevices/imsai-sio2.c expectations.
void hal_reset(void);
void hal_status_in(sio_port_t sio, BYTE *stat);
int hal_data_in(sio_port_t sio);
void hal_data_out(sio_port_t sio, BYTE data);
bool hal_carrier_detect(sio_port_t sio);

#ifdef __cplusplus
}
#endif

#endif
