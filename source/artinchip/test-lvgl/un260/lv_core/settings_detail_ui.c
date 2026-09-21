#include "un260/lv_resources/ui_page_background.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/protocol/protocol_send.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_components/lv_damped_button.h"
#include "un260/lv_components/lv_nav_button.h"
#include "un260/lv_components/lv_popup_style.h"
#include "un260/lv_resources/lv_img_init.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/app_service/app_command_runtime.h"
#include "un260/app_service/work_mode_service.h"

#include <string.h>

static void prepare_manual(void *unused)
{
    (void)unused;work_mode_service_retry();
}
static void action_explain(const char *reason)
{
    work_mode_snapshot_t mode;work_mode_service_get_snapshot(&mode);
    if(mode.phase==WORK_MODE_FAILED&&!strcmp(reason,work_mode_service_status_text())){
        settings_detail_dialog_show_ex(SETTINGS_DIALOG_WARNING,"Manual mode not confirmed",
            "Live sensor readings are independent of work mode. Prepare Manual mode before running or calibrating the machine.",
            "Prepare Manual","Cancel",prepare_manual,NULL,NULL);
        return;
    }
    settings_detail_dialog_show_ex(SETTINGS_DIALOG_INFO,"Please note",reason,"OK",NULL,NULL,NULL,NULL);
}
void settings_detail_action_block(lv_obj_t *button,const char *reason)
{
    lv_settings_action_block(button,reason,action_explain);
}

static void settings_run_clicked(lv_event_t *e)
{
    (void)e;
    const char *reason=app_command_runtime_diagnostic_run_blocker();
    if(!reason&&app_command_runtime_request_diagnostic_run())return;
    action_explain(reason?reason:"The command could not be sent. Please try again.");
}
void settings_detail_add_run(lv_obj_t *page)
{
    lv_obj_t *actions=lv_settings_actions(page);
    lv_settings_button(actions?actions:page,actions?0:1038,actions?0:21,110,46,"RUN",true,settings_run_clicked,NULL);
}

lv_color_t settings_theme_color_hex(uint32_t color)
{
    switch (color) {
    /* Legacy accents; actionable controls use lv_settings_action_style. */
    case 0x08C5D6: return lv_color_hex(0x1559B7);
    case 0x0878C8: return lv_color_hex(0x1F6FE5);
    case 0x075E9C:
    case 0x0466AD: return lv_color_hex(0x185BC2);
    case 0x0EA5E9:
    case 0x38BDF8: return lv_color_hex(0x62A4FF);
    case 0x11BFE0:
    case 0x12C8DD: return lv_color_hex(0x4795FF);
    case 0x62E6FF: return lv_color_hex(0x94C1FF);
    case 0x6366F1:
    case 0x8B5CF6: return lv_color_hex(0x62A4FF);

    /* Selected and pressed surfaces use pale primary-blue variants. */
    case 0xB9EEF6:
    case 0xBCE9F7: return lv_color_hex(0xC9E0FF);
    case 0xD3F2F8: return lv_color_hex(0xDCEBFF);
    case 0xD8F4FF: return lv_color_hex(0xE1EDFF);
    case 0xDDEBFF: return lv_color_hex(0xD9E9FF);
    case 0xE3F4FF: return lv_color_hex(0xE6F0FF);
    case 0xE3FAFD: return lv_color_hex(0xEAF3FF);
    case 0xEAF8FF: return lv_color_hex(0xF0F6FF);
    case 0xF2FBFF: return lv_color_hex(0xF4F8FF);
    case 0xF6FBFF: return lv_color_hex(0xF7FAFF);

    /* Borders, grids and secondary copy use low-saturation blue-gray. */
    case 0xCFE0EE: return lv_color_hex(0xC9DAF2);
    case 0xD6E6F5:
    case 0xDDE6EF: return lv_color_hex(0xD5E2F3);
    case 0xE9EDF2: return lv_color_hex(0xE4EBF5);
    case 0xECEFF3: return lv_color_hex(0xE8EEF6);
    case 0xECF4FA:
    case 0xEDF2F6: return lv_color_hex(0xEAF1FA);
    case 0xEFF4F8: return lv_color_hex(0xF0F4FA);
    case 0x5686A5: return lv_color_hex(0x5F779B);
    case 0x5F6E7D: return lv_color_hex(0x5B6A80);
    case 0x6B7A90: return lv_color_hex(0x6D7B90);
    case 0x8792A8: return lv_color_hex(0x808FA8);
    case 0x8AA8B8:
    case 0x8B9AAF:
    case 0x94A3B8: return lv_color_hex(0x889AB5);
    case 0x9AA6B2:
    case 0x9AB6C2: return lv_color_hex(0x97A8BF);

    /* Main copy and technology preview panels use deep navy variants. */
    case 0x0D3440: return lv_color_hex(0x17223B);
    case 0x101820: return lv_color_hex(0x111827);
    case 0x102233: return lv_color_hex(0x12203A);
    case 0x111827: return lv_color_hex(0x172036);
    case 0x142332: return lv_color_hex(0x17263F);
    case 0x164865: return lv_color_hex(0x23528A);
    case 0x183B61: return lv_color_hex(0x234578);
    case 0x1F2937: return lv_color_hex(0x253149);
    case 0x233249:
    case 0x243047: return lv_color_hex(0x293957);
    case 0x24465C: return lv_color_hex(0x315E96);
    case 0x2D3A4A: return lv_color_hex(0x34425E);
    case 0x355779: return lv_color_hex(0x4874A8);
    default: return lv_color_hex(color);
    }
}

