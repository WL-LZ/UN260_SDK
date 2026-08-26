#include "page_18_pure.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/lv_page_event.h"
#include "un260/lv_components/smart_island.h"
#include "un260/counting/counting_data_store.h"
#include "un260/lv_system/ui_lang.h"
#include "un260/lv_system/ui_text.h"
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(MONTSERRAT_EXTRABOLD_35);
LV_FONT_DECLARE(MONTSERRAT_Medium_15);
LV_FONT_DECLARE(lv_font_rajdhani_194);
LV_FONT_DECLARE(lv_font_rajdhani_142);
LV_FONT_DECLARE(lv_font_rajdhani_27);

typedef struct {
    lv_obj_t *page;
    lv_obj_t *amount_value;
    lv_obj_t *pcs_value;
    lv_obj_t *reject_value;
    lv_obj_t *amount_label;
    lv_obj_t *total_top_label;
    lv_obj_t *value_label;
    lv_obj_t *pcs_label;
    lv_obj_t *total_bottom_label;
    lv_obj_t *pieces_label;
    lv_obj_t *reject_label;
    lv_obj_t *start_btn_label;
    lv_obj_t *clear_btn_label;
    lv_timer_t *refresh_timer;
    language_t language;
    char displayed_amount[32];
    char displayed_pcs[32];
    int displayed_reject;
    bool values_valid;
    bool exiting;
} pure_page_context_t;

static pure_page_context_t g_pure_page = {
    .language = LANGUAGE_EN,
};

static void pure_page_context_reset(void)
{
    memset(&g_pure_page, 0, sizeof(g_pure_page));
    g_pure_page.language = LANGUAGE_EN;
}

static void pure_btn_touch_feedback_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* btn = lv_event_get_target(e);
    uint32_t i;
    uint32_t child_cnt;
    lv_opa_t text_opa;

    if (btn == NULL || !lv_obj_is_valid(btn)) {
        return;
    }

    if (code == LV_EVENT_PRESSED) {
        lv_obj_set_style_bg_opa(btn, LV_OPA_70, 0);
        text_opa = LV_OPA_70;
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        text_opa = LV_OPA_COVER;
    } else {
        return;
    }

    child_cnt = lv_obj_get_child_cnt(btn);
    for (i = 0; i < child_cnt; i++) {
        lv_obj_t* child = lv_obj_get_child(btn, i);
        if (child && lv_obj_is_valid(child)) {
            lv_obj_set_style_text_opa(child, text_opa, 0);
        }
    }
}

static void pure_format_amount(char* buf, size_t size, float amount)
{
    int len;
    int dest_index;

    if (buf == NULL || size == 0U) {
        return;
    }

    if (amount < 0.0f) {
        amount = 0.0f;
    }

    snprintf(buf, size, "%.0f", amount);

    len = (int)strlen(buf);
    if (len <= 3) {
        return;
    }

    {
        char temp[32];
        int i;

        strncpy(temp, buf, sizeof(temp) - 1);
        temp[sizeof(temp) - 1] = '\0';

        dest_index = 0;
        for (i = 0; i < len && dest_index < (int)size - 1; i++) {
            buf[dest_index++] = temp[i];
            if (i < len - 1 && ((len - i - 1) % 3) == 0 && dest_index < (int)size - 1) {
                buf[dest_index++] = ',';
            }
        }
        buf[dest_index] = '\0';
    }
}

static void pure_format_pcs(char* buf, size_t size, int pcs)
{
    int len;
    int dest_index;

    if (buf == NULL || size == 0U) {
        return;
    }

    if (pcs < 0) {
        pcs = 0;
    }

    snprintf(buf, size, "%d", pcs);

    len = (int)strlen(buf);
    if (len <= 3) {
        return;
    }

    {
        char temp[32];
        int i;

        strncpy(temp, buf, sizeof(temp) - 1);
        temp[sizeof(temp) - 1] = '\0';

        dest_index = 0;
        for (i = 0; i < len && dest_index < (int)size - 1; i++) {
            buf[dest_index++] = temp[i];
            if (i < len - 1 && ((len - i - 1) % 3) == 0 && dest_index < (int)size - 1) {
                buf[dest_index++] = ',';
            }
        }
        buf[dest_index] = '\0';
    }
}

