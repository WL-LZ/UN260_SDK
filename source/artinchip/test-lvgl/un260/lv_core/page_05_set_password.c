#include "page_05_set_password.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_components/lv_pin_keypad.h"
#include "un260/lv_components/lv_nav_button.h"
#include "un260/lv_components/lv_settings.h"
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
    ui_page_05_set_password_suspend();
}

static void password_confirm(const char *pin, void *user_data)
{
    LV_UNUSED(user_data);
    if (strcmp(user_cfg_password_get(), pin) == 0) {
        ui_page_05_set_password_suspend();
        ui_manager_switch(UI_PAGE_SETTING);
        return;
    }
    lv_pin_keypad_clear(&g_password_page.keypad);
    lv_pin_keypad_set_error(&g_password_page.keypad, "Incorrect password. Try again.");
}

static void password_outside_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED&&
        lv_event_get_target(event)==g_password_page.page) password_cancel(NULL);
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
        .title = "Settings access",
        .prompt = "Enter your 4-digit password.",
        .leading_icon = LVGL_DIR "pin_icons/lock.png",
        .footnote_icon = LVGL_DIR "pin_icons/shield.png",
        .footnote = "For authorized configuration and service.",
        .idle_status = "Opens automatically when the code is correct.",
        .confirm_cb = password_confirm,
        .cancel_cb = password_cancel,
        .auto_confirm = true,
        .compact = true,
    };
    lv_pin_keypad_show(&g_password_page.keypad, &config, "");
}

void ui_page_05_set_password_create(lv_obj_t *parent)
{
    if (g_password_page.page && lv_obj_is_valid(g_password_page.page)) return;
    g_password_page.page=lv_settings_box(parent,0,0,1280,400,0x20313B);
    lv_obj_set_style_bg_opa(g_password_page.page,0x26,0);
    lv_obj_add_flag(g_password_page.page,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_password_page.page,password_outside_cb,LV_EVENT_CLICKED,NULL);
    lv_obj_add_event_cb(g_password_page.page, password_deleted_cb,
                        LV_EVENT_DELETE, NULL);
    lv_pin_keypad_create(&g_password_page.keypad,g_password_page.page,412,18);
    lv_nav_button_mark_back(g_password_page.keypad.cancel);
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

void ui_page_05_set_password_open(void)
{
    if(ui_page_05_set_password_is_open())return;
    if(!ui_page_05_set_password_resume())ui_page_05_set_password_create(lv_layer_top());
}
bool ui_page_05_set_password_is_open(void)
{
    return lv_pin_keypad_is_visible(&g_password_page.keypad);
}
bool ui_page_05_set_password_request_back(void)
{
    if(!ui_page_05_set_password_is_open())return false;
    password_cancel(NULL);return true;
}
