#include "page_36_display_test.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "un260/lv_core/settings_detail_ui.h"
#include "lv_page_manager.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_system/backlight_service.h"
#include "un260/lv_system/user_cfg.h"
#include <stdio.h>
#include <string.h>

/* Reference patches must bypass palette remapping. The present UI scanout has
 * no verified RGB/Gamma transform; do not equate video-layer CSC with UI output. */
static lv_settings_frame_t frame;
static lv_obj_t *patterns, *tabs[4], *brightness_value, *adjust_button;
static lv_timer_t *monitor;
static unsigned stage;
static int displayed_level = -1, displayed_max = -1;

static void reference_row(int y, const uint32_t *colors, unsigned count)
{
    int width = 744 / (int)count;
    for (unsigned i = 0; i < count; ++i) {
        char hex[12];
        snprintf(hex, sizeof(hex), "#%06lX", (unsigned long)colors[i]);
        lv_settings_box(patterns, 24 + i * width, y, width, 55, colors[i]);
        lv_obj_t *label = lv_settings_label(patterns, hex, 24 + i * width, y + 62,
            &lv_font_instrument_sans_medium_12, 0xFFFFFF);
        lv_obj_set_width(label, width);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    }
}

static void draw_reference(void)
{
    static const uint32_t gray[] = {0x000000,0x101010,0x202020,0x404040,0x606060,
        0x808080,0xA0A0A0,0xC0C0C0,0xE0E0E0,0xFFFFFF};
    static const uint32_t black[] = {0x000000,0x040404,0x080808,0x0C0C0C,0x101010,
        0x141414,0x181818,0x1C1C1C,0x202020,0x242424};
    static const uint32_t white[] = {0xDCDCDC,0xE4E4E4,0xECECEC,0xF0F0F0,0xF4F4F4,
        0xF7F7F7,0xFAFAFA,0xFCFCFC,0xFEFEFE,0xFFFFFF};
    static const uint32_t color[] = {0xFF0000,0x00FF00,0x0000FF,0x00FFFF,0xFF00FF,0xFFFF00};
    static const uint32_t ui[] = {0xFFFFFF,0xF7F7F7,0xE2E9EE,0x1559B7,0x1D2B34,0xB42332};
    static const char *instructions[] = {
        "Gray should stay neutral. Look for smooth separation from dark to light.",
        "Check subtle white steps without glare. Not every panel resolves every near-white step.",
        "Check for tint and uneven color. Reference colors are not a measured calibration.",
        "Check real controls and text after adjusting brightness. Keep only a comfortable result."
    };
    for (unsigned i = 0; i < 4; ++i) {
        if (i == stage) lv_obj_add_state(tabs[i], LV_STATE_CHECKED);
        else lv_obj_clear_state(tabs[i], LV_STATE_CHECKED);
    }
    lv_obj_clean(patterns);
    lv_settings_label(patterns, stage == 0 ? "NEUTRAL GRAY" : stage == 1 ? "NEAR WHITE" :
        stage == 2 ? "RGB / CMY" : "INTERFACE PALETTE", 24, 15,
        &lv_font_instrument_sans_medium_14, 0xFFFFFF);
    reference_row(44, stage == 0 ? gray : stage == 1 ? white : stage == 2 ? color : ui,
        stage < 2 ? 10 : 6);
    if (stage == 3) {
        lv_settings_label(patterns, "CONTROL LEGIBILITY", 24, 136,
            &lv_font_instrument_sans_medium_14, 0xFFFFFF);
        lv_obj_t *sample = lv_settings_button(patterns, 24, 171, 170, 48, "Secondary", false, NULL, NULL);
        lv_obj_clear_flag(sample, LV_OBJ_FLAG_CLICKABLE);
        sample = lv_settings_button(patterns, 214, 171, 170, 48, "Primary", true, NULL, NULL);
        lv_obj_clear_flag(sample, LV_OBJ_FLAG_CLICKABLE);
        sample = lv_settings_button(patterns, 404, 171, 170, 48, "Destructive", true, NULL, NULL);
        lv_settings_action_style(sample, LV_SETTINGS_ACTION_DESTRUCTIVE);
        lv_obj_clear_flag(sample, LV_OBJ_FLAG_CLICKABLE);
        sample = lv_settings_button(patterns, 594, 171, 170, 48, "Unavailable", false, NULL, NULL);
        lv_obj_add_state(sample, LV_STATE_DISABLED);
    } else {
        lv_settings_label(patterns, stage == 0 ? "SHADOW DETAIL" : "NEUTRAL REFERENCE", 24, 136,
            &lv_font_instrument_sans_medium_14, 0xFFFFFF);
        reference_row(165, stage == 0 ? black : gray, 10);
    }
    lv_label_set_text(frame.message, instructions[stage]);
}

