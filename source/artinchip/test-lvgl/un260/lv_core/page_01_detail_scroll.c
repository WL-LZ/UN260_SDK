#include "un260/lv_core/page_01_detail_scroll.h"

#include <stdint.h>

#include "un260/lv_core/page_01_main.h"
#include "un260/counting/counting_data_store.h"
#include "un260/lv_system/counting_ui_runtime.h"

#define PAGE_01_SCROLL_HINT_UP_IMG_PATH       "L:/usr/local/share/lvgl_data/main_pol_up.png"
#define PAGE_01_SCROLL_HINT_DOWN_IMG_PATH     "L:/usr/local/share/lvgl_data/main_pol_down.png"
#define PAGE_01_SCROLL_HINT_UP_IMG_PATH_USB   "L:/mnt/usb/lvgl_data/main_pol_up.png"
#define PAGE_01_SCROLL_HINT_DOWN_IMG_PATH_USB "L:/mnt/usb/lvgl_data/main_pol_down.png"
#define PAGE_01_DETAIL_ROW_Y_OFFSET           6
#define PAGE_01_DETAIL_SCROLL_EDGE_BUFFER     32

static lv_obj_t* s_scroll_spacer = NULL;
static lv_obj_t* s_scroll_hint_up = NULL;
static lv_obj_t* s_scroll_hint_down = NULL;
static bool s_scroll_hint_up_show = false;
static bool s_scroll_hint_down_show = false;
static lv_coord_t s_detail_scroll_y[PAGE_01_DETAIL_SECTION_C + 1] = { 0 };
static uint16_t s_detail_first_row_cache[PAGE_01_DETAIL_SECTION_C + 1] = { 0 };
static int16_t s_detail_last_render_first_row[PAGE_01_DETAIL_SECTION_C + 1] = { -1, -1, -1 };

static int page_01_detail_row_count_get(page_01_detail_section_t section);
static uint8_t page_01_detail_visible_row_limit_get(page_01_detail_section_t section);
static void page_01_scroll_hint_refresh(void);

static bool page_01_detail_section_is_valid(int section)
{
    return section >= PAGE_01_DETAIL_SECTION_A && section <= PAGE_01_DETAIL_SECTION_C;
}

static bool page_01_scroll_container_is_valid(void)
{
    lv_obj_t *scroll_container = page_01_main_scroll_obj();

    return scroll_container && lv_obj_is_valid(scroll_container);
}

static lv_coord_t page_01_detail_view_h_get(page_01_detail_section_t section)
{
    return section == PAGE_01_DETAIL_SECTION_A ? 240 : 273;
}

static lv_coord_t page_01_detail_content_h_get(page_01_detail_section_t section)
{
    return PAGE_01_DETAIL_ROW_Y_OFFSET
        + (lv_coord_t)page_01_detail_row_count_get(section) * page_01_detail_row_gap_get(section)
        + PAGE_01_DETAIL_SCROLL_EDGE_BUFFER;
}

static bool page_01_set_hint_img_src(lv_obj_t* img_obj, const char* primary_path,
                                     const char* fallback_path)
{
    lv_img_header_t header;

    if (!img_obj || !lv_obj_is_valid(img_obj)) return false;
    if (lv_img_decoder_get_info(primary_path, &header) == LV_RES_OK) {
        lv_img_set_src(img_obj, primary_path);
        return true;
    }
    if (fallback_path && lv_img_decoder_get_info(fallback_path, &header) == LV_RES_OK) {
        lv_img_set_src(img_obj, fallback_path);
        return true;
    }
    return false;
}

static uint8_t page_01_detail_visible_row_limit_get(page_01_detail_section_t section)
{
    return section == PAGE_01_DETAIL_SECTION_A ? 8 : 9;
}

int page_01_detail_row_gap_get(int section)
{
    return section == PAGE_01_DETAIL_SECTION_A ? 32 : 31;
}

static int page_01_detail_row_count_get(page_01_detail_section_t section)
{
    int count = 0;

    switch (section) {
    case PAGE_01_DETAIL_SECTION_A:
        for (int i = 0; i < counting_data_current()->denom_number &&
             i < (int)(sizeof(counting_data_current()->denom) / sizeof(counting_data_current()->denom[0])); i++) {
            if (counting_data_current()->denom[i].value > 0) count++;
        }
        if (count > 10) count = 10;
        break;
    case PAGE_01_DETAIL_SECTION_B:
        if (counting_data_current()->sn_str != NULL) {
            int row_limit = counting_data_current()->total_pcs;
            const int mix_capacity = (int)(sizeof(counting_data_current()->denom_mix) / sizeof(counting_data_current()->denom_mix[0]));

            if (row_limit > counting_data_current()->sn_capacity) row_limit = counting_data_current()->sn_capacity;
            if (row_limit > mix_capacity) row_limit = mix_capacity;
            if (row_limit < 0) row_limit = 0;
            for (int i = 0; i < row_limit; i++) {
                if (counting_data_current()->sn_str[i] != NULL && counting_data_current()->denom_mix[i] > 0) count++;
            }
        }
        break;
    case PAGE_01_DETAIL_SECTION_C:
        count = counting_data_error_detail_count(counting_data_current());
        if (count > 10) count = 10;
        break;
    default:
        break;
    }
    return count;
}

