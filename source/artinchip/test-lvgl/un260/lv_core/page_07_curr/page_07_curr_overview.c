#include "page_07_curr_overview.h"
#include "page_07_curr_layout.h"
#include "page_07_curr_view.h"
#include "page_07_curr_star.h"
#include "un260/currency/currency_state.h"
#include "un260/lv_components/lv_damped_button.h"
#include "un260/lv_resources/lv_img_init.h"
#include "un260/lv_resources/lv_curr/curr_view_atlas.h"
#include "lv_port_indev.h"
#include <stdint.h>
#include <string.h>

static lv_obj_t *surface(lv_obj_t *parent, int x, int y, int w, int h,
                         uint32_t color, int radius)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
    return obj;
}

static lv_obj_t *text(lv_obj_t *parent, const char *value, int x, int y,
                      const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, value);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    return label;
}

static lv_obj_t *action(lv_obj_t *parent, const char *label, int x, int y,
                        int w, lv_event_cb_t cb, intptr_t data)
{
    lv_obj_t *button = surface(parent, x, y, w, 44, 0xF7F7F7, 10);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, (void *)data);
    lv_damped_button_register(button, lv_color_hex(0xF7F7F7),
        lv_damped_button_pressed_color(lv_color_hex(0xF7F7F7)));
    lv_obj_t *name = text(button, label, 0, 0,
        &lv_font_instrument_sans_medium_18, 0x20313B);
    lv_obj_center(name);
    return button;
}

