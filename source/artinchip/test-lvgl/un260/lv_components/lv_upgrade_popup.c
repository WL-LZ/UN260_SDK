#include "lv_upgrade_popup.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_core/ui_upgrade_service.h"
#include "un260/lv_components/lv_print_toast.h"
#include "un260/lv_system/ui_text.h"

#include "lvgl/lvgl.h"
#include <stdbool.h>
#include <string.h>

#define UPGRADE_POPUP_BG_COLOR            0xF5F5F7
#define UPGRADE_POPUP_CARD_COLOR          0xFFFFFF
#define UPGRADE_POPUP_TEXT_MAIN_COLOR     0x111111
#define UPGRADE_POPUP_TEXT_DESC_COLOR     0x466666
#define UPGRADE_POPUP_TAG_COLOR           0x000000
#define UPGRADE_POPUP_TEXT_LIGHT_COLOR    0x8AAAAA
#define UPGRADE_POPUP_BORDER_COLOR        0xEFEFF1
#define UPGRADE_POPUP_BTN_GHOST_COLOR     0xF3F4F6
#define UPGRADE_POPUP_BTN_GHOST_PRS_COLOR 0xEBEDF0
#define UPGRADE_POPUP_BTN_MAIN_COLOR      0x111111
#define UPGRADE_POPUP_BTN_MAIN_PRS_COLOR  0x222222
#define UPGRADE_POPUP_OK_COLOR            0x2D8A4E
#define UPGRADE_POPUP_OK_BG_COLOR         0xF3FAF5
#define UPGRADE_POPUP_FAIL_COLOR          0xC0392B
#define UPGRADE_POPUP_FAIL_BG_COLOR       0xFDF3F2
#define UPGRADE_POPUP_BAR_BG_COLOR        0xECECEC

#define UPGRADE_POPUP_CARD_RADIUS         28
#define UPGRADE_POPUP_BTN_RADIUS          10
#define UPGRADE_POPUP_STATUS_TIMER_MS     200
#define UPGRADE_POPUP_RESULT_DELAY_MS     400
#define UPGRADE_POPUP_REBOOT_DELAY_MS     1000
#define UPGRADE_POPUP_SHOW_TIME_MS        400
#define UPGRADE_POPUP_HIDE_TIME_MS        300

typedef enum {
    UPGRADE_POPUP_STATE_IDLE = 0,
    UPGRADE_POPUP_STATE_PROMPT,
    UPGRADE_POPUP_STATE_PROGRESS,
    UPGRADE_POPUP_STATE_SUCCESS,
    UPGRADE_POPUP_STATE_FAIL,
} upgrade_popup_state_t;

typedef enum {
    UPGRADE_POPUP_BTN_GHOST = 0,
    UPGRADE_POPUP_BTN_MAIN,
    UPGRADE_POPUP_BTN_SUCCESS,
} upgrade_popup_btn_style_t;

typedef struct {
    lv_obj_t* root;

    lv_obj_t* prompt_card;
    lv_obj_t* progress_card;
    lv_obj_t* success_card;
    lv_obj_t* fail_card;

    lv_obj_t* prompt_icon;
    lv_obj_t* prompt_title_group;
    lv_obj_t* prompt_content_group;
    lv_obj_t* prompt_tag;
    lv_obj_t* prompt_title;
    lv_obj_t* prompt_desc;
    lv_obj_t* prompt_btn_cancel;
    lv_obj_t* prompt_btn_confirm;

    lv_obj_t* progress_icon;
    lv_obj_t* progress_title_group;
    lv_obj_t* progress_tag;
    lv_obj_t* progress_content_group;
    lv_obj_t* progress_title;
    lv_obj_t* progress_subtitle;
    lv_obj_t* progress_bar;
    lv_obj_t* progress_percent;
    lv_obj_t* progress_step;

    lv_obj_t* success_icon_wrap;
    lv_obj_t* success_icon;
    lv_obj_t* success_title_group;
    lv_obj_t* success_tag;
    lv_obj_t* success_content_group;
    lv_obj_t* success_title;
    lv_obj_t* success_desc;
    lv_obj_t* success_btn_later;
    lv_obj_t* success_btn;

    lv_obj_t* fail_icon_wrap;
    lv_obj_t* fail_icon;
    lv_obj_t* fail_title_group;
    lv_obj_t* fail_content_group;
    lv_obj_t* fail_title;
    lv_obj_t* fail_desc;
    lv_obj_t* fail_btn;

    lv_timer_t* status_timer;
    lv_timer_t* result_timer;
    lv_timer_t* reboot_timer;

    upgrade_popup_state_t state;
    bool result_success;
    bool prompt_hiding;
    bool hide_to_prompt;
} upgrade_popup_ctx_t;

static upgrade_popup_ctx_t g_upgrade_popup;
static bool g_upgrade_popup_detect_latched = false;

static lv_style_t style_upgrade_popup_card;
static lv_style_t style_upgrade_popup_btn_ghost;
static lv_style_t style_upgrade_popup_btn_ghost_pressed;
static lv_style_t style_upgrade_popup_btn_main;
static lv_style_t style_upgrade_popup_btn_main_pressed;
static lv_style_t style_upgrade_popup_btn_success;
static lv_style_t style_upgrade_popup_btn_success_pressed;
static bool style_upgrade_popup_inited = false;

static void upgrade_popup_hide(void);
static void upgrade_popup_set_state(upgrade_popup_state_t state);
static void upgrade_popup_show_success(void);
static void upgrade_popup_show_fail(const char* desc_text);
static void upgrade_popup_status_timer_cb(lv_timer_t* timer);
static void upgrade_popup_prepare_result_popup(void);
static void upgrade_popup_prompt_hide_ready_cb(lv_anim_t* a);
static void upgrade_popup_result_hide_ready_cb(lv_anim_t* a);
static void upgrade_popup_refresh_text_internal(void);

static void upgrade_popup_anim_opa_cb(void* var, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t*)var, (lv_opa_t)v, 0);
}

static void upgrade_popup_anim_card_opa_cb(void* var, int32_t v)
{
    lv_obj_t* obj = (lv_obj_t*)var;
    lv_opa_t opa = (lv_opa_t)v;

    lv_obj_set_style_bg_opa(obj, opa, 0);
    lv_obj_set_style_border_opa(obj, opa, 0);
    lv_obj_set_style_shadow_opa(obj, (lv_opa_t)((v * LV_OPA_10) / LV_OPA_COVER), 0);
}

static void upgrade_popup_anim_translate_y_cb(void* var, int32_t v)
{
    lv_obj_set_style_translate_y((lv_obj_t*)var, (lv_coord_t)v, 0);
}

static void upgrade_popup_anim_translate_x_cb(void* var, int32_t v)
{
    lv_obj_set_style_translate_x((lv_obj_t*)var, (lv_coord_t)v, 0);
}

static void upgrade_popup_start_fade_in_up(lv_obj_t* obj, uint32_t delay)
{
    lv_anim_t a;

    if (obj == NULL || !lv_obj_is_valid(obj)) return;

    lv_anim_del(obj, upgrade_popup_anim_opa_cb);
    lv_anim_del(obj, upgrade_popup_anim_translate_y_cb);

    lv_obj_set_style_opa(obj, LV_OPA_0, 0);
    lv_obj_set_style_translate_y(obj, -12, 0);

    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, upgrade_popup_anim_opa_cb);
    lv_anim_set_values(&a, LV_OPA_0, LV_OPA_COVER);
    lv_anim_set_delay(&a, delay);
    lv_anim_set_time(&a, 180);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, upgrade_popup_anim_translate_y_cb);
    lv_anim_set_values(&a, -12, 0);
    lv_anim_set_delay(&a, delay);
    lv_anim_set_time(&a, 180);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

