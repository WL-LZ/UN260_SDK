#include "page_36_display_test.h"
#include "lv_page_manager.h"
#include "un260/lv_components/lv_nav_button.h"
#include "un260/lv_system/user_cfg.h"
#include <stdio.h>

/* Diagnostic pixels bypass settings colour remapping and decorative skins. */
static lv_obj_t *test_page;

static lv_obj_t *test_box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t rgb)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(rgb), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return o;
}

static void test_text(lv_obj_t *parent, const char *text, int x, int y, int width)
{
    lv_obj_t *o = lv_label_create(parent);
    lv_label_set_text(o, text);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_width(o, width);
    lv_obj_set_style_text_font(o, &lv_font_instrument_sans_medium_14, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
}

static void test_row(int y, const char *name, const uint32_t *colors, unsigned count)
{
    test_text(test_page, name, 8, y + 16, 116);
    int width = 1128 / (int)count;
    for(unsigned i = 0; i < count; ++i) {
        char hex[12];
        snprintf(hex, sizeof(hex), "#%06lX", (unsigned long)colors[i]);
        test_box(test_page, 132 + i * width, y, width, 44, colors[i]);
        test_text(test_page, hex, 132 + i * width, y + 48, width);
    }
}

static void test_back(lv_event_t *event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED) ui_manager_pop_page();
}

void ui_page_36_display_test_create(lv_obj_t *parent)
{
    if(test_page) return;
    static const uint32_t gray[] = {0x000000,0x040404,0x080808,0x101010,
        0x202020,0x404040,0x606060,0x808080,0xA0A0A0,0xC0C0C0,0xE0E0E0,0xFFFFFF};
    static const uint32_t white[] = {0xDCDCDC,0xE0E0E0,0xE4E4E4,0xE8E8E8,
        0xECECEC,0xF0F0F0,0xF4F4F4,0xF8F8F8,0xFAFAFA,0xFCFCFC,0xFEFEFE,0xFFFFFF};
    static const uint32_t warm[] = {0xFFFFFF,0xF6F8FA,0xF6F1ED,0xFFF4DB,0xFFE8AD,0xFFD477};
    static const uint32_t basic[] = {0xFF0000,0x00FF00,0x0000FF,0x00FFFF,
        0xFF00FF,0xFFFF00,0x808080,0xFFFFFF};
    test_page = test_box(parent, 0, 0, 1280, 400, 0x303030);
    test_text(test_page, "DISPLAY TEST  /  Fixed RGB - no calibration applied", 16, 15, 620);
    lv_nav_button_create(test_page, 1150, 6, 110, 44, test_back, NULL);
    test_row(64, "Gray levels", gray, 12);
    test_row(142, "Near white", white, 12);
    test_row(220, "Warm / UI", warm, 6);
    test_row(298, "RGB / CMY", basic, 8);
    test_text(test_page, "Warm / UI: patch 3 = original background. Photograph straight on; lock exposure and white balance.", 16, 377, 1248);
}

void ui_page_36_display_test_destroy(void)
{
    if(test_page) lv_obj_del(test_page);
    test_page = NULL;
}