static void pure_refresh_values(void)
{
    char amount_buf[32];
    char pcs_buf[32];
    int reject_cnt = counting_data_reject_pcs_count(counting_data_current());

    pure_format_amount(amount_buf, sizeof(amount_buf), counting_data_current()->total_amount);
    pure_format_pcs(pcs_buf, sizeof(pcs_buf), counting_data_current()->total_pcs);

    if ((!g_pure_page.values_valid ||
         strcmp(g_pure_page.displayed_amount, amount_buf) != 0) &&
        g_pure_page.amount_value && lv_obj_is_valid(g_pure_page.amount_value)) {
        lv_label_set_text(g_pure_page.amount_value, amount_buf);
    }

    if ((!g_pure_page.values_valid ||
         strcmp(g_pure_page.displayed_pcs, pcs_buf) != 0) &&
        g_pure_page.pcs_value && lv_obj_is_valid(g_pure_page.pcs_value)) {
        lv_label_set_text(g_pure_page.pcs_value, pcs_buf);
    }

    if ((!g_pure_page.values_valid ||
         g_pure_page.displayed_reject != reject_cnt) &&
        g_pure_page.reject_value && lv_obj_is_valid(g_pure_page.reject_value)) {
        lv_label_set_text_fmt(g_pure_page.reject_value, "%d", reject_cnt);
    }

    lv_snprintf(g_pure_page.displayed_amount,
                sizeof(g_pure_page.displayed_amount), "%s", amount_buf);
    lv_snprintf(g_pure_page.displayed_pcs,
                sizeof(g_pure_page.displayed_pcs), "%s", pcs_buf);
    g_pure_page.displayed_reject = reject_cnt;
    g_pure_page.values_valid = true;
}

static void pure_refresh_language_texts(void)
{
    if (g_pure_page.amount_label && lv_obj_is_valid(g_pure_page.amount_label)) {
        lv_label_set_text(g_pure_page.amount_label, ui_text_get(UI_TEXT_WIDGET_PURE_AMOUNT));
    }
    if (g_pure_page.total_top_label && lv_obj_is_valid(g_pure_page.total_top_label)) {
        lv_label_set_text(g_pure_page.total_top_label, ui_text_get(UI_TEXT_WIDGET_PURE_TOTAL));
    }
    if (g_pure_page.value_label && lv_obj_is_valid(g_pure_page.value_label)) {
        lv_label_set_text(g_pure_page.value_label, ui_text_get(UI_TEXT_WIDGET_PURE_VALUE));
    }
    if (g_pure_page.pcs_label && lv_obj_is_valid(g_pure_page.pcs_label)) {
        lv_label_set_text(g_pure_page.pcs_label, ui_text_get(UI_TEXT_WIDGET_PURE_PCS));
    }
    if (g_pure_page.total_bottom_label && lv_obj_is_valid(g_pure_page.total_bottom_label)) {
        lv_label_set_text(g_pure_page.total_bottom_label, ui_text_get(UI_TEXT_WIDGET_PURE_TOTAL));
    }
    if (g_pure_page.pieces_label && lv_obj_is_valid(g_pure_page.pieces_label)) {
        lv_label_set_text(g_pure_page.pieces_label, ui_text_get(UI_TEXT_WIDGET_PURE_PIECES));
    }
    if (g_pure_page.reject_label && lv_obj_is_valid(g_pure_page.reject_label)) {
        lv_label_set_text(g_pure_page.reject_label, ui_text_get(UI_TEXT_WIDGET_PURE_REJECT));
    }
    if (g_pure_page.start_btn_label && lv_obj_is_valid(g_pure_page.start_btn_label)) {
        lv_label_set_text(g_pure_page.start_btn_label, ui_text_get(UI_TEXT_WIDGET_PURE_START));
    }
    if (g_pure_page.clear_btn_label && lv_obj_is_valid(g_pure_page.clear_btn_label)) {
        lv_label_set_text(g_pure_page.clear_btn_label, ui_text_get(UI_TEXT_WIDGET_PURE_CLEAR));
    }
}

static void pure_refresh_timer_cb(lv_timer_t* t)
{
    language_t language;

    (void)t;
    language = ui_lang_get();
    if (g_pure_page.language != language) {
        g_pure_page.language = language;
        pure_refresh_language_texts();
    }
    pure_refresh_values();
}