static void upgrade_popup_run_dialog_enter_anim(lv_obj_t* card)
{
    lv_anim_t a;

    if (card == NULL || !lv_obj_is_valid(card)) return;

    lv_anim_del(card, upgrade_popup_anim_translate_y_cb);

    lv_obj_set_style_translate_x(card, 0, 0);
    lv_obj_set_style_translate_y(card, -18, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_10, 0);

    lv_anim_init(&a);
    lv_anim_set_var(&a, card);
    lv_anim_set_exec_cb(&a, upgrade_popup_anim_translate_y_cb);
    lv_anim_set_values(&a, -18, 0);
    lv_anim_set_time(&a, 190);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

static void upgrade_popup_run_layer_enter_anim(lv_obj_t* icon_obj,
                                               lv_obj_t* title_obj,
                                               lv_obj_t* content_obj)
{
    upgrade_popup_start_fade_in_up(icon_obj, 30);
    upgrade_popup_start_fade_in_up(title_obj, 70);
    upgrade_popup_start_fade_in_up(content_obj, 110);
}

static void upgrade_popup_run_success_icon_anim(void)
{
    if (g_upgrade_popup.success_icon_wrap == NULL) return;

    lv_obj_set_style_transform_zoom(g_upgrade_popup.success_icon_wrap, 256, 0);
    lv_obj_set_style_opa(g_upgrade_popup.success_icon_wrap, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(g_upgrade_popup.success_icon, LV_OPA_COVER, 0);
}

static void upgrade_popup_run_fail_icon_anim(void)
{
    lv_anim_t a;

    if (g_upgrade_popup.fail_icon_wrap == NULL) return;

    lv_obj_set_style_opa(g_upgrade_popup.fail_icon_wrap, LV_OPA_0, 0);
    lv_obj_set_style_translate_x(g_upgrade_popup.fail_icon_wrap, 0, 0);

    lv_anim_init(&a);
    lv_anim_set_var(&a, g_upgrade_popup.fail_icon_wrap);
    lv_anim_set_exec_cb(&a, upgrade_popup_anim_opa_cb);
    lv_anim_set_values(&a, LV_OPA_0, LV_OPA_COVER);
    lv_anim_set_time(&a, 140);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, g_upgrade_popup.fail_icon_wrap);
    lv_anim_set_exec_cb(&a, upgrade_popup_anim_translate_x_cb);
    lv_anim_set_values(&a, 0, -5);
    lv_anim_set_time(&a, 70);
    lv_anim_set_playback_time(&a, 70);
    lv_anim_set_repeat_count(&a, 2);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

static void upgrade_popup_style_init(void)
{
    if (style_upgrade_popup_inited) return;

    lv_style_init(&style_upgrade_popup_card);
    lv_style_set_bg_color(&style_upgrade_popup_card, lv_color_hex(UPGRADE_POPUP_CARD_COLOR));
    lv_style_set_bg_opa(&style_upgrade_popup_card, LV_OPA_COVER);
    lv_style_set_radius(&style_upgrade_popup_card, UPGRADE_POPUP_CARD_RADIUS);
    lv_style_set_border_width(&style_upgrade_popup_card, 1);
    lv_style_set_border_color(&style_upgrade_popup_card, lv_color_hex(UPGRADE_POPUP_BORDER_COLOR));
    lv_style_set_shadow_color(&style_upgrade_popup_card, lv_color_black());
    lv_style_set_shadow_width(&style_upgrade_popup_card, 24);
    lv_style_set_shadow_opa(&style_upgrade_popup_card, LV_OPA_10);
    lv_style_set_shadow_ofs_y(&style_upgrade_popup_card, 8);

    lv_style_init(&style_upgrade_popup_btn_ghost);
    lv_style_set_bg_color(&style_upgrade_popup_btn_ghost, lv_color_hex(UPGRADE_POPUP_BTN_GHOST_COLOR));
    lv_style_set_bg_opa(&style_upgrade_popup_btn_ghost, LV_OPA_COVER);
    lv_style_set_radius(&style_upgrade_popup_btn_ghost, UPGRADE_POPUP_BTN_RADIUS);
    lv_style_set_border_width(&style_upgrade_popup_btn_ghost, 0);
    lv_style_set_text_color(&style_upgrade_popup_btn_ghost, lv_color_hex(UPGRADE_POPUP_TEXT_DESC_COLOR));

    lv_style_init(&style_upgrade_popup_btn_ghost_pressed);
    lv_style_set_bg_color(&style_upgrade_popup_btn_ghost_pressed, lv_color_hex(UPGRADE_POPUP_BTN_GHOST_PRS_COLOR));
    lv_style_set_bg_opa(&style_upgrade_popup_btn_ghost_pressed, LV_OPA_COVER);
    lv_style_set_transform_zoom(&style_upgrade_popup_btn_ghost_pressed, 246);

    lv_style_init(&style_upgrade_popup_btn_main);
    lv_style_set_bg_color(&style_upgrade_popup_btn_main, lv_color_hex(UPGRADE_POPUP_BTN_MAIN_COLOR));
    lv_style_set_bg_opa(&style_upgrade_popup_btn_main, LV_OPA_COVER);
    lv_style_set_radius(&style_upgrade_popup_btn_main, UPGRADE_POPUP_BTN_RADIUS);
    lv_style_set_border_width(&style_upgrade_popup_btn_main, 0);
    lv_style_set_text_color(&style_upgrade_popup_btn_main, lv_color_white());

    lv_style_init(&style_upgrade_popup_btn_main_pressed);
    lv_style_set_bg_color(&style_upgrade_popup_btn_main_pressed, lv_color_hex(UPGRADE_POPUP_BTN_MAIN_PRS_COLOR));
    lv_style_set_bg_opa(&style_upgrade_popup_btn_main_pressed, LV_OPA_COVER);
    lv_style_set_transform_zoom(&style_upgrade_popup_btn_main_pressed, 246);

    lv_style_init(&style_upgrade_popup_btn_success);
    lv_style_set_bg_color(&style_upgrade_popup_btn_success, lv_color_hex(UPGRADE_POPUP_OK_COLOR));
    lv_style_set_bg_opa(&style_upgrade_popup_btn_success, LV_OPA_COVER);
    lv_style_set_radius(&style_upgrade_popup_btn_success, UPGRADE_POPUP_BTN_RADIUS);
    lv_style_set_border_width(&style_upgrade_popup_btn_success, 0);
    lv_style_set_text_color(&style_upgrade_popup_btn_success, lv_color_white());

    lv_style_init(&style_upgrade_popup_btn_success_pressed);
    lv_style_set_bg_color(&style_upgrade_popup_btn_success_pressed, lv_color_hex(0x257A43));
    lv_style_set_bg_opa(&style_upgrade_popup_btn_success_pressed, LV_OPA_COVER);
    lv_style_set_transform_zoom(&style_upgrade_popup_btn_success_pressed, 246);

    style_upgrade_popup_inited = true;
}

static lv_obj_t* upgrade_popup_create_btn(lv_obj_t* parent,
                                          const char* text,
                                          lv_coord_t x,
                                          lv_coord_t y,
                                          lv_coord_t w,
                                          lv_coord_t h,
                                          upgrade_popup_btn_style_t btn_style,
                                          lv_event_cb_t event_cb)
{
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_pos(btn, x, y);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

    if (btn_style == UPGRADE_POPUP_BTN_GHOST) {
        lv_obj_add_style(btn, &style_upgrade_popup_btn_ghost, LV_STATE_DEFAULT);
        lv_obj_add_style(btn, &style_upgrade_popup_btn_ghost_pressed, LV_STATE_PRESSED);
    } else if (btn_style == UPGRADE_POPUP_BTN_SUCCESS) {
        lv_obj_add_style(btn, &style_upgrade_popup_btn_success, LV_STATE_DEFAULT);
        lv_obj_add_style(btn, &style_upgrade_popup_btn_success_pressed, LV_STATE_PRESSED);
    } else {
        lv_obj_add_style(btn, &style_upgrade_popup_btn_main, LV_STATE_DEFAULT);
        lv_obj_add_style(btn, &style_upgrade_popup_btn_main_pressed, LV_STATE_PRESSED);
    }

    if (event_cb) {
        lv_obj_add_event_cb(btn, event_cb, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_instrument_sans_medium_16, 0);
    lv_obj_center(label);

    return btn;
}

static void upgrade_popup_btn_set_text(lv_obj_t* btn, const char* text) //设置弹窗按钮文本
{
    lv_obj_t* label;

    if (btn == NULL || !lv_obj_is_valid(btn)) return;

    label = lv_obj_get_child(btn, 0);
    if (label == NULL || !lv_obj_is_valid(label)) return;

    lv_label_set_text(label, text ? text : "");
}

static lv_obj_t* upgrade_popup_create_card(lv_obj_t* parent, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_add_style(card, &style_upgrade_popup_card, 0);
    lv_obj_set_size(card, w, h);
    lv_obj_center(card);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_HIDDEN);
    return card;
}

static void upgrade_popup_apply_text_style(lv_obj_t* label,
                                           lv_color_t color,
                                           const lv_font_t* font)
{
    if (label == NULL) return;
    lv_obj_set_style_text_color(label, color, 0);
    if (font) {
        lv_obj_set_style_text_font(label, font, 0);
    }
}

static void upgrade_popup_on_cancel_click(lv_event_t* e)
{
    (void)e;
    upgrade_popup_hide();
}

static void upgrade_popup_on_confirm_click(lv_event_t* e)
{
    (void)e;

    if (g_upgrade_popup.prompt_btn_confirm) {
        lv_obj_add_state(g_upgrade_popup.prompt_btn_confirm, LV_STATE_DISABLED);
    }

    if (ui_upgrade_service_start() != UI_UPGRADE_START_OK) {
        upgrade_popup_show_fail(ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_FAIL_START_SCRIPT));
        return;
    }

    if (g_upgrade_popup.status_timer) {
        lv_timer_resume(g_upgrade_popup.status_timer);
    }

    upgrade_popup_set_state(UPGRADE_POPUP_STATE_PROGRESS);
}

static void upgrade_popup_reboot_timer_cb(lv_timer_t* timer)
{
    (void)timer;
    g_upgrade_popup.reboot_timer = NULL;
    ui_upgrade_service_reboot();
}

static void upgrade_popup_on_reboot_later_click(lv_event_t* e)
{
    (void)e;

    if (g_upgrade_popup.reboot_timer) {
        lv_timer_del(g_upgrade_popup.reboot_timer);
        g_upgrade_popup.reboot_timer = NULL;
    }

    upgrade_popup_hide();
}

static void upgrade_popup_on_reboot_click(lv_event_t* e)
{
    uint8_t clear_cmd = 0x01;

    (void)e;

    if (g_upgrade_popup.success_btn) {
        lv_obj_add_state(g_upgrade_popup.success_btn, LV_STATE_DISABLED);
    }

    (void)settings_detail_send_command(0x3B, &clear_cmd, 1);

    if (g_upgrade_popup.reboot_timer) {
        lv_timer_del(g_upgrade_popup.reboot_timer);
    }

    g_upgrade_popup.reboot_timer =
        lv_timer_create(upgrade_popup_reboot_timer_cb, UPGRADE_POPUP_REBOOT_DELAY_MS, NULL);
    if (g_upgrade_popup.reboot_timer) {
        lv_timer_set_repeat_count(g_upgrade_popup.reboot_timer, 1);
    }
}

static void upgrade_popup_on_retry_click(lv_event_t* e)
{
    ui_upgrade_detect_info_t detect_info;

    (void)e;

    ui_upgrade_service_reset();
    ui_upgrade_service_detect(&detect_info);

    g_upgrade_popup.hide_to_prompt =
        detect_info.package_found &&
        detect_info.package_hash_status == UI_UPGRADE_PACKAGE_HASH_DIFFERENT;
    upgrade_popup_hide();
}

static void upgrade_popup_create_prompt_card(void)
{
    lv_obj_t* icon_label;

    g_upgrade_popup.prompt_card = upgrade_popup_create_card(g_upgrade_popup.root, 560, 244);

    g_upgrade_popup.prompt_icon = lv_obj_create(g_upgrade_popup.prompt_card);
    lv_obj_remove_style_all(g_upgrade_popup.prompt_icon);
    lv_obj_set_size(g_upgrade_popup.prompt_icon, 74, 74);
    lv_obj_set_pos(g_upgrade_popup.prompt_icon, 34, 34);
    lv_obj_set_style_bg_color(g_upgrade_popup.prompt_icon, lv_color_hex(0xF3F4F6), 0);
    lv_obj_set_style_bg_opa(g_upgrade_popup.prompt_icon, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(g_upgrade_popup.prompt_icon, 22, 0);

    icon_label = lv_label_create(g_upgrade_popup.prompt_icon);
    lv_label_set_text(icon_label, LV_SYMBOL_UPLOAD);
    upgrade_popup_apply_text_style(icon_label,
                                   lv_color_hex(UPGRADE_POPUP_TEXT_MAIN_COLOR),
                                   &lv_font_montserrat_28);
    lv_obj_center(icon_label);

    g_upgrade_popup.prompt_title_group = lv_obj_create(g_upgrade_popup.prompt_card);
    lv_obj_remove_style_all(g_upgrade_popup.prompt_title_group);
    lv_obj_set_size(g_upgrade_popup.prompt_title_group, 390, 56);
    lv_obj_set_pos(g_upgrade_popup.prompt_title_group, 128, 38);

    g_upgrade_popup.prompt_tag = lv_label_create(g_upgrade_popup.prompt_title_group);
    lv_label_set_text(g_upgrade_popup.prompt_tag, ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_TAG));
    upgrade_popup_apply_text_style(g_upgrade_popup.prompt_tag,
                                   lv_color_hex(UPGRADE_POPUP_TAG_COLOR),
                                   &lv_font_instrument_sans_medium_10);
    lv_obj_set_pos(g_upgrade_popup.prompt_tag, 0, 0);

    g_upgrade_popup.prompt_title = lv_label_create(g_upgrade_popup.prompt_title_group);
    lv_label_set_text(g_upgrade_popup.prompt_title, ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_PROMPT_TITLE));
    upgrade_popup_apply_text_style(g_upgrade_popup.prompt_title,
                                   lv_color_hex(UPGRADE_POPUP_TEXT_MAIN_COLOR),
                                   &lv_font_instrument_sans_medium_18);
    lv_obj_set_pos(g_upgrade_popup.prompt_title, 0, 22);

    g_upgrade_popup.prompt_content_group = lv_obj_create(g_upgrade_popup.prompt_card);
    lv_obj_remove_style_all(g_upgrade_popup.prompt_content_group);
    lv_obj_set_size(g_upgrade_popup.prompt_content_group, 500, 132);
    lv_obj_set_pos(g_upgrade_popup.prompt_content_group, 28, 96);

    g_upgrade_popup.prompt_desc = lv_label_create(g_upgrade_popup.prompt_content_group);
    lv_obj_set_width(g_upgrade_popup.prompt_desc, 372);
    lv_label_set_long_mode(g_upgrade_popup.prompt_desc, LV_LABEL_LONG_WRAP);
    lv_label_set_text(g_upgrade_popup.prompt_desc,
                      ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_PROMPT_DESC));
    upgrade_popup_apply_text_style(g_upgrade_popup.prompt_desc,
                                   lv_color_hex(UPGRADE_POPUP_TEXT_DESC_COLOR),
                                   &lv_font_instrument_sans_medium_14);
    lv_obj_set_pos(g_upgrade_popup.prompt_desc, 100, 0);

    g_upgrade_popup.prompt_btn_cancel =
        upgrade_popup_create_btn(g_upgrade_popup.prompt_content_group,
                                 ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_PROMPT_BTN_CANCEL),
                                 216, 56, 116, 48,
                                 UPGRADE_POPUP_BTN_GHOST,
                                 upgrade_popup_on_cancel_click);
    g_upgrade_popup.prompt_btn_confirm =
        upgrade_popup_create_btn(g_upgrade_popup.prompt_content_group,
                                 ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_PROMPT_BTN_CONFIRM),
                                 348, 56, 136, 48,
                                 UPGRADE_POPUP_BTN_MAIN,
                                 upgrade_popup_on_confirm_click);
}

