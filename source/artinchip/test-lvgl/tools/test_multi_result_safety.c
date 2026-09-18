#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "un260/counting/counting_data_store_internal.h"
#include "un260/counting/counting_history_service.h"
#include "un260/counting/counting_multi.h"
const counting_multi_t *counting_multi_current(void) {static const counting_multi_t empty={0};return &empty;}
#include "un260/currency/currency_state.h"
#include "un260/lv_system/ui_history_data.h"
#include "un260/lv_system/ui_qr_data.h"
#include "un260/lv_system/machine_time.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_core/page_02_list_data.h"

/* Only transport, files, presentation and formatting are faked. Production
 * request guards, snapshot builder, spool, data store and QR code run unchanged. */
static unsigned uart_calls, usb_calls, write_calls, toast_calls, append_calls;
static uint32_t history_total;
static ui_history_record_t accepted;
static storage_job_status_t accepted_status = STORAGE_JOB_SUCCEEDED;
static bool history_available = true;
static const char *last_toast;

typedef struct { int code; } lv_event_t;
typedef struct {
    int x, w, h;
    const char *text;
    bool show_loader, align_center, use_text_area;
    uint32_t loader_color, auto_hide_ms;
} lv_print_toast_config_t;
#define LV_EVENT_CLICKED 1
#define lv_color_hex(value) (value)
#define lv_snprintf snprintf
#define UI_EXPORT_TOAST_TEXT_EXPORTING "Exporting"
#define UI_EXPORT_TOAST_TEXT_COUNT_FIRST "Count first"
#define UI_EXPORT_TOAST_TEXT_EXPORT_FAILED "Export failed"
static bool g_ui_export_data_lock;
static int lv_event_get_code(lv_event_t *event) { return event->code; }
static lv_print_toast_config_t lv_print_toast_get_default_config(void)
{ return (lv_print_toast_config_t){ 0 }; }
static void lv_print_toast_show_with_config(const lv_print_toast_config_t *config)
{ toast_calls++; last_toast = config->text; }
static void lv_print_toast_show(const char *text) { toast_calls++; last_toast = text; }
const char *ui_text_get(ui_text_id_t id)
{
    switch (id) {
    case UI_TEXT_WIDGET_MULTI_RESULT_UNSUPPORTED: return "MULTI_UNSUPPORTED";
    case UI_TEXT_WIDGET_SMART_ISLAND_CUR_PCS_AMOUNT_FMT: return "%s %d pcs AMOUNT %.0f";
    case UI_TEXT_WIDGET_SMART_ISLAND_CUR_PCS_FMT: return "%s %d pcs";
    case UI_TEXT_WIDGET_SMART_ISLAND_CUR_MODE_FMT: return "%s %s";
    case UI_TEXT_WIDGET_SMART_ISLAND_PCS_AMOUNT_FMT: return "%d pcs AMOUNT %.0f";
    case UI_TEXT_WIDGET_SMART_ISLAND_READY_CUR_FMT: return "Ready %s";
    default: return "TEXT";
    }
}
void machine_time_get(machine_time_value_t *value)
{ *value = (machine_time_value_t){ 2026, 9, 9, 12, 0, 0 }; }
static int protocol_send(uint8_t command, const uint8_t *payload, uint16_t len)
{
    assert(command == 0x3C && len == 9 && memcmp(payload, "CNY", 3) == 0);
    uart_calls++;
    return len;
}
static void ui_export_data_show_alarm_toast(const char *text) { last_toast = text; toast_calls++; }
static void ui_export_data_show_normal_toast(const char *text) { last_toast = text; toast_calls++; }
static bool ui_export_data_is_empty(void) { return counting_data_current()->total_pcs == 0; }
static bool usb_storage_prepare(void) { usb_calls++; return true; }
static void ui_export_data_start_lock(void) {}
static void ui_export_data_build_export_name(char *out, size_t size) { snprintf(out, size, "TEST"); }
static bool usb_storage_make_unique_file_pair(const char *prefix,
    const char *ext1, char *out1, size_t size1, const char *ext2, char *out2, size_t size2)
{
    (void)prefix; (void)ext1; (void)ext2;
    snprintf(out1, size1, "TEST.csv"); snprintf(out2, size2, "TEST.html"); return true;
}
static bool ui_export_data_write_csv_file(const char *path) { (void)path; write_calls++; return true; }
static bool ui_export_data_write_html_file(const char *path) { (void)path; write_calls++; return true; }
static bool usb_storage_commit_file_pair(const char *a, const char *b, const char *c, const char *d)
{ (void)a; (void)b; (void)c; (void)d; return true; }
static int fake_unlink(const char *path) { (void)path; return 0; }
#define unlink fake_unlink