void ui_page_18_pure_create(lv_obj_t* parent)
{
    lv_obj_t* bg = NULL;
    lv_obj_t* btn_start = NULL;
    lv_obj_t* btn_clear = NULL;
    lv_coord_t reject_num_h = 0;

    if (parent == NULL) {
        parent = lv_scr_act();
    }

    if (g_pure_page.page && lv_obj_is_valid(g_pure_page.page)) {
        return;
    }

    ui_page_18_pure_destroy();

    g_pure_page.page = lv_obj_create(parent);
    lv_obj_remove_style_all(g_pure_page.page);
    lv_obj_set_size(g_pure_page.page, 1280, 400);
    lv_obj_set_pos(g_pure_page.page, 0, 0);
    lv_obj_clear_flag(g_pure_page.page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(g_pure_page.page, lv_color_hex(0xFFFFFF), 0);

    bg = lv_img_create(g_pure_page.page);
    lv_img_set_src(bg, "L:/usr/local/share/lvgl_data/page_pure.png");
    lv_obj_set_pos(bg, 0, 0);

    g_pure_page.amount_label = lv_label_create(g_pure_page.page);
    lv_label_set_text(g_pure_page.amount_label, ui_text_get(UI_TEXT_WIDGET_PURE_AMOUNT));
    lv_obj_set_pos(g_pure_page.amount_label, 68, 75);
    lv_obj_set_style_text_font(g_pure_page.amount_label, &MONTSERRAT_EXTRABOLD_35, 0);
    lv_obj_set_style_text_color(g_pure_page.amount_label, lv_color_hex(0x636363), 0);

    g_pure_page.total_top_label = lv_label_create(g_pure_page.page);
    lv_label_set_text(g_pure_page.total_top_label, ui_text_get(UI_TEXT_WIDGET_PURE_TOTAL));
    lv_obj_set_pos(g_pure_page.total_top_label, 70, 114);
    lv_obj_set_style_text_font(g_pure_page.total_top_label, &MONTSERRAT_Medium_15, 0);
    lv_obj_set_style_text_color(g_pure_page.total_top_label, lv_color_hex(0x505050), 0);

    g_pure_page.value_label = lv_label_create(g_pure_page.page);
    lv_label_set_text(g_pure_page.value_label, ui_text_get(UI_TEXT_WIDGET_PURE_VALUE));
    lv_obj_set_pos(g_pure_page.value_label, 141, 114);
    lv_obj_set_style_text_font(g_pure_page.value_label, &MONTSERRAT_Medium_15, 0);
    lv_obj_set_style_text_color(g_pure_page.value_label, lv_color_hex(0x505050), 0);

    g_pure_page.amount_value = lv_label_create(g_pure_page.page);
    lv_label_set_text(g_pure_page.amount_value, "0");
    lv_obj_set_pos(g_pure_page.amount_value, 256, 66);
    lv_obj_set_size(g_pure_page.amount_value, 935, 125);
    lv_obj_set_style_text_align(g_pure_page.amount_value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(g_pure_page.amount_value, &lv_font_rajdhani_194, 0);
    lv_obj_set_style_text_color(g_pure_page.amount_value, lv_color_hex(0x000000), 0);

    g_pure_page.pcs_label = lv_label_create(g_pure_page.page);
    lv_label_set_text(g_pure_page.pcs_label, ui_text_get(UI_TEXT_WIDGET_PURE_PCS));
    lv_obj_set_pos(g_pure_page.pcs_label, 68, 248);
    lv_obj_set_style_text_font(g_pure_page.pcs_label, &MONTSERRAT_EXTRABOLD_35, 0);
    lv_obj_set_style_text_color(g_pure_page.pcs_label, lv_color_hex(0x636363), 0);

    g_pure_page.total_bottom_label = lv_label_create(g_pure_page.page);
    lv_label_set_text(g_pure_page.total_bottom_label, ui_text_get(UI_TEXT_WIDGET_PURE_TOTAL));
    lv_obj_set_pos(g_pure_page.total_bottom_label, 70, 287);
    lv_obj_set_style_text_font(g_pure_page.total_bottom_label, &MONTSERRAT_Medium_15, 0);
    lv_obj_set_style_text_color(g_pure_page.total_bottom_label, lv_color_hex(0x505050), 0);

    g_pure_page.pieces_label = lv_label_create(g_pure_page.page);
    lv_label_set_text(g_pure_page.pieces_label, ui_text_get(UI_TEXT_WIDGET_PURE_PIECES));
    lv_obj_set_pos(g_pure_page.pieces_label, 141, 287);
    lv_obj_set_style_text_font(g_pure_page.pieces_label, &MONTSERRAT_Medium_15, 0);
    lv_obj_set_style_text_color(g_pure_page.pieces_label, lv_color_hex(0x505050), 0);

    g_pure_page.pcs_value = lv_label_create(g_pure_page.page);
    lv_label_set_text(g_pure_page.pcs_value, "0");
    lv_obj_set_pos(g_pure_page.pcs_value, 495, 250);
    lv_obj_set_size(g_pure_page.pcs_value, 684, 91);
    lv_obj_set_style_text_align(g_pure_page.pcs_value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(g_pure_page.pcs_value, &lv_font_rajdhani_142, 0);
    lv_obj_set_style_text_color(g_pure_page.pcs_value, lv_color_hex(0x000000), 0);

    g_pure_page.reject_label = lv_label_create(g_pure_page.page);
    lv_label_set_text(g_pure_page.reject_label, ui_text_get(UI_TEXT_WIDGET_PURE_REJECT));
    lv_obj_set_pos(g_pure_page.reject_label, 70, 361);
    lv_obj_set_style_text_font(g_pure_page.reject_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(g_pure_page.reject_label, lv_color_hex(0xFF7171), 0);

    g_pure_page.reject_value = lv_label_create(g_pure_page.page);
    lv_label_set_text(g_pure_page.reject_value, "0");
    reject_num_h = lv_font_get_line_height(&lv_font_montserrat_18);
    lv_obj_set_size(g_pure_page.reject_value, 50, reject_num_h);
    lv_obj_set_pos(g_pure_page.reject_value, 183, 361);
    lv_obj_set_style_text_align(g_pure_page.reject_value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(g_pure_page.reject_value, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(g_pure_page.reject_value, lv_color_hex(0xFF7171), 0);

    btn_start = lv_btn_create(g_pure_page.page);
    lv_obj_set_pos(btn_start, 1105, 361);
    lv_obj_set_size(btn_start, 131, 36);
    lv_obj_clear_flag(btn_start, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(btn_start, 8, 0);
    lv_obj_set_style_bg_color(btn_start, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(btn_start, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_start, 2, 0);
    lv_obj_set_style_border_color(btn_start, lv_color_hex(0x818181), 0);
    lv_obj_set_style_shadow_width(btn_start, 0, 0);
    lv_obj_add_event_cb(btn_start, page_01_start_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_start, pure_btn_touch_feedback_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(btn_start, pure_btn_touch_feedback_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(btn_start, pure_btn_touch_feedback_cb, LV_EVENT_PRESS_LOST, NULL);

    g_pure_page.start_btn_label = lv_label_create(btn_start);
    lv_label_set_text(g_pure_page.start_btn_label, ui_text_get(UI_TEXT_WIDGET_PURE_START));
    lv_obj_set_style_text_font(g_pure_page.start_btn_label, &lv_font_rajdhani_27, 0);
    lv_obj_set_style_text_color(g_pure_page.start_btn_label, lv_color_hex(0x818181), 0);
    lv_obj_center(g_pure_page.start_btn_label);

    btn_clear = lv_btn_create(g_pure_page.page);
    lv_obj_set_pos(btn_clear, 961, 361);
    lv_obj_set_size(btn_clear, 131, 36);
    lv_obj_clear_flag(btn_clear, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(btn_clear, 8, 0);
    lv_obj_set_style_bg_color(btn_clear, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(btn_clear, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_clear, 0, 0);
    lv_obj_set_style_shadow_width(btn_clear, 0, 0);
    lv_obj_add_event_cb(btn_clear, page_01_esc_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_clear, pure_btn_touch_feedback_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(btn_clear, pure_btn_touch_feedback_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(btn_clear, pure_btn_touch_feedback_cb, LV_EVENT_PRESS_LOST, NULL);

    g_pure_page.clear_btn_label = lv_label_create(btn_clear);
    lv_label_set_text(g_pure_page.clear_btn_label, ui_text_get(UI_TEXT_WIDGET_PURE_CLEAR));
    lv_obj_set_style_text_font(g_pure_page.clear_btn_label, &lv_font_rajdhani_27, 0);
    lv_obj_set_style_text_color(g_pure_page.clear_btn_label, lv_color_hex(0x818181), 0);
    lv_obj_center(g_pure_page.clear_btn_label);

    g_pure_page.language = ui_lang_get();
    pure_refresh_language_texts();

    pure_refresh_values();

    if (g_pure_page.refresh_timer == NULL) {
        g_pure_page.refresh_timer = lv_timer_create(pure_refresh_timer_cb, 100, NULL);
    }

    smart_island_create(g_pure_page.page);
}

void ui_page_18_pure_request_exit(void)
{
    if (g_pure_page.page == NULL || !lv_obj_is_valid(g_pure_page.page)) {
        ui_manager_switch(UI_PAGE_MAIN);
        return;
    }

    if (g_pure_page.exiting) {
        return;
    }

    g_pure_page.exiting = true;
    smart_island_close();
    ui_manager_switch(UI_PAGE_MAIN);
    g_pure_page.exiting = false;
}

void ui_page_18_pure_destroy(void)
{
    if (g_pure_page.refresh_timer) {
        lv_timer_del(g_pure_page.refresh_timer);
        g_pure_page.refresh_timer = NULL;
    }

    if (g_pure_page.page && smart_island_is_attached_to(g_pure_page.page)) {
        smart_island_destroy();
    }

    if (g_pure_page.page && lv_obj_is_valid(g_pure_page.page)) {
        lv_obj_del(g_pure_page.page);
    }

    pure_page_context_reset();
}
