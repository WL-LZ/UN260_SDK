#include "un260/lv_resources/ui_page_background.h"
#include "un260/lv_core/page_06_settings.h"
#include "un260/lv_components/lv_nav_button.h"
#include "un260/app_service/app_command_runtime.h"
#include "un260/lv_system/app_clock.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/lv_page_event.h"
#include "un260/lv_core/page_09_cis_cala.h"
#include "un260/lv_core/page_27_set_cfd_level.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_system/machine_time.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/device_info/device_info.h"
#include "un260/data_collection/data_collection.h"
#include "un260/lv_drivers/lv_drivers.h"
#include "aic_ui/perf_stats.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static lv_obj_t* settings_page = NULL;

#define SETTINGS_SCREEN_W            1280
#define SETTINGS_SCREEN_H            400
#define SETTINGS_SIDEBAR_W           216
#define SETTINGS_HEADER_H            55
#define SETTINGS_FOOTER_H            48
#define SETTINGS_CONTENT_X           SETTINGS_SIDEBAR_W
#define SETTINGS_CONTENT_Y           SETTINGS_HEADER_H
#define SETTINGS_CONTENT_W           (SETTINGS_SCREEN_W - SETTINGS_SIDEBAR_W)
#define SETTINGS_CONTENT_H           (SETTINGS_SCREEN_H - SETTINGS_HEADER_H - SETTINGS_FOOTER_H)
#define SETTINGS_TILE_W              488
#define SETTINGS_TILE_H              42
#define SETTINGS_TILE_GAP_X          32
#define SETTINGS_TILE_GAP_Y          11
#define SETTINGS_INTERNAL_PAGE_MAX   32
#define SETTINGS_NAV_STACK_MAX       8
#define SETTINGS_NAV_TITLE_LEN       40
#define SETTINGS_OPTION_MAX          96

typedef page_06_settings_menu_t settings_menu_t;

#define SETTINGS_MENU_SYSTEM           PAGE_06_SETTINGS_MENU_SYSTEM
#define SETTINGS_MENU_MAINTENANCE      PAGE_06_SETTINGS_MENU_MAINTENANCE
#define SETTINGS_MENU_USER             PAGE_06_SETTINGS_MENU_USER
#define SETTINGS_MENU_VERSION          PAGE_06_SETTINGS_MENU_VERSION
#define SETTINGS_MENU_DATA_COLLECTION  PAGE_06_SETTINGS_MENU_DATA_COLLECTION
#define SETTINGS_MENU_COUNT            PAGE_06_SETTINGS_MENU_COUNT

typedef struct {
    const char* title;
    const char* icon;
} settings_menu_info_t;

typedef struct {
    lv_obj_t* page;
    page_06_settings_menu_t menu;
    page_06_settings_sub_page_t sub_page;
    char title[SETTINGS_NAV_TITLE_LEN];
} settings_internal_page_t;

typedef struct {
    lv_obj_t* page;
    page_06_settings_menu_t menu;
    page_06_settings_sub_page_t sub_page;
    char title[SETTINGS_NAV_TITLE_LEN];
} settings_nav_item_t;

typedef struct {
    lv_obj_t* tile;
    lv_obj_t* rail;
    lv_obj_t* title_label;
    lv_obj_t* no_label;
    page_06_settings_menu_t menu;
    bool context_internal;
    page_06_settings_sub_page_t context_sub_page;
    int col;
    int row;
    bool selectable;
} settings_option_item_t;

static const settings_menu_info_t g_menu_info[SETTINGS_MENU_COUNT] = {
    { "SYSTEM",          LV_SYMBOL_SETTINGS },
    { "MAINTENANCE",     LV_SYMBOL_EDIT },
    { "USER",            LV_SYMBOL_HOME },
    { "VERSION",         LV_SYMBOL_FILE },
    { "DATA COLLECTION", LV_SYMBOL_DIRECTORY },
};

static lv_obj_t* root = NULL;
static lv_obj_t* sidebar = NULL;
static lv_obj_t* content_host = NULL;
static lv_obj_t* footer = NULL;
static lv_obj_t* menu_btns[SETTINGS_MENU_COUNT] = { NULL };
static lv_obj_t* menu_icons[SETTINGS_MENU_COUNT] = { NULL };
static lv_obj_t* menu_labels[SETTINGS_MENU_COUNT] = { NULL };
static lv_obj_t* pages[SETTINGS_MENU_COUNT] = { NULL };
static settings_internal_page_t internal_pages[SETTINGS_INTERNAL_PAGE_MAX];
static int internal_page_count = 0;
static settings_nav_item_t nav_stack[SETTINGS_NAV_STACK_MAX];
static int nav_stack_depth = 0;
static lv_obj_t* active_content_page = NULL;
static page_06_settings_menu_t active_content_menu = PAGE_06_SETTINGS_MENU_SYSTEM;
static page_06_settings_sub_page_t active_content_sub_page = PAGE_06_SETTINGS_SUB_NONE;
static char active_content_title[SETTINGS_NAV_TITLE_LEN] = { 0 };
static settings_option_item_t option_items[SETTINGS_OPTION_MAX];
static int option_item_count = 0;

static page_06_settings_menu_t saved_menu_index = PAGE_06_SETTINGS_MENU_SYSTEM;
static bool saved_content_internal = false;
static page_06_settings_sub_page_t saved_content_sub_page = PAGE_06_SETTINGS_SUB_NONE;
static char saved_content_title[SETTINGS_NAV_TITLE_LEN] = { 0 };
static page_06_settings_menu_t saved_option_menu = PAGE_06_SETTINGS_MENU_SYSTEM;
static bool saved_option_internal = false;
static page_06_settings_sub_page_t saved_option_context_sub_page = PAGE_06_SETTINGS_SUB_NONE;
static int saved_option_col = -1;
static int saved_option_row = -1;

static lv_obj_t* breadcrumb_current = NULL;
static lv_obj_t* status_dot = NULL;
static lv_obj_t* status_label = NULL;
static lv_obj_t* footer_status_label = NULL;
static lv_obj_t* footer_time_label = NULL;
static lv_timer_t* footer_time_timer = NULL;

#define SETTINGS_STATUS_TEXT_CAPACITY 32
static char settings_status_text[SETTINGS_STATUS_TEXT_CAPACITY] = "READY";
static lv_color_t settings_status_color;
static bool settings_status_dirty = true;
static bool data_collection_dirty = true;
static bool data_collection_snapshot_valid = false;
static data_collect_mode_t data_collection_last_mode;
static bool data_collection_last_pending;
static int data_collection_last_pcs;
static char data_collection_last_status[128];

static int current_menu_index = -1;

static lv_obj_t* dc_btn_all = NULL;
static lv_obj_t* dc_btn_false = NULL;
static lv_obj_t* dc_btn_start = NULL;
static lv_obj_t* dc_btn_disable = NULL;
static lv_obj_t* dc_label_all = NULL;
static lv_obj_t* dc_label_false = NULL;
static lv_obj_t* dc_check_all = NULL;
static lv_obj_t* dc_check_false = NULL;
static lv_obj_t* dc_mode_value_label = NULL;
static lv_obj_t* dc_pcs_label = NULL;
static lv_obj_t* dc_status_label = NULL;
static lv_obj_t* version_value_labels[6] = { NULL };

static void page_06_update_menu_state(int index);
static void page_06_switch_sub_page(int index);
static bool page_06_is_valid_menu(page_06_settings_menu_t menu);
static void page_06_reset_saved_navigation(void);
static bool page_06_find_page_info(lv_obj_t* page, settings_nav_item_t* out);
static lv_obj_t* page_06_find_internal_page(page_06_settings_menu_t menu,
                                            page_06_settings_sub_page_t sub_page);
static const char* page_06_get_menu_title(page_06_settings_menu_t menu);
static void page_06_show_content_page(page_06_settings_menu_t menu,
                                      lv_obj_t* page,
                                      page_06_settings_sub_page_t sub_page,
                                      const char* title,
                                      bool push_current);
