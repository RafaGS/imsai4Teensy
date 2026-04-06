#include "sim.h"
#include "simdefs.h"
#include "simglb.h"
#include "simio.h"

#include "imsai-sio2.h"

#include "boot_mode.h"
#include "disk_fif_minimal.h"
#include "imsai_hal_teensy.h"

static BYTE io_quiet_in(void)
{
  return IO_DATA_UNUSED;
}

/*
 * Match upstream IMSAI behavior:
 * parallel ports 20/21 must read as 0, otherwise MPU-A monitor
 * assumes a parallel keyboard and won't use SIO1 console.
 */
static BYTE io_pport_in(void)
{
  return 0x00;
}

static int fif_disk_available(void)
{
#if IMSAI_BOOT_MODE == IMSAI_BOOT_MONITOR_ONLY
  return 0;
#elif IMSAI_BOOT_MODE == IMSAI_BOOT_AUTOBOOT_IMDOS
  return 1;
#else
  return imsai_hal_teensy_disk_boot_armed() ? 1 : 0;
#endif
}

static BYTE fif_manual_in(void)
{
  if (fif_disk_available()) {
    return imsai_fif_minimal_in();
  }
  return IO_DATA_UNUSED;
}

static void io_no_card_out(BYTE data)
{
  UNUSED(data);
}

static void fif_manual_out(BYTE data)
{
  if (fif_disk_available()) {
    imsai_fif_minimal_out(data);
    return;
  }
  UNUSED(data);
}

in_func_t *const port_in[256] = {
  [0 ... 255] = io_quiet_in,
  [2] = imsai_sio1a_data_in,
  [3] = imsai_sio1a_status_in,
  [4] = imsai_sio1b_data_in,
  [5] = imsai_sio1b_status_in,
  [8] = imsai_sio1_ctl_in,
  [20] = io_pport_in,
  [21] = io_pport_in,
  [34] = imsai_sio2a_data_in,
  [35] = imsai_sio2a_status_in,
  [36] = imsai_sio2b_data_in,
  [37] = imsai_sio2b_status_in,
  [40] = imsai_sio2_ctl_in,
  [253] = fif_manual_in,
};

out_func_t *const port_out[256] = {
  [0 ... 255] = io_no_card_out,
  [2] = imsai_sio1a_data_out,
  [3] = imsai_sio1a_status_out,
  [4] = imsai_sio1b_data_out,
  [5] = imsai_sio1b_status_out,
  [8] = imsai_sio1_ctl_out,
  [34] = imsai_sio2a_data_out,
  [35] = imsai_sio2a_status_out,
  [36] = imsai_sio2b_data_out,
  [37] = imsai_sio2b_status_out,
  [40] = imsai_sio2_ctl_out,
  [253] = fif_manual_out,
};

void init_io(void)
{
  sio1a_upper_case = false;
  sio1a_strip_parity = true;
  sio1a_drop_nulls = true;
  sio1a_baud_rate = 0;    /* USB serial: no baud rate limiting needed */

  sio1b_upper_case = false;
  sio1b_strip_parity = false;
  sio1b_drop_nulls = false;
  sio1b_baud_rate = 1200;

  sio2a_upper_case = false;
  sio2a_strip_parity = false;
  sio2a_drop_nulls = false;
  sio2a_baud_rate = 9600;

  sio2b_upper_case = false;
  sio2b_strip_parity = false;
  sio2b_drop_nulls = false;
  sio2b_baud_rate = 2400;

  hal_reset();
  imsai_sio_reset();
  imsai_fif_minimal_reset();
}

void exit_io(void)
{
}

void reset_io(void)
{
  hal_reset();
  imsai_sio_reset();
  imsai_fif_minimal_reset();
}
