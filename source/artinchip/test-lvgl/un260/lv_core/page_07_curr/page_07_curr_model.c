#include "un260/lv_core/page_07_curr/page_07_curr_internal.h"

#include "un260/currency/currency_state.h"
#include "un260/lv_system/ui_state_runtime.h"

#include <stdio.h>
#include <string.h>

static void page07_state_pull_from_runtime(ui_state_page07_t *state);
static bool curr_has_currency_code(const char *code);
static bool curr_is_favorite_code(const char *code);
static bool curr_add_favorite_code(const char *code);
static void curr_remove_favorite_code(const char *code);

static void page07_state_pull_from_runtime(ui_state_page07_t* state)
{
    if (!state) return;

    memset(state, 0, sizeof(*state));
    state->view_mode = g_page07_curr.model.view_mode;
    state->fav_only = g_page07_curr.model.favorite_only ? 1 : 0;
    state->selected_abs_idx = g_page07_curr.model.selected_abs_idx;
    state->fav_count = g_page07_curr.model.favorite_count;
    for (int i = 0; i < g_page07_curr.model.favorite_count && i < PAGE07_CURR_MAX_ITEMS; i++) {
        memcpy(state->fav_codes[i], g_page07_curr.model.favorite_codes[i], 4);
    }
}

void page07_curr_model_load(void)
{
    ui_state_page07_t state;
    char curr_code[4];

    ui_state_page07_get(&state);
    g_page07_curr.model.view_mode = state.view_mode;
    if (g_page07_curr.model.view_mode != PAGE07_CURR_VIEW_CARD && g_page07_curr.model.view_mode != PAGE07_CURR_VIEW_GRID) {
        g_page07_curr.model.view_mode = PAGE07_CURR_VIEW_CARD;
        page07_curr_model_save();
    }
    g_page07_curr.model.favorite_only = state.fav_only != 0;

    g_page07_curr.model.favorite_count = 0;
    memset(g_page07_curr.model.favorite_codes, 0, sizeof(g_page07_curr.model.favorite_codes));
    for (int i = 0; i < state.fav_count && i < PAGE07_CURR_MAX_ITEMS; i++) {
        if (!curr_has_currency_code(state.fav_codes[i])) continue;
        curr_add_favorite_code(state.fav_codes[i]);
    }

    currency_state_get_selected_code(curr_code);
    /* The controller-confirmed selection is authoritative. Saved indices can
     * shift when virtual entries such as AUTO/MIX are inserted. */
    g_page07_curr.model.selected_abs_idx =
        page07_curr_model_find_abs_idx(curr_code);
}

bool page07_curr_model_code_equal(const char* a, const char* b)
{
    return (a && b && strncmp(a, b, 3) == 0);
}

int page07_curr_model_find_abs_idx(const char* code)
{
    uint8_t index;

    if (currency_state_find_code(code, &index)) return index;
    return 0;
}

static bool curr_has_currency_code(const char* code)
{
    return code != NULL && code[0] != '\0' && currency_state_find_code(code, NULL);
}

void page07_curr_model_save(void)
{
    ui_state_page07_t state;

    page07_state_pull_from_runtime(&state);
    ui_state_save_page07(&state);
}

static bool curr_is_favorite_code(const char* code)
{
    for (int i = 0; i < g_page07_curr.model.favorite_count; i++) {
        if (page07_curr_model_code_equal(code, g_page07_curr.model.favorite_codes[i])) return true;
    }
    return false;
}

static bool curr_add_favorite_code(const char* code)
{
    if (code == NULL || code[0] == '\0') return false;
    if (curr_is_favorite_code(code)) return true;
    if (g_page07_curr.model.favorite_count >= PAGE07_CURR_MAX_ITEMS) return false;

    g_page07_curr.model.favorite_codes[g_page07_curr.model.favorite_count][0] = code[0];
    g_page07_curr.model.favorite_codes[g_page07_curr.model.favorite_count][1] = code[1];
    g_page07_curr.model.favorite_codes[g_page07_curr.model.favorite_count][2] = code[2];
    g_page07_curr.model.favorite_codes[g_page07_curr.model.favorite_count][3] = '\0';
    g_page07_curr.model.favorite_count++;
    return true;
}

static void curr_remove_favorite_code(const char* code)
{
    for (int i = 0; i < g_page07_curr.model.favorite_count; i++) {
        if (!page07_curr_model_code_equal(code, g_page07_curr.model.favorite_codes[i])) continue;
        for (int j = i; j < g_page07_curr.model.favorite_count - 1; j++) {
            memcpy(g_page07_curr.model.favorite_codes[j], g_page07_curr.model.favorite_codes[j + 1], 4);
        }
        g_page07_curr.model.favorite_count--;
        return;
    }
}

bool page07_curr_model_is_favorite(int abs_idx)
{
    char curr_code[4];

    if (abs_idx < 0 || !currency_state_get_code((uint8_t)abs_idx, curr_code)) return false;
    return curr_is_favorite_code(curr_code);
}

void page07_curr_model_toggle_favorite(int abs_idx)
{
    char curr_code[4];

    if (abs_idx < 0 || !currency_state_get_code((uint8_t)abs_idx, curr_code)) return;

#if LV_DEBUG
    printf("[curr_fav] toggle abs_idx=%d code=%s\n",
           abs_idx, curr_code);
#endif

    if (page07_curr_model_is_favorite(abs_idx)) {
        curr_remove_favorite_code(curr_code);
    } else {
        curr_add_favorite_code(curr_code);
    }

    page07_curr_model_save();
}

void page07_curr_model_refresh_visible(void)
{
    g_page07_curr.model.visible_count = 0;
    int total = currency_state_count();
    if (total > PAGE07_CURR_MAX_ITEMS) total = PAGE07_CURR_MAX_ITEMS;

    for (int i = 0; i < total; i++) {
        bool keep = true;
        if (g_page07_curr.model.favorite_only) keep = page07_curr_model_is_favorite(i);
        if (!keep) continue;
        g_page07_curr.model.visible_indices[g_page07_curr.model.visible_count++] = i;
    }
}

int page07_curr_model_find_visible_pos(int abs_idx)
{
    for (int i = 0; i < g_page07_curr.model.visible_count; i++) {
        if (g_page07_curr.model.visible_indices[i] == abs_idx) return i;
    }
    return 0;
}