static void page_06_set_selected_option(page_06_settings_menu_t menu,
                                        bool context_internal,
                                        page_06_settings_sub_page_t context_sub_page,
                                        int col, int row);

static lv_color_t color_bg(void)       { return lv_color_hex(0xF7F8FA); }
static lv_color_t color_panel(void)    { return lv_color_hex(0xFFFFFF); }
static lv_color_t color_line(void)     { return lv_color_hex(0xE9EDF2); }
static lv_color_t color_primary(void)  { return lv_color_hex(0x08C5D6); }
static lv_color_t color_primary_2(void){ return lv_color_hex(0xE3FAFD); }
static lv_color_t color_text(void)     { return lv_color_hex(0x0D3440); }
static lv_color_t color_muted(void)    { return lv_color_hex(0x5686A5); }
static lv_color_t color_option_no(void)
{
    return lv_color_hex(0x9AB6C2);
}
static lv_color_t color_tile_idle(void){ return lv_color_hex(0xF8F9FB); }
static lv_color_t color_rail_idle(void){ return lv_color_hex(0x5F6E7D); }

static void style_plain(lv_obj_t* obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
}

static bool settings_page_is_visible(void)
{
    return settings_page != NULL && lv_obj_is_valid(settings_page) &&
           !lv_obj_has_flag(settings_page, LV_OBJ_FLAG_HIDDEN);
}

static void settings_apply_status(void)
{
    if (status_label) {
        lv_label_set_text(status_label, settings_status_text);
        lv_obj_set_style_text_color(status_label, settings_status_color, 0);
    }

    if (status_dot) {
        lv_obj_set_style_bg_color(status_dot, settings_status_color, 0);
    }

    if (footer_status_label) {
        lv_label_set_text(footer_status_label, settings_status_text);
        lv_obj_set_style_text_color(footer_status_label, settings_status_color, 0);
    }

    settings_status_dirty = false;
}

static void settings_set_status(const char* text, lv_color_t color)
{
    if (text == NULL) text = "";
    if (!settings_status_dirty && strcmp(settings_status_text, text) == 0 &&
        memcmp(&settings_status_color, &color, sizeof(color)) == 0) {
        return;
    }
    snprintf(settings_status_text, sizeof(settings_status_text), "%s", text);
    settings_status_color = color;
    settings_status_dirty = true;
    if (settings_page_is_visible()) settings_apply_status();
}

void page_06_settings_set_status(const char* text, lv_color_t color)
{
    settings_set_status(text, color);
}

static bool page_06_is_valid_menu(page_06_settings_menu_t menu)
{
    return (int)menu >= 0 && menu < PAGE_06_SETTINGS_MENU_COUNT;
}

static const char* page_06_get_menu_title(page_06_settings_menu_t menu)
{
    if (!page_06_is_valid_menu(menu)) {
        return "";
    }

    if (menu == PAGE_06_SETTINGS_MENU_USER) {
        return ui_text_get(UI_TEXT_SETTINGS_MENU_PREFERENCE);
    }

    return g_menu_info[menu].title;
}

static void page_06_reset_saved_navigation(void)
{
    saved_menu_index = PAGE_06_SETTINGS_MENU_SYSTEM;
    saved_content_internal = false;
    saved_content_sub_page = PAGE_06_SETTINGS_SUB_NONE;
    saved_content_title[0] = '\0';
    saved_option_menu = PAGE_06_SETTINGS_MENU_SYSTEM;
    saved_option_internal = false;
    saved_option_context_sub_page = PAGE_06_SETTINGS_SUB_NONE;
    saved_option_col = -1;
    saved_option_row = -1;
}

static void page_06_copy_title(char* dst, size_t dst_size, const char* title)
{
    if (!dst || dst_size == 0) {
        return;
    }

    lv_snprintf(dst, dst_size, "%s", title ? title : "");
}

static void page_06_set_breadcrumb(page_06_settings_menu_t menu, const char* title)
{
    char buf[80];

    if (!breadcrumb_current || !page_06_is_valid_menu(menu)) {
        return;
    }

    if (!title || title[0] == '\0' || strcmp(title, page_06_get_menu_title(menu)) == 0) {
        lv_label_set_text(breadcrumb_current, page_06_get_menu_title(menu));
        return;
    }

    lv_snprintf(buf, sizeof(buf), "%s > %s", page_06_get_menu_title(menu), title);
    lv_label_set_text(breadcrumb_current, buf);
}

static bool page_06_find_page_info(lv_obj_t* page, settings_nav_item_t* out)
{
    if (!page) {
        return false;
    }

    for (int i = 0; i < SETTINGS_MENU_COUNT; i++) {
        if (pages[i] == page) {
            if (out) {
                out->page = page;
                out->menu = (page_06_settings_menu_t)i;
                out->sub_page = PAGE_06_SETTINGS_SUB_NONE;
                page_06_copy_title(out->title, sizeof(out->title),
                                   page_06_get_menu_title((page_06_settings_menu_t)i));
            }
            return true;
        }
    }

    for (int i = 0; i < internal_page_count; i++) {
        if (internal_pages[i].page == page) {
            if (out) {
                out->page = page;
                out->menu = internal_pages[i].menu;
                out->sub_page = internal_pages[i].sub_page;
                page_06_copy_title(out->title, sizeof(out->title), internal_pages[i].title);
            }
            return true;
        }
    }

    return false;
}

static lv_obj_t* page_06_find_internal_page(page_06_settings_menu_t menu,
                                            page_06_settings_sub_page_t sub_page)
{
    if (!page_06_is_valid_menu(menu) || sub_page == PAGE_06_SETTINGS_SUB_NONE) {
        return NULL;
    }

    for (int i = 0; i < internal_page_count; i++) {
        if (internal_pages[i].menu == menu &&
            internal_pages[i].sub_page == sub_page) {
            return internal_pages[i].page;
        }
    }

    return NULL;
}

