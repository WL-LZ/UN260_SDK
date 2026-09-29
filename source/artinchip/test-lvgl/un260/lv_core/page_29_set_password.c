#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_components/ui_notice.h"
#include "page_29_set_password.h"
#include "un260/lv_components/lv_pin_keypad.h"
#include "un260/lv_core/lv_page_manager.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/gesture/gesture_service.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/user_cfg.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef enum {
    PASSWORD_FIELD_CURRENT = 0,
    PASSWORD_FIELD_NEW,
    PASSWORD_FIELD_CONFIRM,
    PASSWORD_FIELD_COUNT,
} password_field_t;

static lv_settings_frame_t password_frame;
static bool password_leave_home;
static lv_obj_t* password_setting_page = NULL;
static lv_obj_t* password_setting_content = NULL;
static lv_obj_t* password_setting_form = NULL;
static lv_pin_keypad_t password_setting_keypad;
static lv_obj_t* field_cards[PASSWORD_FIELD_COUNT] = { NULL };
static lv_obj_t* field_values[PASSWORD_FIELD_COUNT] = { NULL };
static char field_text[PASSWORD_FIELD_COUNT][USER_PASSWORD_MAX_LEN + 1] = { 0 };
static password_field_t active_field = PASSWORD_FIELD_CURRENT;
static lv_obj_t *password_save_button;
static void password_setting_open_keyboard(password_field_t field);

static const char *const field_titles[PASSWORD_FIELD_COUNT] = {
    UI_N_("Current password"),
    UI_N_("New password"),
    UI_N_("Confirm password"),
};

static const char *const field_prompts[PASSWORD_FIELD_COUNT] = {
    UI_N_("Please enter your current 4-digit PIN"),
    UI_N_("Please enter your new 4-digit PIN"),
    UI_N_("Please re-enter your new 4-digit PIN"),
};

