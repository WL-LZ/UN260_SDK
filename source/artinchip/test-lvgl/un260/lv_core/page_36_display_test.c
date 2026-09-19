#include "page_36_display_test.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "un260/lv_core/settings_detail_ui.h"
#include "lv_page_manager.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_system/user_cfg.h"
#include <stdio.h>
#include <string.h>

/* Diagnostic RGB values bypass the settings palette remapper. */
static lv_settings_frame_t test_frame;

static void test_row(int y, const char *name, const uint32_t *colors, unsigned count)
{
    lv_obj_t *label = lv_settings_label(test_frame.body, name, 8, y + 12,
        &lv_font_instrument_sans_medium_14, 0xFFFFFF);
    lv_obj_set_width(label, 108);
    int width = 1108 / (int)count;
    for (unsigned i = 0; i < count; ++i) {
        char hex[12];
        snprintf(hex, sizeof(hex), "#%06lX", (unsigned long)colors[i]);
        lv_settings_box(test_frame.body, 124 + i * width, y, width, 32, colors[i]);
        label = lv_settings_label(test_frame.body, hex, 124 + i * width, y + 36,
            &lv_font_instrument_sans_medium_14, 0xFFFFFF);
        lv_obj_set_width(label, width);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    }
}

static void test_back(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) ui_manager_pop_page();
}

void ui_page_36_display_test_create(lv_obj_t *parent)
{
    if (test_frame.root) return;
    static const uint32_t gray[] = {0x000000,0x040404,0x080808,0x101010,
        0x202020,0x404040,0x606060,0x808080,0xA0A0A0,0xC0C0C0,0xE0E0E0,0xFFFFFF};
    static const uint32_t white[] = {0xDCDCDC,0xE0E0E0,0xE4E4E4,0xE8E8E8,
        0xECECEC,0xF0F0F0,0xF4F4F4,0xF8F8F8,0xFAFAFA,0xFCFCFC,0xFEFEFE,0xFFFFFF};
    static const uint32_t warm[] = {0xFFFFFF,0xF1F4F5,0xF8FAFB,0xE2E9EE,0xFFF4DB,0xFFD477};
    static const uint32_t basic[] = {0xFF0000,0x00FF00,0x0000FF,0x00FFFF,
        0xFF00FF,0xFFFF00,0x808080,0xFFFFFF};
    lv_settings_header_t header = { .title = "Display test", .icon = "Sun", .back = test_back };
    test_frame = lv_settings_frame_create(parent, &header);
    lv_obj_set_style_bg_color(test_frame.body, lv_color_hex(0x303030), 0);
    lv_obj_set_style_border_width(test_frame.body, 0, 0);
    test_row(4, "Gray levels", gray, 12);
    test_row(64, "Near white", white, 12);
    test_row(124, "UI / warm", warm, 6);
    test_row(184, "RGB / CMY", basic, 8);
    lv_label_set_text(test_frame.message, "Fixed RGB test patches - labels show application color values.");
}

void ui_page_36_display_test_destroy(void)
{
    if (test_frame.root) lv_obj_del(test_frame.root);
    memset(&test_frame, 0, sizeof(test_frame));
}
