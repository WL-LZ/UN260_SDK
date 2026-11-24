#include "un260/font/ui_message_font.h"
#include "un260/lv_system/ui_i18n.h"
#include "page_07_curr_overview.h"
#include "page_07_curr_layout.h"
#include "page_07_curr_view.h"
#include "page_07_curr_view_icons.h"
#include "un260/currency/currency_state.h"
#include "un260/lv_components/lv_damped_button.h"
#include "un260/lv_components/lv_nav_button.h"
#include "un260/lv_resources/lv_img_init.h"
#include "un260/lv_resources/lv_curr/curr_view_atlas.h"
#include "lv_port_indev.h"
#include <stdint.h>
#include <string.h>

LV_FONT_DECLARE(lv_font_instrument_sans_semibold_26);

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

static lv_obj_t *text(lv_obj_t *parent, const char *value,
                      const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, value);
    lv_obj_set_style_text_font(label, ui_message_font(font), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_update_layout(label);
    return label;
}

static void center_y(lv_obj_t *obj, int center)
{
    lv_obj_update_layout(obj);
    lv_obj_set_y(obj, center - lv_obj_get_height(obj) / 2);
}

static lv_obj_t *action(lv_obj_t *parent, int x, int y, int w, int h,
                        lv_event_cb_t cb, void *data)
{
    lv_obj_t *button = surface(parent, x, y, w, h, 0xF4F6F7, 11);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, data);
    lv_damped_button_register(button, lv_color_hex(0xF4F6F7),
                              lv_color_hex(0xDFE9EE));
    return button;
}

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

static void press_surface(page07_curr_context_t *ctx, lv_obj_t *target, bool pressed)
{
    for (int i = 0; i < ctx->model.visible_count; ++i) {
        page07_curr_grid_item_t *item = &ctx->grid_items[i];
        if (item->item == target && item->selected_mark) {
            lv_obj_set_style_bg_color(item->selected_mark,
                lv_color_hex(pressed ? 0xDFE9EE : 0xF3F7FC), 0);
            return;
        }
    }
}

static void cancel_press(page07_curr_context_t *ctx, lv_indev_t *indev)
{
    page07_curr_overview_refs_t *refs = &ctx->objects.overview;
    if (refs->click_cancelled) return;
    refs->click_cancelled = true;
    if (!refs->pressed) return;
    lv_obj_clear_state(refs->pressed, LV_STATE_PRESSED);
    press_surface(ctx, refs->pressed, false);
    /* CANCEL reaches the shared sound/press feedback before SHORT_CLICKED.
     * It leaves native horizontal scrolling and its input ownership intact. */
    lv_event_send(refs->pressed, LV_EVENT_CANCEL, indev);
}

static void contact_check(page07_curr_context_t *ctx, lv_indev_t *indev)
{
    page07_curr_overview_refs_t *refs = &ctx->objects.overview;
    if (!refs->pressed || !indev || lv_indev_get_type(indev) != LV_INDEV_TYPE_POINTER ||
        refs->click_cancelled) return;
    lv_point_t point;
    lv_area_t bounds;
    lv_indev_get_point(indev, &point);
    lv_obj_get_coords(refs->pressed, &bounds);
    int threshold = LV_MAX(8, indev->driver->scroll_limit);
    if (LV_ABS(point.x - refs->press_point.x) > threshold ||
        LV_ABS(point.y - refs->press_point.y) > threshold ||
        point.x < bounds.x1 || point.x > bounds.x2 ||
        point.y < bounds.y1 || point.y > bounds.y2)
        cancel_press(ctx, indev);
}

static void item_event(lv_event_t *event)
{
    page07_curr_context_t *ctx = lv_event_get_user_data(event);
    page07_curr_overview_refs_t *refs = &ctx->objects.overview;
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *target = lv_event_get_target(event);
    lv_indev_t *indev = lv_indev_get_act();
    if (code == LV_EVENT_PRESSED) {
        refs->pressed = target;
        refs->click_cancelled = false;
        if (indev && lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER)
            lv_indev_get_point(indev, &refs->press_point);
        press_surface(ctx, target, true);
    } else if (code == LV_EVENT_PRESSING) {
        contact_check(ctx, indev);
    } else if (code == LV_EVENT_RELEASED) {
        contact_check(ctx, indev);
        press_surface(ctx, target, false);
        refs->pressed = NULL;
    } else if (code == LV_EVENT_CANCEL || code == LV_EVENT_PRESS_LOST) {
        refs->click_cancelled = true;
        lv_obj_clear_state(target, LV_STATE_PRESSED);
        press_surface(ctx, target, false);
        if (code == LV_EVENT_PRESS_LOST) refs->pressed = NULL;
    } else if ((code == LV_EVENT_SHORT_CLICKED || code == LV_EVENT_CLICKED) &&
               refs->click_cancelled) {
        lv_event_stop_processing(event);
    }
}