static void history_record_defaults(ui_history_record_t *record) { memset(record, 0, sizeof(*record)); }
static uint32_t history_amount_to_u32(float amount) { return amount > 0 ? (uint32_t)amount : 0; }
static void history_format_denom(const counting_sim_t *sim, char *out, size_t size)
{ (void)sim; snprintf(out, size, "10|12|120"); }
static void history_format_sn(const counting_sim_t *sim, char *out, size_t size)
{ (void)sim; snprintf(out, size, "TEST001"); }
static void history_format_sn_detail(const counting_sim_t *sim, char *out, size_t size)
{ (void)sim; snprintf(out, size, "TEST001|10"); }

bool ui_history_data_can_accept(void) { return history_available; }
const ui_history_store_t *ui_history_data_get(void) {static const ui_history_store_t store={.next_record_no=1};return &store;}
void ui_history_total_notes_counted_set(uint32_t total) {history_total=total;}
bool ui_history_record_update_snapshot(const ui_history_record_t *record,uint32_t total)
{(void)record;(void)total;assert(false);return false;}
uint32_t ui_history_total_notes_counted_get(void) { return history_total; }
bool ui_history_record_append_snapshot(const ui_history_record_t *record, uint32_t total)
{
    append_calls++; accepted = *record; history_total = total;
    accepted_status = STORAGE_JOB_PENDING; return true;
}
storage_job_id_t ui_history_last_commit_id(void) { return append_calls; }
storage_job_status_t ui_history_commit_status(storage_job_id_t id)
{ assert(id > 0); return accepted_status; }
storage_job_status_t ui_history_data_status(void)
{ return history_available ? accepted_status : STORAGE_JOB_FAILED; }

typedef struct lv_obj { char text[80]; struct lv_obj *children[3]; } lv_obj_t;
typedef int lv_font_t;
static lv_font_t lv_font_instrument_sans_medium_14, lv_font_instrument_sans_medium_20;
static lv_font_t lv_font_instrument_sans_semibold_22;
static bool lv_obj_is_valid(const lv_obj_t *object) { return object != NULL; }
static const char *lv_label_get_text(const lv_obj_t *object) { return object->text; }
static void lv_label_set_text(lv_obj_t *object, const char *text)
{ snprintf(object->text, sizeof(object->text), "%s", text); }
static void lv_label_set_text_fmt(lv_obj_t *object, const char *format, ...)
{
    va_list args; va_start(args, format);
    vsnprintf(object->text, sizeof(object->text), format, args); va_end(args);
}
static lv_obj_t *lv_obj_get_child(lv_obj_t *object, unsigned index) { return object->children[index]; }
static void lv_obj_set_style_bg_color(lv_obj_t *object, unsigned color, int selector)
{ (void)object; (void)color; (void)selector; }
static void number_set(lv_obj_t *object, double value, const lv_font_t *font)
{ (void)font; lv_label_set_text_fmt(object, "%.0f", value); }
const char *ui_text_counting_reject_reason(uint8_t code) { (void)code; return "ERROR"; }
static struct {
    bool values_valid;
    lv_obj_t *amount_value, *pcs_value, *reject_value;
    char displayed_amount[32], displayed_pcs[32];
    int displayed_reject;
} g_pure_page;