static void upgrade_popup_create_progress_card(void)
{
    lv_obj_t* icon_label;

    g_upgrade_popup.progress_card = upgrade_popup_create_card(g_upgrade_popup.root, 480, 248);

    g_upgrade_popup.progress_icon = lv_obj_create(g_upgrade_popup.progress_card);
    lv_obj_remove_style_all(g_upgrade_popup.progress_icon);
    lv_obj_set_size(g_upgrade_popup.progress_icon, 56, 56);
    lv_obj_set_pos(g_upgrade_popup.progress_icon, 28, 34);
    lv_obj_set_style_bg_color(g_upgrade_popup.progress_icon, lv_color_hex(0xF3F4F6), 0);
    lv_obj_set_style_bg_opa(g_upgrade_popup.progress_icon, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(g_upgrade_popup.progress_icon, 18, 0);

    icon_label = lv_label_create(g_upgrade_popup.progress_icon);
    lv_label_set_text(icon_label, LV_SYMBOL_UPLOAD);
    upgrade_popup_apply_text_style(icon_label,
                                   lv_color_hex(UPGRADE_POPUP_TEXT_MAIN_COLOR),
                                   &lv_font_montserrat_24);
    lv_obj_center(icon_label);

    g_upgrade_popup.progress_title_group = lv_obj_create(g_upgrade_popup.progress_card);
    lv_obj_remove_style_all(g_upgrade_popup.progress_title_group);
    lv_obj_set_size(g_upgrade_popup.progress_title_group, 300, 56);
    lv_obj_set_pos(g_upgrade_popup.progress_title_group, 102, 38);

    g_upgrade_popup.progress_tag = lv_label_create(g_upgrade_popup.progress_title_group);
    lv_label_set_text(g_upgrade_popup.progress_tag, ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_TAG));
    upgrade_popup_apply_text_style(g_upgrade_popup.progress_tag,
                                   lv_color_hex(UPGRADE_POPUP_TAG_COLOR),
                                   &lv_font_instrument_sans_medium_10);
    lv_obj_set_style_text_align(g_upgrade_popup.progress_tag, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_width(g_upgrade_popup.progress_tag, 300);
    lv_obj_set_pos(g_upgrade_popup.progress_tag, 0, 0);

    g_upgrade_popup.progress_title = lv_label_create(g_upgrade_popup.progress_title_group);
    lv_label_set_text(g_upgrade_popup.progress_title, ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_PROGRESS_TITLE));
    upgrade_popup_apply_text_style(g_upgrade_popup.progress_title,
                                   lv_color_hex(UPGRADE_POPUP_TEXT_MAIN_COLOR),
                                   &lv_font_instrument_sans_semibold_16);
    lv_obj_set_style_text_align(g_upgrade_popup.progress_title, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_width(g_upgrade_popup.progress_title, 300);
    lv_obj_set_pos(g_upgrade_popup.progress_title, 0, 18);

    g_upgrade_popup.progress_subtitle = lv_label_create(g_upgrade_popup.progress_card);
    lv_label_set_text(g_upgrade_popup.progress_subtitle, ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_PROGRESS_DESC));
    upgrade_popup_apply_text_style(g_upgrade_popup.progress_subtitle,
                                   lv_color_hex(UPGRADE_POPUP_TEXT_DESC_COLOR),
                                   &lv_font_instrument_sans_medium_12);
    lv_obj_set_style_text_align(g_upgrade_popup.progress_subtitle, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(g_upgrade_popup.progress_subtitle, 320);
    lv_label_set_long_mode(g_upgrade_popup.progress_subtitle, LV_LABEL_LONG_WRAP);
    lv_obj_align(g_upgrade_popup.progress_subtitle, LV_ALIGN_TOP_MID, 0, 114);

    g_upgrade_popup.progress_content_group = lv_obj_create(g_upgrade_popup.progress_card);
    lv_obj_remove_style_all(g_upgrade_popup.progress_content_group);
    lv_obj_set_size(g_upgrade_popup.progress_content_group, 400, 96);
    lv_obj_set_pos(g_upgrade_popup.progress_content_group, 40, 170);

    g_upgrade_popup.progress_bar = lv_bar_create(g_upgrade_popup.progress_content_group);
    lv_obj_set_size(g_upgrade_popup.progress_bar, 400, 5);
    lv_obj_set_pos(g_upgrade_popup.progress_bar, 0, 0);
    lv_bar_set_range(g_upgrade_popup.progress_bar, 0, 100);
    lv_bar_set_value(g_upgrade_popup.progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(g_upgrade_popup.progress_bar, lv_color_hex(UPGRADE_POPUP_BAR_BG_COLOR), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_upgrade_popup.progress_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_upgrade_popup.progress_bar, lv_color_hex(UPGRADE_POPUP_TEXT_MAIN_COLOR), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(g_upgrade_popup.progress_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(g_upgrade_popup.progress_bar, 8, LV_PART_MAIN);
    lv_obj_set_style_radius(g_upgrade_popup.progress_bar, 8, LV_PART_INDICATOR);

    g_upgrade_popup.progress_percent = lv_label_create(g_upgrade_popup.progress_content_group);
    lv_label_set_text(g_upgrade_popup.progress_percent, "0%");
    upgrade_popup_apply_text_style(g_upgrade_popup.progress_percent,
                                   lv_color_hex(UPGRADE_POPUP_TEXT_MAIN_COLOR),
                                   &lv_font_instrument_sans_medium_28);
    lv_obj_set_pos(g_upgrade_popup.progress_percent, 0, 22);

    g_upgrade_popup.progress_step = lv_label_create(g_upgrade_popup.progress_content_group);
    lv_obj_set_width(g_upgrade_popup.progress_step, 220);
    lv_obj_set_style_text_align(g_upgrade_popup.progress_step, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(g_upgrade_popup.progress_step,
                      ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_PROGRESS_STEP_VERIFY));
    upgrade_popup_apply_text_style(g_upgrade_popup.progress_step,
                                   lv_color_hex(UPGRADE_POPUP_TEXT_DESC_COLOR),
                                   &lv_font_instrument_sans_medium_12);
    lv_obj_align(g_upgrade_popup.progress_step, LV_ALIGN_TOP_RIGHT, 0, 36);
    lv_label_set_long_mode(g_upgrade_popup.progress_step, LV_LABEL_LONG_WRAP);
}

static void upgrade_popup_create_success_card(void)
{
    g_upgrade_popup.success_card = upgrade_popup_create_card(g_upgrade_popup.root, 440, 270);

    g_upgrade_popup.success_icon_wrap = lv_obj_create(g_upgrade_popup.success_card);
    lv_obj_remove_style_all(g_upgrade_popup.success_icon_wrap);
    lv_obj_set_size(g_upgrade_popup.success_icon_wrap, 56, 56);
    lv_obj_set_pos(g_upgrade_popup.success_icon_wrap, 28, 34);
    lv_obj_set_style_bg_color(g_upgrade_popup.success_icon_wrap, lv_color_hex(0xF3F4F6), 0);
    lv_obj_set_style_bg_opa(g_upgrade_popup.success_icon_wrap, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(g_upgrade_popup.success_icon_wrap, 18, 0);

    g_upgrade_popup.success_icon = lv_label_create(g_upgrade_popup.success_icon_wrap);
    lv_label_set_text(g_upgrade_popup.success_icon, LV_SYMBOL_UPLOAD);
    upgrade_popup_apply_text_style(g_upgrade_popup.success_icon,
                                   lv_color_hex(UPGRADE_POPUP_TEXT_MAIN_COLOR),
                                   &lv_font_montserrat_24);
    lv_obj_center(g_upgrade_popup.success_icon);
    lv_obj_move_foreground(g_upgrade_popup.success_icon);

    g_upgrade_popup.success_title_group = lv_obj_create(g_upgrade_popup.success_card);
    lv_obj_remove_style_all(g_upgrade_popup.success_title_group);
    lv_obj_set_size(g_upgrade_popup.success_title_group, 260, 56);
    lv_obj_set_pos(g_upgrade_popup.success_title_group, 102, 38);

    g_upgrade_popup.success_tag = lv_label_create(g_upgrade_popup.success_title_group);
    lv_label_set_text(g_upgrade_popup.success_tag, ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_TAG));
    upgrade_popup_apply_text_style(g_upgrade_popup.success_tag,
                                   lv_color_hex(UPGRADE_POPUP_TAG_COLOR),
                                   &lv_font_instrument_sans_medium_10);
    lv_obj_set_style_text_align(g_upgrade_popup.success_tag, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_width(g_upgrade_popup.success_tag, 260);
    lv_obj_set_pos(g_upgrade_popup.success_tag, 0, 0);

    g_upgrade_popup.success_title = lv_label_create(g_upgrade_popup.success_title_group);
    lv_label_set_text(g_upgrade_popup.success_title, ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_SUCCESS_TITLE));
    upgrade_popup_apply_text_style(g_upgrade_popup.success_title,
                                   lv_color_hex(UPGRADE_POPUP_TEXT_MAIN_COLOR),
                                   &lv_font_instrument_sans_medium_18);
    lv_obj_set_style_text_align(g_upgrade_popup.success_title, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_width(g_upgrade_popup.success_title, 260);
    lv_obj_set_pos(g_upgrade_popup.success_title, 0, 18);

    g_upgrade_popup.success_content_group = lv_obj_create(g_upgrade_popup.success_card);
    lv_obj_remove_style_all(g_upgrade_popup.success_content_group);
    lv_obj_set_size(g_upgrade_popup.success_content_group, 360, 116);
    lv_obj_set_pos(g_upgrade_popup.success_content_group, 40, 122);

    g_upgrade_popup.success_desc = lv_label_create(g_upgrade_popup.success_content_group);
    lv_obj_set_width(g_upgrade_popup.success_desc, 320);
    lv_obj_set_style_text_align(g_upgrade_popup.success_desc, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(g_upgrade_popup.success_desc,
                      ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_SUCCESS_DESC));
    upgrade_popup_apply_text_style(g_upgrade_popup.success_desc,
                                   lv_color_hex(UPGRADE_POPUP_TEXT_DESC_COLOR),
                                   &lv_font_instrument_sans_medium_14);
    lv_label_set_long_mode(g_upgrade_popup.success_desc, LV_LABEL_LONG_WRAP);
    lv_obj_align(g_upgrade_popup.success_desc, LV_ALIGN_TOP_MID, 0, 0);

    g_upgrade_popup.success_btn_later =
        upgrade_popup_create_btn(g_upgrade_popup.success_content_group,
                                 ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_SUCCESS_BTN_LATER),
                                 0, 56, 116, 50,
                                 UPGRADE_POPUP_BTN_GHOST,
                                 upgrade_popup_on_reboot_later_click);
    g_upgrade_popup.success_btn =
        upgrade_popup_create_btn(g_upgrade_popup.success_content_group,
                                 ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_SUCCESS_BTN_CONFIRM),
                                 132, 56, 228, 50,
                                 UPGRADE_POPUP_BTN_SUCCESS,
                                 upgrade_popup_on_reboot_click);
}

static void upgrade_popup_create_fail_card(void)
{
    g_upgrade_popup.fail_card = upgrade_popup_create_card(g_upgrade_popup.root, 440, 270);

    g_upgrade_popup.fail_icon_wrap = lv_obj_create(g_upgrade_popup.fail_card);
    lv_obj_remove_style_all(g_upgrade_popup.fail_icon_wrap);
    lv_obj_set_size(g_upgrade_popup.fail_icon_wrap, 72, 72);
    lv_obj_align(g_upgrade_popup.fail_icon_wrap, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_bg_color(g_upgrade_popup.fail_icon_wrap, lv_color_hex(UPGRADE_POPUP_FAIL_BG_COLOR), 0);
    lv_obj_set_style_bg_opa(g_upgrade_popup.fail_icon_wrap, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(g_upgrade_popup.fail_icon_wrap, LV_RADIUS_CIRCLE, 0);

    g_upgrade_popup.fail_icon = lv_label_create(g_upgrade_popup.fail_icon_wrap);
    lv_label_set_text(g_upgrade_popup.fail_icon, "!");
    upgrade_popup_apply_text_style(g_upgrade_popup.fail_icon,
                                   lv_color_hex(UPGRADE_POPUP_FAIL_COLOR),
                                   &lv_font_instrument_sans_medium_30);
    lv_obj_center(g_upgrade_popup.fail_icon);

    g_upgrade_popup.fail_title_group = lv_obj_create(g_upgrade_popup.fail_card);
    lv_obj_remove_style_all(g_upgrade_popup.fail_title_group);
    lv_obj_set_size(g_upgrade_popup.fail_title_group, 220, 32);
    lv_obj_align(g_upgrade_popup.fail_title_group, LV_ALIGN_TOP_MID, 0, 118);

    g_upgrade_popup.fail_title = lv_label_create(g_upgrade_popup.fail_title_group);
    lv_label_set_text(g_upgrade_popup.fail_title, ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_FAIL_TITLE));
    upgrade_popup_apply_text_style(g_upgrade_popup.fail_title,
                                   lv_color_hex(UPGRADE_POPUP_TEXT_MAIN_COLOR),
                                   &lv_font_instrument_sans_semibold_18);
    lv_obj_center(g_upgrade_popup.fail_title);

    g_upgrade_popup.fail_content_group = lv_obj_create(g_upgrade_popup.fail_card);
    lv_obj_remove_style_all(g_upgrade_popup.fail_content_group);
    lv_obj_set_size(g_upgrade_popup.fail_content_group, 360, 116);
    lv_obj_align(g_upgrade_popup.fail_content_group, LV_ALIGN_TOP_MID, 0, 152);

    g_upgrade_popup.fail_desc = lv_label_create(g_upgrade_popup.fail_content_group);
    lv_obj_set_width(g_upgrade_popup.fail_desc, 320);
    lv_obj_set_style_text_align(g_upgrade_popup.fail_desc, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(g_upgrade_popup.fail_desc,
                      ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_FAIL_DESC));
    upgrade_popup_apply_text_style(g_upgrade_popup.fail_desc,
                                   lv_color_hex(UPGRADE_POPUP_TEXT_DESC_COLOR),
                                   &lv_font_instrument_sans_medium_14);
    lv_label_set_long_mode(g_upgrade_popup.fail_desc, LV_LABEL_LONG_WRAP);
    lv_obj_align(g_upgrade_popup.fail_desc, LV_ALIGN_TOP_MID, 0, 0);

    g_upgrade_popup.fail_btn =
        upgrade_popup_create_btn(g_upgrade_popup.fail_content_group,
                                 ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_FAIL_BTN_RETRY),
                                 82, 56, 196, 50,
                                 UPGRADE_POPUP_BTN_MAIN,
                                 upgrade_popup_on_retry_click);
}

static void upgrade_popup_hide_all_cards(void)
{
    if (g_upgrade_popup.prompt_card) lv_obj_add_flag(g_upgrade_popup.prompt_card, LV_OBJ_FLAG_HIDDEN);
    if (g_upgrade_popup.progress_card) lv_obj_add_flag(g_upgrade_popup.progress_card, LV_OBJ_FLAG_HIDDEN);
    if (g_upgrade_popup.success_card) lv_obj_add_flag(g_upgrade_popup.success_card, LV_OBJ_FLAG_HIDDEN);
    if (g_upgrade_popup.fail_card) lv_obj_add_flag(g_upgrade_popup.fail_card, LV_OBJ_FLAG_HIDDEN);
}

static void upgrade_popup_reset_card_pos(lv_obj_t* card)
{
    if (card == NULL || !lv_obj_is_valid(card)) return;

    lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_translate_x(card, 0, 0);
    lv_obj_set_style_translate_y(card, 0, 0);
}

static void upgrade_popup_run_state_anim(upgrade_popup_state_t state)
{
    switch (state) {
    case UPGRADE_POPUP_STATE_PROMPT:
        break;

    case UPGRADE_POPUP_STATE_PROGRESS:
        upgrade_popup_run_dialog_enter_anim(g_upgrade_popup.progress_card);
        upgrade_popup_run_layer_enter_anim(g_upgrade_popup.progress_icon,
                                           g_upgrade_popup.progress_title_group,
                                           g_upgrade_popup.progress_content_group);
        break;

    case UPGRADE_POPUP_STATE_SUCCESS:
        upgrade_popup_run_dialog_enter_anim(g_upgrade_popup.success_card);
        upgrade_popup_start_fade_in_up(g_upgrade_popup.success_title_group, 80);
        upgrade_popup_start_fade_in_up(g_upgrade_popup.success_content_group, 130);
        upgrade_popup_run_success_icon_anim();
        break;

    case UPGRADE_POPUP_STATE_FAIL:
        upgrade_popup_run_dialog_enter_anim(g_upgrade_popup.fail_card);
        upgrade_popup_start_fade_in_up(g_upgrade_popup.fail_title_group, 80);
        upgrade_popup_start_fade_in_up(g_upgrade_popup.fail_content_group, 130);
        upgrade_popup_run_fail_icon_anim();
        break;

    default:
        break;
    }
}

static void upgrade_popup_set_state(upgrade_popup_state_t state)
{
    g_upgrade_popup.state = state;
    upgrade_popup_hide_all_cards();

    switch (state) {
    case UPGRADE_POPUP_STATE_PROMPT:
        upgrade_popup_reset_card_pos(g_upgrade_popup.prompt_card);
        lv_obj_clear_flag(g_upgrade_popup.prompt_card, LV_OBJ_FLAG_HIDDEN);
        upgrade_popup_run_state_anim(state);
        break;

    case UPGRADE_POPUP_STATE_PROGRESS:
        lv_obj_set_style_bg_color(g_upgrade_popup.progress_bar,
                                  lv_color_hex(UPGRADE_POPUP_TEXT_MAIN_COLOR),
                                  LV_PART_INDICATOR);
        lv_bar_set_value(g_upgrade_popup.progress_bar, 0, LV_ANIM_OFF);
        lv_label_set_text(g_upgrade_popup.progress_percent, "0%");
        lv_label_set_text(g_upgrade_popup.progress_step,
                          ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_PROGRESS_STEP_VERIFY));
        upgrade_popup_reset_card_pos(g_upgrade_popup.progress_card);
        lv_obj_clear_flag(g_upgrade_popup.progress_card, LV_OBJ_FLAG_HIDDEN);
        upgrade_popup_run_state_anim(state);
        break;

    case UPGRADE_POPUP_STATE_SUCCESS:
        upgrade_popup_reset_card_pos(g_upgrade_popup.success_card);
        lv_obj_clear_flag(g_upgrade_popup.success_card, LV_OBJ_FLAG_HIDDEN);
        upgrade_popup_run_state_anim(state);
        break;

    case UPGRADE_POPUP_STATE_FAIL:
        upgrade_popup_reset_card_pos(g_upgrade_popup.fail_card);
        lv_obj_clear_flag(g_upgrade_popup.fail_card, LV_OBJ_FLAG_HIDDEN);
        upgrade_popup_run_state_anim(state);
        break;

    default:
        break;
    }
}

static void upgrade_popup_build(void)
{
    if (g_upgrade_popup.root) return;

    upgrade_popup_style_init();
    memset(&g_upgrade_popup, 0, sizeof(g_upgrade_popup));

    g_upgrade_popup.root = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(g_upgrade_popup.root);
    lv_obj_set_pos(g_upgrade_popup.root, 0, 0);
    lv_obj_set_size(g_upgrade_popup.root, 1280, 400);
    lv_obj_clear_flag(g_upgrade_popup.root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(g_upgrade_popup.root);

    upgrade_popup_create_prompt_card();
    upgrade_popup_create_progress_card();
    upgrade_popup_create_success_card();
    upgrade_popup_create_fail_card();
    upgrade_popup_refresh_text_internal();

    g_upgrade_popup.status_timer =
        lv_timer_create(upgrade_popup_status_timer_cb, UPGRADE_POPUP_STATUS_TIMER_MS, NULL);
    if (g_upgrade_popup.status_timer) {
        lv_timer_pause(g_upgrade_popup.status_timer);
    }
}

static void upgrade_popup_destroy(void)
{
    if (g_upgrade_popup.status_timer) {
        lv_timer_del(g_upgrade_popup.status_timer);
    }
    if (g_upgrade_popup.result_timer) {
        lv_timer_del(g_upgrade_popup.result_timer);
    }
    if (g_upgrade_popup.reboot_timer) {
        lv_timer_del(g_upgrade_popup.reboot_timer);
    }
    if (g_upgrade_popup.root && lv_obj_is_valid(g_upgrade_popup.root)) {
        lv_obj_del(g_upgrade_popup.root);
    }

    memset(&g_upgrade_popup, 0, sizeof(g_upgrade_popup));
}

static void upgrade_popup_show_prompt(void)
{
    lv_anim_t a;

    if (!g_upgrade_popup.root) {
        upgrade_popup_build();
    }

    ui_upgrade_service_reset();
    if (g_upgrade_popup.status_timer) {
        lv_timer_pause(g_upgrade_popup.status_timer);
    }
    g_upgrade_popup.prompt_hiding = false;
    g_upgrade_popup.hide_to_prompt = false;
    upgrade_popup_set_state(UPGRADE_POPUP_STATE_PROMPT);

    lv_anim_del(g_upgrade_popup.prompt_card, upgrade_popup_anim_card_opa_cb);
    lv_anim_del(g_upgrade_popup.prompt_card, upgrade_popup_anim_translate_y_cb);
    lv_anim_del(g_upgrade_popup.prompt_icon, upgrade_popup_anim_opa_cb);
    lv_anim_del(g_upgrade_popup.prompt_icon, upgrade_popup_anim_translate_y_cb);
    lv_anim_del(g_upgrade_popup.prompt_title_group, upgrade_popup_anim_opa_cb);
    lv_anim_del(g_upgrade_popup.prompt_title_group, upgrade_popup_anim_translate_y_cb);
    lv_anim_del(g_upgrade_popup.prompt_content_group, upgrade_popup_anim_opa_cb);
    lv_anim_del(g_upgrade_popup.prompt_content_group, upgrade_popup_anim_translate_y_cb);

    lv_obj_set_style_bg_opa(g_upgrade_popup.prompt_card, LV_OPA_0, 0);
    lv_obj_set_style_border_opa(g_upgrade_popup.prompt_card, LV_OPA_0, 0);
    lv_obj_set_style_shadow_opa(g_upgrade_popup.prompt_card, LV_OPA_0, 0);
    lv_obj_set_style_translate_y(g_upgrade_popup.prompt_card, -10, 0);
    lv_obj_set_style_opa(g_upgrade_popup.prompt_icon, LV_OPA_0, 0);
    lv_obj_set_style_translate_y(g_upgrade_popup.prompt_icon, -10, 0);
    lv_obj_set_style_opa(g_upgrade_popup.prompt_title_group, LV_OPA_0, 0);
    lv_obj_set_style_translate_y(g_upgrade_popup.prompt_title_group, -10, 0);
    lv_obj_set_style_opa(g_upgrade_popup.prompt_content_group, LV_OPA_0, 0);
    lv_obj_set_style_translate_y(g_upgrade_popup.prompt_content_group, -10, 0);

    lv_anim_init(&a);
    lv_anim_set_var(&a, g_upgrade_popup.prompt_card);
    lv_anim_set_exec_cb(&a, upgrade_popup_anim_card_opa_cb);
    lv_anim_set_values(&a, LV_OPA_0, LV_OPA_COVER);
    lv_anim_set_time(&a, UPGRADE_POPUP_SHOW_TIME_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, g_upgrade_popup.prompt_card);
    lv_anim_set_exec_cb(&a, upgrade_popup_anim_translate_y_cb);
    lv_anim_set_values(&a, -10, 0);
    lv_anim_set_time(&a, UPGRADE_POPUP_SHOW_TIME_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, g_upgrade_popup.prompt_icon);
    lv_anim_set_exec_cb(&a, upgrade_popup_anim_opa_cb);
    lv_anim_set_values(&a, LV_OPA_0, LV_OPA_COVER);
    lv_anim_set_time(&a, UPGRADE_POPUP_SHOW_TIME_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, g_upgrade_popup.prompt_icon);
    lv_anim_set_exec_cb(&a, upgrade_popup_anim_translate_y_cb);
    lv_anim_set_values(&a, -10, 0);
    lv_anim_set_time(&a, UPGRADE_POPUP_SHOW_TIME_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, g_upgrade_popup.prompt_title_group);
    lv_anim_set_exec_cb(&a, upgrade_popup_anim_opa_cb);
    lv_anim_set_values(&a, LV_OPA_0, LV_OPA_COVER);
    lv_anim_set_time(&a, UPGRADE_POPUP_SHOW_TIME_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, g_upgrade_popup.prompt_title_group);
    lv_anim_set_exec_cb(&a, upgrade_popup_anim_translate_y_cb);
    lv_anim_set_values(&a, -10, 0);
    lv_anim_set_time(&a, UPGRADE_POPUP_SHOW_TIME_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, g_upgrade_popup.prompt_content_group);
    lv_anim_set_exec_cb(&a, upgrade_popup_anim_opa_cb);
    lv_anim_set_values(&a, LV_OPA_0, LV_OPA_COVER);
    lv_anim_set_time(&a, UPGRADE_POPUP_SHOW_TIME_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, g_upgrade_popup.prompt_content_group);
    lv_anim_set_exec_cb(&a, upgrade_popup_anim_translate_y_cb);
    lv_anim_set_values(&a, -10, 0);
    lv_anim_set_time(&a, UPGRADE_POPUP_SHOW_TIME_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

static void upgrade_popup_refresh_text_internal(void) //刷新升级弹窗当前语言文本
{
    if (g_upgrade_popup.prompt_tag) {
        lv_label_set_text(g_upgrade_popup.prompt_tag, ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_TAG));
    }
    if (g_upgrade_popup.prompt_title) {
        lv_label_set_text(g_upgrade_popup.prompt_title, ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_PROMPT_TITLE));
    }
    if (g_upgrade_popup.prompt_desc) {
        lv_label_set_text(g_upgrade_popup.prompt_desc, ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_PROMPT_DESC));
    }
    if (g_upgrade_popup.prompt_btn_cancel) {
        upgrade_popup_btn_set_text(g_upgrade_popup.prompt_btn_cancel,
                                   ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_PROMPT_BTN_CANCEL));
    }
    if (g_upgrade_popup.prompt_btn_confirm) {
        upgrade_popup_btn_set_text(g_upgrade_popup.prompt_btn_confirm,
                                   ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_PROMPT_BTN_CONFIRM));
    }

    if (g_upgrade_popup.progress_tag) {
        lv_label_set_text(g_upgrade_popup.progress_tag, ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_TAG));
    }
    if (g_upgrade_popup.progress_title) {
        lv_label_set_text(g_upgrade_popup.progress_title, ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_PROGRESS_TITLE));
    }
    if (g_upgrade_popup.progress_subtitle) {
        lv_label_set_text(g_upgrade_popup.progress_subtitle, ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_PROGRESS_DESC));
    }

    if (g_upgrade_popup.success_tag) {
        lv_label_set_text(g_upgrade_popup.success_tag, ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_TAG));
    }
    if (g_upgrade_popup.success_title) {
        lv_label_set_text(g_upgrade_popup.success_title, ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_SUCCESS_TITLE));
    }
    if (g_upgrade_popup.success_desc) {
        lv_label_set_text(g_upgrade_popup.success_desc, ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_SUCCESS_DESC));
    }
    if (g_upgrade_popup.success_btn_later) {
        upgrade_popup_btn_set_text(g_upgrade_popup.success_btn_later,
                                   ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_SUCCESS_BTN_LATER));
    }
    if (g_upgrade_popup.success_btn) {
        upgrade_popup_btn_set_text(g_upgrade_popup.success_btn,
                                   ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_SUCCESS_BTN_CONFIRM));
    }

    if (g_upgrade_popup.fail_title) {
        lv_label_set_text(g_upgrade_popup.fail_title, ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_FAIL_TITLE));
    }
    if (g_upgrade_popup.fail_desc) {
        lv_label_set_text(g_upgrade_popup.fail_desc, ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_FAIL_DESC));
    }
    if (g_upgrade_popup.fail_btn) {
        upgrade_popup_btn_set_text(g_upgrade_popup.fail_btn,
                                   ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_FAIL_BTN_RETRY));
    }
}