#define lv_color_hex settings_theme_color_hex

#define SETTINGS_DETAIL_W             1280
#define SETTINGS_DETAIL_H             400
#define SETTINGS_DETAIL_HEADER_H      55
#define SETTINGS_KEYBOARD_MAX_TEXT    64

static lv_color_t detail_bg(void)      { return lv_color_hex(0xF7F8FA); }
static lv_color_t detail_panel(void)   { return lv_color_hex(0xFFFFFF); }
static lv_color_t detail_line(void)    { return lv_color_hex(0xE9EDF2); }
static lv_color_t detail_primary(void) { return lv_color_hex(0x08C5D6); }
static lv_color_t detail_primary_2(void){ return lv_color_hex(0xE3FAFD); }
static lv_color_t detail_text(void)    { return lv_color_hex(0x0D3440); }
static lv_color_t detail_select_border(void) { return lv_color_hex(0x0878C8); }

typedef struct {
    lv_obj_t* root;
    lv_obj_t* input_label;
    lv_timer_t* cursor_timer;
    lv_obj_t* shift_btn;
    lv_obj_t* letter_labels[26];
    settings_detail_keyboard_cb_t confirm_cb;
    settings_detail_keyboard_close_cb_t close_cb;
    void* user_data;
    void* close_user_data;
    uint8_t letter_label_count;
    uint16_t max_len;
    bool dirty;
    bool replace_on_next_key;
    bool cursor_visible;
    bool uppercase;
    char value[SETTINGS_KEYBOARD_MAX_TEXT + 1];
} settings_keyboard_ctx_t;

static settings_keyboard_ctx_t g_settings_keyboard = { 0 };

typedef struct {
    lv_obj_t* root;
    settings_detail_dialog_cb_t confirm_cb;
    settings_detail_dialog_cb_t cancel_cb;
    void* user_data;
} settings_dialog_ctx_t;

static settings_dialog_ctx_t g_settings_dialog = { 0 };
bool settings_detail_overlay_is_open(void)
{
    return g_settings_dialog.root != NULL || g_settings_keyboard.root != NULL;
}

static void detail_style_plain(lv_obj_t* obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

static void detail_add_press_style(lv_obj_t* obj, lv_color_t pressed_bg)
{
    lv_color_t normal_bg = lv_obj_get_style_bg_color(obj, LV_PART_MAIN);
    lv_damped_button_register(obj, normal_bg, pressed_bg);
}

lv_obj_t* settings_detail_create_label(lv_obj_t* parent, const char* text,
                                       const lv_font_t* font, lv_color_t color,
                                       lv_coord_t x, lv_coord_t y)
{
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_pos(label, x, y);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_SCROLLABLE);
    return label;
}



lv_obj_t* settings_detail_create_page(lv_obj_t* parent, const char* title,
                                      lv_event_cb_t back_cb,
                                      lv_obj_t** out_content)
{
    return settings_detail_create_page_ex(parent, title, back_cb, out_content, NULL);
}

lv_obj_t* settings_detail_create_page_ex(lv_obj_t* parent, const char* title,
                                         lv_event_cb_t back_cb,
                                         lv_obj_t** out_content,
                                         lv_obj_t** out_back_btn)
{
    lv_obj_t* root_parent = parent ? parent : lv_scr_act();
    lv_obj_t* page = lv_obj_create(root_parent);
    detail_style_plain(page);
    lv_obj_set_pos(page, 0, 0);
    lv_obj_set_size(page, SETTINGS_DETAIL_W, SETTINGS_DETAIL_H);
    lv_obj_set_style_bg_color(page, detail_bg(), 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(page, 0, 0);

    ui_page_background_apply(page, UI_BACKGROUND_SETTINGS);

    lv_obj_t* header = lv_obj_create(page);
    detail_style_plain(header);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, SETTINGS_DETAIL_W, SETTINGS_DETAIL_HEADER_H);
    lv_obj_set_style_bg_color(header, detail_panel(), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header, 0, 0);

    lv_obj_t* accent = lv_obj_create(header);
    detail_style_plain(accent);
    lv_obj_set_pos(accent, 0, 0);
    lv_obj_set_size(accent, 5, SETTINGS_DETAIL_HEADER_H);
    lv_obj_set_style_bg_color(accent, detail_primary(), 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);

    lv_obj_t* gear_bg = lv_obj_create(header);
    detail_style_plain(gear_bg);
    lv_obj_set_pos(gear_bg, 31, 13);
    lv_obj_set_size(gear_bg, 30, 30);
    lv_obj_set_style_bg_color(gear_bg, detail_primary_2(), 0);
    lv_obj_set_style_bg_opa(gear_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(gear_bg, 4, 0);

    lv_obj_t* gear = settings_detail_create_label(gear_bg, LV_SYMBOL_SETTINGS,
                                                  &lv_font_montserrat_16,
                                                  detail_primary(), 0, 0);
    lv_obj_center(gear);

    settings_detail_create_label(header, title, &lv_font_instrument_sans_semibold_20,
                                 detail_text(), 72, 14);

    lv_obj_t* title_line = lv_obj_create(header);
    detail_style_plain(title_line);
    lv_obj_set_pos(title_line, 72, 43);
    lv_obj_set_size(title_line, 72, 3);
    lv_obj_set_style_bg_color(title_line, detail_primary(), 0);
    lv_obj_set_style_bg_opa(title_line, LV_OPA_COVER, 0);

    lv_obj_t* bottom_line = lv_obj_create(header);
    detail_style_plain(bottom_line);
    lv_obj_set_pos(bottom_line, 0, SETTINGS_DETAIL_HEADER_H - 1);
    lv_obj_set_size(bottom_line, SETTINGS_DETAIL_W, 1);
    lv_obj_set_style_bg_color(bottom_line, detail_line(), 0);
    lv_obj_set_style_bg_opa(bottom_line, LV_OPA_COVER, 0);

    lv_obj_t* esc = lv_settings_back(header, 1156, 5, 92, 44, back_cb, NULL);
    lv_obj_set_style_shadow_width(esc, 0, 0);
    if (out_back_btn) {
        *out_back_btn = esc;
    }

    lv_obj_t* content = lv_obj_create(page);
    detail_style_plain(content);
    lv_obj_set_pos(content, 0, SETTINGS_DETAIL_HEADER_H);
    lv_obj_set_size(content, SETTINGS_DETAIL_W, SETTINGS_DETAIL_H - SETTINGS_DETAIL_HEADER_H);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);

    if (out_content) {
        *out_content = content;
    }

    return page;
}

