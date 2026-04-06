#include "disk_fif_minimal.h"

#include <string.h>

#include "boot_mode.h"
#include "disk_image_provider.h"
#include "simmem.h"

/* IMSAI FIF disk descriptor offsets */
#define DD_UNIT   0
#define DD_RESULT 1
#define DD_NN     2
#define DD_TRACK  3
#define DD_SECTOR 4
#define DD_DMAL   5
#define DD_DMAH   6

/* IMSAI FIF commands */
#define WRITE_SEC   1
#define READ_SEC    2
#define FMT_TRACK   3
#define VERIFY_DATA 4

#define SEC_SZ   128
#define SPT8     26
#define TRK8     77
#define FLOPPY_BYTES (SPT8 * TRK8 * SEC_SZ)

/*
 * Volatile write overlay so CP/M can write temporary sectors even when
 * the backing image is opened read-only from SD.
 */
#define OVERLAY_SLOTS 64

typedef struct {
  uint8_t unit;
  uint32_t pos;
  uint8_t data[SEC_SZ];
  uint8_t valid;
} overlay_slot_t;

static int fdstate = 0;
static uint16_t fdaddr[16];
static overlay_slot_t overlay[OVERLAY_SLOTS];
static uint8_t overlay_next = 0;
static uint8_t boot_unit = 1; /* 1=A,2=B,3=C,4=D */

static int overlay_find(uint8_t unit, uint32_t pos)
{
  int i;
  for (i = 0; i < OVERLAY_SLOTS; i++) {
    if (overlay[i].valid && overlay[i].unit == unit && overlay[i].pos == pos) {
      return i;
    }
  }
  return -1;
}

static int overlay_alloc(uint8_t unit, uint32_t pos)
{
  int i;

  for (i = 0; i < OVERLAY_SLOTS; i++) {
    if (!overlay[i].valid) {
      overlay[i].valid = 1;
      overlay[i].unit = unit;
      overlay[i].pos = pos;
      return i;
    }
  }

  i = overlay_next;
  overlay_next = (uint8_t)((overlay_next + 1) % OVERLAY_SLOTS);
  overlay[i].valid = 1;
  overlay[i].unit = unit;
  overlay[i].pos = pos;
  return i;
}

static void disk_io(uint16_t addr)
{
  int i;
  int unit;
  int cmd;
  int res;
  int fmt;
  int track;
  int sector;
  uint16_t dma_addr;
  uint32_t pos;

  unit = dma_read((WORD)(addr + DD_UNIT)) & 0x0f;
  cmd = dma_read((WORD)(addr + DD_UNIT)) >> 4;
  res = dma_read((WORD)(addr + DD_RESULT));
  fmt = dma_read((WORD)(addr + DD_NN));
  track = dma_read((WORD)(addr + DD_TRACK));
  sector = dma_read((WORD)(addr + DD_SECTOR));
  dma_addr = (uint16_t)((dma_read((WORD)(addr + DD_DMAH)) << 8) |
                        dma_read((WORD)(addr + DD_DMAL)));

  if (res) {
    dma_write((WORD)(addr + DD_RESULT), 0xc1);
    return;
  }

  if (fmt) {
    dma_write((WORD)(addr + DD_RESULT), 0xc8);
    return;
  }

  /* Unit 0 is invalid on this controller. */
  if (unit == 0) {
    dma_write((WORD)(addr + DD_RESULT), 0xc2);
    return;
  }

  /* Monitor boots from A; alias A requests to selected boot unit. */
  if (unit == 1 && boot_unit >= 1 && boot_unit <= 4) {
    unit = boot_unit;
  }

  if (unit < 1 || unit > 4) {
    dma_write((WORD)(addr + DD_RESULT), 0xa1);
    return;
  }

  if (!disk_image_provider_is_mounted((uint8_t)unit)) {
    dma_write((WORD)(addr + DD_RESULT), 0xa1);
    return;
  }

  if (track >= TRK8) {
    dma_write((WORD)(addr + DD_RESULT), 0xc5);
    return;
  }
  if (sector < 1 || sector > SPT8) {
    dma_write((WORD)(addr + DD_RESULT), 0xc6);
    return;
  }

  pos = (uint32_t)(track * SPT8 + (sector - 1)) * SEC_SZ;
  if ((pos + SEC_SZ) > disk_image_provider_size_unit((uint8_t)unit)) {
    dma_write((WORD)(addr + DD_RESULT), 0xa1);
    return;
  }

  switch (cmd) {
  case READ_SEC:
    {
      uint8_t sector_buf[SEC_SZ];
      int ov = overlay_find((uint8_t)unit, pos);

      if (ov >= 0) {
        memcpy(sector_buf, overlay[ov].data, sizeof(sector_buf));
      } else if (!disk_image_provider_read_unit((uint8_t)unit, pos, sector_buf, sizeof(sector_buf))) {
        dma_write((WORD)(addr + DD_RESULT), 0x93);
        break;
      }

      for (i = 0; i < SEC_SZ; i++) {
        dma_write((WORD)(dma_addr + i), sector_buf[i]);
      }
    }
    dma_write((WORD)(addr + DD_RESULT), 1);
    break;

  case VERIFY_DATA:
    dma_write((WORD)(addr + DD_RESULT), 1);
    break;

  case WRITE_SEC:
    {
      int ov = overlay_find((uint8_t)unit, pos);
      if (ov < 0) {
        ov = overlay_alloc((uint8_t)unit, pos);
      }
      for (i = 0; i < SEC_SZ; i++) {
        overlay[ov].data[i] = (uint8_t)dma_read((WORD)(dma_addr + i));
      }
      dma_write((WORD)(addr + DD_RESULT), 1);
    }
    break;

  case FMT_TRACK:
    /* Pretend success in minimal backend; no persistent format support. */
    dma_write((WORD)(addr + DD_RESULT), 1);
    break;

  default:
    dma_write((WORD)(addr + DD_RESULT), 0xc4);
    break;
  }
}