static void upgrade_popup_prepare_result_popup(void)
{
    if (!g_upgrade_popup.root) {
        upgrade_popup_build();
    }

    if (g_upgrade_popup.status_timer) {
        lv_timer_pause(g_upgrade_popup.status_timer);
    }
    if (g_upgrade_popup.result_timer) {
        lv_timer_del(g_upgrade_popup.result_timer);
        g_upgrade_popup.result_timer = NULL;
    }
    if (g_upgrade_popup.reboot_timer) {
        lv_timer_del(g_upgrade_popup.reboot_timer);
        g_upgrade_popup.reboot_timer = NULL;
    }

    if (g_upgrade_popup.success_btn) {
        lv_obj_clear_state(g_upgrade_popup.success_btn, LV_STATE_DISABLED);
    }
    if (g_upgrade_popup.success_btn_later) {
        lv_obj_clear_state(g_upgrade_popup.success_btn_later, LV_STATE_DISABLED);
    }
    if (g_upgrade_popup.fail_btn) {
        lv_obj_clear_state(g_upgrade_popup.fail_btn, LV_STATE_DISABLED);
    }
}

static void upgrade_popup_hide(void)
{
    lv_anim_t a;
    lv_obj_t* card = NULL;
    lv_obj_t* icon_obj = NULL;
    lv_obj_t* title_obj = NULL;
    lv_obj_t* content_obj = NULL;

    if (!g_upgrade_popup.root) return;
    if (g_upgrade_popup.state == UPGRADE_POPUP_STATE_PROGRESS) return;
    if (g_upgrade_popup.prompt_hiding) return;

    ui_upgrade_service_reset();
    g_upgrade_popup.prompt_hiding = true;

    if (g_upgrade_popup.state == UPGRADE_POPUP_STATE_PROMPT) {
        card = g_upgrade_popup.prompt_card;
        icon_obj = g_upgrade_popup.prompt_icon;
        title_obj = g_upgrade_popup.prompt_title_group;
        content_obj = g_upgrade_popup.prompt_content_group;
    } else if (g_upgrade_popup.state == UPGRADE_POPUP_STATE_SUCCESS) {
        card = g_upgrade_popup.success_card;
        icon_obj = g_upgrade_popup.success_icon_wrap;
        title_obj = g_upgrade_popup.success_title_group;
        content_obj = g_upgrade_popup.success_content_group;
    } else if (g_upgrade_popup.state == UPGRADE_POPUP_STATE_FAIL) {
        card = g_upgrade_popup.fail_card;
        icon_obj = g_upgrade_popup.fail_icon_wrap;
        title_obj = g_upgrade_popup.fail_title_group;
        content_obj = g_upgrade_popup.fail_content_group;
    }

    if (card == NULL || icon_obj == NULL || title_obj == NULL || content_obj == NULL) {
        upgrade_popup_destroy();
        return;
    }

    lv_anim_del(card, upgrade_popup_anim_card_opa_cb);
    lv_anim_del(card, upgrade_popup_anim_translate_y_cb);
    lv_anim_del(icon_obj, upgrade_popup_anim_opa_cb);
    lv_anim_del(icon_obj, upgrade_popup_anim_translate_y_cb);
    lv_anim_del(title_obj, upgrade_popup_anim_opa_cb);
    lv_anim_del(title_obj, upgrade_popup_anim_translate_y_cb);
    lv_anim_del(content_obj, upgrade_popup_anim_opa_cb);
    lv_anim_del(content_obj, upgrade_popup_anim_translate_y_cb);

    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_10, 0);
    lv_obj_set_style_translate_y(card, 0, 0);
    lv_obj_set_style_opa(icon_obj, LV_OPA_COVER, 0);
    lv_obj_set_style_translate_y(icon_obj, 0, 0);
    lv_obj_set_style_opa(title_obj, LV_OPA_COVER, 0);
    lv_obj_set_style_translate_y(title_obj, 0, 0);
    lv_obj_set_style_opa(content_obj, LV_OPA_COVER, 0);
    lv_obj_set_style_translate_y(content_obj, 0, 0);

    lv_anim_init(&a);
    lv_anim_set_var(&a, card);
    lv_anim_set_exec_cb(&a, upgrade_popup_anim_card_opa_cb);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_0);
    lv_anim_set_time(&a, UPGRADE_POPUP_HIDE_TIME_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_ready_cb(&a, g_upgrade_popup.state == UPGRADE_POPUP_STATE_PROMPT ?
                         upgrade_popup_prompt_hide_ready_cb :
                         upgrade_popup_result_hide_ready_cb);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, card);
    lv_anim_set_exec_cb(&a, upgrade_popup_anim_translate_y_cb);
    lv_anim_set_values(&a, 0, -10);
    lv_anim_set_time(&a, UPGRADE_POPUP_HIDE_TIME_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, icon_obj);
    lv_anim_set_exec_cb(&a, upgrade_popup_anim_opa_cb);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_0);
    lv_anim_set_time(&a, UPGRADE_POPUP_HIDE_TIME_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, icon_obj);
    lv_anim_set_exec_cb(&a, upgrade_popup_anim_translate_y_cb);
    lv_anim_set_values(&a, 0, -10);
    lv_anim_set_time(&a, UPGRADE_POPUP_HIDE_TIME_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, title_obj);
    lv_anim_set_exec_cb(&a, upgrade_popup_anim_opa_cb);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_0);
    lv_anim_set_time(&a, UPGRADE_POPUP_HIDE_TIME_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, title_obj);
    lv_anim_set_exec_cb(&a, upgrade_popup_anim_translate_y_cb);
    lv_anim_set_values(&a, 0, -10);
    lv_anim_set_time(&a, UPGRADE_POPUP_HIDE_TIME_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, content_obj);
    lv_anim_set_exec_cb(&a, upgrade_popup_anim_opa_cb);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_0);
    lv_anim_set_time(&a, UPGRADE_POPUP_HIDE_TIME_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, content_obj);
    lv_anim_set_exec_cb(&a, upgrade_popup_anim_translate_y_cb);
    lv_anim_set_values(&a, 0, -10);
    lv_anim_set_time(&a, UPGRADE_POPUP_HIDE_TIME_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

static void upgrade_popup_prompt_hide_ready_cb(lv_anim_t* a)
{
    LV_UNUSED(a);

    upgrade_popup_destroy();
}

static void upgrade_popup_result_hide_ready_cb(lv_anim_t* a)
{
    bool hide_to_prompt;

    LV_UNUSED(a);

    hide_to_prompt = g_upgrade_popup.hide_to_prompt;
    upgrade_popup_destroy();

    if (hide_to_prompt) {
        upgrade_popup_show_prompt();
    }
}

static void upgrade_popup_result_timer_cb(lv_timer_t* timer)
{
    (void)timer;
    g_upgrade_popup.result_timer = NULL;

    if (g_upgrade_popup.result_success) {
        upgrade_popup_show_success();
    } else {
        upgrade_popup_show_fail(ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_FAIL_DESC));
    }
}