lv_obj_t* settings_detail_create_card(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                                      lv_coord_t w, lv_coord_t h)
{
    lv_obj_t* card = lv_obj_create(parent);
    detail_style_plain(card);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    lv_obj_set_style_bg_color(card, detail_panel(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0xDDE6EF), 0);
    lv_obj_set_style_radius(card, 4, 0);
    lv_obj_set_style_shadow_width(card, 14, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_20, 0);
    lv_obj_set_style_shadow_ofs_y(card, 6, 0);
    return card;
}

lv_obj_t* settings_detail_create_button(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                                        lv_coord_t w, lv_coord_t h,
                                        const char* text, lv_color_t bg,
                                        lv_event_cb_t cb, void* user_data)
{
    lv_obj_t* btn = lv_obj_create(parent);
    detail_style_plain(btn);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 11, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    /* Legacy detail callers still provide a color. Preserve its action class,
       then share the same high-contrast states as new settings components. */
    bool neutral=lv_color_brightness(bg)>200;
    lv_damped_button_register(btn,bg,lv_color_darken(bg,20));
    lv_settings_action_style(btn,neutral?LV_SETTINGS_ACTION_SECONDARY:LV_SETTINGS_ACTION_PRIMARY);

    lv_settings_action_guard_init(btn);
    if (cb) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    }

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label,text);
    lv_obj_set_style_text_font(label,&lv_font_instrument_sans_medium_16,0);
    lv_obj_center(label);
    return btn;
}

lv_obj_t* settings_detail_create_select_box(lv_obj_t* parent,
                                            lv_coord_t x, lv_coord_t y,
                                            lv_coord_t size,
                                            lv_event_cb_t cb,
                                            void* user_data)
{
    lv_obj_t* box = lv_obj_create(parent);
    detail_style_plain(box);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, size, size);
    lv_obj_set_style_bg_color(box, lv_color_hex(0xF8F9FB), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_border_color(box, detail_primary(), 0);
    lv_obj_set_style_radius(box, 4, 0);
    lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);
    detail_add_press_style(box, lv_color_hex(0xD8F4FF));

    lv_settings_action_guard_init(box);
    if (cb) {
        lv_obj_add_event_cb(box, cb, LV_EVENT_CLICKED, user_data);
    }

    lv_obj_t* check = settings_detail_create_label(box, "", &lv_font_montserrat_18,
                                                   detail_primary(), 0, 0);
    lv_obj_center(check);
    return box;
}

void settings_detail_set_select_box_checked(lv_obj_t* box, bool checked)
{
    if (!box || !lv_obj_is_valid(box)) return;

    lv_obj_t* check = lv_obj_get_child(box, 0);
    if (check) {
        lv_label_set_text(check, checked ? LV_SYMBOL_OK : "");
        lv_obj_center(check);
    }
}

void settings_detail_set_select_box_active(lv_obj_t* box, bool active)
{
    if (!box || !lv_obj_is_valid(box)) return;

    lv_obj_set_style_bg_color(box, active ? detail_primary_2() : lv_color_hex(0xF8F9FB), 0);
    lv_obj_set_style_border_color(box, active ? detail_select_border() : detail_primary(), 0);
}

void settings_detail_set_focus_box_active(lv_obj_t* box, bool active)
{
    if (!box || !lv_obj_is_valid(box)) return;

    lv_obj_set_style_bg_color(box, active ? detail_primary_2() : lv_color_hex(0xF8F9FB), 0);
    lv_obj_set_style_border_color(box, active ? detail_select_border() : lv_color_hex(0xDDE6EF), 0);
    lv_obj_set_style_border_width(box, active ? 2 : 1, 0);
}