void page07_curr_overview_set_active(page07_curr_context_t *ctx, bool active)
{
    if (!ctx) return;
    page07_curr_overview_refs_t *refs = &ctx->objects.overview;
    if (!active) {
        cancel_press(ctx, lv_indev_get_act());
        refs->pressed = NULL;
        lv_img_cache_unpin(refs->atlas_pin);
        refs->atlas_pin = NULL;
        return;
    }
    if (!ctx->objects.grid_layer || refs->atlas_pin || !ctx->model.visible_count) return;
#if LV_IMG_CACHE_DEF_SIZE
    refs->atlas_pin = _lv_img_cache_open(CURR_VIEW_ATLAS_PATH, lv_color_black(), 0);
#endif
    if (refs->atlas_pin && (refs->atlas_pin->dec_dsc.header.w != CURR_VIEW_ATLAS_WIDTH ||
                           refs->atlas_pin->dec_dsc.header.h != CURR_VIEW_ATLAS_HEIGHT))
        refs->atlas_pin = NULL;
    lv_img_cache_pin(refs->atlas_pin);
    /* Missing artwork or exhausted DMA memory falls back to the original
     * files. Retry only on entry, never while a page is being dragged. */
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
    page07_curr_overview_set_active(lv_event_get_user_data(event), false);
}

static void position_update(lv_event_t *event)
{
    page07_curr_context_t *ctx = lv_event_get_user_data(event);
    page07_curr_overview_refs_t *refs = &ctx->objects.overview;
    int offset = LV_MAX(0, lv_obj_get_scroll_x(ctx->objects.grid_scroll));
    int page = LV_MIN(refs->page_count - 1,
        (offset + CURR_GRID_VIEWPORT_W / 2) / CURR_GRID_VIEWPORT_W);
    if (refs->page_index == page) return;
    refs->page_index = page;
    if (!refs->position) return;
    lv_label_set_text_fmt(refs->position, "%d / %d", page + 1, refs->page_count);
    lv_obj_align(refs->position, LV_ALIGN_BOTTOM_RIGHT, -100, -18);
    if (page == 0) lv_obj_add_state(refs->page_prev, LV_STATE_DISABLED);
    else lv_obj_clear_state(refs->page_prev, LV_STATE_DISABLED);
    if (page == refs->page_count - 1) lv_obj_add_state(refs->page_next, LV_STATE_DISABLED);
    else lv_obj_clear_state(refs->page_next, LV_STATE_DISABLED);
}

static void page_step(lv_event_t *event)
{
    page07_curr_context_t *ctx = lv_event_get_user_data(event);
    page07_curr_overview_refs_t *refs = &ctx->objects.overview;
    int direction = lv_event_get_target(event) == refs->page_prev ? -1 : 1;
    int page = LV_CLAMP(0, refs->page_index + direction, refs->page_count - 1);
    lv_obj_scroll_to_x(ctx->objects.grid_scroll, page * CURR_GRID_VIEWPORT_W, LV_ANIM_OFF);
    lv_event_send(ctx->objects.grid_scroll, LV_EVENT_SCROLL, NULL);
}

void page07_curr_overview_reveal_selection(page07_curr_context_t *ctx)
{
    if (!ctx || !ctx->objects.grid_scroll) return;
    int visible = page07_curr_model_find_visible_pos(ctx->model.selected_abs_idx);
    lv_obj_update_layout(ctx->objects.grid_scroll);
    lv_obj_scroll_to_x(ctx->objects.grid_scroll,
        (visible / CURR_GRID_PAGE_ITEMS) * CURR_GRID_VIEWPORT_W, LV_ANIM_OFF);
    lv_event_send(ctx->objects.grid_scroll, LV_EVENT_SCROLL, NULL);
}

