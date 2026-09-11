#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include "aic_ui/compiled_asset.h"
#include "un260/lv_system/ui_history_data.h"
#include "un260/lv_system/machine_time.h"
#include "un260/lv_system/ui_history_export_data.h"
#include "un260/lv_components/lv_print_toast.h"
#include "test_history_view_support.h"
#include "un260/lv_core/page_19_history.c"

/* The real page and parser are compiled unchanged. Only history IO, navigation
 * and export side effects are replaced by deterministic host fixtures. */
static ui_history_store_t test_store;
static bool test_available = true, test_accept = true, test_export_ok = true, test_clear_accept = true;
static storage_job_status_t test_status = STORAGE_JOB_SUCCEEDED;
static storage_job_id_t test_commit_id = 1;
static unsigned test_pops, test_deletes, test_exports, test_polls, test_selection_mutations;
static uint32_t test_deleted_ids[UI_HISTORY_MAX_RECORDS];
static size_t test_deleted_count;
static uint32_t test_exported_ids[UI_HISTORY_MAX_RECORDS];
static size_t test_exported_count;
static char test_toast[256];
void machine_time_get(machine_time_value_t *out)
{ *out=(machine_time_value_t){.year=2026,.month=9,.day=11,.hour=12,.minute=30,.second=15}; }

void ui_history_data_init(void) {}
bool ui_history_data_poll(uint32_t now_ms) { (void)now_ms; ++test_polls; return false; }
const ui_history_store_t *ui_history_data_get(void) { return &test_store; }
bool ui_history_data_is_available(void) { return test_available; }
bool ui_history_data_can_accept(void) { return test_available && test_accept; }
storage_job_status_t ui_history_data_status(void) { return test_status; }
storage_job_id_t ui_history_last_commit_id(void) { return test_commit_id; }
storage_job_status_t ui_history_commit_status(storage_job_id_t id)
{ return id == test_commit_id ? test_status : STORAGE_JOB_UNKNOWN; }
uint32_t ui_history_total_notes_counted_get(void) { return test_store.total_notes_counted; }
void ui_history_total_notes_counted_set(uint32_t value)
{
    if (!ui_history_data_can_accept() || !test_clear_accept || test_store.total_notes_counted == value) return;
    test_store.total_notes_counted = value; ++test_commit_id;
}
void ui_history_total_notes_counted_clear(void) { ui_history_total_notes_counted_set(0); }
bool ui_history_record_get(uint8_t index, ui_history_record_t *out)
{
    if (!out || index >= test_store.record_count || !test_store.records[index].valid) return false;
    *out = test_store.records[index]; return true;
}
bool ui_history_record_get_by_no(uint32_t no, ui_history_record_t *out)
{
    for (uint8_t i = 0; i < test_store.record_count; ++i)
        if (test_store.records[i].record_no == no) return ui_history_record_get(i, out);
    return false;
}
bool ui_history_record_set_selected(uint8_t index, bool selected)
{
    ++test_selection_mutations;
    if (index >= test_store.record_count || !test_store.records[index].valid) return false;
    test_store.records[index].selected = selected; return true;
}
bool ui_history_record_toggle_selected(uint8_t index)
{
    if (index >= test_store.record_count) return false;
    return ui_history_record_set_selected(index, !test_store.records[index].selected);
}
void ui_history_record_clear_selected(void)
{ ++test_selection_mutations; for (unsigned i = 0; i < test_store.record_count; ++i) test_store.records[i].selected = false; }
void ui_history_record_set_all_selected(bool selected)
{ ++test_selection_mutations; for (unsigned i = 0; i < test_store.record_count; ++i) test_store.records[i].selected = selected; }
int ui_history_record_selected_count_get(void)
{
    int count = 0;
    for (unsigned i = 0; i < test_store.record_count; ++i) count += test_store.records[i].selected;
    return count;
}
int ui_history_record_selected_first_index_get(void)
{
    for (unsigned i = 0; i < test_store.record_count; ++i)
        if (test_store.records[i].selected) return (int)i;
    return -1;
}
bool ui_history_record_delete_selected(void)
{
    assert(!"History must delete explicit stable IDs with one storage request");
    return false;
}
bool ui_history_record_delete_records(const uint32_t *ids, size_t count)
{
    if (!ui_history_data_can_accept()) return false;
    if (!ids || !count || count > UI_HISTORY_MAX_RECORDS) return false;
    for (size_t i = 0; i < count; ++i) {
        ui_history_record_t record;
        if (!ui_history_record_get_by_no(ids[i], &record)) return false;
        for (size_t j = 0; j < i; ++j) if (ids[j] == ids[i]) return false;
    }
    test_deleted_count = 0;
    unsigned kept = 0;
    for (unsigned i = 0; i < test_store.record_count; ++i) {
        bool removed = false;
        for (size_t j = 0; j < count; ++j)
            if (test_store.records[i].record_no == ids[j]) removed = true;
        if (removed)
            test_deleted_ids[test_deleted_count++] = test_store.records[i].record_no;
        else test_store.records[kept++] = test_store.records[i];
    }
    memset(test_store.records + kept, 0, (UI_HISTORY_MAX_RECORDS - kept) * sizeof(test_store.records[0]));
    test_store.record_count = (uint8_t)kept;
    ++test_deletes; ++test_commit_id;
    return true;
}
bool ui_history_export_data_request_records(const uint32_t *ids, size_t count)
{
    assert(ids && count && count <= UI_HISTORY_MAX_RECORDS);
    test_exported_count = count;
    memcpy(test_exported_ids, ids, count * sizeof(*ids)); ++test_exports;
    return test_export_ok;
}
bool ui_history_export_data_request(void)
{ assert(!"History must export explicit stable record IDs"); return false; }
bool ui_manager_pop_page(void) { ++test_pops; return true; }
void perf_profile_watch_invalidation(const void *object, const char *name) { (void)object; (void)name; }
void perf_profile_unwatch_invalidation(const void *object) { (void)object; }
uint64_t app_clock_monotonic_ms(void) { return lv_tick_get(); }
uint64_t app_clock_monotonic_us(void) { return (uint64_t)lv_tick_get() * 1000U; }
uint32_t app_clock_uptime_ms(void) { return lv_tick_get(); }
lv_print_toast_config_t lv_print_toast_get_default_config(void)
{ lv_print_toast_config_t config = {0}; return config; }
void lv_print_toast_show_with_config(const lv_print_toast_config_t *config)
{ snprintf(test_toast, sizeof(test_toast), "%s", config->text ? config->text : ""); }
void lv_print_toast_show(const char *text)
{ snprintf(test_toast, sizeof(test_toast), "%s", text ? text : ""); }

