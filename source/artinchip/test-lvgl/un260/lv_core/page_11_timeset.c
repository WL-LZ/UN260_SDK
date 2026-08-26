#include "un260/lv_core/page_08_boot.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_resources/lv_image_declear.h" 
#include "un260/lv_resources/lv_img_init.h" 
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/lv_components/lv_components.h"
#include "un260/lv_core/page_01_main.h"
#include "../aic_ui/aic_ui.h"
#include "un260/lv_system/machine_time.h"
#include "page_11_timeset.h"
#include <string.h>

static lv_obj_t* timeset_page = NULL;
static lv_obj_t* lbl_time = NULL;

static lv_obj_t* r_year = NULL;
static lv_obj_t* r_mon = NULL;
static lv_obj_t* r_day = NULL;
static lv_obj_t* r_hour = NULL;
static lv_obj_t* r_min = NULL;
static lv_obj_t* r_sec = NULL;
static bool s_roller_syncing = false;
static machine_time_value_t s_editing_time = { 2024, 10, 26, 11, 28, 30 };

static void refresh_time_label(void)
{
    char buf[64];

    lv_snprintf(buf, sizeof(buf), "%04u/%02u/%02u/%02u/%02u/%02u",
                (unsigned)s_editing_time.year, (unsigned)s_editing_time.month,
                (unsigned)s_editing_time.day, (unsigned)s_editing_time.hour,
                (unsigned)s_editing_time.minute, (unsigned)s_editing_time.second);
    if (lbl_time && lv_obj_is_valid(lbl_time)) {
        lv_label_set_text(lbl_time, buf);
    }
}

static void roller_changed_cb(lv_event_t* e)
{
    if (s_roller_syncing) return;

    s_editing_time.year = (uint16_t)(2000 + lv_roller_get_selected(r_year));
    s_editing_time.month = (uint8_t)(1 + lv_roller_get_selected(r_mon));
    s_editing_time.day = (uint8_t)(1 + lv_roller_get_selected(r_day));
    s_editing_time.hour = (uint8_t)(lv_roller_get_selected(r_hour));
    s_editing_time.minute = (uint8_t)(lv_roller_get_selected(r_min));
    s_editing_time.second = (uint8_t)(lv_roller_get_selected(r_sec));
    machine_time_normalize(&s_editing_time);
    machine_time_confirm(&s_editing_time);
    machine_time_get(&s_editing_time);
    s_roller_syncing = true;
    lv_roller_set_selected(r_year, (int)(s_editing_time.year - 2000), LV_ANIM_OFF);
    lv_roller_set_selected(r_mon, (int)(s_editing_time.month - 1), LV_ANIM_OFF);
    lv_roller_set_selected(r_day, (int)(s_editing_time.day - 1), LV_ANIM_OFF);
    lv_roller_set_selected(r_hour, (int)s_editing_time.hour, LV_ANIM_OFF);
    lv_roller_set_selected(r_min, (int)s_editing_time.minute, LV_ANIM_OFF);
    lv_roller_set_selected(r_sec, (int)s_editing_time.second, LV_ANIM_OFF);
    s_roller_syncing = false;
    refresh_time_label();
}

static void roller_scroll_end_cb(lv_event_t* e)
{
    if ((lv_event_code_t)lv_event_get_code(e) != LV_EVENT_SCROLL_END) return;
    roller_changed_cb(e);
}

