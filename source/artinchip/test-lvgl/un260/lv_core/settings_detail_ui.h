#ifndef SETTINGS_DETAIL_UI_H
#define SETTINGS_DETAIL_UI_H

#include "lvgl/lvgl.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SETTINGS_DETAIL_KEYBOARD_NUM = 0,
    SETTINGS_DETAIL_KEYBOARD_TEXT,
} settings_detail_keyboard_mode_t;

typedef void (*settings_detail_keyboard_cb_t)(const char* value, void* user_data);
typedef void (*settings_detail_keyboard_close_cb_t)(void* user_data);
typedef void (*settings_detail_dialog_cb_t)(void* user_data);

lv_obj_t* settings_detail_create_page(lv_obj_t* parent, const char* title,
                                      lv_event_cb_t back_cb,
                                      lv_obj_t** out_content);
lv_obj_t* settings_detail_create_page_ex(lv_obj_t* parent, const char* title,
                                         lv_event_cb_t back_cb,
                                         lv_obj_t** out_content,
                                         lv_obj_t** out_back_btn);
lv_obj_t* settings_detail_create_card(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                                      lv_coord_t w, lv_coord_t h);
lv_obj_t* settings_detail_create_button(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                                        lv_coord_t w, lv_coord_t h,
                                        const char* text, lv_color_t bg,
                                        lv_event_cb_t cb, void* user_data);
lv_obj_t* settings_detail_create_label(lv_obj_t* parent, const char* text,
                                       const lv_font_t* font, lv_color_t color,
                                       lv_coord_t x, lv_coord_t y);
lv_obj_t* settings_detail_create_select_box(lv_obj_t* parent,
                                            lv_coord_t x, lv_coord_t y,
                                            lv_coord_t size,
                                            lv_event_cb_t cb,
                                            void* user_data);
void settings_detail_set_select_box_checked(lv_obj_t* box, bool checked);
void settings_detail_set_select_box_active(lv_obj_t* box, bool active);
void settings_detail_set_focus_box_active(lv_obj_t* box, bool active);
bool settings_detail_send_command(uint8_t cmd_g, const uint8_t* cmd_s,
                                  uint16_t cmd_s_len);
bool settings_detail_dialog_show(const char* title,
                                 const char* content,
                                 const char* confirm_text,
                                 const char* cancel_text,
                                 settings_detail_dialog_cb_t confirm_cb,
                                 settings_detail_dialog_cb_t cancel_cb,
                                 void* user_data);
void settings_detail_dialog_hide(void);
bool settings_detail_keyboard_show(const char* title,
                                   const char* init_value,
                                   uint16_t max_len,
                                   settings_detail_keyboard_mode_t mode,
                                   settings_detail_keyboard_cb_t confirm_cb,
                                   void* user_data);
bool settings_detail_keyboard_show_ex(const char* title,
                                      const char* init_value,
                                      uint16_t max_len,
                                      settings_detail_keyboard_mode_t mode,
                                      settings_detail_keyboard_cb_t confirm_cb,
                                      void* user_data,
                                      settings_detail_keyboard_close_cb_t close_cb,
                                      void* close_user_data);
void settings_detail_keyboard_hide(void);

/*
 * Keep the complete settings flow on one #2e85ff-based blue palette. Settings pages
 * include this header, so existing LVGL colors are translated consistently
 * without duplicating theme constants in every sub-page.
 */
lv_color_t settings_theme_color_hex(uint32_t color);

#ifndef SETTINGS_THEME_DISABLE_COLOR_REMAP
#define lv_color_hex settings_theme_color_hex
#endif

#endif