/* Software decoder bridge reads the exact production compiled BGRA registry;
 * it does not synthesize icons or pretend to validate DMA/GE decoding. */
static lv_res_t test_asset_info(lv_img_decoder_t *decoder, const void *src, lv_img_header_t *header)
{
    (void)decoder;
    if (lv_img_src_get_type(src) != LV_IMG_SRC_FILE) return LV_RES_INV;
    const un260_compiled_asset_t *asset = un260_compiled_asset_find(src);
    if (!asset || !asset->has_alpha || asset->stride != asset->width * 4) return LV_RES_INV;
    memset(header, 0, sizeof(*header));
    header->w = asset->width; header->h = asset->height; header->cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
    return LV_RES_OK;
}
static lv_res_t test_asset_open(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc)
{
    if (test_asset_info(decoder, dsc->src, &dsc->header) != LV_RES_OK) return LV_RES_INV;
    dsc->img_data = un260_compiled_asset_find(dsc->src)->pixels;
    return LV_RES_OK;
}
static void test_asset_close(lv_img_decoder_t *decoder, lv_img_decoder_dsc_t *dsc)
{ (void)decoder; dsc->img_data = NULL; }

static lv_color_t test_framebuffer[1280 * 400];
static lv_point_t test_pointer_point;
static lv_indev_state_t test_pointer_state = LV_INDEV_STATE_RELEASED;
static void test_flush(lv_disp_drv_t *display, const lv_area_t *area, lv_color_t *pixels)
{
    int width = lv_area_get_width(area);
    assert(area->x1 >= 0 && area->x2 < 1280 && area->y1 >= 0 && area->y2 < 400);
    for (int y = area->y1; y <= area->y2; ++y)
        memcpy(test_framebuffer + y * 1280 + area->x1,
               pixels + (y - area->y1) * width, (size_t)width * sizeof(*pixels));
    lv_disp_flush_ready(display);
}
static void test_pointer_read(lv_indev_drv_t *driver, lv_indev_data_t *data)
{ (void)driver; data->point = test_pointer_point; data->state = test_pointer_state; }
void history_test_tick(unsigned ms)
{ for (unsigned i = 0; i < ms; i += 20) { lv_tick_inc(20); lv_timer_handler(); } }
void history_test_render(void)
{ lv_obj_update_layout(lv_scr_act()); lv_obj_invalidate(lv_scr_act()); lv_refr_now(NULL); }
static void history_test_pointer(int x, int y, bool down)
{
    test_pointer_point = (lv_point_t){x, y};
    test_pointer_state = down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    history_test_tick(60);
}
void history_test_click_at(lv_obj_t *object, const char *file, unsigned line)
{
    lv_obj_update_layout(lv_scr_act());
    if (!object || !lv_obj_is_visible(object) || lv_obj_has_state(object, LV_STATE_DISABLED)) {
        fprintf(stderr, "Cannot click control at %s:%u (object=%p)\n", file, line, (void *)object);
        history_test_bmp("history-test-failure");
    }
    assert(object && lv_obj_is_visible(object) && !lv_obj_has_state(object, LV_STATE_DISABLED));
    lv_area_t area; lv_obj_get_coords(object, &area);
    assert(area.x1 >= 0 && area.x2 < 1280 && area.y1 >= 0 && area.y2 < 400);
    int x = (area.x1 + area.x2) / 2, y = (area.y1 + area.y2) / 2;
    history_test_pointer(x, y, true); history_test_pointer(x, y, false);
    history_test_tick(220);
}
lv_obj_t *history_test_button(lv_obj_t *parent, const char *text)
{
    if (!parent || !lv_obj_is_visible(parent)) return NULL;
    for (uint32_t i = lv_obj_get_child_cnt(parent); i > 0; --i) {
        lv_obj_t *found = history_test_button(lv_obj_get_child(parent, (int32_t)i - 1), text);
        if (found) return found;
    }
    if (lv_obj_check_type(parent, &lv_btn_class)) {
        lv_obj_t *label = lv_damped_button_get_label(parent);
        if (label && !strcmp(lv_label_get_text(label), text)) return parent;
    }
    return NULL;
}
unsigned history_test_timers(void)
{ unsigned count = 0; for (lv_timer_t *t = lv_timer_get_next(NULL); t; t = lv_timer_get_next(t)) ++count; return count; }
void history_test_bmp(const char *name)
{
    /* Optional output must not change whether the test lays out a new page. */
    history_test_render();
    const char *directory = getenv("HISTORY_RASTER_OUTPUT");
    if (!directory) return;
    char path[1024]; snprintf(path, sizeof(path), "%s/%s.bmp", directory, name);
    FILE *file = fopen(path, "wb"); assert(file);
    uint32_t bytes = 54 + 1280 * 400 * 4, offset = 54, dib = 40, width = 1280;
    int32_t height = -400; uint16_t planes = 1, bits = 32; uint8_t header[54] = {0};
    header[0] = 'B'; header[1] = 'M'; memcpy(header + 2, &bytes, 4); memcpy(header + 10, &offset, 4);
    memcpy(header + 14, &dib, 4); memcpy(header + 18, &width, 4); memcpy(header + 22, &height, 4);
    memcpy(header + 26, &planes, 2); memcpy(header + 28, &bits, 2);
    assert(fwrite(header, 1, sizeof(header), file) == sizeof(header));
    assert(fwrite(test_framebuffer, 1, sizeof(test_framebuffer), file) == sizeof(test_framebuffer));
    assert(fclose(file) == 0);
}
static void history_test_fixtures(unsigned count)
{
    assert(count <= UI_HISTORY_MAX_RECORDS);
    memset(&test_store, 0, sizeof(test_store));
    test_store.record_count = (uint8_t)count;
    test_store.next_record_no = 1001;
    test_store.next_slot_no = 1;
    test_store.total_notes_counted = 100000;
    for (unsigned i = 0; i < count; ++i) {
        ui_history_record_t *record = &test_store.records[i];
        *record = (ui_history_record_t){ .valid = true, .record_no = 1000 - i,
            .slot_no = (uint8_t)(i + 1), .pcs = 20 + i, .amount = 1000 + i * 100,
            .year = 2026, .month = 9, .day = (uint8_t)(9 - i % 3),
            .hour = (uint8_t)(12 + i % 8), .minute = (uint8_t)((i * 3) % 60), .second = (uint8_t)(i % 60) };
        strcpy(record->currency, i % 2 ? "EUR" : "USD");
        snprintf(record->denom_text, sizeof(record->denom_text), "100 x %u\n50 x 2\n", 18 + i);
        snprintf(record->sn_detail_text, sizeof(record->sn_detail_text), "1\t100\tUNIQUE%04u\n2\t50\tSHARED001\n", i);
        if (i % 3 == 0) {
            for (unsigned n = 0; n < 40; ++n) strcat(record->session_log, "0x05 FD DF 05 05 0A\n");
            strcat(record->session_log, "0x0C FD DF 07 0C 15 02 0A\n");
            strcpy(record->error_frame_text, "FD DF 07 0C 15 02 0A");
        }
    }
}