static void stroke(lv_obj_t *parent, const lv_point_t *points, unsigned count,
                   int x, int y, uint32_t color)
{
    lv_obj_t *line = lv_line_create(parent);
    lv_line_set_points(line, points, count);
    lv_obj_set_pos(line, x, y);
    lv_obj_set_style_line_color(line, lv_color_hex(color), 0);
    lv_obj_set_style_line_width(line, 2, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
}

/* Presentation subtitles only; unknown protocol codes remain fully usable. */
static const char *description(const char *code)
{
    static const struct { const char *code; const char *name; } names[] = {
        {"AUT", "Auto detect"}, {"MUL", "Multi-currency"},
        {"EUR", "Euro area"}, {"USD", "United States"}, {"CNY", "China"},
        {"RUB", "Russia"}, {"TRY", "Turkiye"}, {"GBP", "United Kingdom"},
        {"MXN", "Mexico"}, {"CAD", "Canada"}, {"ILS", "Israel"},
        {"AED", "UAE"}, {"SAR", "Saudi Arabia"}, {"IRR", "Iran"},
        {"KRW", "South Korea"}, {"JPY", "Japan"}, {"HKD", "Hong Kong"},
        {"AUD", "Australia"}, {"SGD", "Singapore"}, {"MOP", "Macao"},
        {"XOF", "West Africa"}, {"XAF", "Central Africa"}, {"UAH", "Ukraine"},
        {"INR", "India"}, {"EGP", "Egypt"}, {"PHP", "Philippines"},
        {"THB", "Thailand"}, {"IDR", "Indonesia"}, {"ZAR", "South Africa"},
        {"QAR", "Qatar"}, {"PKR", "Pakistan"}, {"CHF", "Switzerland"},
        {"TJS", "Tajikistan"}, {"AMD", "Armenia"}, {"AZN", "Azerbaijan"},
        {"LBP", "Lebanon"}, {"MUR", "Mauritius"},
        {"MAD", "Morocco"}, {"HNL", "Honduras"}
    };
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
        if (!strncmp(names[i].code, code, 3)) return names[i].name;
    return "Currency";
}

/* All flags share one decoded image. Offset clipping is 1:1 and bypasses
 * the GE resized-image LRU even when four rows intersect the viewport.
 * Unknown/missing artwork keeps the established image fallback. */
static void grid_flag(lv_obj_t *img, const char *code, bool atlas_available)
{
    if (lv_img_get_src(img)) {
        lv_img_set_offset_x(img, 0);
        lv_img_set_offset_y(img, 0);
    }
    lv_img_set_zoom(img, LV_IMG_ZOOM_NONE);
    const char *source = get_currency_img(code);
    const char *file = strrchr(source, '/');
    file = file ? file + 1 : source;
    if (atlas_available) {
        for (unsigned i = 0; i < sizeof(curr_view_atlas_entries) / sizeof(curr_view_atlas_entries[0]); ++i) {
            if (strcmp(file, curr_view_atlas_entries[i].file)) continue;
            lv_img_set_src(img, CURR_VIEW_ATLAS_PATH);
            lv_obj_set_size(img, CURR_GRID_FLAG_W, CURR_GRID_FLAG_H);
            lv_img_set_offset_x(img, -curr_view_atlas_entries[i].x);
            lv_img_set_offset_y(img, -curr_view_atlas_entries[i].y);
            return;
        }
    }
    lv_img_set_src(img, source);
    lv_obj_set_size(img, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    page07_curr_view_set_img_target_width(img, code, CURR_GRID_FLAG_W);
}

void page07_curr_overview_set_active(page07_curr_context_t *ctx, bool active)
{
    if (!ctx) return;
    page07_curr_overview_refs_t *refs = &ctx->objects.overview;
    if (!active) {
        lv_img_cache_unpin(refs->atlas_pin);
        refs->atlas_pin = NULL;
        return;
    }
    if (!ctx->objects.grid_layer || refs->atlas_pin || !ctx->model.visible_count) return;
#if LV_IMG_CACHE_DEF_SIZE
    /* With recolor opacity zero LVGL leaves the draw key at black. */
    refs->atlas_pin = _lv_img_cache_open(CURR_VIEW_ATLAS_PATH, lv_color_black(), 0);
#endif
    if (refs->atlas_pin && (refs->atlas_pin->dec_dsc.header.w != CURR_VIEW_ATLAS_WIDTH ||
                           refs->atlas_pin->dec_dsc.header.h != CURR_VIEW_ATLAS_HEIGHT))
        refs->atlas_pin = NULL;
    lv_img_cache_pin(refs->atlas_pin);
    /* A missing/corrupt atlas or an exhausted DMA heap must not hide flags.
     * Retry only on the next explicit entry, never from a scroll callback. */
    for (int i = 0; i < ctx->model.visible_count; ++i) {
        char code[4];
        lv_obj_t *img = ctx->grid_items[i].img;
        if (!img || !currency_state_get_code(ctx->grid_items[i].abs_idx, code)) continue;
        grid_flag(img, code, refs->atlas_pin != NULL);
        lv_obj_center(img);
    }
}

static void overview_delete(lv_event_t *event)
{
    page07_curr_context_t *ctx = lv_event_get_user_data(event);
    page07_curr_overview_set_active(ctx, false);
}

static void position_update(lv_event_t *event)
{
    page07_curr_context_t *ctx = lv_event_get_user_data(event);
    int offset = LV_MAX(0, lv_obj_get_scroll_y(ctx->objects.grid_scroll));
    int first = offset / CURR_GRID_ROW_STEP;
    if (offset % CURR_GRID_ROW_STEP >= CURR_GRID_CELL_H) ++first;
    int last = (offset + CURR_GRID_VIEWPORT_H - 1) / CURR_GRID_ROW_STEP;
    int key = first * (PAGE07_CURR_MAX_ITEMS + 1) + last;
    if (ctx->objects.overview.visible_range_key == key) return;
    ctx->objects.overview.visible_range_key = key;
    int count = ctx->model.visible_count;
    lv_label_set_text_fmt(ctx->objects.overview.position, "%d-%d of %d / %s",
        count ? first * CURR_GRID_COLS + 1 : 0,
        LV_MIN(count, (last + 1) * CURR_GRID_COLS), count,
        ctx->model.favorite_only ? "Favorites" : "All currencies");
}

void page07_curr_overview_selection(page07_curr_context_t *ctx)
{
    if (!ctx->objects.grid_layer) return;
    char code[4];
    if (currency_state_get_code((uint8_t)ctx->model.selected_abs_idx, code)) {
        lv_label_set_text_fmt(ctx->objects.overview.current_code, "Selected %s / %s",
            currency_state_display_code(code), description(code));
    }
    for (int i = 0; i < ctx->model.visible_count; ++i) {
        page07_curr_grid_item_t *item = &ctx->grid_items[i];
        if (!item->item) continue;
        bool selected = item->abs_idx == ctx->model.selected_abs_idx;
        if (selected) {
            lv_obj_add_state(item->item, LV_STATE_CHECKED);
            lv_obj_clear_flag(item->selected_mark, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_state(item->item, LV_STATE_CHECKED);
            lv_obj_add_flag(item->selected_mark, LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_set_style_border_color(item->item,
            lv_color_hex(selected ? 0xA4C3EC : 0xE2E8ED), 0);
        lv_obj_set_style_text_color(item->name,
            lv_color_hex(selected ? 0x1559B7 : 0x20313B), 0);
    }
}

void page07_curr_overview_build(page07_curr_context_t *ctx,
                               const page07_curr_overview_actions_t *actions)
{
    lv_obj_t *root = surface(ctx->objects.root, 0, 0, 1280, 400, 0xEBF0F8, 0);
    ctx->objects.grid_layer = root;
    lv_obj_add_event_cb(root, overview_delete, LV_EVENT_DELETE, ctx);
    lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_grad_color(root, lv_color_hex(0xF8F6F3), 0);
    lv_obj_set_style_bg_grad_dir(root, LV_GRAD_DIR_HOR, 0);
    lv_obj_t *glyph_box = surface(root, 24, 16, 42, 42, 0xEDF3F8, 12);
    static const lv_point_t layers[] = {{0,6},{12,0},{24,6},{12,12},{0,6}};
    static const lv_point_t layer_edge[] = {{0,0},{12,6},{24,0}};
    stroke(glyph_box, layers, 5, 9, 9, 0x648393);
    stroke(glyph_box, layer_edge, 3, 9, 20, 0x648393);
    stroke(glyph_box, layer_edge, 3, 9, 25, 0x648393);
    text(root, "Currency", 78, 14, &lv_font_instrument_sans_semibold_28, 0x20313B);
    ctx->objects.overview.current_code = text(root, "", 78, 48,
        &lv_font_instrument_sans_medium_12, 0x637887);

    lv_obj_t *filters = surface(root, 610, 14, 284, 46, 0xE5EBF0, 12);
    for (int i = 0; i < 2; ++i) {
        bool active = (bool)i == ctx->model.favorite_only;
        lv_obj_t *button = action(filters, i ? "Favorites" : "All currencies",
            3 + i * 139, 3, 139, actions->filter, i);
        lv_obj_set_height(button, 40);
        lv_damped_button_set_palette(button, lv_color_hex(active ? 0xFFFFFF : 0xE5EBF0),
            lv_damped_button_pressed_color(lv_color_hex(active ? 0xFFFFFF : 0xE5EBF0)));
        ctx->objects.overview.filter[i] = button;
    }
    ctx->objects.overview.card_button = action(root, "Card view", 918, 15, 170, actions->card, 0);
    surface(root, 1106, 23, 1, 28, 0xB8C4CC, 0);
    ctx->objects.overview.back_button = action(root, "Back", 1124, 15, 132, actions->back, 0);
    static const lv_point_t chevron[] = {{5,0},{0,5},{5,10}};
    stroke(ctx->objects.overview.back_button, chevron, 3, 24, 17, 0x648393);

    lv_obj_t *scroll = surface(root, CURR_GRID_VIEWPORT_X, CURR_GRID_VIEWPORT_Y,
        CURR_GRID_VIEWPORT_W, CURR_GRID_VIEWPORT_H, 0xFFFFFF, 0);
    lv_obj_set_style_bg_opa(scroll, LV_OPA_TRANSP, 0);
    ctx->objects.grid_scroll = scroll;
    lv_obj_add_flag(scroll, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(scroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scroll, LV_SCROLLBAR_MODE_AUTO);

    const int rows = (ctx->model.visible_count + CURR_GRID_COLS - 1) / CURR_GRID_COLS;
    int height = rows ? (rows - 1) * CURR_GRID_ROW_STEP + CURR_GRID_CELL_H : 0;
    lv_obj_t *content = surface(scroll, 0, 0, CURR_GRID_VIEWPORT_W - 12,
        LV_MAX(height, CURR_GRID_VIEWPORT_H), 0xFFFFFF, 0);
    /* No broad white rectangle behind tiles: gaps reveal the page surface. */
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_img_header_t atlas_info;
    bool atlas_available = lv_img_decoder_get_info(CURR_VIEW_ATLAS_PATH, &atlas_info) == LV_RES_OK &&
        atlas_info.w == CURR_VIEW_ATLAS_WIDTH && atlas_info.h == CURR_VIEW_ATLAS_HEIGHT;

    for (int i = 0; i < ctx->model.visible_count; ++i) {
        char code[4];
        int abs_idx = ctx->model.visible_indices[i];
        if (!currency_state_get_code((uint8_t)abs_idx, code)) continue;
        page07_curr_grid_item_t *item = &ctx->grid_items[i];
        item->abs_idx = abs_idx;
        item->item = surface(content,
            CURR_GRID_START_X + i % CURR_GRID_COLS * CURR_GRID_CELL_W,
            CURR_GRID_START_Y + i / CURR_GRID_COLS * CURR_GRID_ROW_STEP,
            CURR_GRID_ITEM_W, CURR_GRID_CELL_H, 0xFFFFFF, 10);
        lv_obj_set_style_border_width(item->item, 1, 0);
        lv_obj_set_style_border_color(item->item, lv_color_hex(0xE2E8ED), 0);
        lv_obj_set_style_bg_color(item->item, lv_color_hex(0xF3F8FF), LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(item->item,
            lv_damped_button_pressed_color(lv_color_hex(0xFFFFFF)), LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(item->item,
            lv_damped_button_pressed_color(lv_color_hex(0xEBF2FE)), LV_STATE_PRESSED | LV_STATE_CHECKED);
        lv_obj_add_flag(item->item, LV_OBJ_FLAG_CLICKABLE);
        lv_port_indev_set_drag_obj(item->item, true);
        lv_obj_add_event_cb(item->item, actions->select, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *flag_box = surface(item->item, 23, 6, CURR_GRID_FLAG_W, CURR_GRID_FLAG_H, 0xFFFFFF, 0);
        lv_obj_set_style_bg_opa(flag_box, LV_OPA_TRANSP, 0);
        lv_obj_add_flag(flag_box, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        item->img = lv_img_create(flag_box);
        grid_flag(item->img, code, atlas_available);
        page07_curr_view_set_image_selected_style(item->img);
        lv_obj_center(item->img);
        item->name = text(item->item, currency_state_display_code(code), 36, 64,
            &lv_font_instrument_sans_medium_18, 0x20313B);
        lv_obj_set_width(item->name, 70);
        lv_obj_set_style_text_align(item->name, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(item->name, LV_LABEL_LONG_CLIP);
        item->selected_mark = surface(item->item, 6, 7, 17, 17, 0x1462CC, LV_RADIUS_CIRCLE);
        static const lv_point_t check_points[] = {{4,8},{7,11},{12,5}};
        stroke(item->selected_mark, check_points, 3, 0, 0, 0xFFFFFF);
        item->fav_btn = surface(item->item, CURR_GRID_FAV_X, CURR_GRID_FAV_Y, 30, 30, 0xFFFFFF, 8);
        lv_obj_set_style_bg_opa(item->fav_btn, LV_OPA_TRANSP, 0);
        lv_obj_add_flag(item->fav_btn, LV_OBJ_FLAG_CLICKABLE);
        lv_port_indev_set_drag_obj(item->fav_btn, true);
        lv_obj_add_event_cb(item->fav_btn, actions->favorite, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(item->fav_btn, actions->favorite_feedback, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(item->fav_btn, actions->favorite_feedback, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(item->fav_btn, actions->favorite_feedback, LV_EVENT_PRESS_LOST, NULL);
        lv_obj_add_event_cb(item->fav_btn, actions->favorite_feedback, LV_EVENT_CANCEL, NULL);
        item->fav_icon = lv_img_create(item->fav_btn);
        lv_img_set_src(item->fav_icon, &curr_star_outline);
        lv_obj_center(item->fav_icon);
    }
    ctx->objects.overview.position = text(root, "", 24, 382, &lv_font_instrument_sans_medium_12, 0x586B78);
    ctx->objects.overview.visible_range_key = -1;
    lv_obj_add_event_cb(scroll, position_update, LV_EVENT_SCROLL, ctx);
    lv_event_send(scroll, LV_EVENT_SCROLL, NULL);
    lv_obj_t *hint = text(root, "Tap to select / Star to save / Swipe for more", 0, 382,
        &lv_font_instrument_sans_medium_12, 0x586B78);
    lv_obj_align(hint, LV_ALIGN_TOP_RIGHT, -24, 382);
    if (!ctx->model.visible_count) {
        lv_obj_t *empty = text(scroll, "No currencies available", 0, 0,
            &lv_font_instrument_sans_medium_18, 0x586B78);
        lv_obj_center(empty);
    }
    page07_curr_overview_selection(ctx);
}
