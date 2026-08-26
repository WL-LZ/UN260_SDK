#include "page_13_upgrade.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_system/ui_text.h"

static lv_obj_t* page_13 = NULL;

static void esc_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ui_manager_pop_page();
}

static void main_upgrade_enter_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ui_manager_push_page(UI_PAGE_MAIN_UPGRADE);
}

static void image_upgrade_enter_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ui_manager_push_page(UI_PAGE_IMAGE_UPGRADE);
}

static void ui_upgrade_enter_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ui_manager_push_page(UI_PAGE_UI_UPGRADE);
}

void ui_page_13_upgrade_create(lv_obj_t* parent)
{
    if (page_13) return;

    lv_obj_t* content = NULL;
    page_13 = settings_detail_create_page(parent, ui_text_get(UI_TEXT_SETTINGS_UPGRADE),
                                          esc_cb, &content);

    lv_obj_t* card = settings_detail_create_card(content, 380, 42, 520, 260);
    settings_detail_create_button(card, 80, 42, 360, 56,
                                  ui_text_get(UI_TEXT_SETTINGS_MAIN_BOARD_UPGRADE),
                                  lv_color_hex(0x08C5D6),
                                  main_upgrade_enter_cb, NULL);
    settings_detail_create_button(card, 80, 110, 360, 56,
                                  ui_text_get(UI_TEXT_SETTINGS_IMAGE_BOARD_UPGRADE),
                                  lv_color_hex(0x08C5D6),
                                  image_upgrade_enter_cb, NULL);
    settings_detail_create_button(card, 80, 178, 360, 56,
                                  ui_text_get(UI_TEXT_SETTINGS_UI_UPGRADE),
                                  lv_color_hex(0x08C5D6),
                                  ui_upgrade_enter_cb, NULL);
}

void ui_page_13_upgrade_destroy(void)
{
    if (page_13 && lv_obj_is_valid(page_13)) {
        lv_obj_del(page_13);
    }
    page_13 = NULL;
}