static void history_test_apply(const history_query_input_t *input)
{
    assert(history && !history->detail_mode && !history->selecting && !history->search);
    history_test_click(history->actions[0]);
    assert(history->search);
    history_test_search_apply(history->search, input);
    assert(!history->search && !history->detail_mode && lv_obj_is_visible(history->list_panel));
}

static uint32_t history_test_click_record(uint32_t index)
{
    assert(history && index < displayed_count() && !history->detail_mode);
    uint32_t id = displayed_id(index);
    assert(lv_recycled_list_scroll_to_index(history->list, index));
    history_test_render();
    const ui_list_window_t *window = lv_recycled_list_window(history->list);
    lv_area_t area; lv_obj_get_coords(lv_recycled_list_object(history->list), &area);
    int x = area.x1 + 400;
    int y = area.y1 + (int)(index - window->first) * HISTORY_ROW_HEIGHT + HISTORY_ROW_HEIGHT / 2;
    history_test_pointer(x, y, true); history_test_pointer(x, y, false);
    history_test_tick(240);
    return id;
}

static void history_test_list_and_detail(void)
{
    assert(history && history->result.matched_count == 20 && !history->result.amount_comparable);
    assert(lv_recycled_list_window(history->list)->count == 20);
    assert(lv_obj_has_flag(lv_recycled_list_object(history->list), LV_OBJ_FLAG_USER_4));
    lv_area_t viewport; lv_obj_get_coords(lv_recycled_list_object(history->list), &viewport);
    history_test_pointer(viewport.x1 + 400, viewport.y1 + 200, true);
    history_test_pointer(viewport.x1 + 400, viewport.y1 + 90, true);
    history_test_pointer(viewport.x1 + 400, viewport.y1 + 90, false);
    history_test_tick(300);
    assert(!history->detail_mode && lv_recycled_list_window(history->list)->offset > 0);
    history_test_bmp("history-scrolled");
    assert(lv_recycled_list_scroll_to_index(history->list, 0));
    history_test_pointer(viewport.x1 + 400, viewport.y1 + 24, true);
    ui_history_record_t swap = test_store.records[0];
    test_store.records[0] = test_store.records[1]; test_store.records[1] = swap;
    ui_page_19_history_refresh();
    history_test_pointer(viewport.x1 + 400, viewport.y1 + 24, false);
    assert(!history->detail_mode);
    history_test_click(history->sort);
    assert(history->result_ids[0] == 981);
    history_test_click(history->sort);
    assert(history->result_ids[0] == 1000);

    history_query_input_t filter = {0};
    strcpy(filter.serial, "UNIQUE0000");
    history_test_apply(&filter);
    assert(history->result.matched_count == 1 && history->result_ids[0] == 1000);
    assert(!history->detail_mode && history->unknown_count == 19);
    history_test_bmp("history-single-result");
    assert(history_test_click_record(0) == 1000);
    assert(history->detail_mode && history->current_id == 1000);
    assert(section_count(0) == 2 && section_count(1) == 2 && section_count(2) == 1);
    assert(current_detail()->saved_reject_pcs == 2);
    char recorded_total[120];
    snprintf(recorded_total, sizeof(recorded_total), ui_text_get(UI_TEXT_HISTORY_TOTAL_FMT), 20UL, "USD", 1000UL);
    assert(!strcmp(lv_label_get_text(history->subtitle), recorded_total));
    history_test_bmp("history-detail");
    history_test_click(history->actions[2]);
    assert(test_exported_count == 1 && test_exported_ids[0] == 1000);
    unsigned pops = test_pops;
    assert(lv_nav_button_request_back() == LV_NAV_BACK_HANDLED);
    assert(!history->detail_mode && test_pops == pops);
    assert(!strcmp(history->input.serial, "UNIQUE0000") && history->result.matched_count == 1);

    memset(&filter, 0, sizeof(filter)); strcpy(filter.currency, "USD");
    history_test_apply(&filter);
    assert(history->result.matched_count == 10 && history->result.amount_comparable);
    assert(!strcmp(history->result.currency, "USD"));
    assert(lv_recycled_list_scroll_to_index(history->list, 6));
    int32_t offset = lv_recycled_list_window(history->list)->offset;
    uint32_t first = lv_recycled_list_window(history->list)->first;
    uint32_t opened = history_test_click_record(first);
    assert(history->current_id == opened && history->detail_mode);
    assert(lv_nav_button_request_back() == LV_NAV_BACK_HANDLED);
    assert(!history->detail_mode && !strcmp(history->input.currency, "USD"));
    assert(lv_recycled_list_window(history->list)->offset == offset);
    assert(lv_recycled_list_window(history->list)->first == first);
    history_test_click_record(0);
    history_test_click(history->actions[1]);
    assert(history->current_id == 998);
    history_test_click(history->actions[0]);
    assert(history->current_id == 1000);
    assert(lv_nav_button_request_back() == LV_NAV_BACK_HANDLED);
    history_test_click(history->actions[1]);
    assert(test_exported_count == 10);
    for (size_t i = 0; i < test_exported_count; ++i) {
        ui_history_record_t record; assert(ui_history_record_get_by_no(test_exported_ids[i], &record));
        assert(!strcmp(record.currency, "USD"));
    }
    puts("PASS: real pointer scroll/tap/revision guard, stable sorting, one-result search stays list, detail/late rejects, back preserves filter/scroll and explicit exports");
}