static void upgrade_popup_status_timer_cb(lv_timer_t* timer)
{
    ui_upgrade_service_status_t status;
    lv_color_t bar_color = lv_color_hex(UPGRADE_POPUP_TEXT_MAIN_COLOR);

    (void)timer;

    ui_upgrade_service_poll(&status);

    if (status.stage == UI_UPGRADE_STAGE_SUCCESS) {
        bar_color = lv_color_hex(UPGRADE_POPUP_OK_COLOR);
    } else if (status.stage == UI_UPGRADE_STAGE_FAIL) {
        bar_color = lv_color_hex(UPGRADE_POPUP_FAIL_COLOR);
    }

    lv_obj_set_style_bg_color(g_upgrade_popup.progress_bar, bar_color, LV_PART_INDICATOR);
    lv_bar_set_value(g_upgrade_popup.progress_bar, status.progress, LV_ANIM_ON);
    lv_label_set_text_fmt(g_upgrade_popup.progress_percent, "%d%%", status.progress);
    if (status.step_text[0] != '\0') {
        lv_label_set_text(g_upgrade_popup.progress_step, status.step_text);
    }

    if (status.finished && g_upgrade_popup.result_timer == NULL) {
        g_upgrade_popup.result_success = status.success;
        g_upgrade_popup.result_timer =
            lv_timer_create(upgrade_popup_result_timer_cb, UPGRADE_POPUP_RESULT_DELAY_MS, NULL);
        if (g_upgrade_popup.result_timer) {
            lv_timer_set_repeat_count(g_upgrade_popup.result_timer, 1);
        }
    }
}

