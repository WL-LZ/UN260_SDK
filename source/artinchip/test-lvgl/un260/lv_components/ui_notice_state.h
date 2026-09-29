#ifndef UI_NOTICE_STATE_H
#define UI_NOTICE_STATE_H

#include <stdbool.h>
#include <stdint.h>
#include "un260/lv_system/ui_message.h"

#define UI_NOTICE_QUEUE_CAPACITY 8
#define UI_NOTICE_KEY_CAPACITY 48
#define UI_NOTICE_TITLE_CAPACITY UI_MESSAGE_KEY_CAPACITY
#define UI_NOTICE_DETAIL_CAPACITY 320

typedef enum {
    UI_NOTICE_SUCCESS,
    UI_NOTICE_ERROR,
    UI_NOTICE_WARNING,
    UI_NOTICE_PROGRESS,
    UI_NOTICE_INFO
} ui_notice_kind_t;

typedef struct {
    ui_notice_kind_t kind;
    const char *key;
    const char *title;
    const char *detail;
    /* 0 selects the type default; progress never expires. */
    uint32_t duration_ms;
    bool localized;
    const ui_message_t *message;
} ui_notice_config_t;

typedef struct {
    ui_notice_kind_t kind;
    char key[UI_NOTICE_KEY_CAPACITY];
    char title[UI_NOTICE_TITLE_CAPACITY];
    char detail[UI_NOTICE_DETAIL_CAPACITY];
    uint32_t remaining_ms;
    uint32_t revision;
    uint16_t repeats;
    bool localized;
    bool has_message;
    ui_message_t message;
} ui_notice_item_t;

/* Presentation state only: no protocol, business timers, callbacks or heap. */
typedef struct {
    ui_notice_item_t active;
    ui_notice_item_t queued[UI_NOTICE_QUEUE_CAPACITY];
    char dismissed_progress[UI_NOTICE_QUEUE_CAPACITY][UI_NOTICE_KEY_CAPACITY];
    uint32_t revision;
    uint8_t queued_count;
    uint8_t dismissed_count;
    bool has_active;
} ui_notice_state_t;

void ui_notice_state_init(ui_notice_state_t *state);
bool ui_notice_state_post(ui_notice_state_t *state, const ui_notice_config_t *config);
/* NULL key targets the current card. keep_dismissed suppresses repeated progress. */
bool ui_notice_state_remove(ui_notice_state_t *state, const char *key, bool keep_dismissed);
/* Consume visible time only; returns true when the current notice expires. */
bool ui_notice_state_elapse(ui_notice_state_t *state, uint32_t elapsed_ms);

#endif
