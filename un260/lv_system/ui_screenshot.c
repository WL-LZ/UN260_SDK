#include "ui_screenshot.h"

#include "lv_fbdev.h"
#include "un260/lv_components/lv_print_toast.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/storage/usb_storage.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define UI_SCREENSHOT_POLL_MS 1000U

static lv_obj_t* g_screenshot_indicator = NULL;
static uint32_t g_screenshot_poll_tick = 0;

static void ui_screenshot_indicator_set_visible(bool visible)
{
    bool is_visible;

    if (g_screenshot_indicator == NULL ||
        !lv_obj_is_valid(g_screenshot_indicator)) {
        return;
    }

    is_visible = !lv_obj_has_flag(g_screenshot_indicator,
                                  LV_OBJ_FLAG_HIDDEN);
    if (is_visible == visible) {
        return;
    }

    if (visible) {
        lv_obj_clear_flag(g_screenshot_indicator, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(g_screenshot_indicator, LV_OBJ_FLAG_HIDDEN);
    }
}

typedef enum {
    UI_SCREENSHOT_OK = 0,
    UI_SCREENSHOT_USB_NOT_MOUNTED,
    UI_SCREENSHOT_SAVE_FAILED
} ui_screenshot_result_t;

static ui_screenshot_result_t ui_screenshot_save_to_usb(void)
{
    char path[256];
    char timestamp[32];
    struct tm local_tm;
    time_t now;
    int suffix = 0;

    if (!usb_storage_prepare()) {
        return UI_SCREENSHOT_USB_NOT_MOUNTED;
    }

    now = time(NULL);
    if (localtime_r(&now, &local_tm) == NULL ||
        strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &local_tm) == 0U) {
        return UI_SCREENSHOT_SAVE_FAILED;
    }

    do {
        if (suffix == 0) {
            snprintf(path, sizeof(path), "%s/screenshot_%s.bmp",
                     USB_STORAGE_MOUNT_POINT, timestamp);
        } else {
            snprintf(path, sizeof(path), "%s/screenshot_%s_%02d.bmp",
                     USB_STORAGE_MOUNT_POINT, timestamp, suffix);
        }
        suffix++;
    } while (suffix <= 99 && access(path, F_OK) == 0);

    if (suffix > 99 || fbdev_save_bmp(path) != 0) {
        return UI_SCREENSHOT_SAVE_FAILED;
    }

    return UI_SCREENSHOT_OK;
}

static void ui_screenshot_show_toast(const char* text, bool alarm)
{
    lv_print_toast_config_t toast_cfg = lv_print_toast_get_default_config();

    toast_cfg.w = 360;
    toast_cfg.h = 101;
    toast_cfg.text = text;
    toast_cfg.show_loader = false;
    toast_cfg.align_center = true;
    toast_cfg.use_text_area = false;
    toast_cfg.loader_color = alarm ? lv_color_hex(0xC0392B) : lv_color_hex(0x18A66A);
    toast_cfg.auto_hide_ms = 1800;
    lv_print_toast_show_with_config(&toast_cfg);
}

static void ui_screenshot_indicator_click_cb(lv_event_t* event)
{
    ui_screenshot_result_t result;

    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    lv_obj_add_flag(g_screenshot_indicator, LV_OBJ_FLAG_HIDDEN);
    lv_refr_now(NULL);
    result = ui_screenshot_save_to_usb();
    lv_obj_clear_flag(g_screenshot_indicator, LV_OBJ_FLAG_HIDDEN);

    if (result == UI_SCREENSHOT_OK) {
        ui_screenshot_show_toast(ui_text_get(UI_TEXT_WIDGET_SCREENSHOT_SAVED), false);
    } else if (result == UI_SCREENSHOT_USB_NOT_MOUNTED) {
        ui_screenshot_show_toast(ui_text_get(UI_TEXT_WIDGET_SCREENSHOT_INSERT_USB), true);
    } else {
        ui_screenshot_show_toast(ui_text_get(UI_TEXT_WIDGET_SCREENSHOT_SAVE_FAILED), true);
    }
}

static void ui_screenshot_indicator_create(void)
{
    lv_obj_t* icon;

    if (g_screenshot_indicator != NULL &&
        lv_obj_is_valid(g_screenshot_indicator)) {
        return;
    }

    g_screenshot_indicator = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(g_screenshot_indicator);
    lv_obj_set_pos(g_screenshot_indicator, 1250, 8);
    lv_obj_set_size(g_screenshot_indicator, 24, 34);
    lv_obj_set_style_bg_opa(g_screenshot_indicator, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(g_screenshot_indicator,
                              lv_color_hex(0xE8E8E8), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(g_screenshot_indicator, LV_OPA_70, LV_STATE_PRESSED);
    lv_obj_set_style_radius(g_screenshot_indicator, 10, 0);
    lv_obj_clear_flag(g_screenshot_indicator, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_screenshot_indicator, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(g_screenshot_indicator, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(g_screenshot_indicator, ui_screenshot_indicator_click_cb,
                        LV_EVENT_CLICKED, NULL);

    icon = lv_img_create(g_screenshot_indicator);
    lv_img_set_src(icon,
                   "L:/usr/local/share/lvgl_data/page_01_usb_icon.png");
    lv_obj_center(icon);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);
}

void ui_screenshot_indicator_poll(void)
{
    uint32_t now = lv_tick_get();
    bool ready;

    if (!user_cfg_screenshot_enabled()) {
        ui_screenshot_indicator_set_visible(false);
        return;
    }

    ui_screenshot_indicator_create();

    if (g_screenshot_poll_tick != 0 &&
        (uint32_t)(now - g_screenshot_poll_tick) < UI_SCREENSHOT_POLL_MS) {
        return;
    }
    g_screenshot_poll_tick = now;

    ready = usb_storage_available();
    ui_screenshot_indicator_set_visible(ready);
}
