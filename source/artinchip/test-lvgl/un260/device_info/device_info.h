#ifndef DEVICE_INFO_H
#define DEVICE_INFO_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t main_app[3];
    uint8_t image_app[3];
    uint8_t fpga[2];
    uint8_t main_boot[3];
    uint8_t image_boot[3];
} device_info_remote_versions_t;

typedef enum {
    DEVICE_REPLY_INVALID = 0,
    DEVICE_REPLY_VERSION_UPDATED,
    DEVICE_REPLY_MAIN_UPGRADE_STATUS,
    DEVICE_REPLY_IMAGE_UPGRADE_STATUS,
} device_reply_kind_t;

typedef struct {
    device_reply_kind_t kind;
    uint8_t status;
} device_reply_result_t;

void device_info_init(const char *display_app);
void device_info_confirm_remote_versions(const device_info_remote_versions_t *versions);
bool device_info_is_valid(void);
const char *device_info_display_app(void);
const char *device_info_main_app(void);
const char *device_info_image_app(void);
const char *device_info_fpga(void);
const char *device_info_main_boot(void);
const char *device_info_image_boot(void);

device_reply_result_t device_reply_dispatch(uint8_t cmd, const uint8_t *buf, uint8_t len);

#endif
