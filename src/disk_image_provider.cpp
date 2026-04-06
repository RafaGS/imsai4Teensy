#include "disk_image_provider.h"

#include <Arduino.h>
#include <SD.h>
#include <string.h>

extern const unsigned char g_drive_a_imdos202_dsk[];
extern const unsigned int g_drive_a_imdos202_dsk_len;

namespace {
constexpr uint32_t kExpectedFloppyBytes = 256256;
constexpr uint32_t kBootSectorSize = 128;
constexpr uint8_t kSdChipSelect = BUILTIN_SDCARD;
constexpr uint8_t kMaxUnits = 4; /* A..D, using unit numbers 1..4 */

enum class SourceKind {
  Embedded,
  SdFile,
};

typedef struct {
  SourceKind source_kind;
  bool mounted;
  File file;
  char name[48];
} unit_slot_t;

bool sd_ready = false;
unit_slot_t units[kMaxUnits];
char last_error[48] = "using embedded image";

unit_slot_t *unit_slot(uint8_t unit)
{
  if (unit < 1 || unit > kMaxUnits) {
    return nullptr;
  }
  return &units[unit - 1];
}

bool read_from_sd(unit_slot_t *slot, uint32_t offset, uint8_t *dst, uint32_t len)
{
  size_t bytes_read;

  if (!slot || !slot->file) {
    return false;
  }
  if (!slot->file.seek(offset)) {
    return false;
  }
  bytes_read = slot->file.read(dst, len);
  return bytes_read == (size_t)len;
}

bool read_from_embedded(uint8_t unit, uint32_t offset, uint8_t *dst, uint32_t len)
{
  if (unit != 1) {
    return false;
  }
  if ((offset + len) > g_drive_a_imdos202_dsk_len) {
    return false;
  }
  memcpy(dst, g_drive_a_imdos202_dsk + offset, len);
  return true;
}

bool unit_read(uint8_t unit, uint32_t offset, uint8_t *dst, uint32_t len)
{
  unit_slot_t *slot = unit_slot(unit);

  if (!slot || !slot->mounted) {
    return false;
  }
  if (slot->source_kind == SourceKind::SdFile) {
    return read_from_sd(slot, offset, dst, len);
  }
  return read_from_embedded(unit, offset, dst, len);
}

bool looks_bootable(uint8_t unit)
{
  uint8_t sector[kBootSectorSize];
  uint32_t i;
  bool all_zero = true;
  bool all_e5 = true;
  bool has_boot_opcode = false;

  if (!unit_read(unit, 0, sector, sizeof(sector))) {
    return false;
  }

  for (i = 0; i < sizeof(sector); i++) {
    if (sector[i] != 0x00) {
      all_zero = false;
    }
    if (sector[i] != 0xE5) {
      all_e5 = false;
    }
    if (i < 16 && (sector[i] == 0xC3 || sector[i] == 0xCD || sector[i] == 0x31 ||
                   sector[i] == 0x21 || sector[i] == 0xF3 || sector[i] == 0x01)) {
      has_boot_opcode = true;
    }
  }

  return !all_zero && !all_e5 && has_boot_opcode;
}

bool try_open_candidate(uint8_t unit, const char *path, const char *display_name)
{
  unit_slot_t *slot = unit_slot(unit);
  File f;

  if (!slot) {
    return false;
  }

  f = SD.open(path, FILE_READ);
  if (!f) {
    return false;
  }
  if ((uint32_t)f.size() != kExpectedFloppyBytes) {
    f.close();
    strncpy(last_error, "invalid dsk size", sizeof(last_error) - 1);
    last_error[sizeof(last_error) - 1] = '\0';
    return false;
  }

  if (slot->file) {
    slot->file.close();
  }
  slot->file = f;
  slot->source_kind = SourceKind::SdFile;
  slot->mounted = true;
  strncpy(slot->name, display_name, sizeof(slot->name) - 1);
  slot->name[sizeof(slot->name) - 1] = '\0';
  strncpy(last_error, "ok", sizeof(last_error) - 1);
  last_error[sizeof(last_error) - 1] = '\0';
  return true;
}
}