static void upgrade_popup_show_success(void)
{
    if (g_upgrade_popup.status_timer) {
        lv_timer_pause(g_upgrade_popup.status_timer);
    }
    if (g_upgrade_popup.result_timer) {
        lv_timer_del(g_upgrade_popup.result_timer);
        g_upgrade_popup.result_timer = NULL;
    }

    lv_obj_set_style_bg_color(g_upgrade_popup.progress_bar,
                              lv_color_hex(UPGRADE_POPUP_OK_COLOR),
                              LV_PART_INDICATOR);
    lv_bar_set_value(g_upgrade_popup.progress_bar, 100, LV_ANIM_ON);
    upgrade_popup_set_state(UPGRADE_POPUP_STATE_SUCCESS);
}

static void upgrade_popup_show_fail(const char* desc_text)
{
    if (g_upgrade_popup.status_timer) {
        lv_timer_pause(g_upgrade_popup.status_timer);
    }
    if (g_upgrade_popup.result_timer) {
        lv_timer_del(g_upgrade_popup.result_timer);
        g_upgrade_popup.result_timer = NULL;
    }

    if (g_upgrade_popup.state == UPGRADE_POPUP_STATE_PROGRESS) {
        lv_obj_set_style_bg_color(g_upgrade_popup.progress_bar,
                                  lv_color_hex(UPGRADE_POPUP_FAIL_COLOR),
                                  LV_PART_INDICATOR);
        lv_bar_set_value(g_upgrade_popup.progress_bar, 12, LV_ANIM_ON);
        lv_label_set_text(g_upgrade_popup.progress_step,
                          ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_PROGRESS_STEP_VERIFY_FAIL));
        lv_label_set_text(g_upgrade_popup.progress_percent, "12%");
    }

    if (g_upgrade_popup.fail_desc) {
        lv_label_set_text(g_upgrade_popup.fail_desc,
                          desc_text ? desc_text : ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_FAIL_DESC));
    }

    upgrade_popup_set_state(UPGRADE_POPUP_STATE_FAIL);
}