bool settings_detail_send_command(uint8_t cmd_g, const uint8_t* cmd_s,
                                  uint16_t cmd_s_len)
{
    /* The action owner presents failures; transport must not mutate a directory. */
    return protocol_send_is_ready() && protocol_send(cmd_g, cmd_s, cmd_s_len) >= 0;
}

static void settings_detail_dialog_close(bool confirmed)
{
    settings_detail_dialog_cb_t cb = confirmed ?
                                     g_settings_dialog.confirm_cb :
                                     g_settings_dialog.cancel_cb;
    void* user_data = g_settings_dialog.user_data;

    settings_detail_dialog_hide();

    if (cb) {
        cb(user_data);
    }
}

static void settings_detail_dialog_confirm_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    settings_detail_dialog_close(true);
}

static void settings_detail_dialog_cancel_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    settings_detail_dialog_close(false);
}

bool settings_detail_dialog_show(const char* title,
                                 const char* content,
                                 const char* confirm_text,
                                 const char* cancel_text,
                                 settings_detail_dialog_cb_t confirm_cb,
                                 settings_detail_dialog_cb_t cancel_cb,
                                 void* user_data)
{
    return settings_detail_dialog_show_ex(cancel_text ? SETTINGS_DIALOG_WARNING : SETTINGS_DIALOG_INFO,
        title,content,confirm_text,cancel_text,confirm_cb,cancel_cb,user_data);
}

bool settings_detail_dialog_show_ex(settings_detail_dialog_kind_t kind,
    const char *title,const char *content,const char *confirm_text,const char *cancel_text,
    settings_detail_dialog_cb_t confirm_cb,settings_detail_dialog_cb_t cancel_cb,void *user_data)
{
    lv_obj_t* parent = lv_scr_act();
    lv_obj_t* root;
    lv_obj_t* card;
    lv_obj_t* title_label;
    lv_obj_t* content_label;
    lv_obj_t* icon_box;
    lv_obj_t* icon_label;
    lv_coord_t confirm_x = 434;

    settings_detail_dialog_hide();

    root = lv_obj_create(parent);
    detail_style_plain(root);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_size(root, SETTINGS_DETAIL_W, SETTINGS_DETAIL_H);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_60, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE);

    card = lv_obj_create(root);
    detail_style_plain(card);
    lv_obj_set_size(card, 620, 280);
    lv_obj_center(card);
    lv_obj_set_style_bg_color(card, detail_panel(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0xDDE6EF), 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_shadow_width(card, 24, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_20, 0);
    lv_obj_set_style_shadow_ofs_y(card, 10, 0);
    lv_popup_style(root, card);

    icon_box = lv_obj_create(card);
    detail_style_plain(icon_box);
    lv_obj_set_pos(icon_box, 32, 28);
    lv_obj_set_size(icon_box, 46, 46);
    lv_obj_set_style_bg_color(icon_box, lv_color_hex(kind==SETTINGS_DIALOG_SUCCESS ? 0xEAF3ED :
        kind>=SETTINGS_DIALOG_WARNING ? 0xFCF2DF : 0xEAF2FC), 0);
    lv_obj_set_style_bg_opa(icon_box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(icon_box, 14, 0);

    icon_label = lv_img_create(icon_box);
    lv_img_set_src(icon_label,kind==SETTINGS_DIALOG_SUCCESS ? LVGL_DIR "popup_icons/check.png" :
        kind>=SETTINGS_DIALOG_WARNING ? LVGL_DIR "popup_icons/warn.png" : LVGL_DIR "popup_icons/info.png");
    lv_obj_center(icon_label);

    title_label = settings_detail_create_label(card, title ? title : "",
                                               &lv_font_instrument_sans_semibold_28,
                                               lv_color_hex(0x1D2B34), 92, 34);
    lv_obj_set_width(title_label, 496);
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_CLIP);

    content_label = settings_detail_create_label(card, content ? content : "",
                                                 &lv_font_instrument_sans_medium_16,
                                                 lv_color_hex(0x586B77), 32, 94);
    lv_obj_set_width(content_label, 556);
    lv_label_set_long_mode(content_label, LV_LABEL_LONG_WRAP);

    if (cancel_text) {
        lv_obj_t *cancel = settings_detail_create_button(card, 282, 208, 140, 46, cancel_text,
                                      lv_color_hex(0xF1F4F5), settings_detail_dialog_cancel_cb, NULL);
        lv_obj_set_style_text_color(lv_obj_get_child(cancel,0), lv_color_hex(0x1D2B34), 0);
        lv_obj_set_style_radius(cancel,12,0);
        lv_obj_set_style_shadow_width(cancel,0,0);
    }

    lv_obj_t *confirm = settings_detail_create_button(card, confirm_x, 208, 154, 46,
                                  confirm_text ? confirm_text : "",
                                  lv_color_hex(kind==SETTINGS_DIALOG_DESTRUCTIVE ? 0xB03838 : 0x1462CC),
                                  settings_detail_dialog_confirm_cb, NULL);
    lv_settings_action_style(confirm,kind==SETTINGS_DIALOG_DESTRUCTIVE ?
        LV_SETTINGS_ACTION_DESTRUCTIVE : LV_SETTINGS_ACTION_PRIMARY);
    lv_obj_set_style_radius(confirm,12,0);
    lv_obj_set_style_shadow_width(confirm,0,0);

    g_settings_dialog.root = root;
    g_settings_dialog.confirm_cb = confirm_cb;
    g_settings_dialog.cancel_cb = cancel_cb;
    g_settings_dialog.user_data = user_data;
    return true;
}

