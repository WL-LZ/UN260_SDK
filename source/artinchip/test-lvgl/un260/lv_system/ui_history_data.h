#ifndef UI_HISTORY_DATA_H
#define UI_HISTORY_DATA_H

#include <stdbool.h>
#include <stdint.h>

#include "un260/counting/counting_data_types.h"

#define UI_HISTORY_MAX_RECORDS 20

typedef struct {
    bool valid;
    bool selected;
    uint8_t slot_no;
    uint32_t record_no;
    uint32_t pcs;
    uint32_t amount;
    char currency[4];
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    char denom_text[320];
    char sn_text[640];
    char sn_detail_text[3072];
    char error_frame_text[160];
    char start_frame_text[160];
    char end_frame_text[160];
    char session_log[3072];
} ui_history_record_t;

typedef struct {
    uint32_t total_notes_counted;
    uint32_t next_record_no;
    uint8_t next_slot_no;
    uint8_t record_count;
    ui_history_record_t records[UI_HISTORY_MAX_RECORDS];
} ui_history_store_t;

void ui_history_data_init(void);
const ui_history_store_t *ui_history_data_get(void);
uint32_t ui_history_total_notes_counted_get(void);
void ui_history_total_notes_counted_set(uint32_t total);
void ui_history_total_notes_counted_clear(void);
bool ui_history_record_append_from_session(const counting_sim_t *sim_data, uint32_t pcs_total,
                                           float amount_total, uint32_t total_notes_after,
                                           const char *error_frame_text,
                                           const char *start_frame_text, const char *end_frame_text,
                                           const char *session_log_text);
bool ui_history_record_toggle_selected(uint8_t index);
bool ui_history_record_set_selected(uint8_t index, bool selected);
int ui_history_record_selected_first_index_get(void);
int ui_history_record_selected_count_get(void);
void ui_history_record_clear_selected(void);
void ui_history_record_set_all_selected(bool selected);
bool ui_history_record_delete_selected(void);
bool ui_history_record_get(uint8_t index, ui_history_record_t *out);
bool ui_history_record_get_by_no(uint32_t record_no, ui_history_record_t *out);
#endif
