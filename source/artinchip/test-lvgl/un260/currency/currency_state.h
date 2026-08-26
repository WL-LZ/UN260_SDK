#ifndef CURRENCY_STATE_H
#define CURRENCY_STATE_H

#include <stdbool.h>
#include <stdint.h>
#include "un260/lv_system/user_cfg.h"

/* Three-byte internal selector used for the four-character UI label "AUTO". */
#define CURRENCY_AUTO_CODE "AUT"

typedef struct {
    uint8_t count;
    char codes[MAX_CURRENCIES][4];
    char active_code[4];
    curr_item_t active_currency;
    uint8_t active_index;
} currency_state_snapshot_t;

void currency_state_reset(void);
void currency_state_begin_list_sync(void);
bool currency_state_append_list_code(uint8_t protocol_index, const char code[4]);
bool currency_state_finish_list_sync(void);
bool currency_state_confirm_active_code(const char* code);
bool currency_state_confirm_active_selection(uint8_t index, const char code[4]);
void currency_state_confirm_active_currency(curr_item_t currency);
void currency_state_confirm_active_index(uint8_t index);
void currency_state_get_snapshot(currency_state_snapshot_t* snapshot);
uint8_t currency_state_count(void);
bool currency_state_get_code(uint8_t index, char code[4]);
bool currency_state_find_code(const char* code, uint8_t* index);
void currency_state_get_active_code(char code[4]);
void currency_state_get_selected_code(char code[4]);
void currency_state_get_effective_code(char code[4]);
const char *currency_state_display_code(const char code[4]);
bool currency_state_is_auto_code(const char code[4]);
bool currency_state_auto_selected(void);
bool currency_state_confirm_auto_selection(void);
bool currency_state_leave_auto_selection(void);
void currency_state_begin_count_session(void);
bool currency_state_confirm_detected_code(const char code[4]);
curr_item_t currency_state_active_currency(void);
uint8_t currency_state_active_index(void);
curr_item_t currency_state_code_to_item(const char* code);

#endif
