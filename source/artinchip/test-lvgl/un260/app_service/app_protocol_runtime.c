#include "app_protocol_runtime.h"

#include "un260/lv_system/app_clock.h"

#include "un260/data_collection/data_collection.h"
#include "un260/device_info/device_info.h"
#include "un260/diagnostic/diagnostic.h"
#include "un260/lv_core/page_06_settings.h"
#include "un260/lv_core/page_09_cis_cala.h"
#include "un260/lv_core/page_14_main_upgrade.h"
#include "un260/lv_core/page_15_image_upgrade.h"
#include "un260/lv_core/page_28_get_image.h"
#include "un260/lv_core/page_31_get_wave.h"
#include "un260/lv_drivers/lv_drivers.h"
#include "un260/protocol/auxiliary_reply.h"
#include "un260/protocol/startup_sync_reply.h"
#include "un260/protocol/stream_data_reply.h"

static void handle_device_reply(uint8_t cmd, const uint8_t *buf, uint8_t len)
{
    device_reply_result_t reply = device_reply_dispatch(cmd, buf, len);

    if (reply.kind == DEVICE_REPLY_VERSION_UPDATED) {
        uart_debug_printf("Version Info Received\n");
    } else if (reply.kind == DEVICE_REPLY_MAIN_UPGRADE_STATUS) {
        ui_page_14_main_upgrade_on_reply(0xA1, reply.status);
        uart_debug_printf("0xA1 res=0x%02X\n", reply.status);
    } else if (reply.kind == DEVICE_REPLY_IMAGE_UPGRADE_STATUS) {
        ui_page_15_image_upgrade_on_reply(0xB0, reply.status);
        uart_debug_printf("0xB0 res=0x%02X\n", reply.status);
    }
}

static void on_calibration_changed(void)
{
    cis_calib_ui_refresh();
}

static const diagnostic_reply_hooks_t g_diagnostic_reply_hooks = {
    .on_calibration_changed = on_calibration_changed,
};

static void handle_auxiliary_reply(uint8_t cmd, const uint8_t *buf, uint8_t len)
{
    auxiliary_reply_result_t reply = auxiliary_reply_dispatch(cmd, buf, len);

    switch (reply.kind) {
    case AUXILIARY_REPLY_DISPLAY_MAIN:
        uart_debug_printf("0x40 switch to main SUCCESS\n");
        break;
    case AUXILIARY_REPLY_DISPLAY_DETAIL:
        uart_debug_printf("0x40 switch to detail SUCCESS\n");
        break;
    case AUXILIARY_REPLY_DISPLAY_UNKNOWN:
        uart_debug_printf("0x40 unknown result=0x%02X\n", reply.value);
        break;
    case AUXILIARY_REPLY_PRINT_DETAIL:
        uart_debug_printf("0x3C print detail frame\n");
        break;
    case AUXILIARY_REPLY_PRINT_DONE:
        uart_debug_printf("0x3C print done\n");
        break;
    case AUXILIARY_REPLY_PRINT_UNKNOWN:
        uart_debug_printf("0x3C unknown len=%d\n", reply.frame_len);
        break;
    case AUXILIARY_REPLY_CLEAR_DATA_ACK:
        uart_debug_printf("0x3B clear data ack: res=0x%02X\n", reply.value);
        break;
    case AUXILIARY_REPLY_INVALID:
    default:
        uart_debug_printf("0x%02X invalid len=%d\n", cmd, len);
        break;
    }
}

static void handle_stream_data_reply(uint8_t cmd,
                                     const uint8_t *buf,
                                     uint8_t len)
{
    stream_data_reply_view_t reply = stream_data_reply_parse(buf, len);

    if (reply.kind == STREAM_DATA_REPLY_IMAGE) {
        ui_page_28_get_image_on_frame(reply.payload, reply.payload_len);
    } else if (reply.kind == STREAM_DATA_REPLY_WAVE) {
        ui_page_31_get_wave_on_frame(reply.payload, reply.payload_len);
    } else {
        uart_debug_printf("0x%02X invalid len=%d\n", cmd, len);
    }
}

bool app_protocol_runtime_handle_reply(uint8_t cmd,
                                       const uint8_t *buf,
                                       uint8_t len)
{
    switch (cmd) {
    case 0x17:
    case 0xA1:
    case 0xB0:
        handle_device_reply(cmd, buf, len);
        return true;

    case 0x1D:
    case 0x5B:
    case 0x5F:
        diagnostic_reply_dispatch(cmd, buf, len, app_clock_uptime_ms(),
                                  &g_diagnostic_reply_hooks);
        return true;

    case 0x40:
    case 0x3C:
    case 0x3B:
        handle_auxiliary_reply(cmd, buf, len);
        return true;

    case 0x47:
    case 0x48:
        handle_stream_data_reply(cmd, buf, len);
        return true;

    case 0xC0:
    {
        data_collection_reply_result_t result =
            data_collection_reply_handle(buf, len, app_clock_uptime_ms());
        if (result != DATA_COLLECTION_REPLY_INVALID &&
            result != DATA_COLLECTION_REPLY_IGNORED) {
            page_06_data_collection_on_reply(result);
        }
        return true;
    }

    case 0x58:
    case 0x56:
        startup_sync_reply_dispatch(cmd, buf, len);
        return true;

    default:
        return false;
    }
}
