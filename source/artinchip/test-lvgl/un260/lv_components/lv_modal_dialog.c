#include "lv_modal_dialog.h"

#include <string.h>

#include "un260/lv_components/lv_damped_button.h"

#define MODAL_SCREEN_WIDTH 1280
#define MODAL_SCREEN_HEIGHT 400

static bool modal_obj_valid(const lv_obj_t *object)
{
    return object != NULL && lv_obj_is_valid((lv_obj_t *)object);
}

static void modal_set_text(lv_obj_t *label, const char *text)
{
    const char *current;

    if (!modal_obj_valid(label)) return;
    if (text == NULL) text = "";
    current = lv_label_get_text(label);
    if (current == NULL || strcmp(current, text) != 0) {
        lv_label_set_text(label, text);
    }
}

static void modal_action_event_cb(lv_event_t *event)
{
    lv_modal_dialog_t *dialog = lv_event_get_user_data(event);
    lv_obj_t *target;
    lv_modal_dialog_action_cb_t action = NULL;

    if (lv_event_get_code(event) != LV_EVENT_CLICKED || dialog == NULL) return;
    target = lv_event_get_target(event);
    if (target == dialog->primary_button) action = dialog->primary_action;
    else if (target == dialog->secondary_button) action = dialog->secondary_action;
    if (action != NULL) action(dialog->action_user_data);
}

static void modal_set_button_colors(lv_obj_t *button, uint32_t color,
                                    uint32_t pressed_color)
{
    if (!modal_obj_valid(button)) return;
    lv_obj_set_style_bg_color(button, lv_color_hex(color), 0);
    lv_obj_set_style_border_color(button, lv_color_hex(color), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(pressed_color),
                              LV_STATE_PRESSED);
}

