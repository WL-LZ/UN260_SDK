#include "page_07_curr_overview.h"
#include "page_07_curr_layout.h"
#include "page_07_curr_view.h"
#include "page_07_curr_star.h"
#include "un260/currency/currency_state.h"
#include "un260/lv_components/lv_damped_button.h"
#include "un260/lv_resources/lv_img_init.h"
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

void page07_curr_overview_selection(page07_curr_context_t *ctx)
{
    if (!ctx->objects.grid_layer) return;
    char code[4];
    if (currency_state_get_code((uint8_t)ctx->model.selected_abs_idx, code)) {
        lv_img_set_src(ctx->objects.overview.current_img, get_currency_img(code));
        page07_curr_view_set_img_target_width(ctx->objects.overview.current_img, code, 30);
        lv_obj_center(ctx->objects.overview.current_img);
        lv_label_set_text(ctx->objects.overview.current_code, currency_state_display_code(code));
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
            lv_color_hex(selected ? 0x1559B7 : 0x20313B), 0);
    }
}

void page07_curr_overview_build(page07_curr_context_t *ctx,
                               const page07_curr_overview_actions_t *actions)
{
    lv_obj_t *root = surface(ctx->objects.root, 0, 0, 1280, 400, 0xEBF0F8, 0);
    ctx->objects.grid_layer = root;
    lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_grad_color(root, lv_color_hex(0xF8F6F3), 0);
    lv_obj_set_style_bg_grad_dir(root, LV_GRAD_DIR_HOR, 0);
    text(root, "Currency", 24, 20, &lv_font_instrument_sans_semibold_28, 0x20313B);

    lv_obj_t *current = surface(root, 208, 14, 180, 46, 0xFFFFFF, 12);
    lv_obj_t *current_flag = surface(current, 10, 8, 34, 30, 0xFFFFFF, 0);
    lv_obj_add_flag(current_flag, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    ctx->objects.overview.current_img = lv_img_create(current_flag);
    text(current, "Current", 56, 5, &lv_font_instrument_sans_medium_12, 0x637887);
    ctx->objects.overview.current_code = text(current, "", 56, 21,
        &lv_font_instrument_sans_medium_18, 0x20313B);

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

    lv_obj_t *scroll = surface(root, CURR_GRID_VIEWPORT_X, CURR_GRID_VIEWPORT_Y,
        CURR_GRID_VIEWPORT_W, CURR_GRID_VIEWPORT_H, 0xFFFFFF, 14);
    ctx->objects.grid_scroll = scroll;
    lv_obj_add_flag(scroll, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(scroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scroll, LV_SCROLLBAR_MODE_AUTO);

    const int rows = (ctx->model.visible_count + CURR_GRID_COLS - 1) / CURR_GRID_COLS;
    int height = CURR_GRID_START_Y + rows * CURR_GRID_ROW_STEP;
    lv_obj_t *content = surface(scroll, 0, 0, CURR_GRID_VIEWPORT_W - 12,
        LV_MAX(height, CURR_GRID_VIEWPORT_H), 0xFFFFFF, 0);
    /* The viewport owns the white rounded surface. An opaque rectangular
     * child would paint over its corners at the first/last scroll position. */
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    for (int row = 1; row < rows; ++row)
        surface(content, 18, CURR_GRID_START_Y + row * CURR_GRID_ROW_STEP - 3,
                CURR_GRID_VIEWPORT_W - 48, 1, 0xE8EDF1, 0);

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
        lv_obj_set_style_bg_color(item->item, lv_color_hex(0xEBF2FE), LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(item->item,
            lv_damped_button_pressed_color(lv_color_hex(0xFFFFFF)), LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(item->item,
            lv_damped_button_pressed_color(lv_color_hex(0xEBF2FE)), LV_STATE_PRESSED | LV_STATE_CHECKED);
        lv_obj_add_flag(item->item, LV_OBJ_FLAG_CLICKABLE);
        lv_port_indev_set_drag_obj(item->item, true);
        lv_obj_add_event_cb(item->item, actions->select, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *flag_box = surface(item->item, 10, 8, 34, 28, 0xFFFFFF, 0);
        lv_obj_set_style_bg_opa(flag_box, LV_OPA_TRANSP, 0);
        lv_obj_add_flag(flag_box, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        item->img = lv_img_create(flag_box);
        lv_img_set_src(item->img, get_currency_img(code));
        page07_curr_view_set_img_target_width(item->img, code, CURR_GRID_FLAG_W);
        page07_curr_view_set_image_selected_style(item->img);
        lv_obj_center(item->img);
        item->name = text(item->item, currency_state_display_code(code), 48, 12,
            &lv_font_instrument_sans_medium_18, 0x20313B);
        /* AUTO/MULTI have no favorite control, so their full mode names can
         * use that space instead of being clipped to three-letter geometry. */
        bool mode = currency_state_is_auto_code(code) || currency_state_is_multi_code(code);
        lv_obj_set_width(item->name, mode ? 84 : 56);
        lv_label_set_long_mode(item->name, LV_LABEL_LONG_CLIP);
        lv_obj_t *subtitle = text(item->item, description(code), 11, 44,
            &lv_font_instrument_sans_medium_12, 0x647786);
        lv_obj_set_width(subtitle, 124);
        lv_label_set_long_mode(subtitle, LV_LABEL_LONG_DOT);

        item->selected_mark = surface(item->item, 0, 11, 3, 24, 0x1559B7, 2);
        item->fav_btn = surface(item->item, CURR_GRID_FAV_X, CURR_GRID_FAV_Y, 40, 40, 0xFFFFFF, 10);
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
    lv_obj_t *count = text(root, "", 24, 381, &lv_font_instrument_sans_medium_12, 0x586B78);
    lv_label_set_text_fmt(count, "%d available / %s", ctx->model.visible_count,
        ctx->model.favorite_only ? "Favorites" : "All currencies");
    lv_obj_t *hint = text(root, "Tap to select / Star to save / Swipe for more", 0, 381,
        &lv_font_instrument_sans_medium_12, 0x586B78);
    lv_obj_align(hint, LV_ALIGN_TOP_RIGHT, -24, 381);
    if (!ctx->model.visible_count) {
        lv_obj_t *empty = text(scroll, "No currencies available", 0, 0,
            &lv_font_instrument_sans_medium_18, 0x586B78);
        lv_obj_center(empty);
    }
    page07_curr_overview_selection(ctx);
}