void settings_detail_dialog_hide(void)
{
    if (g_settings_dialog.root && lv_obj_is_valid(g_settings_dialog.root)) {
        lv_obj_del(g_settings_dialog.root);
    }

    g_settings_dialog.root = NULL;
    g_settings_dialog.confirm_cb = NULL;
    g_settings_dialog.cancel_cb = NULL;
    g_settings_dialog.user_data = NULL;
}

static void settings_keyboard_refresh(void)
{
    char show_value[SETTINGS_KEYBOARD_MAX_TEXT + 2];

    if (g_settings_keyboard.input_label) {
        lv_snprintf(show_value, sizeof(show_value), "%s%s",
                    g_settings_keyboard.value,
                    g_settings_keyboard.cursor_visible ? "|" : "");
        lv_label_set_text(g_settings_keyboard.input_label, show_value);
    }
}

static void settings_keyboard_update_case(void)
{
    for (uint8_t i = 0; i < g_settings_keyboard.letter_label_count; i++) {
        lv_obj_t* label = g_settings_keyboard.letter_labels[i];
        if (!label || !lv_obj_is_valid(label)) continue;

        char text[2];
        text[0] = g_settings_keyboard.uppercase ? (char)('A' + i) : (char)('a' + i);
        text[1] = '\0';
        lv_label_set_text(label, text);
    }

    if (g_settings_keyboard.shift_btn && lv_obj_is_valid(g_settings_keyboard.shift_btn)) {
        lv_obj_set_style_bg_color(g_settings_keyboard.shift_btn,
                                  g_settings_keyboard.uppercase ? detail_primary_2() : lv_color_hex(0xF8F9FB),
                                  0);
        lv_obj_set_style_border_color(g_settings_keyboard.shift_btn,
                                      g_settings_keyboard.uppercase ? detail_primary() : detail_line(),
                                      0);
    }
}

static void settings_keyboard_cursor_timer_cb(lv_timer_t* timer)
{
    (void)timer;

    g_settings_keyboard.cursor_visible = !g_settings_keyboard.cursor_visible;
    settings_keyboard_refresh();
}

static void settings_keyboard_close(bool submit)
{
    settings_detail_keyboard_cb_t cb = g_settings_keyboard.confirm_cb;
    settings_detail_keyboard_close_cb_t close_cb = g_settings_keyboard.close_cb;
    void* user_data = g_settings_keyboard.user_data;
    void* close_user_data = g_settings_keyboard.close_user_data;
    bool dirty = g_settings_keyboard.dirty;
    char value[SETTINGS_KEYBOARD_MAX_TEXT + 1];

    lv_snprintf(value, sizeof(value), "%s", g_settings_keyboard.value);

    if (g_settings_keyboard.cursor_timer) {
        lv_timer_del(g_settings_keyboard.cursor_timer);
        g_settings_keyboard.cursor_timer = NULL;
    }

    if (g_settings_keyboard.root && lv_obj_is_valid(g_settings_keyboard.root)) {
        lv_obj_del(g_settings_keyboard.root);
    }

    g_settings_keyboard.root = NULL;
    g_settings_keyboard.input_label = NULL;
    g_settings_keyboard.cursor_timer = NULL;
    g_settings_keyboard.shift_btn = NULL;
    g_settings_keyboard.confirm_cb = NULL;
    g_settings_keyboard.close_cb = NULL;
    g_settings_keyboard.user_data = NULL;
    g_settings_keyboard.close_user_data = NULL;
    g_settings_keyboard.letter_label_count = 0;
    g_settings_keyboard.max_len = 0;
    g_settings_keyboard.dirty = false;
    g_settings_keyboard.replace_on_next_key = false;
    g_settings_keyboard.cursor_visible = false;
    g_settings_keyboard.uppercase = true;
    memset(g_settings_keyboard.letter_labels, 0, sizeof(g_settings_keyboard.letter_labels));
    g_settings_keyboard.value[0] = '\0';

    if (submit && dirty && cb) {
        cb(value, user_data);
    }
    if (close_cb) {
        close_cb(close_user_data);
    }
}

void settings_detail_keyboard_hide(void)
{
    settings_keyboard_close(false);
}

static void settings_keyboard_commit_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    settings_keyboard_close(true);
}

static void settings_keyboard_cancel_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    settings_keyboard_close(false);
}