void page07_curr_overview_selection(page07_curr_context_t *ctx)
{
    if (!ctx->objects.grid_layer) return;
    char code[4];
    if (currency_state_get_code((uint8_t)ctx->model.selected_abs_idx, code)) {
        lv_label_set_text(ctx->objects.overview.current_code,
                          currency_state_display_code(code));
        center_y(ctx->objects.overview.current_code, 34);
    }
    int favorites = 0;
    int total = currency_state_count();
    for (int i = 0; i < total; ++i)
        if (page07_curr_model_is_favorite(i)) ++favorites;
    for (int i = 0; i < 2; ++i) {
        lv_obj_t *label = ctx->objects.overview.filter_label[i];
        lv_label_set_text_fmt(label, "%s %d", i ? ui_tr("Favorites") : ui_trc("Currency filter", "All"),
                              i ? favorites : total);
        lv_obj_center(label);
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
        lv_obj_set_style_text_color(item->name,
            lv_color_hex(selected ? 0x185BC2 : 0x1D2C36), 0);
    }
}

static void page_arrow(lv_obj_t *button, bool next)
{
    static const lv_point_t previous[] = {{6,0},{0,6},{6,12}};
    static const lv_point_t following[] = {{0,0},{6,6},{0,12}};
    lv_obj_t *line = lv_line_create(button);
    lv_line_set_points(line, next ? following : previous, 3);
    lv_obj_set_style_line_color(line, lv_color_hex(0x516879), 0);
    lv_obj_set_style_line_width(line, 2, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_obj_center(line);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(button, LV_OPA_40, LV_STATE_DISABLED);
}

static void header_build(page07_curr_context_t *ctx,
                          const page07_curr_overview_actions_t *actions)
{
    page07_curr_overview_refs_t *refs = &ctx->objects.overview;
    lv_obj_t *bar = surface(ctx->objects.grid_layer, 0, 0, 1280, 68, 0xFFFFFF, 0);
    refs->back_button = lv_nav_back_text_button_create(bar, 24, 13,
        &lv_font_instrument_sans_medium_16, actions->back, NULL);
    lv_obj_t *title = text(bar, ui_tr("Currency"), &lv_font_instrument_sans_semibold_26, 0x1D2C36);
    lv_obj_set_x(title, 114);
    center_y(title, 34);
    int current_x = 114 + lv_obj_get_width(title) + 20;
    surface(bar, current_x, 22, 1, 24, 0xE4EAEE, 0);
    lv_obj_t *selected = text(bar, ui_tr("Selected"), &lv_font_instrument_sans_medium_16, 0x516879);
    lv_obj_set_x(selected, current_x + 19);
    center_y(selected, 34);
    current_x += 19 + lv_obj_get_width(selected) + 7;
    refs->current_code = text(bar, "", &lv_font_instrument_sans_semibold_16, 0x1D2C36);
    lv_obj_set_x(refs->current_code, current_x);

    lv_obj_t *card_label = text(bar, ui_tr("Card view"), &lv_font_instrument_sans_medium_16, 0x516879);
    int card_width = LV_MAX(130, lv_obj_get_width(card_label) + 53);
    int card_x = 1256 - card_width;
    refs->card_button = action(bar, card_x, 13, card_width, 42, actions->card, NULL);
    lv_obj_set_parent(card_label, refs->card_button);
    lv_obj_set_x(card_label, (card_width - lv_obj_get_width(card_label) - 25) / 2 + 25);
    center_y(card_label, 21);
    lv_obj_t *icon = lv_img_create(refs->card_button);
    lv_img_set_src(icon, &curr_view_card_icon);
    lv_obj_set_pos(icon, lv_obj_get_x(card_label) - 25, 12);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);

    int filter_width = 248;
    int filter_x = (current_x + 64 + card_x - filter_width) / 2;
    lv_obj_t *filters = surface(bar, filter_x, 12, filter_width, 44, 0xF4F6F7, 13);
    for (int i = 0; i < 2; ++i) {
        bool active = (bool)i == ctx->model.favorite_only;
        refs->filter[i] = action(filters, 4 + i * 120, 4, 120, 36,
                                 actions->filter, (void *)(intptr_t)i);
        lv_damped_button_set_palette(refs->filter[i],
            lv_color_hex(active ? 0xFFFFFF : 0xF4F6F7), lv_color_hex(0xDFE9EE));
        refs->filter_label[i] = text(refs->filter[i], "",
            &lv_font_instrument_sans_medium_16, active ? 0x185BC2 : 0x516879);
    }
}