static lv_coord_t page_01_detail_max_scroll_y_get(page_01_detail_section_t section)
{
    lv_coord_t content_h;
    lv_coord_t view_h;

    if (page_01_detail_row_count_get(section) <= page_01_detail_visible_row_limit_get(section)) {
        return 0;
    }
    content_h = page_01_detail_content_h_get(section);
    view_h = page_01_detail_view_h_get(section);
    return content_h > view_h ? content_h - view_h : 0;
}

int page_01_detail_scroll_first_row_get(int section)
{
    lv_coord_t scroll_top;
    lv_coord_t max_scroll;

    if (!page_01_detail_section_is_valid(section)) return 0;
    if (!page_01_scroll_container_is_valid()) {
        return (int)s_detail_first_row_cache[section];
    }

    scroll_top = lv_obj_get_scroll_top(page_01_main_scroll_obj());
    if (scroll_top < 0) scroll_top = 0;
    max_scroll = page_01_detail_max_scroll_y_get((page_01_detail_section_t)section);
    if (scroll_top > max_scroll) scroll_top = max_scroll;
    s_detail_first_row_cache[section] =
        (uint16_t)(scroll_top / page_01_detail_row_gap_get(section));
    return (int)s_detail_first_row_cache[section];
}

bool page_01_is_small_denom_mode(void)
{
    page_01_detail_section_t section = page_01_detail_section_get();

    if (!page_01_detail_section_is_valid(section)) return false;
    return page_01_detail_row_count_get(section) <= page_01_detail_visible_row_limit_get(section);
}

static void page_01_detail_scroll_spacer_refresh(page_01_detail_section_t section)
{
    lv_coord_t spacer_y;

    if (!s_scroll_spacer || !lv_obj_is_valid(s_scroll_spacer)) return;
    spacer_y = page_01_detail_content_h_get(section);
    lv_obj_set_pos(s_scroll_spacer, 0, spacer_y > 0 ? spacer_y : 1);
}

static void page_01_detail_scroll_save_current(void)
{
    page_01_detail_section_t section = page_01_detail_section_get();
    lv_coord_t scroll_top;

    if (!page_01_scroll_container_is_valid() || !page_01_detail_section_is_valid(section)) return;
    scroll_top = lv_obj_get_scroll_top(page_01_main_scroll_obj());
    if (scroll_top < 0) scroll_top = 0;
    s_detail_scroll_y[section] = scroll_top;
    s_detail_first_row_cache[section] =
        (uint16_t)(scroll_top / page_01_detail_row_gap_get(section));
}

static void page_01_detail_scroll_restore_current(bool anim_en)
{
    page_01_detail_section_t section = page_01_detail_section_get();
    lv_coord_t target_scroll;
    lv_coord_t max_scroll;

    if (!page_01_scroll_container_is_valid() || !page_01_detail_section_is_valid(section)) return;
    lv_obj_set_height(page_01_main_scroll_obj(), page_01_detail_view_h_get(section));
    page_01_detail_scroll_spacer_refresh(section);
    max_scroll = page_01_detail_max_scroll_y_get(section);
    target_scroll = s_detail_scroll_y[section];
    if (target_scroll < 0) target_scroll = 0;
    if (target_scroll > max_scroll) target_scroll = max_scroll;
    s_detail_scroll_y[section] = target_scroll;
    lv_obj_scroll_to_y(page_01_main_scroll_obj(), target_scroll,
                       anim_en ? LV_ANIM_ON : LV_ANIM_OFF);
}

void page_01_detail_scroll_before_section_switch(void)
{
    page_01_detail_scroll_save_current();
}

void page_01_detail_scroll_after_section_switch(void)
{
    page_01_detail_scroll_restore_current(false);
    page_01_scroll_hint_on_enter();
}

void page_01_detail_scroll_sync_current_section(void)
{
    page_01_detail_scroll_restore_current(false);
    page_01_scroll_hint_on_enter();
}

void page_01_detail_scroll_reset_all(void)
{
    for (int i = 0; i <= PAGE_01_DETAIL_SECTION_C; i++) {
        s_detail_scroll_y[i] = 0;
        s_detail_first_row_cache[i] = 0;
        s_detail_last_render_first_row[i] = -1;
    }
    if (page_01_scroll_container_is_valid()) {
        page_01_main_scroll_reset();
    }
    page_01_scroll_hint_force_hide();
}