static void settings_keyboard_key_cb(lv_event_t* e)
{
    const char* key = (const char*)lv_event_get_user_data(e);
    size_t len;
    char input;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED || !key) return;

    if (strcmp(key, "SHIFT") == 0) {
        g_settings_keyboard.uppercase = !g_settings_keyboard.uppercase;
        settings_keyboard_update_case();
        return;
    }

    if (strcmp(key, "BACK") == 0) {
        len = strlen(g_settings_keyboard.value);
        if (len > 0) {
            g_settings_keyboard.value[len - 1] = '\0';
            g_settings_keyboard.dirty = true;
        }
        g_settings_keyboard.replace_on_next_key = false;
        g_settings_keyboard.cursor_visible = true;
        settings_keyboard_refresh();
        return;
    }

    if (strcmp(key, "CLEAR") == 0) {
        if (g_settings_keyboard.value[0] != '\0') {
            g_settings_keyboard.value[0] = '\0';
            g_settings_keyboard.dirty = true;
        }
        g_settings_keyboard.replace_on_next_key = false;
        g_settings_keyboard.cursor_visible = true;
        settings_keyboard_refresh();
        return;
    }

    input = key[0];
    if (input >= 'A' && input <= 'Z' && !g_settings_keyboard.uppercase) {
        input = (char)(input - 'A' + 'a');
    }

    if (g_settings_keyboard.replace_on_next_key) {
        g_settings_keyboard.value[0] = '\0';
        g_settings_keyboard.replace_on_next_key = false;
    }

    len = strlen(g_settings_keyboard.value);
    if (len >= g_settings_keyboard.max_len || len >= SETTINGS_KEYBOARD_MAX_TEXT) {
        return;
    }

    g_settings_keyboard.value[len] = input;
    g_settings_keyboard.value[len + 1] = '\0';
    g_settings_keyboard.dirty = true;
    g_settings_keyboard.cursor_visible = true;
    settings_keyboard_refresh();
}

static lv_obj_t* settings_keyboard_create_key(lv_obj_t* parent, int x, int y, int w, int h,
                                              const char* text, const char* key,
                                              lv_color_t bg)
{
    lv_obj_t* btn = lv_obj_create(parent);
    detail_style_plain(btn);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    (void)bg;
    lv_obj_set_style_bg_color(btn, lv_color_hex(LV_SETTINGS_CONTROL_SURFACE), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0xECF0F3), 0);
    lv_obj_set_style_radius(btn, 12, 0);
    if(h == 44) {
        lv_obj_set_style_shadow_color(btn,lv_color_hex(0xAEBCC7),0);
        lv_obj_set_style_shadow_width(btn,2,0);
        lv_obj_set_style_shadow_ofs_y(btn,2,0);
        lv_obj_set_style_shadow_opa(btn,LV_OPA_40,0);
    }
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    detail_add_press_style(btn, lv_color_hex(0xE2E9EE));
    lv_settings_action_style(btn,LV_SETTINGS_ACTION_SECONDARY);
    lv_obj_add_event_cb(btn, settings_keyboard_key_cb, LV_EVENT_CLICKED, (void*)key);

    const lv_font_t* key_font =
        (text && (strcmp(text, LV_SYMBOL_BACKSPACE) == 0 || strcmp(text, LV_SYMBOL_UP) == 0))
            ? &lv_font_montserrat_20
            : (h >= 60 && key[0]>='0' && key[0]<='9' ? &lv_font_instrument_sans_semibold_28 : &lv_font_instrument_sans_medium_20);
    lv_obj_t* label = settings_detail_create_label(btn, strcmp(key,"BACK")==0 ? "" : text, key_font,
                                                   lv_color_hex(0x1D2B34), 0, 0);
    if(strcmp(key,"BACK")==0) {
        lv_obj_t *image=lv_img_create(btn);
        lv_img_set_src(image, LVGL_DIR "popup_icons/backspace.png");
        lv_obj_center(image);
    }
    lv_obj_center(label);
    if (key && key[0] >= 'A' && key[0] <= 'Z' && key[1] == '\0') {
        uint8_t idx = (uint8_t)(key[0] - 'A');
        if (idx < 26) {
            g_settings_keyboard.letter_labels[idx] = label;
            if (g_settings_keyboard.letter_label_count < idx + 1) {
                g_settings_keyboard.letter_label_count = (uint8_t)(idx + 1);
            }
        }
    }
    return btn;
}

static void settings_keyboard_create_action(lv_obj_t* parent, int x, int y, int w, int h,
                                            const char* text, lv_color_t bg,
                                            lv_event_cb_t cb)
{
    lv_obj_t* btn = lv_obj_create(parent);
    detail_style_plain(btn);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    detail_add_press_style(btn, lv_color_darken(bg, 28));
    lv_settings_action_style(btn,cb==settings_keyboard_cancel_cb ?
        LV_SETTINGS_ACTION_SECONDARY : LV_SETTINGS_ACTION_PRIMARY);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    const lv_font_t* action_font =
        (text && strcmp(text, LV_SYMBOL_OK) == 0)
            ? &lv_font_montserrat_24
            : &lv_font_instrument_sans_medium_16;
    lv_obj_t* label = settings_detail_create_label(btn, text, action_font,
                                                   cb == settings_keyboard_cancel_cb ? lv_color_hex(0x1D2B34) : detail_panel(), 0, 0);
    lv_obj_center(label);
}