enum { PAGE_02_SECTION_A, PAGE_02_SECTION_B, PAGE_02_SECTION_C, PAGE_02_SECTION_COUNT };
enum { ALL_SECTIONS = 7 };
typedef struct { int id; } section_layout_t;
typedef struct { const section_layout_t *layout; void *list; lv_obj_t *panel; } list_section_t;
static struct {
    page_02_list_data_t data;
    int located_slot;
    void *search;
    void *multi;
    lv_obj_t *page;
    bool multi_visible;
    language_t language;
    lv_obj_t *pcs, *amount;
    list_section_t section[3];
} list_fixture, *view = &list_fixture;
enum { LV_OBJ_FLAG_HIDDEN=1, UI_PAGE_LIST=2 };
static void lv_obj_add_flag(lv_obj_t *o,int f){(void)o;(void)f;}
static void lv_obj_clear_flag(lv_obj_t *o,int f){(void)o;(void)f;}
static void *ui_multi_detail_create(lv_obj_t *p,bool expanded,void *cb,void *ctx)
{(void)p;(void)expanded;(void)cb;(void)ctx;return &list_fixture;}
static void ui_multi_detail_visible(void *v,bool visible){(void)v;(void)visible;}
static void ui_multi_detail_refresh(void *v){(void)v;}
static bool multi_gesture(int action){(void)action;return false;}
static void gesture_service_set_page_policy(int page,void *drag,bool (*cb)(int))
{(void)page;(void)drag;(void)cb;}
static void gesture_service_clear_page_policy(int page){(void)page;}
static uint32_t dirty, reset_positions, last_row_count;
static bool visible(void) { return true; }
static language_t ui_lang_get(void) { return LANGUAGE_EN; }
static void translate(void) {}
static void lv_recycled_list_refresh(void *list, uint32_t count, bool reset)
{ (void)list; (void)reset; last_row_count = count; }

enum { SMART_ISLAND_SCENE_COUNTING, SMART_ISLAND_SCENE_RESULT, SMART_ISLAND_SCENE_WARNING,
       SMART_ISLAND_SCENE_UPDATE, SMART_ISLAND_SCENE_QR, SMART_ISLAND_SCENE_IDLE };
static struct {
    struct {
        char compact[128], info_title[128], info_summary[128], info_footer[128], info_extra[128];
        char serial_ticker[128], idle_line1[128], idle_line2[128], idle_line3[128];
        bool analysis_valid, idle_has_issue, idle_has_data, idle_no_count;
        int analysis_valid_pcs, analysis_suspect_pcs, analysis_damaged_pcs;
        uint8_t idle_quality_percent;
    } text;
    struct { int scene; struct { const char *title, *subtitle; } content; } view;
    struct { int pcs; } counting;
    struct { const char *text; } warning;
} g_si_ctx;
static const char *smart_island_get_work_mode_text(void) { return "MDC"; }
static uint8_t machine_state_mode(void) { return MODE_MDC; }
static const char *smart_island_text_or_default(const char *text, ui_text_id_t id)
{ return text && text[0] ? text : ui_text_get(id); }
static void smart_island_apply_texts(void) {}
static void smart_island_show_qr_error_toast(const char *text) { last_toast = text; }
static unsigned qr_shows;
static bool lv_qr_popup_show(const char *text) { assert(text[0]); qr_shows++; return true; }

#include "multi_safety_under_test.inc"

