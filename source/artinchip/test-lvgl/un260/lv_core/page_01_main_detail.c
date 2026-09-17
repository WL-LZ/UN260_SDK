#include "page_01_main_detail.h"
#include "lv_page_manager.h"
#include "lv_port_indev.h"
#include "un260/counting/counting_data_store.h"
#include "un260/lv_components/lv_recycled_list.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_resources/lv_img_init.h"
#include <stdio.h>
#include <string.h>

#define DETAIL_SECTIONS 3
#define DETAIL_HEADER_HEIGHT 28
#define DETAIL_ROW_HEIGHT 34
#define DETAIL_TAP_THRESHOLD 10
#define DETAIL_BODY 0x4C606E
#define DETAIL_MUTED 0x7A8D9B

typedef struct {
    page_01_detail_section_t id;
    lv_obj_t *root, *header[3], *empty, *empty_text;
    lv_recycled_list_t *list;
    lv_coord_t column_x[3], column_width[3];
    lv_text_align_t align[3];
    bool dirty;
} main_detail_section_t;

typedef struct {
    lv_obj_t *root;
    main_detail_section_t section[DETAIL_SECTIONS];
    page_01_detail_section_t active;
    uint16_t serial_slots[COUNTING_DATA_MAX_ITEMS];
    uint8_t denom_slots[COUNTING_DENOM_MAX_ITEMS];
    uint32_t serial_count, denom_count, revision;
    bool visible;
    struct {
        lv_obj_t *owner, *row;
        lv_point_t start;
        lv_coord_t row_y;
        float offset;
        uint32_t revision;
        bool pressed, moved;
    } tap;
} main_detail_view_t;

static main_detail_view_t *detail_view;