static void history_test_selection_and_delete(void)
{
    assert(history && !history->detail_mode && history->result.matched_count == 10);
    history_test_click(history->actions[2]); assert(history->selecting && !history->selected_count);
    assert(!lv_damped_button_is_enabled(history->actions[2]));
    history_test_click(history->actions[0]); assert(history->selected_count == 10);
    for (size_t i = 0; i < history->selected_count; ++i) {
        ui_history_record_t record; assert(ui_history_record_get_by_no(history->selected_ids[i], &record));
        assert(!strcmp(record.currency, "USD"));
    }
    assert(ui_history_record_selected_count_get() == 0);
    history_test_click(history->actions[0]); assert(!history->selected_count);
    uint32_t pruned = history_test_click_record(0);
    assert(history->selected_count == 1);
    unsigned prune_index = 0;
    while (test_store.records[prune_index].record_no != pruned) ++prune_index;
    strcpy(test_store.records[prune_index].currency, "EUR");
    ui_page_19_history_refresh();
    assert(!history->selected_count && history->result.matched_count == 9);
    strcpy(test_store.records[prune_index].currency, "USD");
    ui_page_19_history_refresh();
    assert(history->result.matched_count == 10);
    assert(history_test_click_record(0) == 1000);
    assert(history_test_click_record(1) == 998);
    assert(history->selected_count == 2 && !history->detail_mode);
    history_test_click(history->actions[1]);
    assert(test_exported_count == 2 && test_exported_ids[0] == 1000 && test_exported_ids[1] == 998);
    history_test_click(history->actions[2]); assert(history->dialog && history->confirmed_count == 2);
    history_test_bmp("history-delete-confirm");
    unsigned deletes = test_deletes, pops = test_pops;
    assert(lv_nav_button_request_back() == LV_NAV_BACK_HANDLED);
    assert(!history->dialog && history->selecting && test_deletes == deletes && test_pops == pops);
    history_test_click(history->actions[2]); assert(history->dialog);
    ui_history_record_t incoming = test_store.records[0]; incoming.record_no = 1001;
    memmove(test_store.records + 1, test_store.records,
            (UI_HISTORY_MAX_RECORDS - 1) * sizeof(test_store.records[0]));
    test_store.records[0] = incoming;
    ui_page_19_history_refresh();
    assert(history->dialog && history->confirmed_ids[0] == 1000 && history->confirmed_ids[1] == 998);
    history_test_click(history_test_button(history->dialog, ui_text_get(UI_TEXT_HISTORY_APPLY)));
    assert(!history->dialog && !history->selecting && test_deletes == deletes + 1);
    assert(test_deleted_count == 2);
    assert((test_deleted_ids[0] == 1000 && test_deleted_ids[1] == 998) ||
           (test_deleted_ids[0] == 998 && test_deleted_ids[1] == 1000));
    assert(record_find(1001) && !record_find(1000) && !record_find(998));
    assert(test_selection_mutations == 0);

    history_test_click(history->actions[2]);
    uint32_t missing = history_test_click_record(0);
    history_test_click(history->actions[2]); assert(history->dialog);
    unsigned index = 0;
    while (index < test_store.record_count && test_store.records[index].record_no != missing) ++index;
    assert(index < test_store.record_count);
    memmove(test_store.records + index, test_store.records + index + 1,
            (test_store.record_count - index - 1) * sizeof(test_store.records[0]));
    --test_store.record_count;
    ui_page_19_history_refresh();
    deletes = test_deletes;
    history_test_click(history_test_button(history->dialog, ui_text_get(UI_TEXT_HISTORY_APPLY)));
    assert(test_deletes == deletes && !history->dialog);
    assert(!strcmp(test_toast, ui_text_get(UI_TEXT_HISTORY_SAVE_FAILED)));
    assert(lv_nav_button_request_back() == LV_NAV_BACK_HANDLED);
    assert(!history->selecting && !history->selected_count);
    unsigned count = test_store.record_count;
    history_test_click(history->reset_total); assert(history->dialog);
    history_test_click(history_test_button(history->dialog, ui_text_get(UI_TEXT_HISTORY_APPLY)));
    assert(!history->dialog && test_store.record_count == count && test_store.total_notes_counted == 0);
    test_store.total_notes_counted = 77;
    history_test_click(history->reset_total); assert(history->dialog);
    test_clear_accept = false;
    storage_job_id_t before_commit = test_commit_id;
    history_test_click(history_test_button(history->dialog, ui_text_get(UI_TEXT_HISTORY_APPLY)));
    assert(!history->dialog && test_store.record_count == count && test_store.total_notes_counted == 77);
    assert(test_commit_id == before_commit && !strcmp(test_toast, ui_text_get(UI_TEXT_HISTORY_SAVE_FAILED)));
    test_clear_accept = true;
    puts("PASS: selection only current results, local ID state, confirmation cancel, incoming-record-safe deletion, stale-ID refusal and lifetime reset preserves reports");
}

