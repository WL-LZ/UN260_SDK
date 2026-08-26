#include "un260/lv_system/ui_state_runtime.h"

#include <string.h>

#include "un260/lv_components/lv_fault_popup.h"
#include "un260/lv_components/smart_island.h"
#include "un260/lv_core/page_01_main.h"

#define UI_STATE_CURR_VIEW_CARD 0
#define UI_STATE_CURR_VIEW_GRID 1

static ui_persist_state_t s_ui_state;
static bool s_ui_state_loaded = false;

static int ui_state_normalize_detail_section(int section)
{
    if (section < PAGE_01_DETAIL_SECTION_A || section > PAGE_01_DETAIL_SECTION_C) {
        return PAGE_01_DETAIL_SECTION_A;
    }
    return section;
}

static void ui_state_normalize_page07(ui_state_page07_t* state)
{
    if (state->view_mode != UI_STATE_CURR_VIEW_CARD &&
        state->view_mode != UI_STATE_CURR_VIEW_GRID) {
        state->view_mode = UI_STATE_CURR_VIEW_CARD;
    }
    state->fav_only = state->fav_only != 0;
    if (state->fav_count < 0) state->fav_count = 0;
    if (state->fav_count > MAX_CURRENCIES) state->fav_count = MAX_CURRENCIES;
    for (int i = 0; i < MAX_CURRENCIES; i++) {
        state->fav_codes[i][3] = '\0';
    }
}

static void ui_state_set_defaults(void)
{
    memset(&s_ui_state, 0, sizeof(s_ui_state));
    s_ui_state.magic = UI_STATE_STORE_MAGIC;
    s_ui_state.version = UI_STATE_STORE_VERSION;
    s_ui_state.page01.detail_section = PAGE_01_DETAIL_SECTION_A;
    s_ui_state.page07.view_mode = UI_STATE_CURR_VIEW_CARD;
    s_ui_state.page06.reserved06_enable = 1;
}

static void ui_state_ensure_loaded(void)
{
    if (s_ui_state_loaded) return;
    ui_state_set_defaults();
    (void)ui_state_store_load(&s_ui_state);
    s_ui_state.page01.detail_section =
        ui_state_normalize_detail_section(s_ui_state.page01.detail_section);
    ui_state_normalize_page07(&s_ui_state.page07);
    s_ui_state_loaded = true;
}

static void ui_state_pull_common_runtime(void)
{
    s_ui_state.page01.detail_section =
        ui_state_normalize_detail_section((int)page_01_detail_section_get());
    s_ui_state.page06.reserved06_enable = fault_popup_get_auto_enabled() ? 1 : 0;
    s_ui_state.page18.reserved18_enable = smart_island_pure_count_is_enabled() ? 1 : 0;
}

static void ui_state_save_all(void)
{
    ui_state_pull_common_runtime();
    (void)ui_state_store_save(&s_ui_state);
}

void ui_state_apply_common_runtime(void)
{
    ui_state_ensure_loaded();
    page_01_detail_section_set(
        (page_01_detail_section_t)s_ui_state.page01.detail_section, false);
    fault_popup_set_auto_enabled(s_ui_state.page06.reserved06_enable != 0);
    smart_island_set_pure_count_enabled(s_ui_state.page18.reserved18_enable != 0);
}

void ui_state_save_popup_auto_state(void)
{
    ui_state_ensure_loaded();
    ui_state_save_all();
}

void ui_state_save_pure_count_state(void)
{
    ui_state_ensure_loaded();
    ui_state_save_all();
}

bool ui_state_pure_count_is_enabled(void)
{
    bool enabled;

    ui_state_ensure_loaded();
    enabled = s_ui_state.page18.reserved18_enable != 0;
    smart_island_set_pure_count_enabled(enabled);
    return enabled;
}

int ui_state_page01_detail_section_get(void)
{
    ui_state_ensure_loaded();
    return s_ui_state.page01.detail_section;
}

void ui_state_save_page01_detail_section(void)
{
    ui_state_ensure_loaded();
    ui_state_save_all();
}

void ui_state_page07_get(ui_state_page07_t* state)
{
    if (!state) return;
    ui_state_ensure_loaded();
    *state = s_ui_state.page07;
}

void ui_state_save_page07(const ui_state_page07_t* state)
{
    if (!state) return;
    ui_state_ensure_loaded();
    s_ui_state.page07 = *state;
    ui_state_normalize_page07(&s_ui_state.page07);
    ui_state_save_all();
}
