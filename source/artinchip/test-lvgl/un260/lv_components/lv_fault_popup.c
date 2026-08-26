#include "lv_fault_popup.h"
#include "un260/lv_components/lv_components.h"
#include "un260/protocol/protocol_send.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_components/smart_island.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/lv_system/machine_time.h"
#include "un260/device_info/device_info.h"
#include "un260/machine_state/machine_state.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifndef MACHINE_MODEL_NAME
#define MACHINE_MODEL_NAME "UN260"
#endif
#define FAULT_PROTO_BOOT    0x37
#define FAULT_PROTO_START   0x0a
#define FAULT_PROTO_RUNTIME 0x0f
#define FAULT_AUTO_CONFIRM_DELAY_MS 2000
#define FAULT_AUTO_RETRY_INTERVAL_MS 2000
#define FAULT_AUTO_RETRY_MAX         3

#define FAULT_POPUP_BG_PATH   "L:/usr/local/share/lvgl_data/fault_popup_bg.png"
#define FAULT_MACHINE_IMG_FMT "L:/usr/local/share/lvgl_data/%02Xmachine.png"
#define FAULT_ERR_IMG_FMT     "L:/usr/local/share/lvgl_data/%02X_err.png"

static lv_obj_t* g_fault_popup = NULL;
static lv_obj_t* g_fault_machine_img = NULL;
static lv_obj_t* g_fault_err_img = NULL;
static lv_obj_t* g_fault_radar_1 = NULL;
static lv_obj_t* g_fault_radar_2 = NULL;
static lv_timer_t* g_fault_anim_timer = NULL;
static uint16_t g_fault_anim_tick = 0;

static lv_obj_t* g_fault_title_1 = NULL;
static lv_obj_t* g_fault_title_2 = NULL;
static lv_obj_t* g_fault_time_label = NULL;
static lv_obj_t* g_fault_model_label = NULL;
static lv_obj_t* g_fault_main_desc_label = NULL;
static lv_obj_t* g_fault_reason_label = NULL;
static lv_obj_t* g_fault_solution_label = NULL;
static lv_obj_t* g_fault_version_label = NULL;
static lv_obj_t* g_solution_title = NULL;
static lv_obj_t* g_reason_title = NULL;

void show_start_fault_popup(uint8_t type, uint8_t code);
void show_runtime_fault_popup(uint8_t code);

static fault_popup_data_t g_fault_popup_data;
typedef enum {
    FAULT_PENDING_NONE = 0,
    FAULT_PENDING_START_FAULT,
    FAULT_PENDING_RUNTIME_FAULT
} fault_pending_type_t;

typedef struct {
    bool valid;
    fault_pending_type_t type;
    uint8_t fault_type;
    uint8_t code;
} fault_pending_t;

static bool g_fault_popup_auto_enabled = true;
static fault_pending_t g_fault_pending = { false, FAULT_PENDING_NONE, 0, 0 };
static lv_timer_t* g_fault_auto_confirm_timer = NULL;
static uint8_t g_fault_auto_retry_count = 0;
static uint8_t g_fault_auto_retry_type = 0;
static uint8_t g_fault_auto_retry_code = 0;
static uint32_t g_fault_auto_retry_last_tick = 0;

static bool fault_popup_should_auto_clear(uint8_t type, uint8_t code)
{
    /* type=0x01 code=0x00: no banknotes, 不需要发 0x3D */
    if (type == 0x01 && code == 0x00) {
        return false;
    }
    return true;
}

static void fault_popup_track_retry_key(uint8_t type, uint8_t code)
{
    if (g_fault_auto_retry_type == type && g_fault_auto_retry_code == code) {
        return;
    }

    g_fault_auto_retry_type = type;
    g_fault_auto_retry_code = code;
    g_fault_auto_retry_count = 0;
    g_fault_auto_retry_last_tick = 0;
}

/* =========================
 * 原因 / 方案映射
 * ========================= */
static const char* g_boot_reason_desc[0x100] = {
    [0x01] = "Sensor self-test returned abnormal result.",
    [0x02] = "Motor self-test returned abnormal result.",
    [0x03] = "Electromagnet self-test returned abnormal result.",
    [0x04] = "Configuration readback abnormal.",
    [0x05] = "Image board self-test abnormal.",
};

static const char* g_boot_solution_desc[0x100] = {
    [0x01] = "Press CONFIRM and enter the voltage page to check sensor voltage.",
    [0x02] = "Press CONFIRM and enter the voltage page to check motor voltage.",
    [0x03] = "Press CONFIRM and enter the voltage page to check electromagnet voltage.",
    [0x04] = "Check whether the mainboard cable is loose and whether the LED is on.",
    [0x05] = "Check whether the image board cable is loose and whether the LED is on.",
};