static void select_stage(lv_event_t *event)
{
    unsigned next = (unsigned)(uintptr_t)lv_event_get_user_data(event);
    if (next >= 4 || next == stage) return;
    stage = next;
    draw_reference();
}

static void refresh_brightness(lv_timer_t *timer)
{
    if (!frame.root || (timer && !lv_obj_is_visible(frame.root))) return;
    int level = backlight_service_level(), maximum = backlight_service_max();
    if (level == displayed_level && maximum == displayed_max) return;
    displayed_level = level; displayed_max = maximum;
    if (maximum > 0) {
        lv_label_set_text_fmt(brightness_value, "%d%%", level * 100 / maximum);
        settings_detail_action_block(adjust_button, NULL);
    } else {
        lv_label_set_text(brightness_value, "Unavailable");
        settings_detail_action_block(adjust_button, "Adjustable backlight is unavailable on this device.");
    }
}

static void adjust(lv_event_t *event)
{
    (void)event;
    if (backlight_service_max() > 0) ui_manager_push_page(UI_PAGE_BRIGHTNESS_SETTING);
}

static void back(lv_event_t *event)
{
    (void)event;
    ui_manager_pop_page();
}

static void deleted(lv_event_t *event)
{
    if (lv_event_get_target(event) != frame.root) return;
    if (monitor) lv_timer_del(monitor);
    monitor = NULL;
    patterns = brightness_value = adjust_button = NULL;
    memset(tabs, 0, sizeof(tabs));
    memset(&frame, 0, sizeof(frame));
}

void ui_page_36_display_test_create(lv_obj_t *parent)
{
    if (frame.root) return;
    stage = 0; displayed_level = displayed_max = -1;
    lv_settings_header_t header = { .title = "Color calibration", .icon = "Sun", .back = back };
    frame = lv_settings_frame_create(parent, &header);
    lv_obj_add_event_cb(frame.root, deleted, LV_EVENT_DELETE, NULL);
    lv_obj_set_style_bg_opa(frame.body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(frame.body, 0, 0);
    const char *names[] = {"1  Gray levels", "2  Near white", "3  Color", "4  Interface"};
    lv_obj_t *base = lv_settings_segment_base(frame.body, 0, 0, 792, 42);
    for (unsigned i = 0; i < 4; ++i)
        tabs[i] = lv_settings_segment(base, i, 4, names[i], select_stage, (void *)(uintptr_t)i);
    patterns = lv_settings_box(frame.body, 0, 50, 792, 250, 0x484848);
    lv_obj_set_style_radius(patterns, 14, 0);
    lv_obj_t *panel = lv_settings_panel(frame.body, 812, 0, 420, 300);
    lv_settings_label(panel, "Display brightness", 24, 20,
        &lv_font_instrument_sans_medium_18, 0x1D2B34);
    brightness_value = lv_settings_label(panel, "", 24, 52,
        &lv_font_instrument_sans_semibold_28, 0x1D2B34);
    adjust_button = lv_settings_button(panel, 252, 45, 144, 46, "Adjust", true, adjust, NULL);
    lv_obj_t *hint = lv_settings_label(panel,
        "Preview safely in Brightness.\nUnsaved changes revert after 8 seconds.", 24, 106,
        &lv_font_instrument_sans_medium_14, 0x586B78);
    lv_obj_set_width(hint, 372);
    lv_settings_box(panel, 24, 162, 372, 1, 0xE3E9ED);
    lv_settings_label(panel, "RGB / Gamma", 24, 186,
        &lv_font_instrument_sans_medium_18, 0x1D2B34);
    hint = lv_settings_label(panel, "Not enabled for the current UI output.\nThese references do not alter color output.",
        24, 222, &lv_font_instrument_sans_medium_14, 0x586B78);
    lv_obj_set_width(hint, 372);
    backlight_service_probe();
    refresh_brightness(NULL);
    monitor = lv_timer_create(refresh_brightness, 500, NULL);
    draw_reference();
}

void ui_page_36_display_test_destroy(void)
{
    if (frame.root) lv_obj_del(frame.root);
}