void page07_curr_overview_build(page07_curr_context_t *ctx,
                               const page07_curr_overview_actions_t *actions)
{
    page07_curr_overview_refs_t *refs = &ctx->objects.overview;
    lv_obj_t *root = surface(ctx->objects.root, 0, 0, 1280, 400, 0xEEF2F6, 0);
    ctx->objects.grid_layer = root;
    lv_obj_add_event_cb(root, overview_delete, LV_EVENT_DELETE, ctx);
    lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_grad_color(root, lv_color_hex(0xF6F6F5), 0);
    lv_obj_set_style_bg_grad_dir(root, LV_GRAD_DIR_HOR, 0);
    header_build(ctx, actions);
    refs->page_count = LV_MAX(1, (ctx->model.visible_count + CURR_GRID_PAGE_ITEMS - 1) / CURR_GRID_PAGE_ITEMS);
    refs->page_index = -1;
    refs->pressed = NULL;
    refs->click_cancelled = false;
    int height = refs->page_count > 1 ? CURR_GRID_PAGED_VIEWPORT_H : CURR_GRID_VIEWPORT_H;
    lv_obj_t *scroll = surface(root, CURR_GRID_VIEWPORT_X, CURR_GRID_VIEWPORT_Y,
        CURR_GRID_VIEWPORT_W, height, 0xFFFFFF, 15);
    ctx->objects.grid_scroll = scroll;
    lv_obj_set_style_clip_corner(scroll, true, 0);
    lv_obj_add_flag(scroll, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ONE);
    lv_obj_set_scroll_dir(scroll, LV_DIR_HOR);
    lv_obj_set_scroll_snap_x(scroll, LV_SCROLL_SNAP_START);
    lv_obj_add_event_cb(scroll, position_update, LV_EVENT_SCROLL, ctx);
    lv_img_header_t atlas_info;
    bool atlas_available = lv_img_decoder_get_info(CURR_VIEW_ATLAS_PATH, &atlas_info) == LV_RES_OK &&
        atlas_info.w == CURR_VIEW_ATLAS_WIDTH && atlas_info.h == CURR_VIEW_ATLAS_HEIGHT;
    for (int page = 0; page < refs->page_count; ++page) {
        lv_obj_t *content = surface(scroll, page * CURR_GRID_VIEWPORT_W, 0,
            CURR_GRID_VIEWPORT_W, height, 0xFFFFFF, 0);
        lv_obj_add_flag(content, LV_OBJ_FLAG_SNAPPABLE);
        for (int slot = 0; slot < CURR_GRID_PAGE_ITEMS; ++slot) {
            int i = page * CURR_GRID_PAGE_ITEMS + slot;
            if (i >= ctx->model.visible_count) break;
            char code[4];
            int abs_idx = ctx->model.visible_indices[i];
            if (!currency_state_get_code((uint8_t)abs_idx, code)) continue;
            int col = slot % CURR_GRID_COLS, row = slot / CURR_GRID_COLS;
            int top = row * height / CURR_GRID_ROWS;
            int cell_h = (row + 1) * height / CURR_GRID_ROWS - top;
            page07_curr_grid_item_t *item = &ctx->grid_items[i];
            item->abs_idx = abs_idx;
            item->item = surface(content, col * CURR_GRID_CELL_W, top,
                CURR_GRID_CELL_W, cell_h, 0xFFFFFF, 0);
            lv_obj_set_style_bg_color(item->item, lv_color_hex(0xDFE9EE), LV_STATE_PRESSED);
            lv_obj_add_flag(item->item, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(item->item, LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_SNAPPABLE);
            lv_port_indev_set_drag_obj(item->item, true);
            lv_obj_add_event_cb(item->item, item_event, LV_EVENT_ALL, ctx);
            lv_obj_add_event_cb(item->item, actions->select, LV_EVENT_CLICKED, (void *)(intptr_t)i);
            item->selected_mark = surface(item->item, 6, 4, CURR_GRID_CELL_W - 12,
                cell_h - 8, 0xF3F7FC, 10);
            lv_obj_t *identity = surface(item->item, 0, 0, 1, CURR_GRID_FLAG_H, 0xFFFFFF, 0);
            lv_obj_set_style_bg_opa(identity, LV_OPA_TRANSP, 0);
            lv_obj_add_flag(identity, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
            item->name = text(identity, currency_state_display_code(code),
                &lv_font_instrument_sans_semibold_20, 0x1D2C36);
            int identity_h = LV_MAX(CURR_GRID_FLAG_H, lv_obj_get_height(item->name));
            int identity_w = CURR_GRID_FLAG_W + 8 + lv_obj_get_width(item->name);
            lv_obj_set_size(identity, identity_w, identity_h);
            lv_obj_set_pos(identity, (CURR_GRID_CELL_W - identity_w) / 2, (cell_h - identity_h) / 2);
            lv_obj_set_x(item->name, CURR_GRID_FLAG_W + 8);
            center_y(item->name, identity_h / 2);
            lv_obj_t *flag_box = surface(identity, 0, (identity_h - CURR_GRID_FLAG_H) / 2,
                CURR_GRID_FLAG_W, CURR_GRID_FLAG_H, 0xFFFFFF, 0);
            lv_obj_set_style_bg_opa(flag_box, LV_OPA_TRANSP, 0);
            lv_obj_add_flag(flag_box, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
            item->img = lv_img_create(flag_box);
            grid_flag(item->img, code, atlas_available);
            lv_obj_center(item->img);
            item->fav_btn = surface(item->item, CURR_GRID_FAV_X, cell_h < 72 ? 5 : CURR_GRID_FAV_Y,
                16, 16, 0xFFFFFF, 0);
            lv_obj_set_style_bg_opa(item->fav_btn, LV_OPA_TRANSP, 0);
            lv_obj_add_flag(item->fav_btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(item->fav_btn, LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_SNAPPABLE);
            lv_port_indev_set_drag_obj(item->fav_btn, true);
            lv_obj_add_event_cb(item->fav_btn, item_event, LV_EVENT_ALL, ctx);
            lv_obj_add_event_cb(item->fav_btn, actions->favorite, LV_EVENT_CLICKED, (void *)(intptr_t)i);
            lv_obj_add_event_cb(item->fav_btn, actions->favorite_feedback, LV_EVENT_PRESSED, NULL);
            lv_obj_add_event_cb(item->fav_btn, actions->favorite_feedback, LV_EVENT_RELEASED, NULL);
            lv_obj_add_event_cb(item->fav_btn, actions->favorite_feedback, LV_EVENT_PRESS_LOST, NULL);
            lv_obj_add_event_cb(item->fav_btn, actions->favorite_feedback, LV_EVENT_CANCEL, NULL);
            item->fav_icon = lv_img_create(item->fav_btn);
            lv_img_set_src(item->fav_icon, &curr_view_star_outline);
            lv_obj_center(item->fav_icon);
            if (row < CURR_GRID_ROWS - 1)
                surface(item->item, 8, cell_h - 1, CURR_GRID_CELL_W - 16, 1, 0xEFF3F5, 0);
            if (col < CURR_GRID_COLS - 1)
                surface(item->item, CURR_GRID_CELL_W - 1, cell_h * 23 / 100,
                    1, cell_h * 54 / 100, 0xE6ECF0, 0);
        }
    }
    if (refs->page_count > 1) {
        refs->page_prev = action(root, 1036, 354, 76, 36, page_step, ctx);
        refs->page_next = action(root, 1180, 354, 76, 36, page_step, ctx);
        page_arrow(refs->page_prev, false);
        page_arrow(refs->page_next, true);
        refs->position = text(root, "", &lv_font_instrument_sans_medium_16, 0x516879);
    }
    if (!ctx->model.visible_count) {
        ctx->objects.empty_label = text(scroll, ui_tr("No currencies available"),
            &lv_font_instrument_sans_medium_18, 0x516879);
        lv_obj_center(ctx->objects.empty_label);
    }
    page07_curr_overview_selection(ctx);
    refs->page_index = -1;
    lv_event_send(scroll, LV_EVENT_SCROLL, NULL);
}