static void esc_btn_cb(lv_event_t* e)
{
    if ((lv_event_code_t)lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ui_manager_pop_page();
}

static lv_obj_t* create_roller(lv_obj_t* parent, int x, int y, int w, int h,
    const char* opts, int sel)
{
    lv_obj_t* r = lv_roller_create(parent);
    lv_obj_set_pos(r, x, y);
    lv_obj_set_size(r, w, h);
    lv_roller_set_options(r, opts, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(r, 3);
    lv_roller_set_selected(r, sel, LV_ANIM_OFF);
    lv_obj_set_style_text_align(r, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_add_event_cb(r, roller_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(r, roller_scroll_end_cb, LV_EVENT_SCROLL_END, NULL);
    return r;
}


void ui_page_11_timeset_create(lv_obj_t* parent)
{
    (void)parent;
    if (timeset_page) return;

    /* 进入设置页时暂停自动走时，避免调节时被跳动 */
    machine_time_pause(true);

    lv_obj_t* content = NULL;
    timeset_page = settings_detail_create_page(parent, ui_text_get(UI_TEXT_SETTINGS_TIME_SET_TITLE),
                                               esc_btn_cb, &content);

    /* 当前时间显示 */
    lbl_time = settings_detail_create_label(content, "", &lv_font_instrument_sans_medium_24,
                                            lv_color_hex(0x08C5D6), 32, 18);
    lv_obj_set_size(lbl_time, 1220, 40);
    refresh_time_label();

    settings_detail_create_label(content, ui_text_get(UI_TEXT_SETTINGS_TIME_HINT),
                                 &lv_font_instrument_sans_medium_16,
                                 lv_color_hex(0x5686A5), 32, 66);

    /* 生成roller选项字符串 */
    static char opt_year[2048];
    static char opt_mon[128];
    static char opt_day[256];
    static char opt_h[256];
    static char opt_m[256];
    static char opt_s[256];

    opt_year[0] = '\0';
    opt_mon[0] = '\0';
    opt_day[0] = '\0';
    opt_h[0] = '\0';
    opt_m[0] = '\0';
    int pos = 0;
    for (int i = 2000; i <= 2099; i++) {
        pos += lv_snprintf(opt_year + pos, sizeof(opt_year) - pos, (i == 2099) ? "%d" : "%d\n", i);
    }
    pos = 0;
    for (int i = 1; i <= 12; i++) {
        pos += lv_snprintf(opt_mon + pos, sizeof(opt_mon) - pos, (i == 12) ? "%02d" : "%02d\n", i);
    }
    pos = 0;
    for (int i = 1; i <= 31; i++) {
        pos += lv_snprintf(opt_day + pos, sizeof(opt_day) - pos, (i == 31) ? "%02d" : "%02d\n", i);
    }
    pos = 0;
    for (int i = 0; i <= 23; i++) {
        pos += lv_snprintf(opt_h + pos, sizeof(opt_h) - pos, (i == 23) ? "%02d" : "%02d\n", i);
    }
    pos = 0;
    for (int i = 0; i <= 59; i++) {
        pos += lv_snprintf(opt_m + pos, sizeof(opt_m) - pos, (i == 59) ? "%02d" : "%02d\n", i);
    }
    memcpy(opt_s, opt_m, sizeof(opt_s));

    machine_time_get(&s_editing_time);

    lv_obj_t* card = settings_detail_create_card(content, 32, 110, 1028, 205);

    int base_x = 18;
    int base_y = 12;
    int rw = 150;
    int rh = 200;
    int gap = 20;

    r_year = create_roller(card, base_x + (rw + gap) * 0, base_y, rw, rh, opt_year, (int)(s_editing_time.year - 2000));
    r_mon = create_roller(card, base_x + (rw + gap) * 1, base_y, rw, rh, opt_mon, (int)(s_editing_time.month - 1));
    r_day = create_roller(card, base_x + (rw + gap) * 2, base_y, rw, rh, opt_day, (int)(s_editing_time.day - 1));
    r_hour = create_roller(card, base_x + (rw + gap) * 3, base_y, rw, rh, opt_h, (int)s_editing_time.hour);
    r_min = create_roller(card, base_x + (rw + gap) * 4, base_y, rw, rh, opt_m, (int)s_editing_time.minute);
    r_sec = create_roller(card, base_x + (rw + gap) * 5, base_y, rw, rh, opt_s, (int)s_editing_time.second);
}

void ui_page_11_timeset_destroy(void)
{
    if (timeset_page) {
        lv_obj_del(timeset_page);
        timeset_page = NULL;
        lbl_time = NULL;
        r_year = r_mon = r_day = r_hour = r_min = r_sec = NULL;
    }
    machine_time_pause(false);
}