static const char* g_start_reason_desc[0x100] = {
    [0x01] = "Upper channel sensors are blocked or abnormal.",
    [0x02] = "Lower channel sensor is blocked or abnormal.",
    [0x03] = "Reject exit path is blocked, sensor detection abnormal.",
    [0x04] = "Reject pocket sensor is blocked and reject count is zero.",
    [0x05] = "Reject pocket is full and sensors are blocked.",
    [0x06] = "Stacker pocket sensor is blocked and stacker count is zero.",
    [0x07] = "Stacker pocket is full and sensors are blocked.",
    [0x08] = "Stacker and reject pockets are both full, sensors are blocked.",
    [0x09] = "Upper and lower channels are not properly closed.",
    [0x0A] = "Banknote exit sensor is blocked or abnormal.",
    [0x0B] = "Dust cover or baffle is not properly closed.",
    [0x0C] = "Flap position abnormal, switch detection abnormal.",
    [0x0D] = "Encoder disk detection abnormal, sensor signal abnormal.",
};

static const char* g_start_solution_desc[0x100] = {
    [0x01] = "Open the upper channel and check for foreign objects or banknotes blocking the sensors.",
    [0x02] = "Open the lower channel and check for foreign objects or banknotes blocking the sensors.",
    [0x03] = "Check the reject exit for foreign objects or banknotes blocking the sensors.",
    [0x04] = "Check the reject pocket for foreign objects or banknotes blocking the sensors.",
    [0x05] = "Reject pocket is full. Remove the banknotes from the reject pocket.",
    [0x06] = "Check the stacker pocket for foreign objects or banknotes blocking the sensors.",
    [0x07] = "Stacker pocket is full. Remove the banknotes from the stacker pocket.",
    [0x08] = "Stacker and reject pockets are full. Remove the banknotes.",
    [0x09] = "Close the upper and lower channels properly.",
    [0x0A] = "Remove the banknotes from the stacker pocket.",
    [0x0B] = "Check the sensors of the dust cover and baffle.",
    [0x0C] = "Remove the machine cover and check the flap sensor wiring and flap condition.",
    [0x0D] = "Remove the machine cover and check the encoder wiring and encoder condition.",
};

static const char* g_runtime_reason_desc[0x100] = {
    [0x00] = "No machine fault.",
    [0x01] = "Banknote jam detected at the feeder.",
    [0x02] = "Banknote jam detected in the upper channel.",
    [0x03] = "Banknote jam detected in the lower channel.",
    [0x04] = "Banknote jam detected at the reject exit.",
    [0x05] = "Banknote jam detected at the genuine note exit.",
    [0x06] = "Diverter electromagnet flap error detected.",
    [0x07] = "Residual banknotes detected in the stacker area.",
};

static const char* g_runtime_solution_desc[0x100] = {
    [0x01] = "Stop counting and check the feeder for jammed banknotes or foreign objects.",
    [0x02] = "Stop counting, remove the banknotes, open the upper channel and check for jams or foreign objects.",
    [0x03] = "Stop counting, remove the banknotes, open the lower channel and check for jams or foreign objects.",
    [0x04] = "Stop counting, remove the banknotes from the reject pocket and check for jams or foreign objects.",
    [0x05] = "Stop counting, remove the banknotes from the banknote exit and check for jams or foreign objects.",
    [0x06] = "Stop counting, remove the banknotes, open the machine cover and check the electromagnet wiring and condition.",
    [0x07] = "Stop counting and remove the banknotes from the stacker pocket.",
};

/* =========================
 * 工具函数
 * ========================= */
static void get_time_str(char* buf, size_t size)
{
    machine_time_value_t device_time;
    time_t now = time(NULL);
    struct tm tm_now;
    struct tm* p = localtime_r(&now, &tm_now);
    if (p == NULL) {
        snprintf(buf, size, "--:--:--");
        return;
    }

    /*
     * Prefer confirmed device time when available.
     * Keep current localtime defaults if fields are out of range.
     */
    machine_time_get(&device_time);
    if (device_time.year > 0) {
        tm_now.tm_year = (device_time.year >= 1900) ? (device_time.year - 1900) : device_time.year;
    }
    if (device_time.month <= 11) {
        tm_now.tm_mon = device_time.month;
    }
    if (device_time.day >= 1 && device_time.day <= 31) {
        tm_now.tm_mday = device_time.day;
    }
    if (device_time.hour <= 23) {
        tm_now.tm_hour = device_time.hour;
    }
    if (device_time.minute <= 59) {
        tm_now.tm_min = device_time.minute;
    }
    if (device_time.second <= 59) {
        tm_now.tm_sec = device_time.second;
    }

    snprintf(buf, size, "%04d-%02d-%02d %02d:%02d:%02d",
             tm_now.tm_year + 1900,
             tm_now.tm_mon + 1,
             tm_now.tm_mday,
             tm_now.tm_hour,
             tm_now.tm_min,
             tm_now.tm_sec);
}

