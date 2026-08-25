#include "page_21_set_language.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_system/ui_lang.h"
#include "un260/lv_system/ui_text.h"

#include <stddef.h>
#include <stdint.h>

#define LANGUAGE_ITEM_H 62
#define LANGUAGE_ITEM_GAP 10

typedef struct {
    language_t lang;
    const char* code;
    ui_text_id_t name_text;
    ui_text_id_t region_text;
    ui_text_id_t sample_text;
} language_option_t;

typedef struct {
    lv_obj_t* card;
    lv_obj_t* check;
} language_item_t;

static const language_option_t g_language_options[] = {
    { LANGUAGE_EN, "EN", UI_TEXT_SETTINGS_LANGUAGE_ENGLISH,
      UI_TEXT_SETTINGS_LANGUAGE_REGION_US, UI_TEXT_SETTINGS_LANGUAGE_SAMPLE_READY },
};
#define LANGUAGE_OPTION_COUNT (sizeof(g_language_options) / sizeof(g_language_options[0]))

static lv_obj_t* language_page = NULL;
static lv_obj_t* status_label = NULL;
static lv_obj_t* list_area = NULL;
static lv_obj_t* summary_code_label = NULL;
static lv_obj_t* preview_code_label = NULL;
static lv_obj_t* preview_name_label = NULL;
static lv_obj_t* preview_sample_label = NULL;
static language_item_t g_language_items[LANGUAGE_OPTION_COUNT] = { 0 };

static const language_option_t* language_find_option(language_t lang)
{
    for (size_t i = 0; i < LANGUAGE_OPTION_COUNT; i++) {
        if (g_language_options[i].lang == lang) {
            return &g_language_options[i];
        }
    }

    return &g_language_options[0];
}

static void language_refresh_view(void)
{
    language_t current = ui_lang_get();
    const language_option_t* current_option = language_find_option(current);
    const char* current_name = ui_text_get(current_option->name_text);

    for (size_t i = 0; i < LANGUAGE_OPTION_COUNT; i++) {
        bool selected = (current == g_language_options[i].lang);
        settings_detail_set_select_box_checked(g_language_items[i].check, selected);
        settings_detail_set_select_box_active(g_language_items[i].check, selected);

        if (g_language_items[i].card) {
            lv_obj_set_style_bg_color(g_language_items[i].card,
                                      selected ? lv_color_hex(0xE3FAFD) : lv_color_hex(0xFFFFFF),
                                      0);
            lv_obj_set_style_border_color(g_language_items[i].card,
                                          selected ? lv_color_hex(0x0878C8) : lv_color_hex(0xDDE6EF),
                                          0);
            lv_obj_set_style_border_width(g_language_items[i].card, selected ? 2 : 1, 0);
        }
    }

    if (status_label) {
        lv_label_set_text_fmt(status_label,
                              ui_text_get(UI_TEXT_SETTINGS_LANGUAGE_ACTIVE_FMT),
                              current_name);
    }
    if (summary_code_label) lv_label_set_text(summary_code_label, current_option->code);
    if (preview_code_label) lv_label_set_text(preview_code_label, current_option->code);
    if (preview_name_label) lv_label_set_text(preview_name_label, current_name);
    if (preview_sample_label) {
        lv_label_set_text(preview_sample_label, ui_text_get(current_option->sample_text));
    }
}

static void language_esc_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ui_manager_pop_page();
}

static void language_option_cb(lv_event_t* e)
{
    language_t lang;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    lang = (language_t)(uintptr_t)lv_event_get_user_data(e);
    ui_lang_set(lang);
    /* Cached pages retain their label objects. Rebuild them lazily after a
     * language change so hidden pages cannot resume with stale text. */
    ui_manager_invalidate_all_page_caches();
    language_refresh_view();
}

static lv_obj_t* language_create_card(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                                      lv_coord_t w, lv_coord_t h)
{
    lv_obj_t* card = settings_detail_create_card(parent, x, y, w, h);
    lv_obj_set_style_shadow_width(card, 10, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_10, 0);
    return card;
}