static void password_setting_refresh_fields(void)
{
    if(password_save_button){
        if(lv_pin_input_is_complete(field_text[PASSWORD_FIELD_CONFIRM]))settings_detail_action_block(password_save_button, NULL);
        else settings_detail_action_block(password_save_button, UI_N_("Verify the current PIN, then enter and confirm your new PIN."));
    }
    for (uint8_t i = 0; i < PASSWORD_FIELD_COUNT; i++) {
        bool active = (i == active_field);
        char masked[USER_PASSWORD_MAX_LEN + 1];
        size_t len = strlen(field_text[i]);

        if (len > USER_PASSWORD_MAX_LEN) len = USER_PASSWORD_MAX_LEN;
        for (size_t j = 0; j < len; j++) {
            masked[j] = '*';
        }
        masked[len] = '\0';

        if (field_cards[i]) {
            if(active)lv_obj_clear_flag(field_cards[i],LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(field_cards[i],LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_color(field_cards[i],
                                      active ? lv_color_hex(0xEDF4FF) : lv_color_hex(0xFFFFFF),
                                      0);
            lv_obj_set_style_border_color(field_cards[i],
                                          active ? lv_color_hex(0x1462CC) : lv_color_hex(0xE3E9ED),
                                          0);
        }
        if (field_values[i]) {
            lv_label_set_text(field_values[i],
                              len > 0 ? masked : ui_tr("Tap to enter PIN"));
            lv_obj_set_style_text_color(field_values[i],
                                        len > 0 ? lv_color_hex(0x1D2B34) : lv_color_hex(0x586B78),
                                        0);
        }
    }
}

static void password_setting_show_toast(const char *text,ui_notice_kind_t kind)
{ui_notice_post_text(kind,"settings.password",UI_N_("Password"),text);}

static void password_setting_close_keyboard(void *user_data)
{
    LV_UNUSED(user_data);
    lv_pin_keypad_hide(&password_setting_keypad);
    if (password_setting_form && lv_obj_is_valid(password_setting_form))
        lv_obj_clear_flag(password_setting_form, LV_OBJ_FLAG_HIDDEN);
    if (password_frame.footer) lv_obj_clear_flag(password_frame.footer, LV_OBJ_FLAG_HIDDEN);
}

static void password_setting_keyboard_cb(const char* value, void* user_data)
{
    password_field_t field = (password_field_t)(uintptr_t)user_data;

    if (field >= PASSWORD_FIELD_COUNT || !lv_pin_input_is_complete(value)) return;
    if(field!=active_field)return;
    if(field==PASSWORD_FIELD_CURRENT&&strcmp(value,user_cfg_password_get())){
        lv_pin_keypad_set_status(&password_setting_keypad,ui_tr("Incorrect current PIN. Try again."));return;
    }
    if(field==PASSWORD_FIELD_CONFIRM&&strcmp(value,field_text[PASSWORD_FIELD_NEW])){
        lv_pin_keypad_set_status(&password_setting_keypad,ui_tr("PINs do not match. Re-enter the new PIN."));return;
    }
    lv_snprintf(field_text[field], sizeof(field_text[field]), "%s", value);
    for(unsigned i=field+1;i<PASSWORD_FIELD_COUNT;i++)memset(field_text[i],0,sizeof(field_text[i]));
    password_setting_close_keyboard(NULL);
    if(field<PASSWORD_FIELD_CONFIRM){
        active_field=field+1;password_setting_open_keyboard(active_field);
    }else ui_notice_post_text(UI_NOTICE_INFO,"settings.password",UI_N_("PIN verified"),UI_N_("Tap Save to apply the new password."));
    password_setting_refresh_fields();
}

static void password_setting_open_keyboard(password_field_t field)
{
    if (field >= PASSWORD_FIELD_COUNT || field!=active_field) return;
    const lv_pin_keypad_config_t config = {
        .eyebrow = ui_tr("CHANGE PASSWORD"),
        .title = ui_tr(field_titles[field]),
        .prompt = ui_tr(field_prompts[field]),
        .confirm_cb = password_setting_keyboard_cb,
        .cancel_cb = password_setting_close_keyboard,
        .user_data = (void *)(uintptr_t)field,
        .digits_visible = user_cfg_password_visibility_enabled(),
        .save_visibility = user_cfg_password_visibility_save,
    };
    active_field = field;
    password_setting_refresh_fields();
    if (!lv_pin_keypad_create(&password_setting_keypad, password_setting_content, 80, 76))
        return;
    if (lv_pin_keypad_show(&password_setting_keypad, &config, field_text[field])) {
        lv_obj_add_flag(password_setting_form, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(password_frame.footer, LV_OBJ_FLAG_HIDDEN);
    }
}

static void password_setting_field_cb(lv_event_t* e)
{
    password_field_t field;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    field = (password_field_t)(uintptr_t)lv_event_get_user_data(e);
    if (field >= PASSWORD_FIELD_COUNT) return;
    password_setting_open_keyboard(field);
}

static void password_setting_save_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    if (field_text[PASSWORD_FIELD_NEW][0] == '\0') {
        password_setting_show_toast(UI_N_("Password cannot be empty"), UI_NOTICE_WARNING);
        return;
    }

    for (unsigned i = 0; i < PASSWORD_FIELD_COUNT; ++i) {
        if (!lv_pin_input_is_complete(field_text[i])) {
            password_setting_show_toast(UI_N_("Enter exactly 4 digits in each field."), UI_NOTICE_WARNING);
            return;
        }
    }

    if (strcmp(field_text[PASSWORD_FIELD_CURRENT], user_cfg_password_get()) != 0) {
        password_setting_show_toast(UI_N_("Incorrect current PIN."), UI_NOTICE_WARNING);
        return;
    }

    if (strcmp(field_text[PASSWORD_FIELD_NEW], field_text[PASSWORD_FIELD_CONFIRM]) != 0) {
        password_setting_show_toast(UI_N_("Passwords do not match"), UI_NOTICE_WARNING);
        return;
    }

    if (!user_cfg_password_save(field_text[PASSWORD_FIELD_NEW])) {
        password_setting_show_toast(UI_N_("Save failed"), UI_NOTICE_ERROR);
        return;
    }

    memset(field_text, 0, sizeof(field_text));
    active_field = PASSWORD_FIELD_CURRENT;
    password_setting_refresh_fields();
    password_setting_show_toast(UI_N_("Password saved"), UI_NOTICE_SUCCESS);
}

static bool password_dirty(void)
{
    for (unsigned i = 0; i < PASSWORD_FIELD_COUNT; ++i)
        if (field_text[i][0]) return true;
    return false;
}

static void password_leave(void *data)
{
    (void)data;
    if (password_leave_home) { ui_manager_suspend_to_home(); }
    else ui_manager_pop_page();
}

static void password_ask_leave(bool home)
{
    password_leave_home = home;
    if (password_dirty()) settings_detail_dialog_show_ex(SETTINGS_DIALOG_WARNING,
        ui_tr("Discard changes?"), ui_tr("Your new password has not been saved."),
        ui_tr("Discard"), ui_tr("Keep editing"), password_leave, NULL, NULL);
    else password_leave(NULL);
}

static void password_setting_esc_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    if (lv_pin_keypad_is_visible(&password_setting_keypad)) {
        password_setting_close_keyboard(NULL);
        return;
    }
    password_ask_leave(false);
}

static void password_setting_cancel(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    password_leave_home = false;
    password_leave(NULL);
}

static bool password_gesture(gesture_action_t action)
{
    if (!password_setting_page || !lv_obj_is_visible(password_setting_page)) return false;
    if (settings_detail_overlay_is_open() || lv_pin_keypad_is_visible(&password_setting_keypad)) return true;
    if (action == GESTURE_ACTION_HOME && password_dirty()) {
        password_ask_leave(true);
        return true;
    }
    return false;
}

static void password_setting_deleted_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_DELETE ||
        lv_event_get_target(event) != password_setting_page) return;
    /* The component's own delete event releases its timer after this event. */
    lv_pin_keypad_hide(&password_setting_keypad);
    settings_detail_dialog_hide();
    gesture_service_clear_page_policy(UI_PAGE_PASSWORD_CHANGE);
    memset(&password_frame, 0, sizeof(password_frame));
    password_setting_page = NULL;
    password_setting_content = NULL;
    password_setting_form = NULL;
    password_save_button=NULL;
    memset(field_cards, 0, sizeof(field_cards));
    memset(field_values, 0, sizeof(field_values));
    memset(field_text, 0, sizeof(field_text));
    active_field = PASSWORD_FIELD_CURRENT;
}