BYTE imsai_fif_minimal_in(void)
{
  return 0;
}

void imsai_fif_minimal_out(BYTE data)
{
  static int descno = 0;

  switch (fdstate) {
  case 0:
    switch (data & 0xf0) {
    case 0x00:
      descno = data & 0x0f;
      disk_io(fdaddr[descno]);
      break;

    case 0x10:
      descno = data & 0x0f;
      fdstate = 1;
      break;

    case 0x50:
      /* Optional ROM boot helper command: load first sector to 0x0000. */
#if IMSAI_BOOT_MODE == IMSAI_BOOT_AUTOBOOT_IMDOS
      if (disk_image_provider_size_unit(boot_unit) >= SEC_SZ) {
        uint8_t sector_buf[SEC_SZ];
        int i;

        if (!disk_image_provider_read_unit(boot_unit, 0, sector_buf, sizeof(sector_buf))) {
          break;
        }
        for (i = 0; i < SEC_SZ; i++) {
          dma_write((WORD)i, sector_buf[i]);
        }
      }
#endif
      break;

    default:
      /* 0x20,0x30,0x40 and others: ignored in minimal backend. */
      break;
    }
    break;

  case 1:
    fdaddr[descno] = data;
    fdstate = 2;
    break;

  case 2:
    fdaddr[descno] |= (uint16_t)data << 8;
    fdstate = 0;
    break;

  default:
    fdstate = 0;
    break;
  }
}

void imsai_fif_minimal_reset(void)
{
  int i;

  disk_image_provider_init();
  boot_unit = 1;
  fdstate = 0;
  for (i = 0; i < 16; i++) {
    fdaddr[i] = (uint16_t)(i << 12);
  }

  if (disk_image_provider_size_unit(1) < FLOPPY_BYTES) {
    /* Keep defaults even with bad image; descriptor operations will error out. */
    (void)memset(fdaddr, 0, sizeof(fdaddr));
  }

  (void)memset(overlay, 0, sizeof(overlay));
  overlay_next = 0;
}

void imsai_fif_minimal_set_boot_unit(uint8_t unit)
{
  if (unit >= 1 && unit <= 4) {
    boot_unit = unit;
  }
}

uint8_t imsai_fif_minimal_get_boot_unit(void)
{
  return boot_unit;
}