static void settings_keyboard_create_number_keys(lv_obj_t* parent)
{
    static const char* keys[4][3] = {
        { "1", "2", "3" },
        { "4", "5", "6" },
        { "7", "8", "9" },
        { "CLEAR", "0", "BACK" },
    };
    int key_w = 113;
    int key_h = 64;
    int gap_x = 10;
    int gap_y = 10;
    int start_x = 0;
    int start_y = 0;

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 3; c++) {
            settings_keyboard_create_key(parent,
                                         start_x + c * (key_w + gap_x),
                                         start_y + r * (key_h + gap_y),
                                         key_w, key_h,
                                         strcmp(keys[r][c],"CLEAR")==0 ? "Clear" : keys[r][c], keys[r][c],
                                         lv_color_hex(0xF8F9FB));
        }
    }

    lv_obj_t *card=lv_obj_get_parent(parent);
    /* Keep the existing signed/decimal contract; validation belongs to field owners. */
    settings_keyboard_create_key(card,30,204,80,46,"-","-",lv_color_hex(0xF1F4F5));
    settings_keyboard_create_key(card,120,204,80,46,".",".",lv_color_hex(0xF1F4F5));
    settings_keyboard_create_action(card,30,280,190,46,"Cancel",
                                    lv_color_hex(0xF1F4F5),settings_keyboard_cancel_cb);
    settings_keyboard_create_action(card,232,280,190,46,"Apply",
                                    lv_color_hex(0x1462CC),settings_keyboard_commit_cb);
}

static void settings_keyboard_create_text_keys(lv_obj_t* parent)
{
    static const char* nums[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "0" };
    static const char* row0[] = { "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P" };
    static const char* row1[] = { "A", "S", "D", "F", "G", "H", "J", "K", "L" };
    static const char* row2[] = { "Z", "X", "C", "V", "B", "N", "M" };
    int key_w = 74;
    int key_h = 44;
    int gap = 7;

    for (int i = 0; i < 10; i++) {
        settings_keyboard_create_key(parent, 113 + i * (key_w + gap), 0,
                                     key_w, key_h, nums[i], nums[i],
                                     lv_color_hex(0xF8F9FB));
    }
    for (int i = 0; i < 10; i++) {
        settings_keyboard_create_key(parent, 113 + i * (key_w + gap), 51,
                                     key_w, key_h, row0[i], row0[i],
                                     lv_color_hex(0xF8F9FB));
    }
    for (int i = 0; i < 9; i++) {
        settings_keyboard_create_key(parent, 154 + i * (key_w + gap), 102,
                                     key_w, key_h, row1[i], row1[i],
                                     lv_color_hex(0xF8F9FB));
    }
    for (int i = 0; i < 7; i++) {
        settings_keyboard_create_key(parent, 106 + i * (key_w + gap), 153,
                                     key_w, key_h, row2[i], row2[i],
                                     lv_color_hex(0xF8F9FB));
    }

    g_settings_keyboard.shift_btn = settings_keyboard_create_key(parent, 20, 153, 74, 44,
                                                                "Aa", "SHIFT",
                                                                lv_color_hex(0xF8F9FB));
    settings_keyboard_create_key(parent, 673, 153, 122, 44, "",
                                 "BACK", lv_color_hex(0xF8F9FB));
    settings_keyboard_create_key(parent, 802, 153, 122, 44, "Clear", "CLEAR",lv_color_hex(0xF1F4F5));
    lv_obj_t *card=lv_obj_get_parent(parent);
    settings_keyboard_create_action(card, 824, 36, 186, 44, "Apply",
                                    lv_color_hex(0x1462CC), settings_keyboard_commit_cb);
    settings_keyboard_create_action(card, 682, 36, 130, 44, "Cancel",
                                    lv_color_hex(0xF1F4F5), settings_keyboard_cancel_cb);
}

bool settings_detail_keyboard_show(const char* title,
                                   const char* init_value,
                                   uint16_t max_len,
                                   settings_detail_keyboard_mode_t mode,
                                   settings_detail_keyboard_cb_t confirm_cb,
                                   void* user_data)
{
    return settings_detail_keyboard_show_ex(title, init_value, max_len, mode,
                                            confirm_cb, user_data, NULL, NULL);
}

