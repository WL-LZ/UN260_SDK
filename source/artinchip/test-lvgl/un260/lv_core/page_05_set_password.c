#include "page_05_set_password.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_components/lv_pin_keypad.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/user_cfg.h"

#include <string.h>

typedef struct {
    lv_obj_t *page;
    lv_pin_keypad_t keypad;
} password_page_context_t;

static password_page_context_t g_password_page;

static void password_cancel(void *user_data)
{
    LV_UNUSED(user_data);
    lv_pin_keypad_hide(&g_password_page.keypad);
    ui_manager_switch(UI_PAGE_MAIN);
}

static void password_confirm(const char *pin, void *user_data)
{
    LV_UNUSED(user_data);
    if (strcmp(user_cfg_password_get(), pin) == 0) {
        lv_pin_keypad_hide(&g_password_page.keypad);
        ui_manager_switch(UI_PAGE_SETTING);
        return;
    }
    lv_pin_keypad_clear(&g_password_page.keypad);
    lv_pin_keypad_set_status(&g_password_page.keypad, "Incorrect PIN. Please try again.");
}

static void password_back_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) password_cancel(NULL);
}

static void password_deleted_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_DELETE &&
        lv_event_get_target(event) == g_password_page.page)
        g_password_page.page = NULL;
}

static void password_show_keypad(void)
{
    const lv_pin_keypad_config_t config = {
        .eyebrow = "SECURE ACCESS",
        .title = "Enter access PIN",
        .prompt = "Please enter your 4-digit PIN",
        .confirm_cb = password_confirm,
        .cancel_cb = password_cancel,
        .digits_visible = user_cfg_password_visibility_enabled(),
        .save_visibility = user_cfg_password_visibility_save,
    };
    lv_pin_keypad_show(&g_password_page.keypad, &config, "");
}

void ui_page_05_set_password_create(lv_obj_t *parent)
{
    lv_obj_t *content = NULL;
    if (g_password_page.page && lv_obj_is_valid(g_password_page.page)) return;
    g_password_page.page = settings_detail_create_page(parent,
        ui_text_get(UI_TEXT_PASSWORD_LOGIN_TITLE), password_back_cb, &content);
    lv_obj_add_event_cb(g_password_page.page, password_deleted_cb,
                        LV_EVENT_DELETE, NULL);
    /* The same 1120 x 320 component is used for each Change Password field. */
    lv_pin_keypad_create(&g_password_page.keypad, content, 80, 12);
    password_show_keypad();
}

void ui_page_05_set_password_destroy(void)
{
    lv_pin_keypad_destroy(&g_password_page.keypad);
    if (g_password_page.page && lv_obj_is_valid(g_password_page.page))
        lv_obj_del(g_password_page.page);
    memset(&g_password_page, 0, sizeof(g_password_page));
}

bool ui_page_05_set_password_resume(void)
{
    if (!g_password_page.page || !lv_obj_is_valid(g_password_page.page)) return false;
    lv_obj_clear_flag(g_password_page.page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_password_page.page);
    password_show_keypad();
    return true;
}

void ui_page_05_set_password_suspend(void)
{
    if (!g_password_page.page || !lv_obj_is_valid(g_password_page.page)) return;
    lv_pin_keypad_hide(&g_password_page.keypad);
    lv_obj_add_flag(g_password_page.page, LV_OBJ_FLAG_HIDDEN);
}