static void history_test_unknown_and_lifecycle(unsigned baseline_timers)
{
    history_query_input_t filter = {0}; strcpy(filter.serial, "NOTSAVED");
    history_test_apply(&filter);
    assert(!history->result.matched_count && history->unknown_count == history->record_count);
    assert(!lv_obj_has_flag(history->empty, LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_is_visible(history->unknown));
    history_test_bmp("history-unknown-results");
    history_test_click(history->unknown);
    assert(history->reviewing_unknown && displayed_count() == history->unknown_count);
    for (size_t i = 1; i < history->unknown_count; ++i)
        assert(history->unknown_ids[i - 1] > history->unknown_ids[i]);
    history_test_click(history->sort);
    assert(history->input.oldest_first && history->reviewing_unknown);
    for (size_t i = 1; i < history->unknown_count; ++i)
        assert(history->unknown_ids[i - 1] < history->unknown_ids[i]);
    history_test_click(history->sort); assert(!history->input.oldest_first);
    assert(!lv_obj_is_visible(history->empty));
    history_test_bmp("history-unknown-review");
    history_test_click(history->unknown); assert(!history->reviewing_unknown);
    history_test_click(history->actions[0]); assert(history->search);
    unsigned pops = test_pops;
    assert(lv_nav_button_request_back() == LV_NAV_BACK_HANDLED);
    assert(!history->search && test_pops == pops);
    ui_page_19_history_suspend(); assert(!lv_obj_is_visible(history->root));
    size_t cached = history->record_count;
    history_test_fixtures(1); ui_page_19_history_refresh(); history_test_tick(100);
    assert(history->record_count == cached && history->model_dirty);
    assert(ui_page_19_history_resume());
    assert(history->record_count == 1 && history->result.matched_count == 1 && !history->input.serial[0]);
    ui_page_19_history_suspend(); ui_lang_set(LANGUAGE_CN);
    assert(ui_page_19_history_resume());
    assert(!strcmp(lv_label_get_text(history->title), ui_text_get(UI_TEXT_HISTORY_RECORDS)));
    assert(!strcmp(lv_label_get_text(lv_obj_get_child(history->list_panel, 0)), ui_text_get(UI_TEXT_PAGE01_DETAIL_COL_NO)));
    ui_page_19_history_suspend(); ui_lang_set(LANGUAGE_EN);
    assert(ui_page_19_history_resume());
    assert(!strcmp(lv_label_get_text(lv_obj_get_child(history->list_panel, 0)), ui_text_get(UI_TEXT_PAGE01_DETAIL_COL_NO)));
    ui_page_19_history_destroy(); history_test_tick(300);
    assert(!history && history_test_timers() == baseline_timers);
    history_test_fixtures(1);
    test_store.records[0].sn_detail_text[0] = '\0';
    strcpy(test_store.records[0].sn_text, "LEGACYONLY\n");
    ui_page_19_history_create(lv_scr_act()); history_test_click_record(0);
    assert(history->detail_mode && section_count(1) == 1);
    assert(current_detail()->serials[0].denom == 0);
    lv_obj_t *legacy_view = lv_recycled_list_object(history->sections[1].list);
    lv_obj_t *legacy_row = lv_obj_get_child(legacy_view, 0);
    assert(!strcmp(lv_label_get_text(lv_obj_get_child(legacy_row, 2)), "--"));
    history_test_bmp("history-legacy-detail");
    ui_page_19_history_destroy(); history_test_tick(300);
    lv_mem_monitor_t warm; lv_mem_monitor(&warm);
    for (unsigned i = 0; i < 8; ++i) {
        history_test_fixtures(UI_HISTORY_MAX_RECORDS);
        ui_page_19_history_create(lv_scr_act()); assert(history);
        history_test_click_record(0);
        assert(history->detail_mode);
        ui_page_19_history_suspend(); assert(ui_page_19_history_resume());
        history_test_click(history->actions[0]); assert(history->search);
        ui_page_19_history_suspend(); assert(!history->search);
        for(unsigned slot=0;slot<UI_HISTORY_MAX_RECORDS;++slot) {
            assert(!history->details[slot] && !history->records[slot].detail);
        }
        assert(history->model_dirty);
        ui_page_19_history_suspend(); /* Repeated hide is idempotent. */
        assert(ui_page_19_history_resume());
        ui_page_19_history_destroy(); history_test_tick(300);
        assert(!history && history_test_timers() == baseline_timers);
        lv_mem_monitor_t now; lv_mem_monitor(&now);
        assert(now.free_size == warm.free_size && lv_mem_test() == LV_RES_OK);
    }
    test_available = false;
    ui_page_19_history_create(lv_scr_act()); assert(history);
    assert(!history->record_count && !history->result.matched_count);
    assert(!lv_damped_button_is_enabled(history->actions[0]));
    assert(!lv_damped_button_is_enabled(history->actions[1]));
    assert(!lv_damped_button_is_enabled(history->actions[2]));
    assert(!lv_damped_button_is_enabled(history->reset_total));
    assert(!strcmp(lv_label_get_text(history->empty), ui_text_get(UI_TEXT_HISTORY_STORAGE_FAILED)));
    history_test_bmp("history-unreadable");
    pops = test_pops;
    assert(lv_nav_button_request_back() == LV_NAV_BACK_HANDLED && test_pops == pops + 1);
    ui_page_19_history_destroy(); history_test_tick(300); test_available = true;
    assert(history_test_timers() == baseline_timers);
    puts("PASS: incomplete results separated from confirmed matches, layered page ESC, hidden refresh deferral, repeated lifecycle heap/timer recovery and unreadable-store protection");
}

static void history_test_row_number(uint32_t index,unsigned number)
{
    assert(index<displayed_count());
    assert(lv_recycled_list_scroll_to_index(history->list,index));
    history_test_render();
    lv_obj_t *row=lv_obj_get_child(lv_recycled_list_object(history->list),index%(HISTORY_ROWS+1U));
    char expected[24];snprintf(expected,sizeof(expected),"%u",number);
    assert(!strcmp(lv_label_get_text(lv_obj_get_child(row,0)),expected));
}

static void history_test_delete_confirm(void)
{
    history_test_click(history->actions[2]);assert(history->dialog);
    history_test_click(history_test_button(history->dialog,ui_text_get(UI_TEXT_HISTORY_APPLY)));
    assert(!history->dialog && !history->selecting);
}

static void history_test_contiguous_rows(unsigned baseline_timers)
{
    history_test_fixtures(5);
    for(unsigned i=0;i<5;++i)test_store.records[i].record_no=5-i;
    test_store.next_record_no=6;
    ui_page_19_history_create(lv_scr_act());history_test_tick(100);
    for(unsigned i=0;i<5;++i)history_test_row_number(i,5-i);
    history_test_click(history->actions[2]);
    assert(history_test_click_record(1)==4 && history_test_click_record(2)==3);
    history_test_delete_confirm();
    assert(displayed_count()==3 && displayed_id(0)==5 && displayed_id(1)==2 && displayed_id(2)==1);
    for(unsigned i=0;i<3;++i)history_test_row_number(i,3-i);
    history_test_bmp("history-after-middle-delete");
    assert(history_test_click_record(0)==5 && history->current_id==5);
    assert(strstr(lv_label_get_text(history->title),"#5"));
    history_test_click(history->actions[2]);
    assert(test_exported_count==1 && test_exported_ids[0]==5);
    assert(lv_nav_button_request_back()==LV_NAV_BACK_HANDLED);
    history_test_click(history->sort);
    assert(displayed_id(0)==1 && displayed_id(2)==5);
    for(unsigned i=0;i<3;++i)history_test_row_number(i,i+1);
    ui_page_19_history_destroy();history_test_tick(300);

    /* Removing the last viewport clamps to surviving rows, then empty state. */
    history_test_fixtures(10);ui_page_19_history_create(lv_scr_act());
    history_test_click(history->actions[2]);
    for(unsigned i=5;i<10;++i)history_test_click_record(i);
    assert(lv_recycled_list_window(history->list)->first==5);
    history_test_delete_confirm();
    assert(displayed_count()==5 && lv_recycled_list_window(history->list)->first==0);
    for(unsigned i=0;i<5;++i)history_test_row_number(i,5-i);
    history_test_click(history->actions[2]);history_test_click(history->actions[0]);
    assert(history->selected_count==5);history_test_delete_confirm();
    assert(!displayed_count() && lv_obj_is_visible(history->empty));
    assert(!strcmp(lv_label_get_text(history->range),"0 / 0"));
    assert(!lv_damped_button_is_enabled(history->actions[1]));
    assert(!lv_damped_button_is_enabled(history->actions[2]));
    history_test_bmp("history-after-delete-all");
    history_test_fixtures(1);ui_page_19_history_refresh();
    assert(displayed_count()==1 && !lv_obj_is_visible(history->empty));
    history_test_row_number(0,1);
    ui_page_19_history_destroy();history_test_tick(300);
    assert(history_test_timers()==baseline_timers);
    puts("PASS: contiguous result ordinals after middle/tail/all deletion; record IDs/detail/export stay stable; empty list recovers on arrival");
}

static void history_test_capacity_and_missing_detail(unsigned baseline_timers)
{
    assert(UI_HISTORY_MAX_RECORDS==100);
    history_test_fixtures(UI_HISTORY_MAX_RECORDS);
    ui_page_19_history_create(lv_scr_act());history_test_tick(100);
    assert(displayed_count()==100);
    assert(!strcmp(lv_label_get_text(history->notice),ui_text_get(UI_TEXT_HISTORY_CAPACITY_FULL)));
    history_test_bmp("history-100-full");
    history_test_row_number(0,100);history_test_row_number(99,1);
    assert(history_test_click_record(99)==901 && history->detail_mode);
    assert(current_detail() && section_count(1)==2);
    assert(lv_nav_button_request_back()==LV_NAV_BACK_HANDLED);
    assert(lv_recycled_list_window(history->list)->first==95);
    history_test_bmp("history-100-tail");
    history_query_input_t filter={0};strcpy(filter.currency,"USD");
    history_test_apply(&filter);
    assert(displayed_count()==50 && !history->detail_mode);
    history_test_row_number(0,50);history_test_row_number(49,1);
    history_test_click(history->sort);
    assert(displayed_id(0)==902 && displayed_id(49)==1000);
    history_test_row_number(0,1);history_test_row_number(49,50);
    history_test_click(history->sort);
    uint32_t expired=history_test_click_record(10);
    assert(history->detail_mode && history->current_id==expired);
    float offset=lv_recycled_list_window(history->list)->offset;
    assert(ui_history_record_delete_records(&expired,1));ui_page_19_history_refresh();
    assert(!history->detail_mode && history->current_id==0);
    assert(!strcmp(history->input.currency,"USD") && displayed_count()==49);
    assert(lv_recycled_list_window(history->list)->offset==offset);
    assert(lv_obj_is_visible(history->list_panel));
    assert(!strcmp(test_toast,ui_text_get(UI_TEXT_HISTORY_MISSING)));
    for(unsigned i=0;i<3;++i)assert(!lv_obj_is_visible(history->sections[i].panel));
    history_test_bmp("history-missing-detail-return");
    ui_page_19_history_destroy();history_test_tick(300);
    assert(history_test_timers()==baseline_timers);
    puts("PASS: 100-record tail/detail/search, filtered ascending/descending ordinals, disappeared detail returns without clearing filter/scroll");
}

static void history_test_inertia_tap(unsigned baseline_timers)
{
    history_test_fixtures(20);ui_page_19_history_create(lv_scr_act());history_test_tick(100);
    lv_area_t viewport;lv_obj_get_coords(lv_recycled_list_object(history->list),&viewport);
    int x=viewport.x1+400,y=viewport.y1+100;
    history_test_pointer(x,viewport.y1+210,true);
    history_test_pointer(x,viewport.y1+170,true);
    history_test_pointer(x,viewport.y1+90,false);
    assert(!history->detail_mode);
    float moving_offset=lv_recycled_list_window(history->list)->offset;
    history_test_tick(40);
    assert(lv_recycled_list_window(history->list)->offset>moving_offset);
    history_test_pointer(x,y,true);history_test_pointer(x,y,false);
    assert(!history->detail_mode && !lv_recycled_list_tap_allowed(history->list));
    history_test_pointer(x,y,true);history_test_pointer(x,y,false);
    assert(history->detail_mode);
    assert(lv_nav_button_request_back()==LV_NAV_BACK_HANDLED);
    history_test_click(history->actions[2]);
    assert(lv_recycled_list_scroll_to_index(history->list,0));
    history_test_pointer(x,viewport.y1+210,true);
    history_test_pointer(x,viewport.y1+170,true);
    history_test_pointer(x,viewport.y1+90,false);
    history_test_pointer(x,y,true);history_test_pointer(x,y,false);
    assert(history->selecting && !history->selected_count);
    history_test_pointer(x,y,true);history_test_pointer(x,y,false);
    assert(history->selected_count==1);
    ui_page_19_history_destroy();history_test_tick(300);
    assert(history_test_timers()==baseline_timers);
    puts("PASS: touching to stop inertia cannot open or select a record; a fresh stationary tap works");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    lv_init();
    lv_img_decoder_t *decoder = lv_img_decoder_create(); assert(decoder);
    lv_img_decoder_set_info_cb(decoder, test_asset_info);
    lv_img_decoder_set_open_cb(decoder, test_asset_open);
    lv_img_decoder_set_close_cb(decoder, test_asset_close);
    static lv_color_t pixels[1280 * 40]; static lv_disp_draw_buf_t buffer;
    lv_disp_draw_buf_init(&buffer, pixels, NULL, 1280 * 40);
    lv_disp_drv_t display; lv_disp_drv_init(&display);
    display.hor_res = 1280; display.ver_res = 400; display.draw_buf = &buffer; display.flush_cb = test_flush;
    assert(lv_disp_drv_register(&display));
    lv_indev_drv_t pointer; lv_indev_drv_init(&pointer);
    pointer.type = LV_INDEV_TYPE_POINTER; pointer.read_cb = test_pointer_read;
    assert(lv_indev_drv_register(&pointer));
    unsigned baseline_timers = history_test_timers();
    history_test_fixtures(0);
    ui_page_19_history_create(lv_scr_act()); history_test_tick(100);
    assert(history && !history->record_count && !history->result.matched_count);
    assert(lv_obj_is_visible(history->empty));
    history_test_bmp("history-empty");
    ui_page_19_history_destroy(); history_test_tick(300);
    assert(history_test_timers() == baseline_timers);
    history_test_fixtures(20);
    ui_page_19_history_create(lv_scr_act()); history_test_tick(100);
    history_test_bmp("history-full");
    history_test_list_and_detail();
    history_test_selection_and_delete();
    history_test_unknown_and_lifecycle(baseline_timers);
    assert(history_test_timers() == baseline_timers);
    history_test_contiguous_rows(baseline_timers);
    history_test_capacity_and_missing_detail(baseline_timers);
    history_test_inertia_tap(baseline_timers);
    history_test_search_module();
    assert(history_test_timers() == baseline_timers);
    puts("PASS: actual History LVGL empty/full smoke and timer ownership");
    return 0;
}