static void object_visible(lv_obj_t *object, bool visible)
{
    if (visible == !lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN)) return;
    if (visible) lv_obj_clear_flag(object, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *surface(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                         lv_coord_t width, lv_coord_t height, uint32_t color)
{
    lv_obj_t *object = lv_obj_create(parent);
    if (!object) return NULL;
    lv_obj_remove_style_all(object);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
    lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return object;
}

static lv_obj_t *label_create(lv_obj_t *parent, lv_coord_t x, lv_coord_t width,
                              const lv_font_t *font, lv_text_align_t align,
                              uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    if (!label) return NULL;
    lv_obj_remove_style_all(label);
    lv_obj_set_size(label, width, LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(label, align, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, x, 0);
    return label;
}

static void label_text(lv_obj_t *label, const char *text)
{
    if (strcmp(lv_label_get_text(label), text)) lv_label_set_text(label, text);
}

static void number_text(lv_obj_t *label, const char *text)
{
    const lv_font_t *fonts[] = {&lv_font_instrument_sans_medium_20,
        &lv_font_instrument_sans_medium_18, &lv_font_instrument_sans_medium_16,
        &lv_font_instrument_sans_medium_14};
    const lv_font_t *font = fonts[3];
    for (unsigned i = 0; i < sizeof(fonts) / sizeof(fonts[0]); ++i) {
        lv_point_t size;
        lv_txt_get_size(&size, text, fonts[i], 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (size.x <= lv_obj_get_style_width(label, 0)) { font = fonts[i]; break; }
    }
    if (lv_obj_get_style_text_font(label, 0) != font)
        lv_obj_set_style_text_font(label, font, 0);
    label_text(label, text);
}

static lv_obj_t *row_create(lv_obj_t *parent, lv_coord_t width, void *context)
{
    main_detail_section_t *section = context;
    lv_obj_t *row = surface(parent, 0, 0, width, DETAIL_ROW_HEIGHT, 0xFFFFFF);
    if (!row) return NULL;
    for (unsigned column = 0; column < 3; ++column) {
        const lv_font_t *font = &lv_font_instrument_sans_medium_20;
        if (section->id != PAGE_01_DETAIL_SECTION_A)
            font = column == 0 ? &lv_font_instrument_sans_medium_14 :
                                    &lv_font_instrument_sans_medium_18;
        if (!label_create(row, section->column_x[column], section->column_width[column],
                          font, section->align[column],
                          column == 0 && section->id != PAGE_01_DETAIL_SECTION_A ?
                          DETAIL_MUTED : DETAIL_BODY)) {
            lv_obj_del(row);
            return NULL;
        }
    }
    return row;
}

static void row_bind(lv_obj_t *row, uint32_t index, void *context)
{
    main_detail_section_t *section = context;
    const counting_sim_t *data = counting_data_current();
    lv_obj_t *cells[3] = {lv_obj_get_child(row, 0), lv_obj_get_child(row, 1),
                         lv_obj_get_child(row, 2)};
    char text[48];
    lv_obj_set_style_bg_color(row, lv_color_hex(index % 2 ? 0xF4F6F7 : 0xFFFFFF), 0);
    if (section->id == PAGE_01_DETAIL_SECTION_A) {
        denom_t denom = {0};
        if (index < detail_view->denom_count) {
            unsigned slot = detail_view->denom_slots[index];
            if (slot < data->denom_number && slot < COUNTING_DENOM_MAX_ITEMS)
                denom = data->denom[slot];
        }
        snprintf(text, sizeof(text), "%d", denom.value);
        number_text(cells[0], text);
        snprintf(text, sizeof(text), "%u", (unsigned)denom.pcs);
        number_text(cells[1], text);
        snprintf(text, sizeof(text), "%.0f", (double)denom.amount);
        number_text(cells[2], text);
    } else if (section->id == PAGE_01_DETAIL_SECTION_B) {
        unsigned slot = index < detail_view->serial_count ? detail_view->serial_slots[index] :
                                                           COUNTING_DATA_MAX_ITEMS;
        bool valid = slot < (unsigned)counting_data_serial_scan_limit(data) &&
                     data->sn_str[slot] && data->denom_mix[slot] > 0;
        snprintf(text, sizeof(text), "%u", slot + 1U);
        label_text(cells[0], valid ? text : "--");
        label_text(cells[1], valid ? data->sn_str[slot] : "");
        snprintf(text, sizeof(text), "%d", valid ? data->denom_mix[slot] : 0);
        number_text(cells[2], text);
    } else {
        bool valid = index < (unsigned)counting_data_error_detail_count(data);
        snprintf(text, sizeof(text), "%u", (unsigned)index + 1U);
        label_text(cells[0], text);
        snprintf(text, sizeof(text), "%u", valid ? (unsigned)data->err_pcs[index] : 0U);
        number_text(cells[1], text);
        label_text(cells[2], ui_text_counting_reject_reason(valid ? data->err_code[index] : UINT8_MAX));
    }
}

static void tap_event(lv_event_t *event)
{
    main_detail_view_t *view = detail_view;
    if (!view || lv_event_get_target(event) != lv_event_get_current_target(event)) return;
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *owner = lv_event_get_target(event);
    if (code == LV_EVENT_PRESS_LOST) { view->tap.pressed = false; return; }
    if (!lv_obj_is_visible(owner)) return;
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING && code != LV_EVENT_RELEASED) return;
    lv_indev_t *indev = lv_event_get_indev(event);
    if (!indev || indev->driver->type != LV_INDEV_TYPE_POINTER) return;
    lv_point_t point;
    lv_indev_get_point(indev, &point);
    lv_recycled_list_t *list = view->section[view->active].list;
    const ui_list_window_t *window = lv_recycled_list_window(list);
    if (code == LV_EVENT_PRESSED) {
        view->tap.owner = owner;
        view->tap.start = point;
        view->tap.offset = window->offset;
        view->tap.row = NULL;
        /* Detect even a sub-10px scroll or rubber-band movement. The common
         * viewport's pointer callback runs first, before this observer. */
        if (owner == lv_recycled_list_object(list)) {
            for (unsigned i = 0; i <= window->rows; ++i) {
                lv_obj_t *row = lv_obj_get_child(owner, i);
                if (!lv_obj_has_flag(row, LV_OBJ_FLAG_HIDDEN)) {
                    view->tap.row = row;
                    view->tap.row_y = lv_obj_get_y(row);
                    break;
                }
            }
        }
        view->tap.revision = view->revision;
        view->tap.pressed = true;
        view->tap.moved = false;
        return;
    }
    if (!view->tap.pressed || view->tap.owner != owner) return;
    if (LV_ABS((int)point.x - view->tap.start.x) >= DETAIL_TAP_THRESHOLD ||
        LV_ABS((int)point.y - view->tap.start.y) >= DETAIL_TAP_THRESHOLD ||
        window->offset != view->tap.offset ||
        (view->tap.row && lv_obj_get_y(view->tap.row) != view->tap.row_y))
        view->tap.moved = true;
    if (code == LV_EVENT_RELEASED) {
        lv_area_t bounds;
        lv_obj_get_coords(owner, &bounds);
        bool open = !view->tap.moved && view->tap.revision == view->revision &&
                    point.x >= bounds.x1 && point.x <= bounds.x2 &&
                    point.y >= bounds.y1 && point.y <= bounds.y2;
        view->tap.pressed = false;
        if (open) ui_manager_push_page(UI_PAGE_LIST);
    }
}

void page_01_main_detail_bind_tap(lv_obj_t *object)
{
    if (!object) return;
    lv_obj_remove_event_cb(object, tap_event);
    lv_obj_add_flag(object, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(object, tap_event, LV_EVENT_ALL, NULL);
}

static void section_refresh(main_detail_section_t *section)
{
    static const ui_text_id_t headers[DETAIL_SECTIONS][3] = {
        {UI_TEXT_PAGE01_DETAIL_COL_DENOM, UI_TEXT_PAGE01_DETAIL_COL_PCS, UI_TEXT_PAGE01_DETAIL_COL_AMOUNT},
        {UI_TEXT_PAGE01_DETAIL_COL_NO, UI_TEXT_LIST_COL_SERIAL_NUMBER, UI_TEXT_PAGE01_DETAIL_COL_DENOM},
        {UI_TEXT_PAGE01_DETAIL_COL_NO, UI_TEXT_PAGE01_DETAIL_COL_PCS, UI_TEXT_LIST_COL_REASON}
    };
    const counting_sim_t *data = counting_data_current();
    uint32_t count;
    for (unsigned i = 0; i < 3; ++i) label_text(section->header[i], ui_text_get(headers[section->id][i]));
    if (section->id == PAGE_01_DETAIL_SECTION_A) {
        detail_view->denom_count = 0;
        for (unsigned i = 0; i < data->denom_number && i < COUNTING_DENOM_MAX_ITEMS; ++i)
            if (data->denom[i].value > 0) detail_view->denom_slots[detail_view->denom_count++] = (uint8_t)i;
        /* Same no-catalog fallback as List; known denominations retain zero PCS. */
        count = detail_view->denom_count ? detail_view->denom_count : 1;
    } else if (section->id == PAGE_01_DETAIL_SECTION_B) {
        detail_view->serial_count = 0;
        int limit = counting_data_serial_scan_limit(data);
        for (int slot = 0; slot < limit; ++slot)
            if (data->sn_str[slot] && data->denom_mix[slot] > 0)
                detail_view->serial_slots[detail_view->serial_count++] = (uint16_t)slot;
        count = detail_view->serial_count;
        label_text(section->empty_text, ui_text_get(UI_TEXT_LIST_NO_SERIAL_NUMBERS));
    } else {
        count = (uint32_t)counting_data_error_detail_count(data);
        label_text(section->empty_text, ui_text_get(UI_TEXT_LIST_NO_REJECT_DETAILS));
    }
    lv_recycled_list_refresh(section->list, count, false);
    object_visible(section->empty, count == 0);
    section->dirty = false;
}

static bool section_create(main_detail_section_t *section, unsigned id,
                            lv_coord_t width, lv_coord_t height)
{
    section->id = (page_01_detail_section_t)id;
    section->dirty = true;
    section->root = surface(detail_view->root, 0, 0, width + 18, height, 0xFFFFFF);
    if (!section->root) return false;
    lv_coord_t usable = width - 24;
    if (id == PAGE_01_DETAIL_SECTION_A) {
        section->column_x[0] = 10; section->column_width[0] = usable * 2 / 5 - 20;
        section->column_x[1] = usable * 2 / 5; section->column_width[1] = usable / 5 - 12;
        section->column_x[2] = usable * 3 / 5; section->column_width[2] = usable - section->column_x[2] - 10;
        section->align[1] = section->align[2] = LV_TEXT_ALIGN_RIGHT;
    } else {
        section->column_x[0] = 10; section->column_width[0] = 48;
        section->column_x[1] = 70;
        if (id == PAGE_01_DETAIL_SECTION_B) {
            section->column_width[1] = usable - 180;
            section->column_x[2] = usable - 98; section->column_width[2] = 88;
            section->align[2] = LV_TEXT_ALIGN_RIGHT;
        } else {
            section->column_width[1] = 60;
            section->column_x[2] = 148; section->column_width[2] = usable - 158;
        }
    }
    lv_obj_t *header = surface(section->root, 0, 0, width, DETAIL_HEADER_HEIGHT, 0xEDF2F5);
    if (!header) return false;
    for (unsigned i = 0; i < 3; ++i) {
        section->header[i] = label_create(header, section->column_x[i], section->column_width[i],
            &lv_font_instrument_sans_medium_12, section->align[i], DETAIL_MUTED);
        if (!section->header[i]) return false;
    }
    lv_recycled_list_config_t config = {0};
    config.y = DETAIL_HEADER_HEIGHT;
    config.width = width + 18;
    config.content_width = width - 24;
    config.scrollbar_right_inset = 6;
    config.rows = (height - DETAIL_HEADER_HEIGHT) / DETAIL_ROW_HEIGHT;
    config.row_height = DETAIL_ROW_HEIGHT;
    config.create_row = row_create;
    config.bind_row = row_bind;
    config.context = section;
    section->list = lv_recycled_list_create(section->root, &config);
    if (!section->list) return false;
    lv_obj_t *viewport = lv_recycled_list_object(section->list);
    lv_port_indev_set_drag_obj(viewport, true);
    page_01_main_detail_bind_tap(viewport);
    section->empty = surface(section->root,0,DETAIL_HEADER_HEIGHT,width,height-DETAIL_HEADER_HEIGHT,0xFFFFFF);
    if (!section->empty) return false;
    lv_obj_set_flex_flow(section->empty,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(section->empty,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(section->empty,12,0);
    if(id!=PAGE_01_DETAIL_SECTION_A) {
        lv_obj_t *icon=lv_img_create(section->empty);
        lv_img_set_src(icon,id==PAGE_01_DETAIL_SECTION_B?LVGL_DIR"list_icons/barcode_36.png":LVGL_DIR"list_icons/warning_circle_36.png");
        lv_obj_clear_flag(icon,LV_OBJ_FLAG_CLICKABLE);
    }
    section->empty_text = label_create(section->empty, 0, width - 48,
                                  &lv_font_instrument_sans_medium_18,
                                  LV_TEXT_ALIGN_CENTER, DETAIL_MUTED);
    if (!section->empty_text) return false;
    object_visible(section->root, false);
    return true;
}

static void root_deleted(lv_event_t *event)
{
    main_detail_view_t *view = lv_event_get_user_data(event);
    if (lv_event_get_target(event) != lv_event_get_current_target(event)) return;
    if (detail_view == view) detail_view = NULL;
    lv_mem_free(view);
}

bool page_01_main_detail_create(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                lv_coord_t width, lv_coord_t height)
{
    if (detail_view || !parent || width < 300 || width > LV_COORD_MAX - 18 || height < DETAIL_HEADER_HEIGHT + DETAIL_ROW_HEIGHT ||
        (height - DETAIL_HEADER_HEIGHT) / DETAIL_ROW_HEIGHT >= 16) return false;
    main_detail_view_t *view = lv_mem_alloc(sizeof(*view));
    if (!view) return false;
    memset(view, 0, sizeof(*view));
    view->root = surface(parent, x, y, width + 18, height, 0xFFFFFF);
    if (!view->root) { lv_mem_free(view); return false; }
    if (!lv_obj_add_event_cb(view->root, root_deleted, LV_EVENT_DELETE, view)) {
        lv_obj_del(view->root);
        lv_mem_free(view);
        return false;
    }
    detail_view = view;
    view->visible = true;
    for (unsigned i = 0; i < DETAIL_SECTIONS; ++i) {
        if (!section_create(&view->section[i], i, width, height)) {
            page_01_main_detail_destroy();
            return false;
        }
    }
    page_01_main_detail_refresh(PAGE_01_DETAIL_SECTION_A);
    return true;
}

void page_01_main_detail_refresh(page_01_detail_section_t section)
{
    main_detail_view_t *view = detail_view;
    if (!view || (unsigned)section >= DETAIL_SECTIONS) return;
    /* This tap opens List, not a particular record. Incoming count packets
     * must not continually cancel a user's otherwise stationary touch. */
    if (view->active != section) {
        ++view->revision;
        view->tap.pressed = false;
        lv_recycled_list_stop(view->section[view->active].list);
    }
    view->active = section;
    for (unsigned i = 0; i < DETAIL_SECTIONS; ++i) {
        view->section[i].dirty = true;
        object_visible(view->section[i].root, i == (unsigned)section);
    }
    if (view->visible) section_refresh(&view->section[section]);
}

void page_01_main_detail_set_visible(bool visible)
{
    main_detail_view_t *view = detail_view;
    if (!view || view->visible == visible) return;
    view->visible = visible;
    view->tap.pressed = false;
    object_visible(view->root, visible);
    if (!visible) {
        for (unsigned i = 0; i < DETAIL_SECTIONS; ++i) lv_recycled_list_stop(view->section[i].list);
    } else if (view->section[view->active].dirty) section_refresh(&view->section[view->active]);
}

void page_01_main_detail_reset(void)
{
    main_detail_view_t *view = detail_view;
    if (!view) return;
    ++view->revision;
    view->tap.pressed = false;
    for (unsigned i = 0; i < DETAIL_SECTIONS; ++i) {
        lv_recycled_list_refresh(view->section[i].list, 0, true);
        view->section[i].dirty = true;
    }
    if (view->visible) section_refresh(&view->section[view->active]);
}

void page_01_main_detail_destroy(void)
{
    if (!detail_view) return;
    page_01_main_detail_set_visible(false);
    lv_obj_del(detail_view->root);
}

lv_obj_t *page_01_main_detail_scroll_obj(void)
{
    return detail_view ? lv_recycled_list_object(detail_view->section[detail_view->active].list) : NULL;
}
