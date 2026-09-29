#ifndef UI_MESSAGE_H
#define UI_MESSAGE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Locale-independent presentation descriptor. Workers may construct/copy it;
 * only the UI thread renders it. Keys are source templates, never user data. */
#define UI_MESSAGE_KEY_CAPACITY 320
#define UI_MESSAGE_ARGUMENT_CAPACITY 160
typedef enum {
    UI_MESSAGE_KEY, UI_MESSAGE_LITERAL, UI_MESSAGE_STRING,
    UI_MESSAGE_UINT, UI_MESSAGE_UINT3, UI_MESSAGE_STRING_UINT,
    UI_MESSAGE_UINT3_INT64
} ui_message_format_t;
typedef struct {
    ui_message_format_t format;
    char key[UI_MESSAGE_KEY_CAPACITY];
    char argument[UI_MESSAGE_ARGUMENT_CAPACITY];
    uint32_t numbers[3];
    int64_t amount;
    bool argument_is_key;
} ui_message_t;

void ui_message_key(ui_message_t *message, const char *key);
void ui_message_literal(ui_message_t *message, const char *text);
void ui_message_string(ui_message_t *message, const char *key, const char *value);
void ui_message_uint(ui_message_t *message, const char *key, uint32_t value);
void ui_message_uint3(ui_message_t *message, const char *key, uint32_t a, uint32_t b, uint32_t c);
void ui_message_uint3_int64(ui_message_t *message, const char *key, uint32_t a, uint32_t b, uint32_t c, int64_t amount);
void ui_message_string_uint(ui_message_t *message, const char *key, const char *value, uint32_t count);
void ui_message_text(ui_message_t *message, const char *key, const char *argument_key);
void ui_message_text_uint(ui_message_t *message, const char *key, const char *argument_key, uint32_t count);
bool ui_message_equal(const ui_message_t *a, const ui_message_t *b);
/* Returns false on truncation. UTF-8 is always terminated at a complete scalar. */
bool ui_message_render(const ui_message_t *message, char *output, size_t size);
/* English source rendering for stable logs/legacy APIs; never consults locale. */
bool ui_message_source(const ui_message_t *message, char *output, size_t size);

#endif