static void language_create_option(lv_obj_t* parent, size_t index,
                                   const language_option_t* option)
{
    lv_coord_t y = (lv_coord_t)(index * (LANGUAGE_ITEM_H + LANGUAGE_ITEM_GAP));
    lv_obj_t* item = lv_obj_create(parent);
    lv_obj_remove_style_all(item);
    lv_obj_set_pos(item, 0, y);
    lv_obj_set_size(item, 682, LANGUAGE_ITEM_H);
    lv_obj_set_style_bg_color(item, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(item, 1, 0);
    lv_obj_set_style_border_color(item, lv_color_hex(0xDDE6EF), 0);
    lv_obj_set_style_radius(item, 6, 0);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(item, language_option_cb, LV_EVENT_CLICKED,
                        (void*)(uintptr_t)option->lang);

    lv_obj_t* code = lv_obj_create(item);
    lv_obj_remove_style_all(code);
    lv_obj_set_pos(code, 20, 14);
    lv_obj_set_size(code, 40, 34);
    lv_obj_set_style_bg_color(code, lv_color_hex(0xF6FBFF), 0);
    lv_obj_set_style_bg_opa(code, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(code, 1, 0);
    lv_obj_set_style_border_color(code, lv_color_hex(0xDDEBFF), 0);
    lv_obj_set_style_radius(code, 5, 0);
    lv_obj_clear_flag(code, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* code_label = settings_detail_create_label(code, option->code,
                                                        &lv_font_instrument_sans_medium_14,
                                                        lv_color_hex(0x0878C8), 0, 0);
    lv_obj_center(code_label);

    settings_detail_create_label(item, ui_text_get(option->name_text), &lv_font_instrument_sans_medium_18,
                                 lv_color_hex(0x0D3440), 82, 15);
    settings_detail_create_label(item, ui_text_get(option->region_text), &lv_font_instrument_sans_medium_14,
                                 lv_color_hex(0x5686A5), 196, 18);

    g_language_items[index].card = item;
    g_language_items[index].check = settings_detail_create_select_box(item, 630, 16, 30,
                                                                      language_option_cb,
                                                                      (void*)(uintptr_t)option->lang);
}

static void language_create_list(lv_obj_t* parent)
{
    list_area = lv_obj_create(parent);
    lv_obj_remove_style_all(list_area);
    lv_obj_set_pos(list_area, 24, 88);
    lv_obj_set_size(list_area, 682, 188);
    lv_obj_set_style_bg_opa(list_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list_area, 0, 0);
    lv_obj_set_scroll_dir(list_area, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list_area, LV_SCROLLBAR_MODE_OFF);

    for (size_t i = 0; i < LANGUAGE_OPTION_COUNT; i++) {
        language_create_option(list_area, i, &g_language_options[i]);
    }
}

static void language_create_panel(lv_obj_t* parent)
{
    lv_obj_t* card = language_create_card(parent, 38, 18, 730, 306);

    lv_obj_t* badge = lv_obj_create(card);
    lv_obj_remove_style_all(badge);
    lv_obj_set_pos(badge, 24, 22);
    lv_obj_set_size(badge, 46, 46);
    lv_obj_set_style_bg_color(badge, lv_color_hex(0xE3FAFD), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(badge, 1, 0);
    lv_obj_set_style_border_color(badge, lv_color_hex(0xB9EEF6), 0);
    lv_obj_set_style_radius(badge, 8, 0);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);

    summary_code_label = settings_detail_create_label(badge, "", &lv_font_instrument_sans_medium_18,
                                                      lv_color_hex(0x0878C8), 0, 0);
    lv_obj_center(summary_code_label);

    settings_detail_create_label(card, ui_text_get(UI_TEXT_SETTINGS_LANGUAGE_DISPLAY),
                                 &lv_font_instrument_sans_medium_20, lv_color_hex(0x0D3440), 88, 22);
    settings_detail_create_label(card, ui_text_get(UI_TEXT_SETTINGS_LANGUAGE_SUBTITLE),
                                 &lv_font_instrument_sans_medium_14, lv_color_hex(0x5686A5), 90, 52);

    status_label = settings_detail_create_label(card, "", &lv_font_instrument_sans_medium_14,
                                                lv_color_hex(0x0878C8), 520, 42);

    language_create_list(card);
}

static void language_create_preview(lv_obj_t* parent)
{
    lv_obj_t* card = settings_detail_create_card(parent, 820, 18, 370, 306);
    lv_obj_set_style_shadow_width(card, 8, 0);

    lv_obj_t* header = lv_obj_create(card);
    lv_obj_remove_style_all(header);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, 370, 42);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x08C5D6), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    settings_detail_create_label(header, ui_text_get(UI_TEXT_SETTINGS_LANGUAGE_PREVIEW),
                                 &lv_font_instrument_sans_medium_18,
                                 lv_color_hex(0xFFFFFF), 150, 12);

    lv_obj_t* code_box = lv_obj_create(card);
    lv_obj_remove_style_all(code_box);
    lv_obj_set_pos(code_box, 112, 70);
    lv_obj_set_size(code_box, 146, 96);
    lv_obj_set_style_bg_color(code_box, lv_color_hex(0x0878C8), 0);
    lv_obj_set_style_bg_opa(code_box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(code_box, 8, 0);
    lv_obj_clear_flag(code_box, LV_OBJ_FLAG_SCROLLABLE);

    preview_code_label = settings_detail_create_label(code_box, "", &lv_font_instrument_sans_medium_40,
                                                      lv_color_hex(0xFFFFFF), 0, 0);
    lv_obj_center(preview_code_label);

    preview_name_label = settings_detail_create_label(card, "", &lv_font_instrument_sans_medium_24,
                                                      lv_color_hex(0x0D3440), 0, 190);
    lv_obj_set_width(preview_name_label, 370);
    lv_obj_set_style_text_align(preview_name_label, LV_TEXT_ALIGN_CENTER, 0);

    preview_sample_label = settings_detail_create_label(card, "", &lv_font_instrument_sans_medium_16,
                                                        lv_color_hex(0x0878C8), 0, 226);
    lv_obj_set_width(preview_sample_label, 370);
    lv_obj_set_style_text_align(preview_sample_label, LV_TEXT_ALIGN_CENTER, 0);
}

void ui_page_21_set_language_create(lv_obj_t* parent)
{
    lv_obj_t* content = NULL;

    if (language_page) return;

    language_page = settings_detail_create_page(parent,
                                                ui_text_get(UI_TEXT_SETTINGS_LANGUAGE_TITLE),
                                                language_esc_cb, &content);

    language_create_panel(content);
    language_create_preview(content);
    language_refresh_view();
}

void ui_page_21_set_language_destroy(void)
{
    if (language_page && lv_obj_is_valid(language_page)) {
        lv_obj_del(language_page);
    }

    language_page = NULL;
    status_label = NULL;
    list_area = NULL;
    summary_code_label = NULL;
    preview_code_label = NULL;
    preview_name_label = NULL;
    preview_sample_label = NULL;
    for (size_t i = 0; i < LANGUAGE_OPTION_COUNT; i++) {
        g_language_items[i].card = NULL;
        g_language_items[i].check = NULL;
    }
}