bool settings_detail_keyboard_show_ex(const char* title,
                                      const char* init_value,
                                      uint16_t max_len,
                                      settings_detail_keyboard_mode_t mode,
                                      settings_detail_keyboard_cb_t confirm_cb,
                                      void* user_data,
                                      settings_detail_keyboard_close_cb_t close_cb,
                                      void* close_user_data)
{
    lv_obj_t* scr = lv_scr_act();
    lv_obj_t* top;
    lv_obj_t* dialog;
    lv_obj_t* input_wrap;
    lv_obj_t* keyboard;

    if (!scr || !confirm_cb || max_len == 0) {
        return false;
    }

    settings_detail_keyboard_hide();

    if (max_len > SETTINGS_KEYBOARD_MAX_TEXT) {
        max_len = SETTINGS_KEYBOARD_MAX_TEXT;
    }

    g_settings_keyboard.root = lv_obj_create(scr);
    detail_style_plain(g_settings_keyboard.root);
    lv_obj_set_pos(g_settings_keyboard.root, 0, 0);
    lv_obj_set_size(g_settings_keyboard.root, SETTINGS_DETAIL_W, SETTINGS_DETAIL_H);
    lv_obj_set_style_bg_color(g_settings_keyboard.root, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(g_settings_keyboard.root, LV_OPA_40, 0);
    lv_obj_add_flag(g_settings_keyboard.root, LV_OBJ_FLAG_CLICKABLE);

    top = lv_obj_create(g_settings_keyboard.root);
    detail_style_plain(top);
    bool numeric = mode != SETTINGS_DETAIL_KEYBOARD_TEXT;
    lv_obj_set_pos(top, numeric ? 174 : 125, numeric ? 24 : 48);
    lv_obj_set_size(top, numeric ? 932 : 1030, numeric ? 352 : 304);
    lv_obj_set_style_bg_opa(top, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(top, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_settings_keyboard.root, settings_keyboard_cancel_cb, LV_EVENT_CLICKED, NULL);
    lv_popup_style(g_settings_keyboard.root,top);
    if(!numeric) lv_obj_set_style_bg_color(top,lv_color_hex(0xE7EDF1),0);

    dialog = lv_obj_create(top);
    detail_style_plain(dialog);
    lv_obj_set_pos(dialog, numeric ? 30 : 20, numeric ? 26 : 10);
    lv_obj_set_size(dialog, numeric ? 484 : 650, numeric ? 170 : 72);
    lv_obj_set_style_bg_color(dialog, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(dialog, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dialog, 0, 0);
    lv_obj_set_style_border_color(dialog, lv_color_hex(0xDDEBFF), 0);
    lv_obj_set_style_radius(dialog, 8, 0);
    lv_obj_set_style_shadow_width(dialog, 0, 0);
    lv_obj_set_style_shadow_opa(dialog, LV_OPA_30, 0);
    lv_obj_set_style_shadow_ofs_y(dialog, 8, 0);

    settings_detail_create_label(dialog, title ? title : "", numeric ? &lv_font_instrument_sans_semibold_28 : &lv_font_instrument_sans_medium_16,
                                 lv_color_hex(0x1D2B34), 0, 0);

    input_wrap = lv_obj_create(dialog);
    detail_style_plain(input_wrap);
    lv_obj_set_pos(input_wrap, 0, numeric ? 58 : 26);
    lv_obj_set_size(input_wrap, numeric ? 440 : 650, numeric ? 78 : 44);
    lv_obj_set_style_bg_color(input_wrap, lv_color_hex(0xF8FAFC), 0);
    lv_obj_set_style_bg_opa(input_wrap, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(input_wrap, 2, 0);
    lv_obj_set_style_border_color(input_wrap, lv_color_hex(0x1462CC), 0);
    lv_obj_set_style_radius(input_wrap, 13, 0);

    g_settings_keyboard.input_label = settings_detail_create_label(input_wrap, "", &lv_font_instrument_sans_medium_24,
                                                                  detail_text(), 14, 13);
    lv_obj_set_size(g_settings_keyboard.input_label, numeric ? 412 : 618, numeric ? 48 : 30);
    if(!numeric) lv_obj_set_y(g_settings_keyboard.input_label,7);
    if(numeric) lv_obj_set_style_text_font(g_settings_keyboard.input_label,&lv_font_instrument_sans_semibold_40,0);
    lv_obj_set_style_text_color(g_settings_keyboard.input_label,lv_color_hex(0x1D2B34),0);

    keyboard = lv_obj_create(top);
    detail_style_plain(keyboard);
    lv_obj_set_pos(keyboard, numeric ? 542 : 0, numeric ? 26 : 92);
    lv_obj_set_size(keyboard, numeric ? 360 : 1030, numeric ? 300 : 197);
    lv_obj_set_style_bg_color(keyboard, detail_panel(), 0);
    lv_obj_set_style_bg_opa(keyboard, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(keyboard, 0, 0);
    lv_obj_set_style_border_color(keyboard, detail_line(), 0);
    lv_obj_set_style_bg_opa(keyboard, LV_OPA_TRANSP, 0);

    g_settings_keyboard.confirm_cb = confirm_cb;
    g_settings_keyboard.close_cb = close_cb;
    g_settings_keyboard.user_data = user_data;
    g_settings_keyboard.close_user_data = close_user_data;
    g_settings_keyboard.max_len = max_len;
    g_settings_keyboard.dirty = false;
    g_settings_keyboard.replace_on_next_key = true;
    g_settings_keyboard.cursor_visible = true;
    g_settings_keyboard.uppercase = true;
    g_settings_keyboard.letter_label_count = 0;
    g_settings_keyboard.shift_btn = NULL;
    memset(g_settings_keyboard.letter_labels, 0, sizeof(g_settings_keyboard.letter_labels));
    lv_snprintf(g_settings_keyboard.value, sizeof(g_settings_keyboard.value),
                "%s", init_value ? init_value : "");

    if (mode == SETTINGS_DETAIL_KEYBOARD_TEXT) {
        settings_keyboard_create_text_keys(keyboard);
    } else {
        settings_keyboard_create_number_keys(keyboard);
    }

    settings_keyboard_update_case();
    g_settings_keyboard.cursor_timer = lv_timer_create(settings_keyboard_cursor_timer_cb, 500, NULL);
    settings_keyboard_refresh();
    return true;
}