extern "C" {

void disk_image_provider_init(void)
{
  if (!sd_ready) {
    sd_ready = SD.begin(kSdChipSelect);
    if (!sd_ready) {
      strncpy(last_error, "microSD init failed", sizeof(last_error) - 1);
      last_error[sizeof(last_error) - 1] = '\0';
    }
  }
  disk_image_provider_reset();
}

void disk_image_provider_reset(void)
{
  uint8_t i;

  for (i = 0; i < kMaxUnits; i++) {
    if (units[i].file) {
      units[i].file.close();
    }
    units[i].source_kind = SourceKind::SdFile;
    units[i].mounted = false;
    units[i].name[0] = '\0';
  }

  units[0].source_kind = SourceKind::Embedded;
  units[0].mounted = true;
  strncpy(units[0].name, "imdos202.dsk", sizeof(units[0].name) - 1);
  units[0].name[sizeof(units[0].name) - 1] = '\0';

  if (sd_ready) {
    strncpy(last_error, "using embedded image", sizeof(last_error) - 1);
    last_error[sizeof(last_error) - 1] = '\0';
  }
}

bool disk_image_provider_mount_named_to_unit(uint8_t unit, const char *name)
{
  char path[64];
  unit_slot_t *slot;

  slot = unit_slot(unit);
  if (!slot) {
    strncpy(last_error, "invalid unit", sizeof(last_error) - 1);
    last_error[sizeof(last_error) - 1] = '\0';
    return false;
  }

  if (name == nullptr || name[0] == '\0') {
    strncpy(last_error, "empty filename", sizeof(last_error) - 1);
    last_error[sizeof(last_error) - 1] = '\0';
    return false;
  }

  if (!sd_ready) {
    strncpy(last_error, "microSD not ready", sizeof(last_error) - 1);
    last_error[sizeof(last_error) - 1] = '\0';
    return false;
  }

  snprintf(path, sizeof(path), "/imsai/%s", name);
  if (try_open_candidate(unit, path, name)) {
    return true;
  }

  strncpy(last_error, "file not found (put DSKs in /imsai/)", sizeof(last_error) - 1);
  last_error[sizeof(last_error) - 1] = '\0';
  return false;
}

bool disk_image_provider_is_bootable_unit(uint8_t unit)
{
  if (disk_image_provider_size_unit(unit) != kExpectedFloppyBytes) {
    strncpy(last_error, "invalid dsk size", sizeof(last_error) - 1);
    last_error[sizeof(last_error) - 1] = '\0';
    return false;
  }
  if (!looks_bootable(unit)) {
    strncpy(last_error, "disk not bootable", sizeof(last_error) - 1);
    last_error[sizeof(last_error) - 1] = '\0';
    return false;
  }
  return true;
}

bool disk_image_provider_is_mounted(uint8_t unit)
{
  unit_slot_t *slot = unit_slot(unit);
  if (!slot) {
    return false;
  }
  return slot->mounted;
}

bool disk_image_provider_read_unit(uint8_t unit, uint32_t offset, uint8_t *dst, uint32_t len)
{
  return unit_read(unit, offset, dst, len);
}

uint32_t disk_image_provider_size_unit(uint8_t unit)
{
  unit_slot_t *slot = unit_slot(unit);
  if (!slot || !slot->mounted) {
    return 0;
  }

  if (slot->source_kind == SourceKind::SdFile && slot->file) {
    return (uint32_t)slot->file.size();
  }
  if (unit == 1) {
    return g_drive_a_imdos202_dsk_len;
  }
  return 0;
}

const char *disk_image_provider_last_error(void)
{
  return last_error;
}

const char *disk_image_provider_unit_name(uint8_t unit)
{
  unit_slot_t *slot = unit_slot(unit);
  if (!slot || !slot->mounted) {
    return "(none)";
  }
  return slot->name;
}

}