static void assert_texts_safe(void)
{
    lv_obj_t a = { 0 }, b = { 0 }, c = { 0 }, row = { .children = { &a, &b, &c } };
    const section_layout_t layout_a = { PAGE_02_SECTION_A }, layout_b = { PAGE_02_SECTION_B };
    list_section_t section = { .layout = &layout_a };
    counting_sim_t *sim = counting_data_mutable();
    unsigned qr_before = qr_shows;
    g_pure_page.values_valid = false;
    g_pure_page.amount_value = &a; g_pure_page.pcs_value = &b; g_pure_page.reject_value = &c;
    pure_refresh_values();
    assert(strcmp(a.text, "--") == 0 && strcmp(b.text, "12") == 0);
    view->pcs = &a; view->amount = &b; dirty = 1; reset_positions = 0;
    commit(NULL, 1);
    if(currency_state_multi_selected())assert(view->multi_visible&&view->multi);
    else assert(strcmp(a.text, "12") == 0 && strcmp(b.text, "--") == 0 && last_row_count == 1);
    row_bind(&row, 0, &section);
    assert(strcmp(a.text, "--") == 0 && strcmp(b.text, "12") == 0 && strcmp(c.text, "--") == 0);
    section.layout = &layout_b;
    row_bind(&row, 0, &section);
    assert(strcmp(c.text, "--") == 0);
    sim->last_total_pcs = 12; sim->last_total_amount = 987654.0f;
    g_si_ctx.view.scene = SMART_ISLAND_SCENE_COUNTING;
    smart_island_rebuild_scene_texts();
    assert(strcmp(g_si_ctx.text.info_summary, "MULTI 12 pcs") == 0);
    g_si_ctx.view.scene = SMART_ISLAND_SCENE_IDLE;
    snprintf(g_si_ctx.text.idle_line1, sizeof(g_si_ctx.text.idle_line1), "STALE AMOUNT 987654");
    smart_island_rebuild_scene_texts();
    assert(strcmp(g_si_ctx.text.info_summary, "MULTI 12 pcs") == 0);
    g_si_ctx.view.scene = SMART_ISLAND_SCENE_QR;
    smart_island_rebuild_scene_texts();
    assert(strcmp(g_si_ctx.text.info_footer, "MULTI 12 pcs") == 0);
    smart_island_show_qr_popup();
    assert(strcmp(last_toast, "MULTI_UNSUPPORTED") == 0 && qr_shows == qr_before);
    g_pure_page.amount_value = NULL; g_pure_page.pcs_value = NULL; g_pure_page.reject_value = NULL;
    view->pcs = NULL; view->amount = NULL;
}

static void pending(counting_session_state_t *session, bool end_seen)
{
    session->history_record.valid = true; session->history_record.end_seen = end_seen;
    session->history_record.pcs = 12; session->history_record.amount = 120.0f;
}
static void assert_outputs_blocked(void)
{
    char text[3000] = "unchanged";
    lv_event_t click = { LV_EVENT_CLICKED };
    unsigned before_uart = uart_calls, before_usb = usb_calls, before_write = write_calls;
    page_01_print_btn_event_cb(&click);
    assert(strcmp(last_toast, "MULTI_UNSUPPORTED") == 0);
    assert(!ui_export_data_request());
    assert(strcmp(last_toast, "MULTI_UNSUPPORTED") == 0);
    assert(!ui_qr_data_is_ready() && !ui_qr_data_build(text, sizeof(text)));
    assert(strcmp(text, "unchanged") == 0);
    assert(uart_calls == before_uart && usb_calls == before_usb && write_calls == before_write);
}

