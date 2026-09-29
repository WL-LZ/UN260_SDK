#include "ui_notice_state.h"
#include <limits.h>
#include <string.h>

static void copy_text(char *dest, size_t size, const char *text)
{
    size_t length;
    if (!text) text = "";
    length = strlen(text);
    if (length >= size) {
        length = size - 1;
        while (length && ((unsigned char)text[length] & 0xc0) == 0x80) --length;
    }
    memcpy(dest, text, length);
    dest[length] = '\0';
}

static unsigned priority(ui_notice_kind_t kind)
{
    switch (kind) {
    case UI_NOTICE_ERROR: return 4;
    case UI_NOTICE_WARNING: return 3;
    case UI_NOTICE_PROGRESS: return 2;
    default: return 1;
    }
}

static uint32_t lifetime(ui_notice_kind_t kind, uint32_t requested)
{
    if (kind == UI_NOTICE_PROGRESS) return 0;
    if (requested) return requested;
    switch (kind) {
    case UI_NOTICE_ERROR: return 7500;
    case UI_NOTICE_WARNING: return 6500;
    case UI_NOTICE_INFO: return 4500;
    default: return 3600;
    }
}

static bool same_content(const ui_notice_item_t *a, const ui_notice_item_t *b)
{
    return a->kind == b->kind && strcmp(a->title, b->title) == 0 &&
           strcmp(a->detail, b->detail) == 0;
}

static bool same_slot(const ui_notice_item_t *a, const ui_notice_item_t *b)
{
    if (a->key[0] || b->key[0]) return strcmp(a->key, b->key) == 0;
    return same_content(a, b);
}

static void merge(ui_notice_item_t *dest, const ui_notice_item_t *item)
{
    uint16_t count = same_content(dest, item) ? dest->repeats : 0;
    *dest = *item;
    dest->repeats = count == UINT16_MAX ? count : count + 1;
}

static void dequeue(ui_notice_state_t *state, unsigned index)
{
    --state->queued_count;
    if (index < state->queued_count)
        memmove(&state->queued[index], &state->queued[index + 1],
                (state->queued_count - index) * sizeof(state->queued[0]));
}

static bool enqueue(ui_notice_state_t *state, const ui_notice_item_t *item)
{
    unsigned i, lowest = 0;
    if (state->queued_count == UI_NOTICE_QUEUE_CAPACITY) {
        for (i = 1; i < state->queued_count; ++i)
            if (priority(state->queued[i].kind) < priority(state->queued[lowest].kind)) lowest = i;
        if (priority(item->kind) < priority(state->queued[lowest].kind)) return false;
        dequeue(state, lowest);
    }
    state->queued[state->queued_count++] = *item;
    return true;
}

static void promote(ui_notice_state_t *state)
{
    unsigned i, best = 0;
    state->has_active = state->queued_count != 0;
    if (!state->has_active) return;
    for (i = 1; i < state->queued_count; ++i)
        if (priority(state->queued[i].kind) > priority(state->queued[best].kind)) best = i;
    state->active = state->queued[best];
    state->active.revision = ++state->revision;
    dequeue(state, best);
}

static void forget_dismissed(ui_notice_state_t *state, const char *key)
{
    unsigned i;
    if (!key || !key[0]) return;
    for (i = 0; i < state->dismissed_count; ++i) {
        if (strcmp(key, state->dismissed_progress[i]) != 0) continue;
        --state->dismissed_count;
        if (i < state->dismissed_count)
            memmove(state->dismissed_progress[i], state->dismissed_progress[i + 1],
                    (state->dismissed_count - i) * UI_NOTICE_KEY_CAPACITY);
        return;
    }
}

static void remember_dismissed(ui_notice_state_t *state, const ui_notice_item_t *item)
{
    if (item->kind != UI_NOTICE_PROGRESS || !item->key[0]) return;
    forget_dismissed(state, item->key);
    if (state->dismissed_count == UI_NOTICE_QUEUE_CAPACITY) {
        memmove(state->dismissed_progress[0], state->dismissed_progress[1],
                (UI_NOTICE_QUEUE_CAPACITY - 1) * UI_NOTICE_KEY_CAPACITY);
        --state->dismissed_count;
    }
    copy_text(state->dismissed_progress[state->dismissed_count++], UI_NOTICE_KEY_CAPACITY, item->key);
}

void ui_notice_state_init(ui_notice_state_t *state)
{
    memset(state, 0, sizeof(*state));
}

bool ui_notice_state_post(ui_notice_state_t *state, const ui_notice_config_t *config)
{
    ui_notice_item_t item;
    unsigned i;
    if (!state || !config || !config->title || !config->title[0] ||
        config->kind < UI_NOTICE_SUCCESS || config->kind > UI_NOTICE_INFO ||
        (config->key && strlen(config->key) >= UI_NOTICE_KEY_CAPACITY)) return false;
    memset(&item, 0, sizeof(item));
    item.kind = config->kind;
    copy_text(item.key, sizeof(item.key), config->key);
    copy_text(item.title, sizeof(item.title), config->title);
    copy_text(item.detail, sizeof(item.detail), config->detail);
    item.remaining_ms = lifetime(item.kind, config->duration_ms);
    item.repeats = 1;
    if (item.kind == UI_NOTICE_PROGRESS && item.key[0]) {
        for (i = 0; i < state->dismissed_count; ++i)
            if (strcmp(item.key, state->dismissed_progress[i]) == 0) return false;
    } else forget_dismissed(state, item.key);
    item.revision = ++state->revision;
    if (!state->has_active) {
        state->active = item;
        state->has_active = true;
        return true;
    }
    if (same_slot(&state->active, &item)) {
        merge(&state->active, &item);
        return true;
    }
    for (i = 0; i < state->queued_count; ++i) {
        if (!same_slot(&state->queued[i], &item)) continue;
        if (priority(item.kind) > priority(state->active.kind)) {
            dequeue(state, i);
            enqueue(state, &state->active);
            state->active = item;
        } else merge(&state->queued[i], &item);
        return true;
    }
    if (priority(item.kind) > priority(state->active.kind)) {
        enqueue(state, &state->active);
        state->active = item;
        return true;
    }
    return enqueue(state, &item);
}

bool ui_notice_state_remove(ui_notice_state_t *state, const char *key, bool keep_dismissed)
{
    bool changed = false;
    unsigned i = 0;
    if (!state) return false;
    if (!keep_dismissed) forget_dismissed(state, key);
    if (key) {
        while (i < state->queued_count) {
            if (strcmp(key, state->queued[i].key) != 0) { ++i; continue; }
            if (keep_dismissed) remember_dismissed(state, &state->queued[i]);
            dequeue(state, i);
        }
    }
    if (state->has_active && (!key || strcmp(key, state->active.key) == 0)) {
        if (keep_dismissed) remember_dismissed(state, &state->active);
        promote(state);
        changed = true;
    }
    return changed;
}

bool ui_notice_state_elapse(ui_notice_state_t *state, uint32_t elapsed_ms)
{
    if (!state->has_active || state->active.kind == UI_NOTICE_PROGRESS) return false;
    if (state->active.remaining_ms > elapsed_ms) {
        state->active.remaining_ms -= elapsed_ms;
        return false;
    }
    state->active.remaining_ms = 0;
    return true;
}