static uint8_t get_fault_proto_code(fault_source_t source)
{
    switch (source) {
    case FAULT_SRC_BOOT:
        return FAULT_PROTO_BOOT;
    case FAULT_SRC_START_COUNT:
        return FAULT_PROTO_START;
    case FAULT_SRC_RUNTIME:
        return FAULT_PROTO_RUNTIME;
    default:
        return 0x00;
    }
}

static const char* get_fault_machine_img(fault_source_t source, uint8_t code)
{
    static char path[128];
    uint8_t proto = get_fault_proto_code(source);

    snprintf(path, sizeof(path),
             "L:/usr/local/share/lvgl_data/%02x%02xmachine.png",
             proto, code);
    return path;
}

static const char* get_fault_err_img(fault_source_t source, uint8_t code)
{
    static char path[128];
    uint8_t proto = get_fault_proto_code(source);

    snprintf(path, sizeof(path),
             "L:/usr/local/share/lvgl_data/%02x%02x_err.png",
             proto, code);
    return path;
}

static void get_fault_radar_pos(fault_source_t source, uint8_t code, lv_coord_t* x, lv_coord_t* y)
{
    uint8_t cmd = get_fault_proto_code(source);

    *x = 110;
    *y = 118;

    switch (cmd) {
    case 0x37:  /* boot self-test */
        switch (code) {
        case 0x01: *x = 48; *y = 216; break;
        case 0x02: *x = 180; *y = 174; break;
        case 0x03: *x = 143; *y = 174; break;
        case 0x04: *x = 150; *y = 233; break;
        case 0x05: *x = 74; *y = 241; break;
        default: break;
        }
        break;

    case 0x0A:  /* start count */
        switch (code) {
        case 0x00: *x = 128; *y = 124; break; /* no note */
        case 0x01: *x = 137; *y = 164; break;
        case 0x02: *x = 134; *y = 234; break;
        case 0x03: *x = 128; *y = 205; break;
        case 0x04: *x = 128; *y = 205; break;
        case 0x05: *x = 128; *y = 205; break;
        case 0x06: *x = 128; *y = 249; break;
        case 0x07: *x = 128; *y = 249; break;
        case 0x08: *x = 128; *y = 223; break;
        case 0x09: *x = 134; *y = 201; break;
        case 0x0A: *x = 128; *y = 250; break;
        case 0x0B: *x = 128; *y = 250; break;
        case 0x0C: *x = 142; *y = 174; break;
        case 0x0D: *x = 100; *y = 177; break;
        default: break;
        }
        break;

    case 0x0f:  /* runtime */
        switch (code) {
        case 0x00: *x = 128; *y = 127; break;
        case 0x01: *x = 128; *y = 127; break;
        case 0x02: *x = 133; *y = 160; break;
        case 0x03: *x = 133; *y = 228; break;
        case 0x04: *x = 129; *y = 202; break;
        case 0x05: *x = 129; *y = 249; break;
        case 0x06: *x = 142; *y = 170; break;
        case 0x07: *x = 129; *y = 249; break;
        default: break;
        }
        break;

    default:
        break;
    }
}

static const char* get_boot_title(uint8_t code)
{
    LV_UNUSED(code);
    return ui_text_get(UI_TEXT_WIDGET_FAULT_SELFTEST_ERROR);
}

static const char* get_boot_main_desc(uint8_t code)
{
    switch (code) {
    case 0x01: return ui_text_get(UI_TEXT_WIDGET_FAULT_BOOT_SENSOR_FAILED);
    case 0x02: return ui_text_get(UI_TEXT_WIDGET_FAULT_BOOT_MOTOR_FAILED);
    case 0x03: return ui_text_get(UI_TEXT_WIDGET_FAULT_BOOT_MAGNET_FAILED);
    case 0x04: return ui_text_get(UI_TEXT_WIDGET_FAULT_BOOT_CONFIG_FAILED);
    case 0x05: return ui_text_get(UI_TEXT_WIDGET_FAULT_BOOT_IMAGE_FAILED);
    default:   return ui_text_get(UI_TEXT_WIDGET_FAULT_BOOT_GENERIC_FAILED);
    }
}

static const char* get_start_title(uint8_t code)
{
    if (code == 0x00) {
        return ui_text_get(UI_TEXT_WIDGET_FAULT_START_FAILED);
    }
    return ui_text_get(UI_TEXT_WIDGET_FAULT_START_COUNT_ERROR);
}

static const char* get_start_main_desc(uint8_t code)
{
    const char *description;

    if (code == 0x00) {
        return ui_text_get(UI_TEXT_WIDGET_FAULT_NO_NOTE_MAIN);
    }

    description = machine_start_error_desc(code);
    if (description != NULL) {
        return description;
    }
    return ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_COUNT_ERROR);
}

