#ifndef UN260_PANEL_ENTRY_H
#define UN260_PANEL_ENTRY_H
#include "lvgl/lvgl.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Page-owned transient input. Strings and choice descriptors are copied on
 * open. Owners must close_for(user) before their context is destroyed. */
typedef enum { PANEL_ENTRY_NUMBER, PANEL_ENTRY_PIN } panel_entry_kind_t;
typedef bool (*panel_entry_confirm_cb_t)(const char *text, void *user);
typedef void (*panel_entry_cancel_cb_t)(void *user);
typedef struct {
    panel_entry_kind_t kind;
    const char *title, *prompt, *field_label, *unit, *range, *initial;
    uint32_t min, max, off_value;
    uint8_t max_digits;
    bool allow_off;
    panel_entry_confirm_cb_t confirm;
    panel_entry_cancel_cb_t cancel;
    void *user;
} panel_entry_config_t;
bool panel_entry_open(lv_obj_t *parent, const panel_entry_config_t *config);
bool panel_entry_is_open(void);
bool panel_entry_is_open_for(const void *user);
void panel_entry_set_error(const char *message);
void panel_entry_close_for(const void *user);
void panel_entry_close(void);
void panel_entry_destroy(void);

typedef struct { uint32_t value; const char *label; bool selected, disabled; } panel_choice_item_t;
typedef bool (*panel_choice_confirm_cb_t)(uint32_t value, void *user);
typedef struct {
    const char *title;
    const panel_choice_item_t *items;
    size_t count;
    panel_choice_confirm_cb_t confirm;
    panel_entry_cancel_cb_t cancel;
    void *user;
} panel_choice_config_t;
/* parent is the fixed 792x400 central content parent. No entry/exit animation. */
bool panel_choice_open(lv_obj_t *parent, const panel_choice_config_t *config);
bool panel_choice_is_open(void);
bool panel_choice_is_open_for(const void *user);
void panel_choice_set_error(const char *message);
void panel_choice_close_for(const void *user);
void panel_choice_close(void);
#endif
