#ifndef DISK_IMAGE_PROVIDER_H
#define DISK_IMAGE_PROVIDER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void disk_image_provider_init(void);
void disk_image_provider_reset(void);
bool disk_image_provider_mount_named_to_unit(uint8_t unit, const char *name);
bool disk_image_provider_is_bootable_unit(uint8_t unit);
bool disk_image_provider_is_mounted(uint8_t unit);
bool disk_image_provider_read_unit(uint8_t unit, uint32_t offset, uint8_t *dst, uint32_t len);
uint32_t disk_image_provider_size_unit(uint8_t unit);
const char *disk_image_provider_unit_name(uint8_t unit);
const char *disk_image_provider_last_error(void);

#ifdef __cplusplus
}
#endif

#endif
