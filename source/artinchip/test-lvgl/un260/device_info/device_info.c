#include "device_info.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    char main_app[32];
    char image_app[32];
    char fpga[32];
    char main_boot[32];
    char image_boot[32];
    char display_app[32];
    bool version_valid;
} device_info_t;

static device_info_t g_device_info = { 0 };

void device_info_init(const char *display_app)
{
    memset(&g_device_info, 0, sizeof(g_device_info));
    if (display_app != NULL) {
        strncpy(g_device_info.display_app, display_app, sizeof(g_device_info.display_app) - 1);
        g_device_info.display_app[sizeof(g_device_info.display_app) - 1] = '\0';
    }
}

void device_info_confirm_remote_versions(const device_info_remote_versions_t *versions)
{
    if (versions == NULL) {
        return;
    }

    snprintf(g_device_info.main_app, sizeof(g_device_info.main_app), "%d.%d.%d", versions->main_app[0], versions->main_app[1], versions->main_app[2]);
    snprintf(g_device_info.image_app, sizeof(g_device_info.image_app), "%d.%d.%d", versions->image_app[0], versions->image_app[1], versions->image_app[2]);
    snprintf(g_device_info.fpga, sizeof(g_device_info.fpga), "%d.%d", versions->fpga[0], versions->fpga[1]);
    snprintf(g_device_info.main_boot, sizeof(g_device_info.main_boot), "%d.%d.%d", versions->main_boot[0], versions->main_boot[1], versions->main_boot[2]);
    snprintf(g_device_info.image_boot, sizeof(g_device_info.image_boot), "%d.%d.%d", versions->image_boot[0], versions->image_boot[1], versions->image_boot[2]);
    g_device_info.version_valid = true;
}

bool device_info_is_valid(void)
{
    return g_device_info.version_valid;
}

const char *device_info_display_app(void)
{
    return g_device_info.display_app;
}

const char *device_info_main_app(void)
{
    return g_device_info.main_app;
}

const char *device_info_image_app(void)
{
    return g_device_info.image_app;
}

const char *device_info_fpga(void)
{
    return g_device_info.fpga;
}

const char *device_info_main_boot(void)
{
    return g_device_info.main_boot;
}

const char *device_info_image_boot(void)
{
    return g_device_info.image_boot;
}

static device_reply_result_t device_reply_version_handle(const uint8_t *buf, uint8_t len)
{
    device_reply_result_t reply = { .kind = DEVICE_REPLY_INVALID };
    const uint8_t *payload;
    device_info_remote_versions_t versions;

    /* 4 字节帧头/命令 + 14 字节版本载荷 + 1 字节 CRC。 */
    if (buf == NULL || len < 19) return reply;

    payload = &buf[4];
    versions = (device_info_remote_versions_t){
        .main_app = {payload[0], payload[1], payload[2]},
        .image_app = {payload[3], payload[4], payload[5]},
        .fpga = {payload[6], payload[7]},
        .main_boot = {payload[8], payload[9], payload[10]},
        .image_boot = {payload[11], payload[12], payload[13]},
    };
    device_info_confirm_remote_versions(&versions);
    reply.kind = DEVICE_REPLY_VERSION_UPDATED;
    return reply;
}

static device_reply_result_t device_reply_upgrade_handle(uint8_t cmd, const uint8_t *buf, uint8_t len)
{
    device_reply_result_t reply = { .kind = DEVICE_REPLY_INVALID };

    if (buf == NULL || len < 6) return reply;
    reply.status = buf[4];
    reply.kind = cmd == 0xA1 ? DEVICE_REPLY_MAIN_UPGRADE_STATUS
                             : DEVICE_REPLY_IMAGE_UPGRADE_STATUS;
    return reply;
}

device_reply_result_t device_reply_dispatch(uint8_t cmd, const uint8_t *buf, uint8_t len)
{
    switch (cmd) {
    case 0x17:
        return device_reply_version_handle(buf, len);
    case 0xA1:
    case 0xB0:
        return device_reply_upgrade_handle(cmd, buf, len);
    default:
        return (device_reply_result_t){ .kind = DEVICE_REPLY_INVALID };
    }
}
