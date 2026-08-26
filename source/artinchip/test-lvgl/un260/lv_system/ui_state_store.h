#ifndef UI_STATE_STORE_H
#define UI_STATE_STORE_H

#include <stdbool.h>

#include "un260/lv_system/user_cfg.h"

#define UI_STATE_STORE_MAGIC   0x55495354U
#define UI_STATE_STORE_VERSION 1U

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int view_mode;
    int fav_only;
    int selected_abs_idx;
    int fav_count;
    char fav_codes[MAX_CURRENCIES][4];
} ui_state_page07_t;

typedef struct {
    int reserved05_enable;
    int reserved06_enable;
} ui_state_common_page_t;

typedef struct {
    int detail_section;
} ui_state_page01_t;

typedef struct {
    int reserved18_enable;
} ui_state_page18_t;

typedef struct {
    unsigned int magic;
    unsigned int version;
    ui_state_page01_t page01;
    ui_state_page07_t page07;
    ui_state_common_page_t page05;
    ui_state_common_page_t page06;
    ui_state_page18_t page18;
} ui_persist_state_t;

bool ui_state_store_load(ui_persist_state_t* state);
bool ui_state_store_save(const ui_persist_state_t* state);

#ifdef __cplusplus
}
#endif

#endif // UI_STATE_STORE_H