static const char* get_runtime_title(uint8_t code)
{
    (void)code;
    return ui_text_get(UI_TEXT_WIDGET_FAULT_RUNTIME_ERROR_SENSOR);
}

static const char* get_runtime_main_desc(uint8_t code)
{
    const char* desc = get_system_error_desc(code);
    return desc ? desc : ui_text_get(UI_TEXT_WIDGET_FAULT_RUNTIME_UNKNOWN);
}

static const char* get_fault_reason_text(fault_source_t source, uint8_t code)
{
    switch (source) {
    case FAULT_SRC_BOOT:
        if (g_boot_reason_desc[code]) return g_boot_reason_desc[code];
        break;
    case FAULT_SRC_START_COUNT:
        if (code == 0x00) return ui_text_get(UI_TEXT_WIDGET_FAULT_NO_NOTE_REASON);
        if (g_start_reason_desc[code]) return g_start_reason_desc[code];
        break;
    case FAULT_SRC_RUNTIME:
        if (g_runtime_reason_desc[code]) return g_runtime_reason_desc[code];
        break;
    default:
        break;
    }

    return ui_text_get(UI_TEXT_WIDGET_FAULT_REASON_FALLBACK);
}

static const char* get_fault_solution_text(fault_source_t source, uint8_t code)
{
    switch (source) {
    case FAULT_SRC_BOOT:
        if (g_boot_solution_desc[code]) return g_boot_solution_desc[code];
        break;
    case FAULT_SRC_START_COUNT:
        if (code == 0x00) return ui_text_get(UI_TEXT_WIDGET_FAULT_NO_NOTE_SOLUTION);
        if (g_start_solution_desc[code]) return g_start_solution_desc[code];
        break;
    case FAULT_SRC_RUNTIME:
        if (g_runtime_solution_desc[code]) return g_runtime_solution_desc[code];
        break;
    default:
        break;
    }

    return ui_text_get(UI_TEXT_WIDGET_FAULT_SOLUTION_FALLBACK);
}

static fault_confirm_action_t get_confirm_action_by_page(void)
{
    if (ui_manager_get_current_page() == UI_PAGE_BOOT) {
        return FAULT_CONFIRM_GOTO_SENSOR;
    }
    return FAULT_CONFIRM_CLOSE;
}

/* =========================
 * 雷达动画
 * ========================= */