int main(void)
{
    counting_sim_t *sim = counting_data_mutable();
    counting_session_state_t session = { 0 };
    ui_history_record_t record, before_record;
    lv_event_t click = { LV_EVENT_CLICKED }, release = { 2 };
    char qr[3000];

    currency_state_reset();
    counting_data_mark_multi_result(NULL); counting_data_reset_result_scope(NULL);
    assert(!counting_data_monetary_result_supported(NULL));
    sim->total_pcs = 12; sim->total_amount = 120.0f; sim->err_expected = 2;
    assert(counting_data_monetary_result_supported(sim));
    assert(ui_qr_data_build(qr, sizeof(qr)) && strstr(qr, "120.00 CNY") != NULL);
    page_01_print_btn_event_cb(&release); assert(uart_calls == 0);
    page_01_print_btn_event_cb(&click); assert(uart_calls == 1);
    assert(ui_export_data_request() && usb_calls == 1 && write_calls == 2);

    /* A mode ACK can precede freezing the just-finished SINGLE result. */
    assert(currency_state_confirm_multi_selection());
    assert_outputs_blocked();
    assert_texts_safe();
    assert(ui_history_record_build_from_session(sim, 12, 120, "", "", "", "", &record));
    assert(strcmp(record.currency, "CNY") == 0 && record.amount == 120);
    pending(&session, true);
    assert(counting_history_try_commit(&session, sim, 0) == COUNTING_HISTORY_COMMIT_PENDING);
    assert(append_calls == 1 && history_total == 12 && accepted.amount == 120);

    counting_data_mark_multi_result(sim);
    assert(sim->total_pcs == 12 && counting_data_reject_pcs_count(sim) == 2);
    assert(!counting_data_monetary_result_supported(sim));
    assert_outputs_blocked();
    assert(currency_state_leave_special_selection());
    /* Switching back, clearing serials/errors, or starting another ADD batch
     * cannot turn unidentified money into a supported single-currency result. */
    counting_data_clear_serials(sim); counting_data_clear_errors(sim);
    assert_outputs_blocked();
    assert_texts_safe();
    memset(&record, 0xA5, sizeof(record)); before_record = record;
    assert(!ui_history_record_build_from_session(sim, 12, 9999, "", "", "", "", &record));
    assert(memcmp(&record, &before_record, sizeof(record)) == 0);
    pending(&session, true);
    assert(counting_history_try_commit(&session, sim, 1) == COUNTING_HISTORY_COMMIT_NOT_READY);
    assert(append_calls == 1 && history_total == 12); /* No MULTI groups supplied. */
    assert(counting_history_try_commit(&session, sim, 2) == COUNTING_HISTORY_COMMIT_NOT_READY);
    accepted_status = STORAGE_JOB_SUCCEEDED;
    assert(counting_history_poll_commit(&session, sim, 2) == COUNTING_HISTORY_COMMIT_SAVED);
    assert(accepted.amount == 120 && strcmp(accepted.currency, "CNY") == 0);
    assert(counting_history_can_start());

    pending(&session, false);
    assert(counting_history_prepare_reset(&session, sim, 3));
    assert(append_calls == 1);
    pending(&session, false);
    assert(counting_history_prepare_start(&session, sim, 4));
    history_available = false; assert(!counting_history_can_start()); history_available = true;

    sim->total_pcs = 0; sim->total_amount = 0;
    counting_data_reset_result_scope(sim);
    assert(counting_data_monetary_result_supported(sim));
    sim->total_pcs = 5; sim->total_amount = 50;
    assert(ui_qr_data_build(qr, sizeof(qr)) && strstr(qr, "50.00 CNY") != NULL);
    assert(ui_export_data_request() && usb_calls == 2 && write_calls == 4);
    page_01_print_btn_event_cb(&click); assert(uart_calls == 2);
    assert(ui_history_record_build_from_session(sim, 5, 50, "", "", "", "", &record));
    assert(record.pcs == 5 && record.amount == 50);
    g_si_ctx.text.idle_line1[0] = '\0';
    g_si_ctx.view.scene = SMART_ISLAND_SCENE_COUNTING;
    smart_island_rebuild_scene_texts();
    assert(strcmp(g_si_ctx.text.info_summary, "CNY 5 pcs AMOUNT 50") == 0);
    smart_island_show_qr_popup(); assert(qr_shows == 1);
    puts("PASS MULTI sticky scope, history/print/export/QR guards, List/Pure/island text, old snapshots and reset/start notices");
    return 0;
}