static void page_01_scroll_hint_sync_visible(void)
{
    if (s_scroll_hint_up && lv_obj_is_valid(s_scroll_hint_up)) {
        if (s_scroll_hint_up_show) lv_obj_clear_flag(s_scroll_hint_up, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_scroll_hint_up, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_scroll_hint_down && lv_obj_is_valid(s_scroll_hint_down)) {
        if (s_scroll_hint_down_show) lv_obj_clear_flag(s_scroll_hint_down, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_scroll_hint_down, LV_OBJ_FLAG_HIDDEN);
    }
}

static void page_01_scroll_hint_refresh(void)
{
    const lv_coord_t bottom_epsilon = 2;
    page_01_detail_section_t section = page_01_detail_section_get();
    lv_coord_t top_hidden;
    lv_coord_t max_scroll;
    int row_count;
    int visible_limit;
    bool top_show;
    bool bottom_show;

    if (!page_01_scroll_container_is_valid() || !page_01_detail_section_is_valid(section)) return;
    row_count = page_01_detail_row_count_get(section);
    visible_limit = page_01_detail_visible_row_limit_get(section);
    max_scroll = page_01_detail_max_scroll_y_get(section);
    top_hidden = lv_obj_get_scroll_top(page_01_main_scroll_obj());
    if (top_hidden < 0) top_hidden = 0;
    top_show = top_hidden > 0;
    bottom_show = row_count > visible_limit && top_hidden + bottom_epsilon < max_scroll;
    if (row_count <= visible_limit) {
        top_show = false;
        bottom_show = false;
    }
    if (s_scroll_hint_up_show != top_show || s_scroll_hint_down_show != bottom_show) {
        s_scroll_hint_up_show = top_show;
        s_scroll_hint_down_show = bottom_show;
        page_01_scroll_hint_sync_visible();
    }
}

static void page_01_scroll_hint_event_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_SCROLL) {
        page_01_detail_section_t section = page_01_detail_section_get();
        page_01_detail_scroll_save_current();
        if (section == PAGE_01_DETAIL_SECTION_B) {
            int first_row = (int)s_detail_first_row_cache[section];
            if (s_detail_last_render_first_row[section] != first_row) {
                s_detail_last_render_first_row[section] = (int16_t)first_row;
                page_01_main_detail_refresh_rows_only();
            }
        }
        page_01_scroll_hint_refresh();
        return;
    }

    if (code == LV_EVENT_SCROLL_END) {
        page_01_detail_section_t section = page_01_detail_section_get();

        if (page_01_detail_section_is_valid(section) && page_01_is_small_denom_mode() &&
            page_01_scroll_container_is_valid()) {
            lv_obj_scroll_to_y(page_01_main_scroll_obj(), 0, LV_ANIM_ON);
            s_detail_scroll_y[section] = 0;
            page_01_scroll_hint_refresh();
            return;
        }
        page_01_detail_scroll_save_current();
        page_01_scroll_hint_refresh();
    }
}

void page_01_scroll_hint_on_enter(void)
{
    page_01_scroll_hint_refresh();
}

void page_01_scroll_hint_force_hide(void)
{
    s_scroll_hint_up_show = false;
    s_scroll_hint_down_show = false;
    page_01_scroll_hint_sync_visible();
}

void page_01_detail_scroll_attach(lv_obj_t* page_parent, lv_obj_t* scroll_container)
{
    if (!page_parent || !scroll_container || !lv_obj_is_valid(page_parent) ||
        !lv_obj_is_valid(scroll_container)) return;

    s_scroll_spacer = lv_obj_create(scroll_container);
    lv_obj_remove_style_all(s_scroll_spacer);
    lv_obj_set_size(s_scroll_spacer, 1, 1);
    lv_obj_set_pos(s_scroll_spacer, 0, 241);
    lv_obj_set_style_bg_opa(s_scroll_spacer, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(s_scroll_spacer, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_flag(scroll_container, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(scroll_container, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_add_event_cb(scroll_container, page_01_scroll_hint_event_cb, LV_EVENT_SCROLL, NULL);
    lv_obj_add_event_cb(scroll_container, page_01_scroll_hint_event_cb, LV_EVENT_SCROLL_END, NULL);

    s_scroll_hint_up = lv_img_create(page_parent);
    lv_obj_remove_style_all(s_scroll_hint_up);
    lv_obj_set_pos(s_scroll_hint_up, 1051, 26);
    page_01_set_hint_img_src(s_scroll_hint_up, PAGE_01_SCROLL_HINT_UP_IMG_PATH,
                             PAGE_01_SCROLL_HINT_UP_IMG_PATH_USB);
    lv_obj_add_flag(s_scroll_hint_up, LV_OBJ_FLAG_HIDDEN);

    s_scroll_hint_down = lv_img_create(page_parent);
    lv_obj_remove_style_all(s_scroll_hint_down);
    lv_obj_set_pos(s_scroll_hint_down, 1051, 309);
    page_01_set_hint_img_src(s_scroll_hint_down, PAGE_01_SCROLL_HINT_DOWN_IMG_PATH,
                             PAGE_01_SCROLL_HINT_DOWN_IMG_PATH_USB);
    lv_obj_add_flag(s_scroll_hint_down, LV_OBJ_FLAG_HIDDEN);

    page_01_detail_scroll_sync_current_section();
}