static void modal_create(lv_modal_dialog_t *dialog, lv_obj_t *parent,
                         const lv_modal_dialog_config_t *config)
{
    lv_damped_button_style_t primary_style = {
        .normal_color = config->primary_color,
        .pressed_color = 0x2467DF,
        .disabled_color = 0xCCD3D8,
        .text_color = 0xFFFFFF,
        .disabled_text_color = 0x8A959E,
        .radius = 14,
    };
    lv_damped_button_style_t secondary_style = {
        .normal_color = config->secondary_color,
        .pressed_color = 0x687680,
        .disabled_color = 0xCCD3D8,
        .text_color = 0xFFFFFF,
        .disabled_text_color = 0x8A959E,
        .radius = 14,
    };

    dialog->parent = parent;
    dialog->root = lv_obj_create(parent);
    lv_obj_remove_style_all(dialog->root);
    lv_obj_set_pos(dialog->root, 0, 0);
    lv_obj_set_size(dialog->root, MODAL_SCREEN_WIDTH, MODAL_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(dialog->root, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(dialog->root, LV_OPA_50, 0);
    lv_obj_add_flag(dialog->root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(dialog->root, LV_OBJ_FLAG_SCROLLABLE);

    dialog->panel = lv_obj_create(dialog->root);
    lv_obj_remove_style_all(dialog->panel);
    lv_obj_set_style_bg_color(dialog->panel, lv_color_hex(0xF4F7FB), 0);
    lv_obj_set_style_bg_opa(dialog->panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(dialog->panel, 24, 0);
    lv_obj_set_style_border_width(dialog->panel, 2, 0);
    lv_obj_set_style_border_color(dialog->panel, lv_color_hex(0xD7DEE8), 0);
    lv_obj_set_style_border_opa(dialog->panel, LV_OPA_COVER, 0);
    lv_obj_clear_flag(dialog->panel, LV_OBJ_FLAG_SCROLLABLE);

    dialog->accent = lv_obj_create(dialog->panel);
    lv_obj_remove_style_all(dialog->accent);
    lv_obj_set_pos(dialog->accent, 28, 24);
    lv_obj_set_size(dialog->accent, 8, 54);
    lv_obj_set_style_bg_opa(dialog->accent, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(dialog->accent, 4, 0);

    dialog->title = lv_label_create(dialog->panel);
    lv_obj_set_pos(dialog->title, 54, 24);
    lv_obj_set_style_text_color(dialog->title, lv_color_hex(0x26333E), 0);

    dialog->body = lv_label_create(dialog->panel);
    lv_obj_set_pos(dialog->body, 54, 67);
    lv_label_set_long_mode(dialog->body, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(dialog->body, lv_color_hex(0x6A7885), 0);

    dialog->primary_button = lv_damped_button_create(dialog->panel,
        &primary_style, config->primary_text, config->button_font);
    lv_obj_add_event_cb(dialog->primary_button, modal_action_event_cb,
                        LV_EVENT_CLICKED, dialog);

    dialog->secondary_button = lv_damped_button_create(dialog->panel,
        &secondary_style, config->secondary_text, config->button_font);
    lv_obj_add_event_cb(dialog->secondary_button, modal_action_event_cb,
                        LV_EVENT_CLICKED, dialog);
}

bool lv_modal_dialog_show(lv_modal_dialog_t *dialog,
                          lv_obj_t *parent,
                          const lv_modal_dialog_config_t *config)
{
    lv_coord_t panel_width;
    lv_coord_t panel_height;
    lv_coord_t primary_width;
    lv_coord_t secondary_width;
    lv_coord_t button_y;
    bool has_secondary;

    if (dialog == NULL || parent == NULL || config == NULL) return false;
    if (!modal_obj_valid(dialog->root) || dialog->parent != parent) {
        lv_modal_dialog_destroy(dialog);
        modal_create(dialog, parent, config);
    }
    if (!modal_obj_valid(dialog->root)) return false;

    panel_width = config->panel_width > 0 ? config->panel_width : 700;
    panel_height = config->panel_height > 0 ? config->panel_height : 270;
    primary_width = config->primary_width > 0 ? config->primary_width : 190;
    secondary_width = config->secondary_width > 0 ? config->secondary_width : 220;
    has_secondary = config->secondary_text != NULL && config->secondary_text[0] != '\0';
    button_y = panel_height - 68;

    lv_obj_set_size(dialog->panel, panel_width, panel_height);
    lv_obj_center(dialog->panel);
    lv_obj_set_style_bg_color(dialog->accent,
                              lv_color_hex(config->accent_color), 0);
    modal_set_button_colors(dialog->primary_button, config->primary_color,
                            config->primary_color == 0x3578F6 ? 0x2467DF :
                            config->primary_color == 0xE45454 ? 0xC84646 :
                            config->primary_color);
    modal_set_button_colors(dialog->secondary_button,
                            config->secondary_color, 0x687680);
    lv_obj_set_style_text_font(dialog->title, config->title_font, 0);
    lv_obj_set_style_text_font(dialog->body, config->body_font, 0);
    lv_obj_set_size(dialog->body, panel_width - 95, panel_height - 150);
    modal_set_text(dialog->title, config->title);
    modal_set_text(dialog->body, config->body);

    lv_damped_button_set_text(dialog->primary_button, config->primary_text);
    lv_obj_set_size(dialog->primary_button, primary_width, 48);
    if (has_secondary) {
        lv_coord_t gap = 22;
        lv_coord_t total = primary_width + secondary_width + gap;
        lv_obj_set_pos(dialog->secondary_button,
                       (panel_width - total) / 2, button_y);
        lv_obj_set_size(dialog->secondary_button, secondary_width, 48);
        lv_damped_button_set_text(dialog->secondary_button,
                                  config->secondary_text);
        lv_obj_clear_flag(dialog->secondary_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(dialog->primary_button,
                       (panel_width - total) / 2 + secondary_width + gap,
                       button_y);
    } else {
        lv_obj_add_flag(dialog->secondary_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(dialog->primary_button,
                       config->center_single_button ?
                           (panel_width - primary_width) / 2 :
                           panel_width - primary_width - 40,
                       button_y);
    }

    dialog->primary_action = config->primary_action;
    dialog->secondary_action = config->secondary_action;
    dialog->action_user_data = config->action_user_data;
    lv_obj_clear_flag(dialog->root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(dialog->root);
    return true;
}

void lv_modal_dialog_hide(lv_modal_dialog_t *dialog)
{
    if (dialog == NULL || !modal_obj_valid(dialog->root)) return;
    if (!lv_obj_has_flag(dialog->root, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(dialog->root, LV_OBJ_FLAG_HIDDEN);
    }
    dialog->primary_action = NULL;
    dialog->secondary_action = NULL;
    dialog->action_user_data = NULL;
}

void lv_modal_dialog_destroy(lv_modal_dialog_t *dialog)
{
    if (dialog == NULL) return;
    if (modal_obj_valid(dialog->root)) lv_obj_del(dialog->root);
    memset(dialog, 0, sizeof(*dialog));
}

bool lv_modal_dialog_is_visible(const lv_modal_dialog_t *dialog)
{
    return dialog != NULL && modal_obj_valid(dialog->root) &&
           !lv_obj_has_flag(dialog->root, LV_OBJ_FLAG_HIDDEN);
}