static void password_setting_outside(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED &&
        lv_event_get_target(event) == password_setting_page &&
        lv_pin_keypad_is_visible(&password_setting_keypad))
        password_setting_close_keyboard(NULL);
}

static void password_setting_create_field(lv_obj_t *parent, password_field_t field)
{
    lv_obj_t *item = lv_settings_button(parent, 420, 22, 788, 188,
        "", false, password_setting_field_cb, (void *)(uintptr_t)field);
    lv_obj_set_style_bg_color(item, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(item, 1, 0);
    lv_obj_set_style_border_color(item, lv_color_hex(0xE3E9ED), 0);
    lv_settings_label(item, ui_tr(field_titles[field]), 24, 24,
        &lv_font_instrument_sans_medium_18, 0x1D2B34);
    field_values[field] = lv_settings_label(item, ui_tr("Tap to enter PIN"), 24, 90,
        &lv_font_instrument_sans_medium_18, 0x586B78);
    lv_settings_label(item, ui_tr(field_prompts[field]), 24, 142,
        &lv_font_instrument_sans_medium_14, 0x586B78);
    field_cards[field] = item;
}

void ui_page_29_set_password_create(lv_obj_t *parent)
{
    if (password_setting_page) return;
    lv_settings_header_t header = {
        .title = ui_text_get(UI_TEXT_SETTINGS_PASSWORD), .icon = "ShieldCheck",
        .back = password_setting_esc_cb
    };
    password_frame = lv_settings_frame_create(parent, &header);
    password_setting_page = password_frame.root;
    password_setting_content = password_frame.root;
    password_setting_form = password_frame.body;
    lv_obj_set_style_bg_opa(password_setting_form, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(password_setting_form, 0, 0);
    lv_obj_add_event_cb(password_setting_page, password_setting_deleted_cb, LV_EVENT_DELETE, NULL);
    lv_obj_add_flag(password_setting_page, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(password_setting_page, password_setting_outside, LV_EVENT_CLICKED, NULL);
    lv_settings_label(password_setting_form,ui_tr("Protect access"),24,24,&lv_font_instrument_sans_semibold_24,0x1D2B34);
    lv_settings_label(password_setting_form,ui_tr("1  Verify current PIN\n\n2  Enter a new PIN\n\n3  Confirm and save"),24,78,&lv_font_instrument_sans_medium_16,0x586B78);
    for (unsigned field = 0; field < PASSWORD_FIELD_COUNT; ++field)
        password_setting_create_field(password_setting_form, (password_field_t)field);
    lv_label_set_text(password_frame.message, ui_tr("Enter your current PIN, then enter and confirm a new PIN."));
    lv_settings_button(password_frame.footer, 964, 0, 124, 46, ui_tr("Cancel"), false, password_setting_cancel, NULL);
    password_save_button=lv_settings_button(password_frame.footer, 1100, 0, 132, 46, ui_tr("Save"), true, password_setting_save_cb, NULL);
    active_field = PASSWORD_FIELD_CURRENT;
    password_leave_home = false;
    memset(field_text, 0, sizeof(field_text));
    password_setting_refresh_fields();
    gesture_service_set_page_policy(UI_PAGE_PASSWORD_CHANGE, NULL, password_gesture);
}

void ui_page_29_set_password_destroy(void)
{
    gesture_service_clear_page_policy(UI_PAGE_PASSWORD_CHANGE);
    settings_detail_dialog_hide();
    lv_pin_keypad_destroy(&password_setting_keypad);

    if (password_setting_page && lv_obj_is_valid(password_setting_page)) {
        lv_obj_del(password_setting_page);
    }

    password_setting_page = NULL;
    password_setting_content = NULL;
    password_setting_form = NULL;
    memset(field_cards, 0, sizeof(field_cards));
    memset(field_values, 0, sizeof(field_values));
    memset(field_text, 0, sizeof(field_text));
    active_field = PASSWORD_FIELD_CURRENT;
}