static void fault_anim_timer_cb(lv_timer_t* timer)
{
    (void)timer;

    if (g_fault_radar_1 == NULL || g_fault_radar_2 == NULL) return;

    g_fault_anim_tick++;

    uint16_t phase1 = g_fault_anim_tick % 80;
    uint16_t phase2 = (g_fault_anim_tick + 40) % 80;

    lv_obj_t* ripples[2] = { g_fault_radar_1, g_fault_radar_2 };
    uint16_t phases[2] = { phase1, phase2 };

    for (int i = 0; i < 2; i++) {
        uint16_t p = phases[i];

        if (p < 60) {
            lv_coord_t size = 4 + (p * 78) / 60;
            lv_opa_t opa = 220 - (p * 200) / 60;

            lv_obj_clear_flag(ripples[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_size(ripples[i], size, size);
            lv_obj_set_pos(ripples[i],
                           g_fault_popup_data.radar_x - size / 2,
                           g_fault_popup_data.radar_y - size / 2);
            lv_obj_set_style_border_opa(ripples[i], opa, 0);
        } else {
            lv_obj_add_flag(ripples[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void fault_popup_confirm_cb(lv_event_t* e)
{
    (void)e;
    uint8_t clear_cmd = 0x01;
    protocol_send(0x3D, &clear_cmd, 1);

    if (g_fault_popup_data.confirm_action == FAULT_CONFIRM_GOTO_SENSOR) {
        hide_fault_popup();
        /*
         * Boot fault flow should not keep BOOT in back stack.
         * Enter sensor page directly, ESC will then fallback to MAIN when stack is empty.
         */
        ui_manager_switch(UI_PAGE_SENSOR);
    } else {
        hide_fault_popup();
        /*
         * Non-boot pages should stay on the current page after confirm.
         * MAIN/PURE both host smart-island and must restore to idle after confirm.
         */
        if (ui_manager_get_current_page() == UI_PAGE_MAIN ||
            ui_manager_get_current_page() == UI_PAGE_PURE) {
            smart_island_restore_idle();
        }
    }
}

bool fault_popup_is_showing(void)
{
    return g_fault_popup != NULL;
}

void fault_popup_set_auto_enabled(bool enabled)
{
    g_fault_popup_auto_enabled = enabled;
    if (enabled && g_fault_auto_confirm_timer) {
        lv_timer_del(g_fault_auto_confirm_timer);
        g_fault_auto_confirm_timer = NULL;
    }
}

bool fault_popup_get_auto_enabled(void)
{
    return g_fault_popup_auto_enabled;
}

void fault_popup_clear_pending(void)
{
    g_fault_pending.valid = false;
    g_fault_pending.type = FAULT_PENDING_NONE;
    g_fault_pending.fault_type = 0;
    g_fault_pending.code = 0;
    if (g_fault_auto_confirm_timer) {
        lv_timer_del(g_fault_auto_confirm_timer);
        g_fault_auto_confirm_timer = NULL;
    }
}

void hide_fault_popup(void)
{
    if (g_fault_anim_timer) {
        lv_timer_del(g_fault_anim_timer);
        g_fault_anim_timer = NULL;
    }

    if (g_fault_popup) {
        lv_obj_del(g_fault_popup);
        g_fault_popup = NULL;
    }

    g_fault_machine_img = NULL;
    g_fault_err_img = NULL;
    g_fault_radar_1 = NULL;
    g_fault_radar_2 = NULL;
    g_fault_title_1 = NULL;
    g_fault_title_2 = NULL;
    g_fault_time_label = NULL;
    g_fault_model_label = NULL;
    g_fault_main_desc_label = NULL;
    g_fault_reason_label = NULL;
    g_fault_solution_label = NULL;
    g_fault_version_label = NULL;

    memset(&g_fault_popup_data, 0, sizeof(g_fault_popup_data));
}

static bool fault_popup_show_pending_internal(void)
{
    if (!g_fault_pending.valid) {
        return false;
    }

    switch (g_fault_pending.type) {
    case FAULT_PENDING_START_FAULT:
        show_start_fault_popup(g_fault_pending.fault_type, g_fault_pending.code);
        break;
    case FAULT_PENDING_RUNTIME_FAULT:
        show_runtime_fault_popup(g_fault_pending.code);
        break;
    case FAULT_PENDING_NONE:
    default:
        fault_popup_clear_pending();
        return false;
    }

    fault_popup_clear_pending();
    return true;
}

bool fault_popup_show_pending_now(void)
{
    return fault_popup_show_pending_internal();
}

bool fault_popup_get_pending_fault(fault_source_t* source, uint8_t* fault_type, uint8_t* code)
{
    if (!g_fault_pending.valid) {
        return false;
    }

    if (fault_type) {
        *fault_type = g_fault_pending.fault_type;
    }
    if (code) {
        *code = g_fault_pending.code;
    }

    switch (g_fault_pending.type) {
    case FAULT_PENDING_START_FAULT:
        if (source) {
            *source = FAULT_SRC_START_COUNT;
        }
        return true;
    case FAULT_PENDING_RUNTIME_FAULT:
        if (source) {
            *source = FAULT_SRC_RUNTIME;
        }
        return true;
    case FAULT_PENDING_NONE:
    default:
        return false;
    }
}

bool fault_popup_has_pending_start_issue(void)
{
    if (!g_fault_pending.valid) {
        return false;
    }

    return (g_fault_pending.type == FAULT_PENDING_START_FAULT);
}

void fault_popup_auto_confirm_pending_if_needed(void)
{
    uint8_t clear_cmd = 0x01;
    uint32_t now_tick;

    if (g_fault_popup_auto_enabled) {
        return;
    }

    if (!fault_popup_has_pending_start_issue()) {
        return;
    }

    if (!fault_popup_should_auto_clear(g_fault_pending.fault_type, g_fault_pending.code)) {
        return;
    }

    fault_popup_track_retry_key(g_fault_pending.fault_type, g_fault_pending.code);

    if (g_fault_auto_retry_count >= FAULT_AUTO_RETRY_MAX) {
        show_start_fault_popup(g_fault_pending.fault_type, g_fault_pending.code);
        fault_popup_clear_pending();
        return;
    }

    now_tick = lv_tick_get();
    if (g_fault_auto_retry_last_tick != 0 &&
        lv_tick_elaps(g_fault_auto_retry_last_tick) < FAULT_AUTO_RETRY_INTERVAL_MS) {
        return;
    }

    protocol_send(0x3D, &clear_cmd, 1);
    g_fault_auto_retry_count++;
    g_fault_auto_retry_last_tick = now_tick;
    fault_popup_clear_pending();
}

static void fault_popup_auto_confirm_timer_cb(lv_timer_t* timer)
{
    LV_UNUSED(timer);
    g_fault_auto_confirm_timer = NULL;
    fault_popup_auto_confirm_pending_if_needed();
}

void fault_popup_cancel_auto_confirm(void)
{
    if (g_fault_auto_confirm_timer) {
        lv_timer_del(g_fault_auto_confirm_timer);
        g_fault_auto_confirm_timer = NULL;
    }
}

void fault_popup_schedule_auto_confirm(void)
{
    if (g_fault_popup_auto_enabled) {
        return;
    }

    if (!fault_popup_has_pending_start_issue()) {
        return;
    }

    if (fault_popup_is_showing()) {
        return;
    }

    if (g_fault_auto_confirm_timer) {
        lv_timer_del(g_fault_auto_confirm_timer);
        g_fault_auto_confirm_timer = NULL;
    }

    g_fault_auto_confirm_timer = lv_timer_create(fault_popup_auto_confirm_timer_cb, FAULT_AUTO_CONFIRM_DELAY_MS, NULL);
    if (g_fault_auto_confirm_timer) {
        lv_timer_set_repeat_count(g_fault_auto_confirm_timer, 1);
    }
}

void fault_popup_report_start_fault(uint8_t type, uint8_t code)
{
    fault_popup_track_retry_key(type, code);

    g_fault_pending.valid = true;
    g_fault_pending.type = FAULT_PENDING_START_FAULT;
    g_fault_pending.fault_type = type;
    g_fault_pending.code = code;

    if (g_fault_popup_auto_enabled) {
        (void)fault_popup_show_pending_internal();
    }
}

void fault_popup_report_start_no_note(void)
{
    fault_popup_track_retry_key(0x01, 0x00);

    g_fault_pending.valid = true;
    g_fault_pending.type = FAULT_PENDING_START_FAULT;
    g_fault_pending.fault_type = 0x01;
    g_fault_pending.code = 0x00;

    if (g_fault_popup_auto_enabled) {
        (void)fault_popup_show_pending_internal();
    }
}

void fault_popup_report_runtime_fault(uint8_t code)
{
    g_fault_pending.valid = true;
    g_fault_pending.type = FAULT_PENDING_RUNTIME_FAULT;
    g_fault_pending.fault_type = 0x00;
    g_fault_pending.code = code;

    if (g_fault_popup_auto_enabled) {
        (void)fault_popup_show_pending_internal();
    }
}

void fault_popup_reset_auto_retry(void)
{
    g_fault_auto_retry_count = 0;
    g_fault_auto_retry_type = 0;
    g_fault_auto_retry_code = 0;
    g_fault_auto_retry_last_tick = 0;
}

void show_fault_popup_ex(const fault_popup_data_t* data)
{
    char time_buf[64];

    if (data == NULL) return;

    hide_fault_popup();
    g_fault_popup_data = *data;
    get_time_str(time_buf, sizeof(time_buf));

    g_fault_popup = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(g_fault_popup);
    lv_obj_set_size(g_fault_popup, 1230, 368);
    lv_obj_center(g_fault_popup);
    lv_obj_clear_flag(g_fault_popup, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* bg = lv_img_create(g_fault_popup);
    lv_img_set_src(bg, FAULT_POPUP_BG_PATH);
    lv_obj_set_size(bg, 1230, 368);
    lv_obj_center(bg);

    /* 左侧图片区 */
    g_fault_machine_img = lv_img_create(g_fault_popup);
    lv_img_set_src(g_fault_machine_img, data->machine_img_path);
    lv_obj_set_pos(g_fault_machine_img, 26, 84);

    g_fault_err_img = lv_img_create(g_fault_popup);
    lv_img_set_src(g_fault_err_img, data->err_img_path);
    lv_obj_set_pos(g_fault_err_img, 304, 84);

    /* 雷达 */
    g_fault_radar_1 = lv_obj_create(g_fault_popup);
    lv_obj_remove_style_all(g_fault_radar_1);
    lv_obj_clear_flag(g_fault_radar_1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(g_fault_radar_1, 4, 4);
    lv_obj_set_pos(g_fault_radar_1,
                data->radar_x - 2,
                data->radar_y - 2);
    lv_obj_set_style_radius(g_fault_radar_1, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(g_fault_radar_1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_fault_radar_1, 2, 0);
    lv_obj_set_style_border_color(g_fault_radar_1, lv_color_hex(0xFF3B30), 0);
    lv_obj_set_style_border_opa(g_fault_radar_1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(g_fault_radar_1, 0, 0);
    lv_obj_set_style_outline_width(g_fault_radar_1, 0, 0);
    lv_obj_add_flag(g_fault_radar_1, LV_OBJ_FLAG_HIDDEN);

    g_fault_radar_2 = lv_obj_create(g_fault_popup);
    lv_obj_remove_style_all(g_fault_radar_2);
    lv_obj_clear_flag(g_fault_radar_2, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(g_fault_radar_2, 4, 4);
    lv_obj_set_pos(g_fault_radar_2,
                data->radar_x - 2,
                data->radar_y - 2);
    lv_obj_set_style_radius(g_fault_radar_2, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(g_fault_radar_2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_fault_radar_2, 2, 0);
    lv_obj_set_style_border_color(g_fault_radar_2, lv_color_hex(0xFF3B30), 0);
    lv_obj_set_style_border_opa(g_fault_radar_2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(g_fault_radar_2, 0, 0);
    lv_obj_set_style_outline_width(g_fault_radar_2, 0, 0);
    lv_obj_add_flag(g_fault_radar_2, LV_OBJ_FLAG_HIDDEN);

    g_fault_anim_tick = 0;
    g_fault_anim_timer = lv_timer_create(fault_anim_timer_cb, 30, NULL);

    /* 红框1 */
    g_fault_title_1 = lv_label_create(g_fault_popup);
    lv_label_set_text(g_fault_title_1, data->diagnostics_title);
    lv_obj_set_style_text_color(g_fault_title_1, lv_color_hex(0x7A7A7A), 0);
    lv_obj_set_style_text_font(g_fault_title_1, &lv_font_instrument_sans_semibold_14, 0);
    lv_obj_set_pos(g_fault_title_1, 25, 12);

    /* 红框2 */
    g_fault_title_2 = lv_label_create(g_fault_popup);
    lv_label_set_text(g_fault_title_2, data->fault_type_title);
    lv_obj_set_style_text_color(g_fault_title_2, lv_color_hex(0xFF3B30), 0);
    lv_obj_set_style_text_font(g_fault_title_2, &lv_font_instrument_sans_semibold_16, 0);
    lv_obj_set_pos(g_fault_title_2, 650, 20);

    /* 时间/机型 */
    g_fault_time_label = lv_label_create(g_fault_popup);
    lv_label_set_text_fmt(g_fault_time_label, ui_text_get(UI_TEXT_WIDGET_FAULT_TIME_FMT), time_buf);
    lv_obj_set_style_text_color(g_fault_time_label, lv_color_hex(0xB0B0B0), 0);
    lv_obj_set_style_text_font(g_fault_time_label, &lv_font_instrument_sans_medium_14, 0);
    lv_obj_set_pos(g_fault_time_label, 650, 46);

    g_fault_model_label = lv_label_create(g_fault_popup);
    lv_label_set_text_fmt(g_fault_model_label, ui_text_get(UI_TEXT_WIDGET_FAULT_MODEL_FMT), MACHINE_MODEL_NAME);
    lv_obj_set_style_text_color(g_fault_model_label, lv_color_hex(0xB0B0B0), 0);
    lv_obj_set_style_text_font(g_fault_model_label, &lv_font_instrument_sans_medium_14, 0);
    lv_obj_set_pos(g_fault_model_label, 880, 46);

    /* 红框3 */
    g_fault_main_desc_label = lv_label_create(g_fault_popup);
    lv_label_set_text(g_fault_main_desc_label, data->fault_main_desc);
    lv_obj_set_style_text_color(g_fault_main_desc_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(g_fault_main_desc_label, &lv_font_instrument_sans_bold_24, 0);
    lv_obj_set_pos(g_fault_main_desc_label, 607, 66);

    /* 原因框 */
    g_reason_title = lv_label_create(g_fault_popup);
    lv_label_set_text(g_reason_title, ui_text_get(UI_TEXT_WIDGET_FAULT_REASON_TITLE));
    lv_obj_set_width(g_reason_title, 100);
    lv_obj_set_style_text_color(g_reason_title, lv_color_hex(0xff3b30), 0);
    lv_obj_set_style_text_font(g_reason_title, &lv_font_instrument_sans_semibold_16, 0);
    lv_obj_set_pos(g_reason_title, 629, 105);

    g_fault_reason_label = lv_label_create(g_fault_popup);
    lv_label_set_text_fmt(g_fault_reason_label, "%s", data->reason_text);
    lv_obj_set_width(g_fault_reason_label, 680);
    lv_label_set_long_mode(g_fault_reason_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(g_fault_reason_label, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(g_fault_reason_label, &lv_font_instrument_sans_medium_16, 0);
    lv_obj_set_pos(g_fault_reason_label, 629, 128   );

    g_solution_title = lv_label_create(g_fault_popup);
    lv_label_set_text(g_solution_title, ui_text_get(UI_TEXT_WIDGET_FAULT_SOLUTION_TITLE));
    lv_obj_set_width(g_solution_title, 100);
    lv_obj_set_style_text_color(g_solution_title, lv_color_hex(0x007aff), 0);
    lv_obj_set_style_text_font(g_solution_title, &lv_font_instrument_sans_semibold_16, 0);
    lv_obj_set_pos(g_solution_title, 629, 219);

    g_fault_solution_label = lv_label_create(g_fault_popup);
    lv_label_set_text_fmt(g_fault_solution_label, "%s", data->solution_text);
    lv_obj_set_size(g_fault_solution_label, 563, 56);
    lv_label_set_long_mode(g_fault_solution_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(g_fault_solution_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(g_fault_solution_label, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(g_fault_solution_label, &lv_font_instrument_sans_medium_16, 0);
    lv_obj_set_pos(g_fault_solution_label, 629, 243);

    /* 版本 */
    g_fault_version_label = lv_label_create(g_fault_popup);
    lv_label_set_text_fmt(g_fault_version_label, " %s", device_info_main_app());
    lv_obj_set_style_text_color(g_fault_version_label, lv_color_hex(0x999999), 0);
    lv_obj_set_style_text_font(g_fault_version_label, &lv_font_instrument_sans_medium_14, 0);
    lv_obj_set_pos(g_fault_version_label, 595, 342);

    /* confirm */
    lv_obj_t* confirm_btn = lv_btn_create(g_fault_popup);
    lv_obj_set_size(confirm_btn, 180, 45);
    lv_obj_set_pos(confirm_btn, 1023, 311);
    lv_obj_set_style_radius(confirm_btn, 14, 0);
    lv_obj_set_style_bg_color(confirm_btn, lv_color_hex(0x1677FF), 0);
    lv_obj_set_style_border_width(confirm_btn, 0, 0);
    lv_obj_add_event_cb(confirm_btn, fault_popup_confirm_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* confirm_label = lv_label_create(confirm_btn);
    lv_label_set_text(confirm_label, ui_text_get(UI_TEXT_WIDGET_FAULT_CONFIRM));
    lv_obj_set_style_text_color(confirm_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(confirm_label, &lv_font_instrument_sans_bold_20, 0);
    lv_obj_center(confirm_label);
}

/* =========================
 * 协议适配函数
 * ========================= */
void show_boot_fault_popup(uint8_t selftest_type, uint8_t result)
{
    fault_popup_data_t data;
    memset(&data, 0, sizeof(data));

    (void)result;

    data.source = FAULT_SRC_BOOT;
    data.code = selftest_type;
    data.diagnostics_title = ui_text_get(UI_TEXT_WIDGET_FAULT_DIAGNOSTICS_TITLE);
    data.fault_type_title = get_boot_title(selftest_type);
    data.fault_main_desc = get_boot_main_desc(selftest_type);
    data.reason_text = get_fault_reason_text(FAULT_SRC_BOOT, selftest_type);
    data.solution_text = get_fault_solution_text(FAULT_SRC_BOOT, selftest_type);
    data.machine_img_path = get_fault_machine_img(FAULT_SRC_BOOT, selftest_type);
    data.err_img_path = get_fault_err_img(FAULT_SRC_BOOT, selftest_type);
    get_fault_radar_pos(FAULT_SRC_BOOT, selftest_type, &data.radar_x, &data.radar_y);
    data.confirm_action = get_confirm_action_by_page();

    show_fault_popup_ex(&data);
}

void show_start_fault_popup(uint8_t type, uint8_t code)
{
    fault_popup_data_t data;
    memset(&data, 0, sizeof(data));

    (void)type;

    data.source = FAULT_SRC_START_COUNT;
    data.code = code;
    data.diagnostics_title = ui_text_get(UI_TEXT_WIDGET_FAULT_DIAGNOSTICS_TITLE);
    data.fault_type_title = get_start_title(code);
    data.fault_main_desc = get_start_main_desc(code);
    data.reason_text = get_fault_reason_text(FAULT_SRC_START_COUNT, code);
    data.solution_text = get_fault_solution_text(FAULT_SRC_START_COUNT, code);
    data.machine_img_path = get_fault_machine_img(FAULT_SRC_START_COUNT, code);
    data.err_img_path = get_fault_err_img(FAULT_SRC_START_COUNT, code);
    get_fault_radar_pos(FAULT_SRC_START_COUNT, code, &data.radar_x, &data.radar_y);
    data.confirm_action = get_confirm_action_by_page();

    show_fault_popup_ex(&data);
}

void show_runtime_fault_popup(uint8_t code)
{
    fault_popup_data_t data;
    memset(&data, 0, sizeof(data));

    data.source = FAULT_SRC_RUNTIME;
    data.code = code;
    data.diagnostics_title = ui_text_get(UI_TEXT_WIDGET_FAULT_DIAGNOSTICS_TITLE);
    data.fault_type_title = get_runtime_title(code);
    data.fault_main_desc = get_runtime_main_desc(code);
    data.reason_text = get_fault_reason_text(FAULT_SRC_RUNTIME, code);
    data.solution_text = get_fault_solution_text(FAULT_SRC_RUNTIME, code);
    data.machine_img_path = get_fault_machine_img(FAULT_SRC_RUNTIME, code);
    data.err_img_path = get_fault_err_img(FAULT_SRC_RUNTIME, code);
    get_fault_radar_pos(FAULT_SRC_RUNTIME, code, &data.radar_x, &data.radar_y);
    data.confirm_action = get_confirm_action_by_page();

    show_fault_popup_ex(&data);
}
