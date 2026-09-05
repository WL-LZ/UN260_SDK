#ifndef LV_MODAL_DIALOG_H
#define LV_MODAL_DIALOG_H

#include "lvgl/lvgl.h"
#include <stdbool.h>
#include <stdint.h>

typedef void (*lv_modal_dialog_action_cb_t)(void *user_data);

typedef struct {
    lv_obj_t *root;
    lv_obj_t *panel;
    lv_obj_t *accent;
    lv_obj_t *title;
    lv_obj_t *body;
    lv_obj_t *primary_button;
    lv_obj_t *secondary_button;
    lv_obj_t *parent;
    lv_modal_dialog_action_cb_t primary_action;
    lv_modal_dialog_action_cb_t secondary_action;
    void *action_user_data;
} lv_modal_dialog_t;

typedef struct {
    const char *title;
    const char *body;
    const char *primary_text;
    const char *secondary_text;
    const lv_font_t *title_font;
    const lv_font_t *body_font;
    const lv_font_t *button_font;
    lv_coord_t panel_width;
    lv_coord_t panel_height;
    lv_coord_t primary_width;
    lv_coord_t secondary_width;
    uint32_t accent_color;
    uint32_t primary_color;
    uint32_t secondary_color;
    lv_modal_dialog_action_cb_t primary_action;
    lv_modal_dialog_action_cb_t secondary_action;
    void *action_user_data;
    bool center_single_button;
} lv_modal_dialog_config_t;

bool lv_modal_dialog_show(lv_modal_dialog_t *dialog,
                          lv_obj_t *parent,
                          const lv_modal_dialog_config_t *config);
void lv_modal_dialog_hide(lv_modal_dialog_t *dialog);
void lv_modal_dialog_destroy(lv_modal_dialog_t *dialog);
bool lv_modal_dialog_is_visible(const lv_modal_dialog_t *dialog);

#endif