void lv_upgrade_popup_process_detect(const ui_upgrade_detect_info_t* detect_info)
{
    if (detect_info == NULL) return;

    if (!detect_info->usb_present) {
        g_upgrade_popup_detect_latched = false;

        if (g_upgrade_popup.state == UPGRADE_POPUP_STATE_PROMPT) {
            upgrade_popup_hide();
        }
        return;
    }

    if (detect_info->package_found) {
        if (detect_info->package_hash_status == UI_UPGRADE_PACKAGE_HASH_MATCH) {
            if (!g_upgrade_popup_detect_latched &&
                (g_upgrade_popup.state == UPGRADE_POPUP_STATE_IDLE || g_upgrade_popup.root == NULL)) {
                static char toast_text[64];
                lv_print_toast_config_t toast_cfg = lv_print_toast_get_default_config();

                lv_snprintf(toast_text, sizeof(toast_text),
                            ui_text_get(UI_TEXT_PAGE16_USB_STATUS_FMT),
                            ui_text_get(UI_TEXT_PAGE16_USB_INSERTED));

                toast_cfg.w = 320;
                toast_cfg.h = 101;
                toast_cfg.text = toast_text;
                toast_cfg.show_loader = true;
                toast_cfg.align_center = true;
                toast_cfg.use_text_area = false;
                toast_cfg.auto_hide_ms = 2000;
                lv_print_toast_show_with_config(&toast_cfg);
            }

            if (g_upgrade_popup.state == UPGRADE_POPUP_STATE_PROMPT) {
                upgrade_popup_hide();
            }
            g_upgrade_popup_detect_latched = true;
            return;
        }

        if (detect_info->package_hash_status == UI_UPGRADE_PACKAGE_HASH_DIFFERENT) {
            if (!g_upgrade_popup_detect_latched &&
                (g_upgrade_popup.state == UPGRADE_POPUP_STATE_IDLE || g_upgrade_popup.root == NULL)) {
                upgrade_popup_show_prompt();
            }
            g_upgrade_popup_detect_latched = true;
            return;
        }

        g_upgrade_popup_detect_latched = false;
        if (g_upgrade_popup.state == UPGRADE_POPUP_STATE_PROMPT) {
            upgrade_popup_hide();
        }
        return;
    }

    g_upgrade_popup_detect_latched = false;
    if (g_upgrade_popup.state == UPGRADE_POPUP_STATE_PROMPT) {
        upgrade_popup_hide();
    }
}

bool lv_upgrade_popup_is_showing(void)
{
    return g_upgrade_popup.root != NULL;
}

void lv_upgrade_popup_show_result(bool success, const char* desc_text)
{
    upgrade_popup_prepare_result_popup();

    if (success) {
        if (g_upgrade_popup.success_desc) {
            lv_label_set_text(g_upgrade_popup.success_desc,
                              (desc_text && desc_text[0] != '\0') ?
                              desc_text :
                              ui_text_get(UI_TEXT_WIDGET_UPGRADE_POPUP_SUCCESS_DESC));
        }
        upgrade_popup_show_success();
        return;
    }

    upgrade_popup_show_fail(desc_text);
}

void lv_upgrade_popup_refresh_text(void) //刷新升级弹窗语言文本
{
    if (g_upgrade_popup.root == NULL) return;

    upgrade_popup_refresh_text_internal();
}