static void page_06_hide_all_content_pages(void)
{
    for (int i = 0; i < SETTINGS_MENU_COUNT; i++) {
        if (pages[i]) {
            lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    for (int i = 0; i < internal_page_count; i++) {
        if (internal_pages[i].page) {
            lv_obj_add_flag(internal_pages[i].page, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void page_06_show_content_page(page_06_settings_menu_t menu,
                                      lv_obj_t* page,
                                      page_06_settings_sub_page_t sub_page,
                                      const char* title,
                                      bool push_current)
{
    if (!page || !page_06_is_valid_menu(menu)) {
        return;
    }

    if (push_current && active_content_page && nav_stack_depth < SETTINGS_NAV_STACK_MAX) {
        nav_stack[nav_stack_depth].page = active_content_page;
        nav_stack[nav_stack_depth].menu = active_content_menu;
        nav_stack[nav_stack_depth].sub_page = active_content_sub_page;
        page_06_copy_title(nav_stack[nav_stack_depth].title,
                           sizeof(nav_stack[nav_stack_depth].title),
                           active_content_title);
        nav_stack_depth++;
    }

    page_06_hide_all_content_pages();
    lv_obj_clear_flag(page, LV_OBJ_FLAG_HIDDEN);

    page_06_update_menu_state(menu);
    page_06_set_breadcrumb(menu, title);
    active_content_page = page;
    active_content_menu = menu;
    active_content_sub_page = sub_page;
    saved_menu_index = menu;
    page_06_copy_title(active_content_title, sizeof(active_content_title), title);

    saved_content_internal = (pages[menu] != page);
    saved_content_sub_page = saved_content_internal ? sub_page : PAGE_06_SETTINGS_SUB_NONE;
    page_06_copy_title(saved_content_title, sizeof(saved_content_title),
                       saved_content_internal ? title : "");
}

static void page_06_apply_option_style(lv_obj_t* tile, lv_obj_t* rail,
                                       lv_obj_t* title_label, bool selected)
{
    if (!tile) {
        return;
    }

    lv_obj_set_style_bg_color(tile, selected ? lv_color_hex(0xD3F2F8) : color_tile_idle(), 0);
    lv_obj_set_style_shadow_width(tile, selected ? 0 : 10, 0);

    if (rail) {
        lv_obj_set_style_bg_color(rail, selected ? color_primary() : color_rail_idle(), 0);
        lv_obj_clear_flag(rail, LV_OBJ_FLAG_HIDDEN);
    }

    if (title_label) {
        lv_obj_set_x(title_label, selected ? 24 : 28);
    }
}

static void page_06_refresh_option_state(void)
{
    for (int i = 0; i < option_item_count; i++) {
        bool selected = option_items[i].selectable &&
                        option_items[i].menu == saved_option_menu &&
                        option_items[i].context_internal == saved_option_internal &&
                        option_items[i].context_sub_page == saved_option_context_sub_page &&
                        option_items[i].col == saved_option_col &&
                        option_items[i].row == saved_option_row;

        page_06_apply_option_style(option_items[i].tile,
                                   option_items[i].rail,
                                   option_items[i].title_label,
                                   selected);
    }
}

static bool page_06_option_same_context(const settings_option_item_t* a,
                                        const settings_option_item_t* b)
{
    return a && b &&
           a->menu == b->menu &&
           a->context_internal == b->context_internal &&
           a->context_sub_page == b->context_sub_page;
}

static void page_06_refresh_option_numbers(void)
{
    for (int i = 0; i < option_item_count; i++) {
        int no = 1;
        char no_buf[4];

        if (!option_items[i].no_label) {
            continue;
        }

        for (int col = 0; col < option_items[i].col; col++) {
            for (int j = 0; j < option_item_count; j++) {
                if (option_items[j].no_label &&
                    page_06_option_same_context(&option_items[i], &option_items[j]) &&
                    option_items[j].col == col) {
                    no++;
                }
            }
        }

        for (int j = 0; j < option_item_count; j++) {
            if (option_items[j].no_label &&
                page_06_option_same_context(&option_items[i], &option_items[j]) &&
                option_items[j].col == option_items[i].col &&
                option_items[j].row < option_items[i].row) {
                no++;
            }
        }

        lv_snprintf(no_buf, sizeof(no_buf), "%02d", no);
        lv_label_set_text(option_items[i].no_label, no_buf);
    }
}

static void page_06_set_selected_option(page_06_settings_menu_t menu,
                                        bool context_internal,
                                        page_06_settings_sub_page_t context_sub_page,
                                        int col, int row)
{
    saved_option_menu = menu;
    saved_option_internal = context_internal;
    saved_option_context_sub_page = context_internal ? context_sub_page : PAGE_06_SETTINGS_SUB_NONE;
    saved_option_col = col;
    saved_option_row = row;
    page_06_refresh_option_state();
}

static void option_select_event_cb(lv_event_t* e)
{
    lv_obj_t* tile = lv_event_get_target(e);

    for (int i = 0; i < option_item_count; i++) {
        if (option_items[i].tile == tile && option_items[i].selectable) {
            saved_menu_index = option_items[i].menu;
            page_06_set_selected_option(option_items[i].menu,
                                        option_items[i].context_internal,
                                        option_items[i].context_sub_page,
                                        option_items[i].col,
                                        option_items[i].row);
            return;
        }
    }
}

static void page_06_register_option(lv_obj_t* tile, lv_obj_t* rail,
                                    lv_obj_t* title_label, lv_obj_t* no_label,
                                    lv_obj_t* parent,
                                    int col, int row, bool selectable)
{
    settings_nav_item_t info;

    if (!tile || option_item_count >= SETTINGS_OPTION_MAX ||
        !page_06_find_page_info(parent, &info)) {
        return;
    }

    option_items[option_item_count].tile = tile;
    option_items[option_item_count].rail = rail;
    option_items[option_item_count].title_label = title_label;
    option_items[option_item_count].no_label = no_label;
    option_items[option_item_count].menu = info.menu;
    option_items[option_item_count].context_internal = (pages[info.menu] != parent);
    option_items[option_item_count].context_sub_page =
        option_items[option_item_count].context_internal ? info.sub_page : PAGE_06_SETTINGS_SUB_NONE;
    option_items[option_item_count].col = col;
    option_items[option_item_count].row = row;
    option_items[option_item_count].selectable = selectable;
    option_item_count++;

    if (selectable) {
        lv_obj_add_event_cb(tile, option_select_event_cb, LV_EVENT_CLICKED, NULL);
    }

    page_06_refresh_option_numbers();
    page_06_refresh_option_state();
}

static void update_footer_time(void)
{
    machine_time_value_t now;
    char buf[16];

    machine_time_get(&now);
    lv_snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
                (unsigned)now.hour, (unsigned)now.minute, (unsigned)now.second);

    if (footer_time_label) {
        lv_label_set_text(footer_time_label, buf);
    }
}

static void footer_time_timer_cb(lv_timer_t* timer)
{
    (void)timer;
    if (!settings_page_is_visible()) return;
    update_footer_time();
}

static lv_obj_t* create_label(lv_obj_t* parent, const char* text,
                              const lv_font_t* font, lv_color_t color)
{
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_SCROLLABLE);
    return label;
}



static void create_header(lv_obj_t* parent)
{
    lv_obj_t* header = lv_obj_create(parent);
    style_plain(header);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, SETTINGS_SCREEN_W, SETTINGS_HEADER_H);
    lv_obj_set_style_bg_color(header, color_panel(), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header, 0, 0);

    lv_obj_t* gear = create_label(header, LV_SYMBOL_SETTINGS, &lv_font_montserrat_18, color_primary());
    lv_obj_set_pos(gear, 36, 19);

    lv_obj_t* title = create_label(header, "SETTINGS", &lv_font_instrument_sans_semibold_16, color_muted());
    lv_obj_set_style_text_letter_space(title, 1, 0);
    lv_obj_set_pos(title, 65, 20);

    lv_obj_t* divider = lv_obj_create(header);
    style_plain(divider);
    lv_obj_set_pos(divider, SETTINGS_SIDEBAR_W - 1, 0);
    lv_obj_set_size(divider, 1, SETTINGS_HEADER_H);
    lv_obj_set_style_bg_color(divider, color_line(), 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);

    lv_obj_t* crumb_base = create_label(header, "SETTINGS  >  ", &lv_font_instrument_sans_medium_16, color_muted());
    lv_obj_set_style_text_letter_space(crumb_base, 1, 0);
    lv_obj_set_pos(crumb_base, 258, 25);

    breadcrumb_current = create_label(header, "SYSTEM", &lv_font_instrument_sans_medium_16, color_primary());
    lv_obj_set_style_text_letter_space(breadcrumb_current, 1, 0);
    lv_obj_set_width(breadcrumb_current, 650);
    lv_label_set_long_mode(breadcrumb_current, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(breadcrumb_current, 398, 25);

    status_dot = lv_obj_create(header);
    style_plain(status_dot);
    lv_obj_set_pos(status_dot, 1146, 28);
    lv_obj_set_size(status_dot, 5, 5);
    lv_obj_set_style_radius(status_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(status_dot, lv_color_hex(0x24D6A1), 0);
    lv_obj_set_style_bg_opa(status_dot, LV_OPA_COVER, 0);

    status_label = create_label(header, "READY", &lv_font_instrument_sans_medium_12, lv_color_hex(0x24D6A1));
    lv_obj_set_pos(status_label, 1160, 23);
}

static void create_sidebar_button(lv_obj_t* parent, settings_menu_t index, lv_coord_t y)
{
    lv_obj_t* btn = lv_obj_create(parent);
    style_plain(btn);
    lv_obj_set_pos(btn, 0, y);
    lv_obj_set_size(btn, SETTINGS_SIDEBAR_W, 50);
    lv_obj_set_style_bg_color(btn, color_panel(), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);

    menu_btns[index] = btn;

    menu_icons[index] = create_label(btn, g_menu_info[index].icon, &lv_font_montserrat_18, color_muted());
    lv_obj_set_pos(menu_icons[index], 15, 16);

    menu_labels[index] = create_label(btn, page_06_get_menu_title(index), &lv_font_instrument_sans_bold_14, color_muted());
    lv_obj_set_style_text_letter_space(menu_labels[index], 1, 0);
    lv_obj_set_pos(menu_labels[index], 65, 18);
}

static void menu_btn_event_cb(lv_event_t* e)
{
    int index = (int)(uintptr_t)lv_event_get_user_data(e);

    if (index == current_menu_index) {
        return;
    }

    page_06_update_menu_state(index);
    page_06_switch_sub_page(index);
}

static void create_sidebar(lv_obj_t* parent)
{
    sidebar = lv_obj_create(parent);
    style_plain(sidebar);
    lv_obj_set_pos(sidebar, 0, SETTINGS_HEADER_H);
    lv_obj_set_size(sidebar, SETTINGS_SIDEBAR_W, SETTINGS_SCREEN_H - SETTINGS_HEADER_H - SETTINGS_FOOTER_H);
    lv_obj_set_style_bg_color(sidebar, color_panel(), 0);
    lv_obj_set_style_bg_opa(sidebar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sidebar, 0, 0);

    for (int i = 0; i < SETTINGS_MENU_COUNT; i++) {
        create_sidebar_button(sidebar, (settings_menu_t)i, i * 50);
        lv_obj_add_event_cb(menu_btns[i], menu_btn_event_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
    }
}

static void page_06_update_menu_state(int index)
{
    for (int i = 0; i < SETTINGS_MENU_COUNT; i++) {
        bool active = (i == index);

        if (!menu_btns[i]) {
            continue;
        }

        lv_obj_set_style_bg_color(menu_btns[i], active ? color_primary_2() : color_panel(), 0);
        lv_obj_set_style_text_color(menu_icons[i], active ? color_primary() : color_muted(), 0);
        lv_obj_set_style_text_color(menu_labels[i], active ? color_text() : color_muted(), 0);
    }

    if (breadcrumb_current && index >= 0 && index < SETTINGS_MENU_COUNT) {
        lv_label_set_text(breadcrumb_current, page_06_get_menu_title((page_06_settings_menu_t)index));
    }

    current_menu_index = index;
}

static lv_obj_t* create_page(lv_obj_t* parent)
{
    lv_obj_t* page = lv_obj_create(parent);
    style_plain(page);
    lv_obj_set_pos(page, 0, 0);
    lv_obj_set_size(page, SETTINGS_CONTENT_W, SETTINGS_CONTENT_H);
    lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    return page;
}

static lv_obj_t* create_tile_ex(lv_obj_t* parent, int col, int row,
                                const char* title, const char* value,
                                bool accent, lv_event_cb_t cb, void* user_data,
                                lv_obj_t** value_label_out)
{
    lv_coord_t x = 31 + col * (SETTINGS_TILE_W + SETTINGS_TILE_GAP_X);
    lv_coord_t y = 18 + row * (SETTINGS_TILE_H + SETTINGS_TILE_GAP_Y);
    lv_obj_t* tile = lv_obj_create(parent);
    lv_obj_t* rail = NULL;
    lv_obj_t* no_label = NULL;
    bool show_no = cb != NULL || (value && strcmp(value, ">") == 0);

    style_plain(tile);
    lv_obj_set_pos(tile, x, y);
    lv_obj_set_size(tile, SETTINGS_TILE_W, SETTINGS_TILE_H);
    lv_obj_set_style_bg_color(tile, cb ? color_tile_idle() : color_panel(), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_set_style_radius(tile, 0, 0);
    lv_obj_set_style_shadow_width(tile, 10, 0);
    lv_obj_set_style_shadow_opa(tile, LV_OPA_10, 0);
    lv_obj_set_style_shadow_ofs_y(tile, 4, 0);

    if (cb) {
        lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
    }

    if (cb) {
        rail = lv_obj_create(tile);
        style_plain(rail);
        lv_obj_set_pos(rail, 0, 0);
        lv_obj_set_size(rail, 4, SETTINGS_TILE_H);
        lv_obj_set_style_bg_color(rail, color_rail_idle(), 0);
        lv_obj_set_style_bg_opa(rail, LV_OPA_COVER, 0);
    }

    lv_obj_t* title_label = create_label(tile, title, &lv_font_instrument_sans_semibold_14, color_text());
    lv_obj_set_width(title_label, show_no ? 326 : 382);
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(title_label, cb ? 24 : 28, 14);

    if (show_no) {
        no_label = create_label(tile, "00", &lv_font_manrope_bold_16, color_option_no());
        lv_obj_set_width(no_label, 40);
        lv_obj_set_style_text_align(no_label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(no_label, 368, 12);
    }

    if (value_label_out != NULL) {
        *value_label_out = NULL;
    }
    if (value && value[0] != '\0') {
        lv_obj_t* value_label = create_label(tile, value, &lv_font_instrument_sans_medium_14, color_muted());
        lv_obj_align(value_label, LV_ALIGN_RIGHT_MID, -26, 0);
        if (value_label_out != NULL) {
            *value_label_out = value_label;
        }
    }

    (void)accent;
    page_06_register_option(tile, rail, title_label, no_label, parent, col, row, cb != NULL);

    if (cb) {
        lv_obj_add_event_cb(tile, cb, LV_EVENT_CLICKED, user_data);
    }

    return tile;
}

static lv_obj_t* create_tile(lv_obj_t* parent, int col, int row,
                             const char* title, const char* value,
                             bool accent, lv_event_cb_t cb, void* user_data)
{
    return create_tile_ex(parent, col, row, title, value, accent, cb,
                          user_data, NULL);
}

static void enter_page_event_cb(lv_event_t* e)
{
    ui_page_t page = (ui_page_t)(uintptr_t)lv_event_get_user_data(e);
    ui_manager_push_page(page);
}

lv_obj_t* page_06_settings_create_option(lv_obj_t* parent, int col, int row,
                                         const char* title, const char* value,
                                         bool accent, lv_event_cb_t cb,
                                         void* user_data)
{
    if (!parent) {
        return NULL;
    }

    return create_tile(parent, col, row, title, value, accent, cb, user_data);
}

lv_obj_t* page_06_settings_get_menu_page(page_06_settings_menu_t menu)
{
    if (!page_06_is_valid_menu(menu)) {
        return NULL;
    }

    return pages[menu];
}

lv_obj_t* page_06_settings_create_menu_option(page_06_settings_menu_t menu,
                                              int col, int row,
                                              const char* title,
                                              const char* value,
                                              bool accent,
                                              lv_event_cb_t cb,
                                              void* user_data)
{
    lv_obj_t* page = page_06_settings_get_menu_page(menu);

    if (!page) {
        return NULL;
    }

    return page_06_settings_create_option(page, col, row, title, value,
                                          accent, cb, user_data);
}

lv_obj_t* page_06_settings_create_page_option(page_06_settings_menu_t menu,
                                              int col, int row,
                                              const char* title,
                                              int page,
                                              bool accent)
{
    return page_06_settings_create_menu_option(menu, col, row, title, ">",
                                               accent, enter_page_event_cb,
                                               (void*)(uintptr_t)page);
}

bool page_06_settings_open_sub_page(lv_obj_t* sub_page)
{
    settings_nav_item_t info;

    if (!settings_page || !page_06_find_page_info(sub_page, &info)) {
        return false;
    }

    page_06_show_content_page(info.menu, info.page, info.sub_page, info.title, true);
    return true;
}

static void sub_page_link_event_cb(lv_event_t* e)
{
    lv_obj_t* sub_page = (lv_obj_t*)lv_event_get_user_data(e);
    (void)page_06_settings_open_sub_page(sub_page);
}

lv_obj_t* page_06_settings_create_sub_page(page_06_settings_menu_t menu,
                                           page_06_settings_sub_page_t sub_page,
                                           const char* title)
{
    lv_obj_t* page = NULL;

    if (!content_host || !page_06_is_valid_menu(menu) ||
        sub_page == PAGE_06_SETTINGS_SUB_NONE ||
        internal_page_count >= SETTINGS_INTERNAL_PAGE_MAX) {
        return NULL;
    }

    page = create_page(content_host);
    lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);

    internal_pages[internal_page_count].page = page;
    internal_pages[internal_page_count].menu = menu;
    internal_pages[internal_page_count].sub_page = sub_page;
    page_06_copy_title(internal_pages[internal_page_count].title,
                       sizeof(internal_pages[internal_page_count].title),
                       title);
    internal_page_count++;

    return page;
}

lv_obj_t* page_06_settings_create_sub_page_link(lv_obj_t* parent,
                                                int col, int row,
                                                const char* title,
                                                lv_obj_t* target_page,
                                                bool accent)
{
    if (!target_page || !page_06_find_page_info(target_page, NULL)) {
        return NULL;
    }

    return page_06_settings_create_option(parent, col, row, title, ">",
                                          accent, sub_page_link_event_cb,
                                          target_page);
}

bool page_06_settings_back_sub_page(void)
{
    settings_nav_item_t prev;

    if (!settings_page || nav_stack_depth <= 0) {
        return false;
    }

    nav_stack_depth--;
    prev = nav_stack[nav_stack_depth];
    page_06_show_content_page(prev.menu, prev.page, prev.sub_page, prev.title, false);
    return true;
}

static void create_system_page_content(lv_obj_t* parent)
{
    lv_obj_t* upgrade_page = page_06_settings_create_sub_page(SETTINGS_MENU_SYSTEM,
                                                              PAGE_06_SETTINGS_SUB_UPGRADE,
                                                              ui_text_get(UI_TEXT_SETTINGS_UPGRADE));
    create_tile(parent, 0, 0, ui_text_get(UI_TEXT_SETTINGS_DEBUG), ">", true,
                enter_page_event_cb, (void*)(uintptr_t)UI_PAGE_DEBUG);
    create_tile(parent, 1, 0, ui_text_get(UI_TEXT_SETTINGS_PRINT_SETTING), ">", true,
                enter_page_event_cb, (void*)(uintptr_t)UI_PAGE_PRINT_SETTING);

    page_06_settings_create_sub_page_link(parent, 0, 1, ui_text_get(UI_TEXT_SETTINGS_UPGRADE), upgrade_page, true);
    create_tile(parent, 1, 1, ui_text_get(UI_TEXT_SETTINGS_TIME_SETTING), ">", false,
                enter_page_event_cb, (void*)(uintptr_t)UI_PAGE_TIMESET);

    create_tile(parent, 0, 2, ui_text_get(UI_TEXT_SETTINGS_LANGUAGE_SETTING), ">", false,
                enter_page_event_cb, (void*)(uintptr_t)UI_PAGE_LANGUAGE_SETTING);
    create_tile(parent, 1, 2, ui_text_get(UI_TEXT_SETTINGS_AGING_SETTING), ">", false,
                enter_page_event_cb, (void*)(uintptr_t)UI_PAGE_AGING_SETTING);
    create_tile(parent, 0, 3, ui_text_get(UI_TEXT_SETTINGS_FACTORY_SETTING), ">", false,
                enter_page_event_cb, (void*)(uintptr_t)UI_PAGE_FACTORY_SETTING);
    create_tile(parent, 1, 3, "BRIGHTNESS", ">", false,
                enter_page_event_cb, (void*)(uintptr_t)UI_PAGE_BRIGHTNESS_SETTING);

    if (upgrade_page) {
        page_06_settings_create_option(upgrade_page, 0, 0,
                                       ui_text_get(UI_TEXT_SETTINGS_MAIN_BOARD_UPGRADE), ">",
                                       true, enter_page_event_cb, (void*)(uintptr_t)UI_PAGE_MAIN_UPGRADE);
        page_06_settings_create_option(upgrade_page, 1, 0,
                                       ui_text_get(UI_TEXT_SETTINGS_IMAGE_BOARD_UPGRADE), ">",
                                       true, enter_page_event_cb, (void*)(uintptr_t)UI_PAGE_IMAGE_UPGRADE);
        page_06_settings_create_option(upgrade_page, 0, 1,
                                       ui_text_get(UI_TEXT_SETTINGS_UI_UPGRADE), ">",
                                       true, enter_page_event_cb, (void*)(uintptr_t)UI_PAGE_UI_UPGRADE);
    }
}

static void create_maintenance_page_content(lv_obj_t* parent)
{
    create_tile(parent, 0, 0, ui_text_get(UI_TEXT_SETTINGS_CIS_CALIBRATION), ">", true, cis_enter_btn_cb, NULL);
    create_tile(parent, 1, 0, ui_text_get(UI_TEXT_SETTINGS_MOTOR_TEST), ">", true,
                enter_page_event_cb, (void*)(uintptr_t)UI_PAGE_MOTOR_TEST);
    create_tile(parent, 0, 1, ui_text_get(UI_TEXT_SETTINGS_SENSOR_PARAMETERS), ">", false,
                enter_page_event_cb, (void*)(uintptr_t)UI_PAGE_SENSOR);
    create_tile(parent, 0, 2, ui_text_get(UI_TEXT_SETTINGS_FLAP_SETTING), ">", false,
                enter_page_event_cb, (void*)(uintptr_t)UI_PAGE_FLAP_SETTING);
    create_tile(parent, 1, 1, ui_text_get(UI_TEXT_SETTINGS_IMAGE_GET_SETTING), ">", false,
                enter_page_event_cb, (void*)(uintptr_t)UI_PAGE_IMAGE_GET);
    create_tile(parent, 1, 2, ui_text_get(UI_TEXT_SETTINGS_WAVE_GET_SETTING), ">", false,
                enter_page_event_cb, (void*)(uintptr_t)UI_PAGE_WAVE_GET);
}

static void create_user_page_content(lv_obj_t* parent)
{
    create_tile(parent, 0, 3, "DISPLAY TEST", ">", false,
                enter_page_event_cb, (void*)(uintptr_t)UI_PAGE_DISPLAY_TEST);
    create_tile(parent, 1, 2, "STANDBY", ">", false,
                enter_page_event_cb, (void*)(uintptr_t)UI_PAGE_STANDBY_SETTING);
    create_tile(parent, 0, 0, ui_text_get(UI_TEXT_SETTINGS_PASSWORD), ">", true,
                enter_page_event_cb, (void*)(uintptr_t)UI_PAGE_PASSWORD_CHANGE);
    create_tile(parent, 1, 0, ui_text_get(UI_TEXT_SETTINGS_DOUBLE_NOTE_SETTING), ">", true,
                enter_page_event_cb, (void*)(uintptr_t)UI_PAGE_DOUBLE_NOTE_SETTING);
    create_tile(parent, 0, 1, ui_text_get(UI_TEXT_SETTINGS_REJECT_POCKET_SETTING), ">", false,
                enter_page_event_cb, (void*)(uintptr_t)UI_PAGE_REJECT_POCKET_SETTING);
    create_tile(parent, 1, 1, ui_text_get(UI_TEXT_SETTINGS_SERIAL_LEVEL_SETTING), ">", false,
                enter_page_event_cb, (void*)(uintptr_t)UI_PAGE_SERIAL_NUMBER_SETTING);
    create_tile(parent, 0, 2, ui_text_get(UI_TEXT_SETTINGS_CFD_LEVEL_SETTING), ">", false,
                enter_page_event_cb, (void*)(uintptr_t)UI_PAGE_CFD_LEVEL_SETTING);
}

static lv_obj_t* create_version_row(lv_obj_t* parent, int row,
                                    const char* title, const char* value,
                                    lv_obj_t** value_label)
{
    char text[96];
    lv_snprintf(text, sizeof(text), "%s", value ? value : "---");
    return create_tile_ex(parent, row % 2, row / 2, title, text, row < 2,
                          NULL, NULL, value_label);
}

static void create_version_page_content(lv_obj_t* parent)
{
    const char* unavailable = ui_text_get(UI_TEXT_SETTINGS_NOT_AVAILABLE);
    bool valid = device_info_is_valid();

    create_version_row(parent, 0, "Main App", valid ? device_info_main_app() : unavailable,
                       &version_value_labels[0]);
    create_version_row(parent, 1, "Image App", valid ? device_info_image_app() : unavailable,
                       &version_value_labels[1]);
    create_version_row(parent, 2, "FPGA", valid ? device_info_fpga() : unavailable,
                       &version_value_labels[2]);
    create_version_row(parent, 3, "Main BOOT", valid ? device_info_main_boot() : unavailable,
                       &version_value_labels[3]);
    create_version_row(parent, 4, "Image BOOT", valid ? device_info_image_boot() : unavailable,
                       &version_value_labels[4]);
    create_version_row(parent, 5, "Display App", device_info_display_app()[0] != '\0' ?
                       device_info_display_app() : unavailable, &version_value_labels[5]);
}

void ui_page_06_settings_refresh_data(uint32_t topics)
{
    const char* unavailable;
    const char* values[6];
    bool valid;
    int i;

    if ((topics & UI_DATA_TOPIC_DEVICE_VERSION) == 0U || settings_page == NULL) {
        return;
    }

    unavailable = ui_text_get(UI_TEXT_SETTINGS_NOT_AVAILABLE);
    valid = device_info_is_valid();
    values[0] = valid ? device_info_main_app() : unavailable;
    values[1] = valid ? device_info_image_app() : unavailable;
    values[2] = valid ? device_info_fpga() : unavailable;
    values[3] = valid ? device_info_main_boot() : unavailable;
    values[4] = valid ? device_info_image_boot() : unavailable;
    values[5] = device_info_display_app()[0] != '\0' ?
                device_info_display_app() : unavailable;

    for (i = 0; i < 6; i++) {
        if (version_value_labels[i] != NULL &&
            lv_obj_is_valid(version_value_labels[i]) &&
            strcmp(lv_label_get_text(version_value_labels[i]), values[i]) != 0) {
            lv_label_set_text(version_value_labels[i], values[i]);
        }
    }
}

static const char* get_data_collect_mode_name(data_collect_mode_t mode)
{
    switch (mode) {
    case DATA_COLLECT_MODE_ALL:
        return "ALL DATA";
    case DATA_COLLECT_MODE_FALSE:
        return "ERROR DATA";
    default:
        return "---";
    }
}

static void update_data_collect_btn_style(lv_obj_t* btn, lv_obj_t* label, lv_obj_t* check, bool selected)
{
    if (!btn || !label || !check) {
        return;
    }

    lv_obj_set_style_bg_color(btn, selected ? color_primary() : color_panel(), 0);
    lv_obj_set_style_border_color(btn, selected ? color_primary() : color_line(), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 4, 0);
    lv_obj_set_style_shadow_width(btn, selected ? 0 : 8, 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_10, 0);
    lv_obj_set_style_text_color(label, selected ? color_panel() : color_text(), 0);
    lv_obj_set_style_text_color(check, selected ? color_panel() : color_primary(), 0);
    lv_label_set_text(check, selected ? LV_SYMBOL_OK : "");
}

void page_06_data_collection_refresh(void)
{
    data_collect_mode_t mode;
    bool request_pending;
    int pcs;
    const char *status;
    bool mode_changed;
    bool pending_changed;
    bool pcs_changed;
    bool status_changed;

    if (!pages[SETTINGS_MENU_DATA_COLLECTION] || !settings_page_is_visible()) {
        data_collection_dirty = true;
        return;
    }

    mode = data_collection_state_mode();
    request_pending = data_collection_request_pending();
    pcs = data_collection_state_pcs();
    status = data_collection_state_status();
    if (status == NULL) status = "";
    mode_changed = data_collection_dirty || !data_collection_snapshot_valid ||
                   mode != data_collection_last_mode;
    pending_changed = data_collection_dirty || !data_collection_snapshot_valid ||
                      request_pending != data_collection_last_pending;
    pcs_changed = data_collection_dirty || !data_collection_snapshot_valid ||
                  pcs != data_collection_last_pcs;
    status_changed = data_collection_dirty || !data_collection_snapshot_valid ||
                     strcmp(status, data_collection_last_status) != 0;

    if (pending_changed) {
        lv_obj_t* action_buttons[] = {
            dc_btn_all, dc_btn_false, dc_btn_start, dc_btn_disable
        };
        for (size_t i = 0; i < sizeof(action_buttons) / sizeof(action_buttons[0]); i++) {
            if (!action_buttons[i] || !lv_obj_is_valid(action_buttons[i])) continue;
            if (request_pending) {
                lv_obj_add_state(action_buttons[i], LV_STATE_DISABLED);
            } else {
                lv_obj_clear_state(action_buttons[i], LV_STATE_DISABLED);
            }
        }
    }

    if (mode_changed) {
        update_data_collect_btn_style(dc_btn_all, dc_label_all, dc_check_all,
                                      mode == DATA_COLLECT_MODE_ALL);
        update_data_collect_btn_style(dc_btn_false, dc_label_false, dc_check_false,
                                      mode == DATA_COLLECT_MODE_FALSE);
        if (dc_mode_value_label) {
            lv_label_set_text(dc_mode_value_label, get_data_collect_mode_name(mode));
        }
    }

    if (pcs_changed && dc_pcs_label) {
        lv_label_set_text_fmt(dc_pcs_label, "PCS:%d", pcs);
    }

    if (status_changed && dc_status_label) {
        lv_label_set_text(dc_status_label, status);
    }

    data_collection_last_mode = mode;
    data_collection_last_pending = request_pending;
    data_collection_last_pcs = pcs;
    snprintf(data_collection_last_status, sizeof(data_collection_last_status),
             "%s", status);
    data_collection_snapshot_valid = true;
    data_collection_dirty = false;
}

void page_06_data_collection_on_reply(data_collection_reply_result_t result)
{
    if (result == DATA_COLLECTION_REPLY_REQUEST_CONFIRMED ||
        result == DATA_COLLECTION_REPLY_EXITED ||
        result == DATA_COLLECTION_REPLY_STATUS_UPDATED) {
        settings_set_status("READY", lv_color_hex(0x24D6A1));
    } else if (result == DATA_COLLECTION_REPLY_REQUEST_FAILED ||
               result == DATA_COLLECTION_REPLY_UNKNOWN) {
        settings_set_status("WARNING", lv_color_hex(0xF59D2A));
    }
    page_06_data_collection_refresh();
}

static void data_collect_mode_btn_event_cb(lv_event_t* e)
{
    uint8_t sub = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    data_collect_mode_t mode;
    const char* status = NULL;

    if (sub == 0x01) {
        mode = DATA_COLLECT_MODE_ALL;
        status = "Requesting ALL DATA collection mode...";
    } else if (sub == 0x02) {
        mode = DATA_COLLECT_MODE_FALSE;
        status = "Requesting ERROR DATA collection mode...";
    } else {
        return;
    }

    if (!data_collection_request_begin(mode, status,
                                       app_clock_uptime_ms())) {
        return;
    }
    if (!settings_detail_send_command(0xC0, &sub, 1)) {
        data_collection_request_cancel();
        page_06_data_collection_refresh();
        return;
    }
    settings_set_status("LOADING", color_primary());
    page_06_data_collection_refresh();
}

static void data_collect_start_btn_event_cb(lv_event_t* e)
{
    (void)e;

    if (data_collection_state_mode() == DATA_COLLECT_MODE_NONE) {
        data_collection_state_set_status("Please select a collection mode first");
        settings_set_status("WARNING", lv_color_hex(0xF59D2A));
        page_06_data_collection_refresh();
        return;
    }

    if (app_command_runtime_request_count_start()) {
        data_collection_state_reset_pcs();
        data_collection_state_set_status("Counting command sent. Waiting for controller reply...");
        settings_set_status("LOADING", color_primary());
        page_06_data_collection_refresh();
    }
}

static void data_collect_disable_btn_event_cb(lv_event_t* e)
{
    uint8_t sub = 0xFF;
    (void)e;

    if (!data_collection_request_begin(DATA_COLLECT_MODE_NONE,
                                       "Exiting collection mode...",
                                       app_clock_uptime_ms())) {
        return;
    }
    if (!settings_detail_send_command(0xC0, &sub, 1)) {
        data_collection_request_cancel();
        page_06_data_collection_refresh();
        return;
    }
    settings_set_status("LOADING", color_primary());
    page_06_data_collection_refresh();
}

static lv_obj_t* create_dc_mode_button(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                                       const char* text, uint8_t sub,
                                       lv_obj_t** out_label, lv_obj_t** out_check)
{
    lv_obj_t* btn = lv_obj_create(parent);
    style_plain(btn);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, 360, 54);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn, data_collect_mode_btn_event_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)sub);

    lv_obj_t* label = create_label(btn, text, &lv_font_instrument_sans_bold_16, color_text());
    lv_obj_set_pos(label, 18, 18);

    lv_obj_t* check = create_label(btn, "", &lv_font_montserrat_18, color_primary());
    lv_obj_align(check, LV_ALIGN_RIGHT_MID, -16, 0);

    if (out_label) {
        *out_label = label;
    }
    if (out_check) {
        *out_check = check;
    }

    return btn;
}

static void create_action_button(lv_obj_t* parent, lv_obj_t** out_btn,
                                 lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                                 const char* text, lv_color_t bg,
                                 lv_event_cb_t cb)
{
    lv_obj_t* btn = lv_obj_create(parent);
    style_plain(btn);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 4, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);

    if (cb) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_t* label = create_label(btn, text, &lv_font_instrument_sans_bold_16, color_panel());
    lv_obj_center(label);

    if (out_btn) {
        *out_btn = btn;
    }
}

static void create_data_collection_page_content(lv_obj_t* parent)
{
    dc_btn_all = create_dc_mode_button(parent, 31, 18, "ALL DATA",
                                       0x01, &dc_label_all, &dc_check_all);
    dc_btn_false = create_dc_mode_button(parent, 31, 86, "ERROR REPORT",
                                         0x02, &dc_label_false, &dc_check_false);

    create_action_button(parent, &dc_btn_start, 552, 18, 220, 54,
                         "START", color_primary(), data_collect_start_btn_event_cb);
    create_action_button(parent, &dc_btn_disable, 552, 86, 220, 54,
                         "DISABLE", lv_color_hex(0x8792A8), data_collect_disable_btn_event_cb);

    lv_obj_t* card = lv_obj_create(parent);
    style_plain(card);
    lv_obj_set_pos(card, 31, 170);
    lv_obj_set_size(card, 1008, 104);
    lv_obj_set_style_bg_color(card, color_panel(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 4, 0);
    lv_obj_set_style_shadow_width(card, 10, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_10, 0);
    lv_obj_set_style_shadow_ofs_y(card, 4, 0);

    lv_obj_t* mode_title = create_label(card, "COLLECTION MODE", &lv_font_instrument_sans_semibold_12, color_primary());
    lv_obj_set_style_text_letter_space(mode_title, 1, 0);
    lv_obj_set_pos(mode_title, 24, 15);

    dc_mode_value_label = create_label(card, "---", &lv_font_instrument_sans_medium_18, color_text());
    lv_obj_set_pos(dc_mode_value_label, 24, 45);

    dc_pcs_label = create_label(card, "PCS:0", &lv_font_manrope_bold_32, color_primary());
    lv_obj_align(dc_pcs_label, LV_ALIGN_TOP_MID, 0, 18);

    dc_status_label = create_label(card, "Please select a collection mode", &lv_font_instrument_sans_medium_18, color_text());
    lv_obj_set_width(dc_status_label, 420);
    lv_label_set_long_mode(dc_status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(dc_status_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(dc_status_label, LV_ALIGN_RIGHT_MID, -28, 0);

    page_06_data_collection_refresh();
}

static void back_event_cb(lv_event_t* e)
{
    (void)e;

    if (page_06_settings_back_sub_page()) {
        return;
    }

    ui_manager_clear_stack();
    ui_manager_switch(UI_PAGE_MAIN);
}

static void create_footer(lv_obj_t* parent)
{
    footer = lv_obj_create(parent);
    style_plain(footer);
    lv_obj_set_pos(footer, 0, SETTINGS_SCREEN_H - SETTINGS_FOOTER_H);
    lv_obj_set_size(footer, SETTINGS_SCREEN_W, SETTINGS_FOOTER_H);
    lv_obj_set_style_bg_color(footer, color_panel(), 0);
    lv_obj_set_style_bg_opa(footer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(footer, 0, 0);

    lv_obj_t* top_line = lv_obj_create(footer);
    style_plain(top_line);
    lv_obj_set_pos(top_line, 0, 0);
    lv_obj_set_size(top_line, SETTINGS_SCREEN_W, 1);
    lv_obj_set_style_bg_color(top_line, color_line(), 0);
    lv_obj_set_style_bg_opa(top_line, LV_OPA_COVER, 0);

    lv_nav_button_create(footer, 32, 10, 92, 35, back_event_cb, NULL);

    footer_time_label = create_label(footer, "00:00:00", &lv_font_instrument_sans_medium_20, color_muted());
    lv_obj_set_pos(footer_time_label, 1158, 16);

    update_footer_time();
    footer_time_timer = lv_timer_create(footer_time_timer_cb, 1000, NULL);
}

static void page_06_switch_sub_page(int index)
{
    if (index < 0 || index >= SETTINGS_MENU_COUNT) {
        return;
    }

    saved_menu_index = (page_06_settings_menu_t)index;
    saved_content_internal = false;
    saved_content_sub_page = PAGE_06_SETTINGS_SUB_NONE;
    saved_content_title[0] = '\0';
    page_06_hide_all_content_pages();
    nav_stack_depth = 0;

    if (pages[index]) {
        lv_obj_clear_flag(pages[index], LV_OBJ_FLAG_HIDDEN);
        active_content_page = pages[index];
        active_content_menu = (page_06_settings_menu_t)index;
        active_content_sub_page = PAGE_06_SETTINGS_SUB_NONE;
        page_06_copy_title(active_content_title, sizeof(active_content_title),
                           page_06_get_menu_title((page_06_settings_menu_t)index));
    }

    page_06_set_breadcrumb((page_06_settings_menu_t)index,
                           page_06_get_menu_title((page_06_settings_menu_t)index));

    if (index == SETTINGS_MENU_DATA_COLLECTION) {
        if (data_collection_state_mode() == DATA_COLLECT_MODE_NONE) {
            data_collection_state_set_status("Please select a collection mode.");
        }
        page_06_data_collection_refresh();
    }
}

static bool page_06_restore_saved_content(void)
{
    lv_obj_t* page = NULL;

    if (!saved_content_internal || !page_06_is_valid_menu(saved_menu_index)) {
        return false;
    }

    page = page_06_find_internal_page(saved_menu_index, saved_content_sub_page);
    if (!page || !pages[saved_menu_index]) {
        return false;
    }

    nav_stack_depth = 0;
    nav_stack[nav_stack_depth].page = pages[saved_menu_index];
    nav_stack[nav_stack_depth].menu = saved_menu_index;
    nav_stack[nav_stack_depth].sub_page = PAGE_06_SETTINGS_SUB_NONE;
    page_06_copy_title(nav_stack[nav_stack_depth].title,
                       sizeof(nav_stack[nav_stack_depth].title),
                       page_06_get_menu_title(saved_menu_index));
    nav_stack_depth++;

    page_06_show_content_page(saved_menu_index, page, saved_content_sub_page,
                              saved_content_title, false);
    return true;
}

bool page_06_settings_switch_menu(page_06_settings_menu_t menu)
{
    if (!settings_page || !page_06_is_valid_menu(menu)) {
        return false;
    }

    saved_menu_index = menu;
    page_06_update_menu_state(menu);
    page_06_switch_sub_page(menu);
    return true;
}

void ui_page_06_settings_create(lv_obj_t* parent)
{
    (void)parent;

    if (settings_page) {
        return;
    }

    ui_page_27_set_cfd_level_query();

    settings_page = lv_obj_create(lv_scr_act());
    root = settings_page;
    style_plain(root);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_size(root, SETTINGS_SCREEN_W, SETTINGS_SCREEN_H);
    lv_obj_set_style_bg_color(root, color_bg(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(root, 0, 0);

    ui_page_background_apply(root, UI_BACKGROUND_SETTINGS);
    create_header(root);
    create_sidebar(root);

    content_host = lv_obj_create(root);
    style_plain(content_host);
    lv_obj_set_pos(content_host, SETTINGS_CONTENT_X, SETTINGS_CONTENT_Y);
    lv_obj_set_size(content_host, SETTINGS_CONTENT_W, SETTINGS_CONTENT_H);
    lv_obj_set_style_bg_opa(content_host, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content_host, 0, 0);

    for (int i = 0; i < SETTINGS_MENU_COUNT; i++) {
        pages[i] = create_page(content_host);
    }

    create_system_page_content(pages[SETTINGS_MENU_SYSTEM]);
    create_maintenance_page_content(pages[SETTINGS_MENU_MAINTENANCE]);
    create_user_page_content(pages[SETTINGS_MENU_USER]);
    create_version_page_content(pages[SETTINGS_MENU_VERSION]);
    create_data_collection_page_content(pages[SETTINGS_MENU_DATA_COLLECTION]);
    create_footer(root);

    if (!page_06_is_valid_menu(saved_menu_index)) {
        saved_menu_index = SETTINGS_MENU_SYSTEM;
    }

    page_06_update_menu_state(saved_menu_index);
    if (!page_06_restore_saved_content()) {
        page_06_switch_sub_page(saved_menu_index);
    }
    settings_set_status("READY", lv_color_hex(0x24D6A1));
}

void ui_page_06_settings_reset_navigation(void)
{
    /* Reset both the model and retained view. Resetting only saved_* does not
     * affect resume(), which reuses the already-created sidebar and tiles. */
    page_06_reset_saved_navigation();
    nav_stack_depth = 0;
    if (settings_page == NULL || !lv_obj_is_valid(settings_page)) return;
    page_06_refresh_option_state();
    page_06_update_menu_state(SETTINGS_MENU_SYSTEM);
    page_06_switch_sub_page(SETTINGS_MENU_SYSTEM);
}

bool ui_page_06_settings_resume(void)
{
    uint64_t started_us;
    bool dirty;

    if (settings_page == NULL || !lv_obj_is_valid(settings_page)) return false;

    started_us = perf_profile_is_enabled() ? app_clock_monotonic_us() : 0;
    dirty = settings_status_dirty || data_collection_dirty ||
            !data_collection_snapshot_valid;
    lv_obj_clear_flag(settings_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(settings_page);
    if (settings_status_dirty) settings_apply_status();
    page_06_data_collection_refresh();
    update_footer_time();
    if (footer_time_timer != NULL) {
        lv_timer_resume(footer_time_timer);
        lv_timer_reset(footer_time_timer);
    }
    ui_page_27_set_cfd_level_query();
    if (started_us != 0) {
        perf_profile_report_event_us("SETTINGS",
            dirty ? "RESUME_DIRTY" : "RESUME_CLEAN",
            app_clock_elapsed_us32(started_us, app_clock_monotonic_us()));
    }
    return true;
}

void ui_page_06_settings_suspend(void)
{
    settings_detail_dialog_hide();
    settings_detail_keyboard_hide();
    if (footer_time_timer != NULL) lv_timer_pause(footer_time_timer);
    if (settings_page != NULL && lv_obj_is_valid(settings_page)) {
        lv_obj_add_flag(settings_page, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_page_06_settings_destroy(void)
{
    if (!settings_page) {
        return;
    }

    settings_detail_dialog_hide();
    settings_detail_keyboard_hide();

    if (footer_time_timer) {
        lv_timer_del(footer_time_timer);
        footer_time_timer = NULL;
    }

    lv_obj_del(settings_page);

    settings_page = NULL;
    root = NULL;
    sidebar = NULL;
    content_host = NULL;
    footer = NULL;
    breadcrumb_current = NULL;
    status_dot = NULL;
    status_label = NULL;
    footer_status_label = NULL;
    footer_time_label = NULL;
    current_menu_index = -1;
    internal_page_count = 0;
    nav_stack_depth = 0;
    option_item_count = 0;
    active_content_page = NULL;
    active_content_menu = PAGE_06_SETTINGS_MENU_SYSTEM;
    active_content_sub_page = PAGE_06_SETTINGS_SUB_NONE;
    active_content_title[0] = '\0';

    for (int i = 0; i < SETTINGS_MENU_COUNT; i++) {
        menu_btns[i] = NULL;
        menu_icons[i] = NULL;
        menu_labels[i] = NULL;
        pages[i] = NULL;
    }
    memset(version_value_labels, 0, sizeof(version_value_labels));

    for (int i = 0; i < SETTINGS_INTERNAL_PAGE_MAX; i++) {
        internal_pages[i].page = NULL;
        internal_pages[i].menu = PAGE_06_SETTINGS_MENU_SYSTEM;
        internal_pages[i].sub_page = PAGE_06_SETTINGS_SUB_NONE;
        internal_pages[i].title[0] = '\0';
    }

    for (int i = 0; i < SETTINGS_NAV_STACK_MAX; i++) {
        nav_stack[i].page = NULL;
        nav_stack[i].menu = PAGE_06_SETTINGS_MENU_SYSTEM;
        nav_stack[i].sub_page = PAGE_06_SETTINGS_SUB_NONE;
        nav_stack[i].title[0] = '\0';
    }

    for (int i = 0; i < SETTINGS_OPTION_MAX; i++) {
        option_items[i].tile = NULL;
        option_items[i].rail = NULL;
        option_items[i].title_label = NULL;
        option_items[i].no_label = NULL;
        option_items[i].menu = PAGE_06_SETTINGS_MENU_SYSTEM;
        option_items[i].context_internal = false;
        option_items[i].context_sub_page = PAGE_06_SETTINGS_SUB_NONE;
        option_items[i].col = -1;
        option_items[i].row = -1;
        option_items[i].selectable = false;
    }

    dc_btn_all = NULL;
    dc_btn_false = NULL;
    dc_btn_start = NULL;
    dc_btn_disable = NULL;
    dc_label_all = NULL;
    dc_label_false = NULL;
    dc_check_all = NULL;
    dc_check_false = NULL;
    dc_mode_value_label = NULL;
    dc_pcs_label = NULL;
    dc_status_label = NULL;
    settings_status_dirty = true;
    data_collection_dirty = true;
    data_collection_snapshot_valid = false;
    data_collection_last_status[0] = '\0';
}
