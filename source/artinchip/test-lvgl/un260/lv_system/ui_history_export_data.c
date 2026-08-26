#include "ui_history_export_data.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "un260/counting/counting_reject_reason.h"
#include "un260/history/history_export_sn_parser.h"
#include "un260/history/history_export_text.h"
#include "un260/lv_components/lv_print_toast.h"
#include "un260/lv_system/ui_history_data.h"
#include "un260/currency/currency_state.h"
#include "un260/machine_state/machine_state.h"
#include "un260/storage/usb_storage.h"

#define UI_HISTORY_EXPORT_LOCK_MS          2000U
#define UI_HISTORY_EXPORT_TEXT_EXPORTING   "Exporting..."
#define UI_HISTORY_EXPORT_TEXT_COUNT_FIRST "Please Count First"
#define UI_HISTORY_EXPORT_TEXT_FAILED      "Export Failed"
#define UI_HISTORY_EXPORT_PATH_SIZE        256U

typedef struct {
    char csv_path[UI_HISTORY_EXPORT_PATH_SIZE];
    char html_path[UI_HISTORY_EXPORT_PATH_SIZE];
} history_export_output_t;

static bool g_history_export_lock = false;
static lv_timer_t *g_history_export_unlock_timer = NULL;

static void history_export_show_toast(const char *text, bool alarm)
{
    lv_print_toast_config_t toast_cfg = lv_print_toast_get_default_config();

    toast_cfg.w = 320;
    toast_cfg.h = 101;
    toast_cfg.text = text ? text : (alarm ? UI_HISTORY_EXPORT_TEXT_FAILED : UI_HISTORY_EXPORT_TEXT_EXPORTING);
    toast_cfg.show_loader = true;
    toast_cfg.align_center = true;
    toast_cfg.use_text_area = false;
    toast_cfg.loader_color = alarm ? lv_color_hex(0xC0392B) : LV_PRINT_TOAST_DEFAULT_LOADER_COLOR;
    toast_cfg.auto_hide_ms = UI_HISTORY_EXPORT_LOCK_MS;

    lv_print_toast_show_with_config(&toast_cfg);
}

static void history_export_unlock_timer_cb(lv_timer_t *timer)
{
    if (timer == g_history_export_unlock_timer) {
        g_history_export_unlock_timer = NULL;
    }
    g_history_export_lock = false;
}

static void history_export_start_lock(void)
{
    if (g_history_export_unlock_timer) {
        lv_timer_del(g_history_export_unlock_timer);
        g_history_export_unlock_timer = NULL;
    }

    g_history_export_lock = true;
    g_history_export_unlock_timer = lv_timer_create(history_export_unlock_timer_cb, UI_HISTORY_EXPORT_LOCK_MS, NULL);
    if (g_history_export_unlock_timer) {
        lv_timer_set_repeat_count(g_history_export_unlock_timer, 1);
    } else {
        g_history_export_lock = false;
    }
}

static void history_export_get_currency_code(char *buf, size_t size)
{
    char curr_code[4];

    if (buf == NULL || size == 0) {
        return;
    }

    currency_state_get_active_code(curr_code);
    if (curr_code[0] != '\0') {
        lv_snprintf(buf, size, "%s", curr_code);
        return;
    }

    lv_snprintf(buf, size, "%s", "CUR");
}

static void history_export_sanitize_token(char *dst, size_t dst_size, const char *src)
{
    size_t i;
    size_t j = 0;

    if (dst == NULL || dst_size == 0) {
        return;
    }

    dst[0] = '\0';
    if (src == NULL) {
        lv_snprintf(dst, dst_size, "%s", "CUR");
        return;
    }

    for (i = 0; src[i] != '\0' && j + 1 < dst_size; i++) {
        unsigned char ch = (unsigned char)src[i];
        if (isalnum(ch)) {
            dst[j++] = (char)toupper(ch);
        }
    }

    if (j == 0) {
        lv_snprintf(dst, dst_size, "%s", "CUR");
    } else {
        dst[j] = '\0';
    }
}

static void history_export_get_mode_text(char *buf, size_t size)
{
    if (buf == NULL || size == 0) {
        return;
    }

    if (machine_state_mode() == MODE_MDC) {
        lv_snprintf(buf, size, "%s", "MDC");
    } else if (machine_state_mode() == MODE_SDC) {
        lv_snprintf(buf, size, "%s", "SDC");
    } else if (machine_state_mode() == MODE_CNT) {
        lv_snprintf(buf, size, "%s", "CNT");
    } else {
        lv_snprintf(buf, size, "%s", "NONE");
    }
}

static void history_export_get_sort_text(char *buf, size_t size)
{
    if (buf == NULL || size == 0) {
        return;
    }
    switch (machine_state_fo_mode()) {
    case 0:
        lv_snprintf(buf, size, "%s", "SORT:OFF");
        break;
    case 1:
        lv_snprintf(buf, size, "%s", "SORT:F");
        break;
    case 2:
        lv_snprintf(buf, size, "%s", "SORT:O");
        break;
    case 3:
        lv_snprintf(buf, size, "%s", "SORT:FO");
        break;
    default:
        lv_snprintf(buf, size, "%s", "SORT:OFF");
        break;
    }
}

static void history_export_get_add_text(char *buf, size_t size)
{
    if (buf == NULL || size == 0) {
        return;
    }
    lv_snprintf(buf, size, "%s", machine_state_add_enabled() ? "ADD:ON" : "ADD:OFF");
}

static void history_export_get_work_text(char *buf, size_t size)
{
    if (buf == NULL || size == 0) {
        return;
    }
    lv_snprintf(buf, size, "%s", machine_state_work_mode() ? "MANUAL" : "AUTO");
}

static void history_export_get_speed_text(char *buf, size_t size)
{
    if (buf == NULL || size == 0) {
        return;
    }

    switch (machine_state_speed()) {
    case 0:
        lv_snprintf(buf, size, "%s", "SPD:LOW");
        break;
    case 1:
        lv_snprintf(buf, size, "%s", "SPD:MID");
        break;
    case 2:
    default:
        lv_snprintf(buf, size, "%s", "SPD:HIGH");
        break;
    }
}

typedef struct {
    unsigned value;
    unsigned pcs;
    unsigned amount;
} history_export_denom_entry_t;

#define HISTORY_EXPORT_REJECT_REASON_SIZE 128

typedef struct {
    unsigned no;
    unsigned pcs;
    char reason[HISTORY_EXPORT_REJECT_REASON_SIZE];
} history_export_reject_entry_t;

static int history_export_hex_value(char ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
    if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
    return -1;
}

static bool history_export_hex_to_bytes(const char *text, uint8_t *buf, int buf_size, int *out_len)
{
    int hi = -1;
    int len = 0;

    if (text == NULL || buf == NULL || buf_size <= 0) {
        return false;
    }

    for (const char *p = text; *p != '\0'; p++) {
        int v = history_export_hex_value(*p);
        if (v < 0) {
            continue;
        }
        if (hi < 0) {
            hi = v;
        } else {
            if (len >= buf_size) {
                break;
            }
            buf[len++] = (uint8_t)((hi << 4) | v);
            hi = -1;
        }
    }

    if (out_len) {
        *out_len = len;
    }
    return len > 0;
}

static int history_export_split_lines(const char *src, char out[][160], int max_lines)
{
    int count = 0;

    if (src == NULL || out == NULL || max_lines <= 0) {
        return 0;
    }

    while (*src != '\0' && count < max_lines) {
        const char *start = src;
        size_t len = 0;

        while (src[len] != '\0' && src[len] != '\n' && src[len] != '\r') {
            len++;
        }

        if (len > 0) {
            size_t copy_len = len;
            if (copy_len >= sizeof(out[count])) {
                copy_len = sizeof(out[count]) - 1;
            }
            memcpy(out[count], start, copy_len);
            out[count][copy_len] = '\0';
            count++;
        }

        src += len;
        while (*src == '\n' || *src == '\r') {
            src++;
        }
    }

    return count;
}

static bool history_export_parse_denom_entries(const ui_history_record_t *rec,
                                               history_export_denom_entry_t **entries,
                                               int *count)
{
    char lines[64][160];
    int line_count;
    int i;
    history_export_denom_entry_t *tmp = NULL;

    if (entries == NULL || count == NULL) {
        return false;
    }
    *entries = NULL;
    *count = 0;

    if (rec == NULL) {
        return false;
    }

    line_count = history_export_split_lines(rec->denom_text, lines, 64);
    if (line_count <= 0) {
        return true;
    }

    tmp = (history_export_denom_entry_t *)calloc((size_t)line_count, sizeof(*tmp));
    if (tmp == NULL) {
        return false;
    }

    for (i = 0; i < line_count; i++) {
        unsigned value = 0;
        unsigned pcs = 0;
        if (sscanf(lines[i], "%u x %u", &value, &pcs) == 2 ||
            sscanf(lines[i], "%uX%u", &value, &pcs) == 2) {
            tmp[i].value = value;
            tmp[i].pcs = pcs;
            tmp[i].amount = value * pcs;
        }
    }

    *entries = tmp;
    *count = line_count;
    return true;
}

static bool history_export_parse_sn_entries(const ui_history_record_t *rec,
                                            history_export_sn_entry_t **entries,
                                            int *count)
{
    if (entries == NULL || count == NULL) {
        return false;
    }
    *entries = NULL;
    *count = 0;

    if (rec == NULL) {
        return false;
    }
    return history_export_sn_parse(rec->sn_detail_text,
                                   rec->session_log,
                                   rec->sn_text,
                                   entries,
                                   count);
}

static bool history_export_parse_reject_entries(const ui_history_record_t *rec,
                                                history_export_reject_entry_t **entries,
                                                int *count)
{
    char lines[32][160];
    int line_count;
    int i;
    int parsed = 0;
    history_export_reject_entry_t *tmp = NULL;

    if (entries == NULL || count == NULL) {
        return false;
    }
    *entries = NULL;
    *count = 0;

    if (rec == NULL) {
        return false;
    }

    line_count = history_export_split_lines(rec->session_log, lines, 32);
    if (line_count > 0) {
        tmp = (history_export_reject_entry_t *)calloc((size_t)line_count, sizeof(*tmp));
        if (tmp == NULL) {
            return false;
        }

        for (i = 0; i < line_count; i++) {
            uint8_t raw[160];
            int raw_len = 0;
            const char *space;
            unsigned code = 0;
            unsigned pcs = 0;
            const char *reason = NULL;

            if (strncmp(lines[i], "0x0C", 4) != 0) {
                continue;
            }

            space = strchr(lines[i], ' ');
            if (space == NULL) {
                continue;
            }
            if (!history_export_hex_to_bytes(space + 1, raw, (int)sizeof(raw), &raw_len) || raw_len < 6) {
                continue;
            }

            code = raw[4];
            pcs = raw[5];
            if (code == 0x00 || code == 0xFF) {
                continue;
            }
            reason = counting_reject_reason_get((uint8_t)code);
            tmp[parsed].no = (unsigned)(parsed + 1);
            tmp[parsed].pcs = pcs;
            lv_snprintf(tmp[parsed].reason, sizeof(tmp[parsed].reason), "%s", reason ? reason : "--");
            parsed++;
        }

        if (parsed > 0) {
            *entries = tmp;
            *count = parsed;
            return true;
        }

        free(tmp);
        tmp = NULL;
    }

    line_count = history_export_split_lines(rec->error_frame_text, lines, 32);
    if (line_count <= 0) {
        return true;
    }

    tmp = (history_export_reject_entry_t *)calloc((size_t)line_count, sizeof(*tmp));
    if (tmp == NULL) {
        return false;
    }

    for (i = 0; i < line_count; i++) {
        if (lines[i][0] == '\0') {
            continue;
        }
        tmp[parsed].no = (unsigned)(parsed + 1);
        tmp[parsed].pcs = 1;
        lv_snprintf(tmp[parsed].reason, sizeof(tmp[parsed].reason), "%.*s",
                    (int)sizeof(tmp[parsed].reason) - 1, lines[i]);
        parsed++;
    }

    *entries = tmp;
    *count = parsed;
    return true;
}

static void history_export_calc_totals(const ui_history_record_t *rec,
                                       const history_export_denom_entry_t *denoms,
                                       int denom_count,
                                       uint32_t *total_pcs,
                                       uint32_t *total_amount)
{
    uint64_t pcs_sum = 0;
    uint64_t amount_sum = 0;
    int i;

    if (total_pcs != NULL) {
        *total_pcs = rec ? rec->pcs : 0;
    }
    if (total_amount != NULL) {
        *total_amount = rec ? rec->amount : 0;
    }

    if (denoms == NULL || denom_count <= 0) {
        return;
    }

    for (i = 0; i < denom_count; i++) {
        if (denoms[i].value <= 0) {
            continue;
        }
        pcs_sum += denoms[i].pcs;
        amount_sum += denoms[i].amount;
    }

    if (pcs_sum > 0 && total_pcs != NULL) {
        *total_pcs = (uint32_t)pcs_sum;
    }
    if (amount_sum > 0 && total_amount != NULL) {
        *total_amount = (uint32_t)amount_sum;
    }
}

static bool history_export_flush_and_verify(FILE *fp, const char *file_path)
{
    struct stat st;

    if (fp == NULL || file_path == NULL || file_path[0] == '\0') {
        return false;
    }

    if (fflush(fp) != 0) {
        fclose(fp);
        return false;
    }

    if (fsync(fileno(fp)) != 0) {
        fclose(fp);
        return false;
    }

    if (fclose(fp) != 0) {
        return false;
    }

    if (stat(file_path, &st) != 0) {
        return false;
    }

    return (st.st_size > 0);
}

static void history_export_build_name_for_record(char *buf, size_t size, const ui_history_record_t *rec)
{
    char curr_raw[8] = {0};
    char curr[8] = {0};

    if (buf == NULL || size == 0 || rec == NULL) {
        return;
    }

    history_export_get_currency_code(curr_raw, sizeof(curr_raw));
    history_export_sanitize_token(curr, sizeof(curr), rec->currency[0] ? rec->currency : curr_raw);

    lv_snprintf(buf, size, "HISTORY_%02u_%s_%04u-%02u-%02u_%02u-%02u-%02u",
                (unsigned)(rec->slot_no ? rec->slot_no : (((rec->record_no - 1u) % UI_HISTORY_MAX_RECORDS) + 1u)),
                curr,
                (unsigned)rec->year,
                (unsigned)rec->month,
                (unsigned)rec->day,
                (unsigned)rec->hour,
                (unsigned)rec->minute,
                (unsigned)rec->second);
}

static bool history_export_write_csv_file(const char *file_path, const ui_history_record_t *rec,
                                          const history_export_denom_entry_t *denoms, int denom_count,
                                          const history_export_sn_entry_t *sns, int sn_count,
                                          const history_export_reject_entry_t *rejects, int reject_count)
{
    FILE *fp;
    int i;
    char mode_buf[8];
    char currency_csv[16];
    char text_csv[257];
    uint32_t total_pcs = 0;
    uint32_t total_amount = 0;

    if (file_path == NULL || file_path[0] == '\0' || rec == NULL) {
        return false;
    }

    history_export_calc_totals(rec, denoms, denom_count, &total_pcs, &total_amount);
    if (!history_export_csv_escape(currency_csv, sizeof(currency_csv),
                                   rec->currency[0] ? rec->currency : "CUR")) {
        return false;
    }

    fp = fopen(file_path, "w");
    if (fp == NULL) {
        return false;
    }

    history_export_get_mode_text(mode_buf, sizeof(mode_buf));
    fprintf(fp, "Un260 Intelligent Cash Counter Report\n");
    fprintf(fp, "Machine Mode,%s\n", mode_buf);
    fprintf(fp, "Export Time,%02u:%02u:%02u\n", (unsigned)rec->hour, (unsigned)rec->minute, (unsigned)rec->second);
    fprintf(fp, "Currency,\"%s\"\n", currency_csv);
    fprintf(fp, "Total Pcs,%u\n", (unsigned)total_pcs);
    fprintf(fp, "Total Amount,%u\n", (unsigned)total_amount);
    fprintf(fp, "Reject Pcs,%d\n\n", reject_count);
    fprintf(fp, "DENOMINATION SUMMARY\n");
    fprintf(fp, "DENOM,PCS,AMOUNT\n");
    for (i = 0; i < denom_count; i++) {
        if (denoms[i].value <= 0) {
            continue;
        }
        fprintf(fp, "%u,%u,%u\n", denoms[i].value, denoms[i].pcs, denoms[i].amount);
    }
    fprintf(fp, "\nSERIAL NUMBER LIST\n");
    fprintf(fp, "NO,SN,DENOM\n");
    if (sn_count > 0) {
        for (i = 0; i < sn_count; i++) {
            if (!history_export_csv_escape(text_csv, sizeof(text_csv), sns[i].sn)) {
                fclose(fp);
                return false;
            }
            fprintf(fp, "%u,\"%s\",%u\n",
                    sns[i].no, text_csv, sns[i].denom);
        }
    } else {
        fprintf(fp, "None\n");
    }
    fprintf(fp, "\nREJECT REPORT\n");
    fprintf(fp, "NO,PCS,REASON\n");
    if (reject_count > 0) {
        for (i = 0; i < reject_count; i++) {
            if (!history_export_csv_escape(text_csv, sizeof(text_csv),
                                           rejects[i].reason)) {
                fclose(fp);
                return false;
            }
            fprintf(fp, "%u,%u,\"%s\"\n",
                    rejects[i].no, rejects[i].pcs, text_csv);
        }
    } else {
        fprintf(fp, "None\n");
    }

    return history_export_flush_and_verify(fp, file_path);
}

static bool history_export_write_html_file(const char *file_path, const ui_history_record_t *rec,
                                           const history_export_denom_entry_t *denoms, int denom_count,
                                           const history_export_sn_entry_t *sns, int sn_count,
                                           const history_export_reject_entry_t *rejects, int reject_count)
{
    FILE *fp;
    int i;
    int sn_no = 0;
    uint32_t total_pcs = 0;
    uint32_t total_amount = 0;
    char mode_buf[8];
    char sort_buf[16];
    char add_buf[16];
    char work_buf[16];
    char batch_buf[24];
    char speed_buf[16];
    char curr_buf[8];
    char curr_html[64];
    char sn_html[HISTORY_EXPORT_SN_TEXT_SIZE * 6 + 1];
    char reason_html[HISTORY_EXPORT_REJECT_REASON_SIZE * 6 + 1];
    char reason_js[sizeof(reason_html) * 2 + 1];

    if (file_path == NULL || file_path[0] == '\0' || rec == NULL) {
        return false;
    }

    history_export_calc_totals(rec, denoms, denom_count, &total_pcs, &total_amount);

    fp = fopen(file_path, "w");
    if (fp == NULL) {
        return false;
    }

    history_export_get_mode_text(mode_buf, sizeof(mode_buf));
    history_export_get_sort_text(sort_buf, sizeof(sort_buf));
    history_export_get_add_text(add_buf, sizeof(add_buf));
    history_export_get_work_text(work_buf, sizeof(work_buf));
    history_export_get_speed_text(speed_buf, sizeof(speed_buf));
    lv_snprintf(batch_buf, sizeof(batch_buf), "%s", "BAT:OFF");
    lv_snprintf(curr_buf, sizeof(curr_buf), "%s", rec->currency[0] ? rec->currency : "CUR");
    if (!history_export_html_escape(curr_html, sizeof(curr_html), curr_buf)) {
        fclose(fp);
        return false;
    }

    fprintf(fp,
        "<!DOCTYPE html>\n"
        "<html lang=\"zh-CN\">\n"
        "<head>\n"
        "<meta charset=\"UTF-8\" />\n"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" />\n"
        "<title>UN260 Smart Count Report</title>\n"
        "<style>\n"
        ":root{--bg-page:#e2e8f0;--bg-container:#f8fafc;--bg-card:#ffffff;--text-main:#0f172a;--text-sub:#475569;--text-muted:#94a3b8;--border-light:#edf2f7;--indigo:#4f46e5;--indigo-bg:#e0e7ff;--emerald:#059669;--emerald-bg:#d1fae5;--amber:#d97706;--amber-bg:#ffedd5;--slate:#64748b;--slate-bg:#f1f5f9;--rose:#e11d48;--rose-bg:#ffe4e6;--shadow-card:0 10px 30px rgba(15,23,42,.06);--shadow-container:0 24px 60px rgba(15,23,42,.12);}*{box-sizing:border-box;margin:0;padding:0;}body{background:linear-gradient(180deg,#dbe4ef 0%%,#eef4f8 100%%);font-family:Inter,system-ui,-apple-system,BlinkMacSystemFont,\"Segoe UI\",sans-serif;display:flex;justify-content:center;align-items:flex-start;min-height:100vh;padding:24px 0;color:var(--text-main);} .dashboard-container{width:1280px;min-height:400px;background:rgba(248,250,252,.95);border-radius:28px;box-shadow:var(--shadow-container);overflow:hidden;border:1px solid rgba(255,255,255,.8);} .header-section{background:rgba(255,255,255,.92);padding:18px 28px 16px;border-bottom:1px solid var(--border-light);} .header-top{display:grid;grid-template-columns:230px 1fr 230px;align-items:start;gap:24px;} .header-left h1{font-size:20px;font-weight:750;letter-spacing:.2px;} .header-left .meta{font-size:12px;color:var(--text-sub);margin-top:6px;} .hero-summary{display:flex;flex-direction:column;align-items:center;justify-content:center;margin-top:-2px;} .total-inline{display:flex;align-items:baseline;justify-content:center;gap:16px;width:100%%;} .value-inline{font-size:44px;line-height:1;font-weight:800;color:var(--text-main);letter-spacing:-1px;} .label-inline{font-size:13px;line-height:1;font-weight:500;color:var(--text-sub);} .hero-summary .sub{margin-top:10px;font-size:13px;color:var(--text-sub);display:flex;flex-direction:column;gap:2px;align-items:center;} .hero-summary .sub strong{color:var(--text-main);} .settings-bar{display:flex;gap:10px;flex-wrap:wrap;margin-top:16px;justify-content:center;} .badge{padding:7px 14px;border-radius:99px;font-size:12px;font-weight:700;display:inline-flex;align-items:center;gap:7px;} .badge::before{content:'';width:7px;height:7px;border-radius:50%%;} .badge-indigo{background:var(--indigo-bg);color:var(--indigo);} .badge-indigo::before{background:var(--indigo);} .badge-emerald{background:var(--emerald-bg);color:var(--emerald);} .badge-emerald::before{background:#10b981;} .badge-amber{background:var(--amber-bg);color:var(--amber);} .badge-amber::before{background:#fb923c;} .badge-slate{background:var(--slate-bg);color:var(--slate);} .badge-slate::before{background:var(--slate);opacity:.6;} .content-section{padding:18px 28px 20px;display:grid;grid-template-columns:1.1fr 1.45fr .9fr;gap:18px;align-items:start;} .panel{background:var(--bg-card);border-radius:18px;border:1px solid rgba(15,23,42,.04);box-shadow:var(--shadow-card);overflow:hidden;display:flex;flex-direction:column;} .panel-header{padding:16px 18px 14px;display:flex;justify-content:space-between;align-items:center;border-bottom:1px solid var(--border-light);} .panel-header h2{font-size:15px;font-weight:700;} .panel-note{font-size:12px;color:var(--text-muted);} .table-wrap{padding:0 10px 8px;} table{width:100%%;border-collapse:collapse;text-align:left;} th{font-size:11px;font-weight:700;color:var(--text-muted);text-transform:uppercase;letter-spacing:.6px;padding:11px 10px;border-bottom:1px solid var(--border-light);background:#fff;position:sticky;top:0;z-index:1;} td{padding:11px 10px;font-size:13px;color:var(--text-sub);border-bottom:1px solid var(--border-light);} tr:last-child td{border-bottom:none;} .num-font{font-variant-numeric:tabular-nums;color:var(--text-main);} .text-right{text-align:right;} .panel-denom .graph-area{padding:8px 18px 16px;border-top:1px solid var(--border-light);background:linear-gradient(180deg,#ffffff 0%%,#fafcff 100%%);} .graph-title{font-size:12px;font-weight:700;color:var(--text-sub);margin-bottom:10px;display:flex;justify-content:space-between;align-items:center;} .graph-title span{font-size:11px;color:var(--text-muted);font-weight:600;} .bar-row{display:grid;grid-template-columns:48px 1fr 82px;gap:10px;align-items:center;margin:10px 0;} .bar-label{font-size:12px;font-weight:700;color:var(--text-main);} .bar-track{height:10px;background:#eef2ff;border-radius:999px;overflow:hidden;position:relative;} .bar-fill{height:100%%;border-radius:999px;background:linear-gradient(90deg,#6366f1 0%%,#8b5cf6 100%%);} .bar-fill-green{background:linear-gradient(90deg,#22c55e 0%%,#16a34a 100%%);} .bar-values{font-size:12px;font-weight:700;color:var(--text-main);display:grid;grid-template-columns:48px 52px;gap:10px;text-align:left;} .bar-values .percent-value{color:var(--text-muted);font-weight:600;position:relative;left:-10px;} .search-box{display:flex;align-items:center;gap:10px;} .search-box input{width:180px;padding:10px 12px;border-radius:12px;border:1px solid var(--border-light);background:#f8fafc;color:var(--text-main);font-size:12px;outline:none;transition:.2s ease;} .search-box input:focus{border-color:#c7d2fe;box-shadow:0 0 0 4px rgba(99,102,241,.08);background:#fff;} .search-status{padding:0 18px 10px;color:var(--text-muted);font-size:12px;} .sn-table-wrap{flex:1;overflow:auto;padding:0 10px 0;} .sn-table-wrap table{table-layout:fixed;} .sn-cell{white-space:normal;overflow-wrap:anywhere;word-break:break-word;line-height:1.35;} .highlight-row td{background:#eef2ff;} .no-match{display:none;padding:18px;text-align:center;color:var(--text-muted);font-size:13px;} .no-match.show{display:block;} .reject-empty{padding:16px 18px 14px;display:flex;flex-direction:column;gap:12px;} .reject-card{border-radius:16px;padding:16px;background:linear-gradient(135deg,#ecfdf5 0%%,#f8fafc 100%%);border:1px solid #d1fae5;} .reject-card .tag{display:inline-flex;align-items:center;gap:8px;padding:7px 12px;border-radius:999px;background:#d1fae5;color:var(--emerald);font-size:12px;font-weight:800;} .reject-card h3{font-size:18px;font-weight:800;margin-top:14px;color:#065f46;} .reject-card p{margin-top:6px;font-size:13px;line-height:1.5;color:#4b5563;} .empty-points{display:grid;grid-template-columns:1fr;gap:10px;} .empty-item{display:flex;justify-content:space-between;align-items:center;padding:10px 12px;border-radius:12px;background:#fff;border:1px solid var(--border-light);font-size:12px;} .empty-item strong{font-size:12px;color:var(--text-main);} .reject-detail{padding:14px 18px 16px;display:flex;flex-direction:column;gap:12px;} .reject-stats{display:grid;grid-template-columns:1fr 1fr;gap:10px;} .reject-stat{padding:12px 14px;border-radius:14px;background:#fff7ed;border:1px solid #fed7aa;} .reject-stat strong{display:block;font-size:12px;color:#9a3412;margin-bottom:6px;} .reject-stat span{font-size:22px;font-weight:800;color:#7c2d12;} .reject-table-wrap{border:1px solid var(--border-light);border-radius:14px;overflow:hidden;background:#fff;} .reject-table-wrap table{table-layout:fixed;} .reject-table-wrap table th,.reject-table-wrap table td{position:static;} .reject-table-wrap td:last-child{white-space:normal;overflow-wrap:anywhere;word-break:break-word;line-height:1.35;} @media (max-width:1320px){body{padding:16px}.dashboard-container{width:100%%}}\n"
        "</style>\n"
        "</head>\n"
        "<body>\n"
        "<div class=\"dashboard-container\">\n"
        "<header class=\"header-section\">\n"
        "<div class=\"header-top\">\n"
        "<div class=\"header-left\">\n"
        "<h1>UN260 Smart Count Report</h1>\n"
        "<div class=\"meta\">%04u-%02u-%02u %02u:%02u:%02u | Currency: <strong>%s</strong></div>\n"
        "</div>\n"
        "<div class=\"hero-summary\">\n"
        "<div class=\"topline total-inline\"><span class=\"value-inline\"><span class=\"counter\" data-target=\"%.0f\">0</span></span><span class=\"label-inline\">Total Amount</span></div>\n"
        "<div class=\"sub\"><span><strong><span class=\"counter\" data-target=\"%u\">0</span></strong> Notes Counted</span><span><strong><span class=\"counter\" data-target=\"%d\">0</span></strong> Reject</span></div>\n"
        "</div>\n"
        "<div class=\"header-spacer\"></div>\n"
        "</div>\n"
        "<div class=\"settings-bar\">\n"
        "<span class=\"badge badge-indigo\">%s</span>\n"
        "<span class=\"badge badge-slate\">%s</span>\n"
        "<span class=\"badge badge-emerald\">%s</span>\n"
        "<span class=\"badge badge-indigo\">%s</span>\n"
        "<span class=\"badge badge-slate\">%s</span>\n"
        "<span class=\"badge badge-amber\">%s</span>\n"
        "</div>\n"
        "</header>\n"
        "<div class=\"content-section\">\n"
        "<section class=\"panel panel-denom\">\n"
        "<div class=\"panel-header\"><h2>Denomination</h2><div class=\"panel-note\">Face value distribution</div></div>\n"
        "<div class=\"table-wrap\"><table><thead><tr><th>Denom</th><th class=\"text-right\">PCS</th><th class=\"text-right\">Amount</th></tr></thead><tbody>\n",
        rec->year, rec->month, rec->day, rec->hour, rec->minute, rec->second,
        curr_html, (double)total_amount, (unsigned)total_pcs, reject_count,
        mode_buf, sort_buf, work_buf, add_buf, batch_buf, speed_buf);

    for (i = 0; i < denom_count; i++) {
        if (denoms[i].value <= 0) {
            continue;
        }
        fprintf(fp,
                "<tr><td class=\"num-font\">%u</td><td class=\"text-right num-font\">%u</td><td class=\"text-right num-font\">%u</td></tr>\n",
                denoms[i].value, denoms[i].pcs, denoms[i].amount);
    }

    fprintf(fp,
        "</tbody></table></div>\n"
        "<div class=\"graph-area\">\n"
        "<div class=\"graph-title\"><div>Amount Distribution</div><span>Horizontal overview</span></div>\n");

    for (i = 0; i < denom_count; i++) {
        float pct = (total_amount > 0) ? ((float)denoms[i].amount * 100.0f / (float)total_amount) : 0.0f;
        if (denoms[i].value <= 0) {
            continue;
        }
        fprintf(fp,
                "<div class=\"bar-row\"><div class=\"bar-label\">%u</div><div class=\"bar-track\"><div class=\"bar-fill\" style=\"width:%.1f%%\"></div></div><div class=\"bar-values\"><span class=\"amount-value\">%u</span><span class=\"percent-value\">%.1f%%</span></div></div>\n",
                denoms[i].value, pct, denoms[i].amount, pct);
    }

    fprintf(fp, "<div class=\"graph-title\" style=\"margin-top:16px;\"><div>PCS Distribution</div><span>Count overview</span></div>\n");
    for (i = 0; i < denom_count; i++) {
        float pct = (total_pcs > 0) ? ((float)denoms[i].pcs * 100.0f / (float)total_pcs) : 0.0f;
        if (denoms[i].value <= 0) {
            continue;
        }
        fprintf(fp,
                "<div class=\"bar-row\"><div class=\"bar-label\">%u</div><div class=\"bar-track\"><div class=\"bar-fill bar-fill-green\" style=\"width:%.1f%%\"></div></div><div class=\"bar-values\"><span class=\"amount-value\">%u</span><span class=\"percent-value\">%.1f%%</span></div></div>\n",
                denoms[i].value, pct, denoms[i].pcs, pct);
    }

    fprintf(fp,
        "</div></section>\n"
        "<section class=\"panel panel-sn\">\n"
        "<div class=\"panel-header\"><h2>Serial Numbers</h2><div class=\"search-box\"><input id=\"snSearch\" type=\"text\" placeholder=\"Search Serial Number\" /></div></div>\n"
        "<div class=\"search-status\" id=\"searchStatus\">%d matches</div>\n"
        "<div class=\"sn-table-wrap\"><table><thead><tr><th>No.</th><th>Serial Number</th><th class=\"text-right\">Value</th></tr></thead><tbody id=\"snTableBody\">\n",
        sn_count > 0 ? sn_count : 0);

    if (sn_count > 0) {
        for (i = 0; i < sn_count; i++) {
            if (!history_export_html_escape(sn_html, sizeof(sn_html), sns[i].sn)) {
                fclose(fp);
                return false;
            }
            fprintf(fp,
                    "<tr data-sn=\"%s\"><td>%02u</td><td class=\"num-font sn-cell\">%s</td><td class=\"text-right num-font\">%u</td></tr>\n",
                    sn_html, sns[i].no, sn_html, sns[i].denom);
            sn_no++;
        }
    }
    if (sn_no == 0) {
        fprintf(fp, "<tr data-sn=\"NONE\"><td>--</td><td class=\"num-font sn-cell\">None</td><td class=\"text-right num-font\">--</td></tr>\n");
    }

    fprintf(fp,
        "</tbody></table><div class=\"no-match\" id=\"noMatch\">No matching serial number found.</div></div></section>\n"
        "<section class=\"panel panel-reject\"><div class=\"panel-header\"><h2>Rejected</h2><div class=\"panel-note\">Status summary</div></div><div id=\"rejectContent\"></div></section>\n"
        "</div></div>\n"
        "<script>(function(){\n"
        "const reportData={totalAmount:%.0f,totalNotes:%u,rejectCount:%d,suspectNotes:%d,damagedNotes:0,rejectDetails:[",
        (double)total_amount, (unsigned)total_pcs, reject_count, reject_count);

    for (i = 0; i < reject_count; i++) {
        if (!history_export_html_escape(reason_html, sizeof(reason_html),
                                        rejects[i].reason) ||
            !history_export_js_escape(reason_js, sizeof(reason_js), reason_html)) {
            fclose(fp);
            return false;
        }
        fprintf(fp, "%s{no:%u,pcs:%u,reason:\"%s\"}",
                i > 0 ? "," : "",
                rejects[i].no,
                rejects[i].pcs,
                reason_js);
    }

    fprintf(fp,
        "]};\n"
        "const counters=document.querySelectorAll('.counter');counters.forEach(el=>{const target=Number(el.dataset.target||0);const duration=1100;const start=performance.now();function tick(now){const progress=Math.min((now-start)/duration,1);const eased=1-Math.pow(1-progress,3);el.textContent=Math.round(target*eased).toLocaleString('en-US');if(progress<1)requestAnimationFrame(tick);}requestAnimationFrame(tick);});\n"
        "const input=document.getElementById('snSearch');const rows=Array.from(document.querySelectorAll('#snTableBody tr'));const status=document.getElementById('searchStatus');const noMatch=document.getElementById('noMatch');\n"
        "function applySearch(){const q=input.value.trim().toUpperCase();let visible=0;rows.forEach(row=>{const sn=(row.dataset.sn||'').toUpperCase();const matched=!q||sn.includes(q);row.style.display=matched?'':'none';row.classList.toggle('highlight-row',!!q&&matched);if(matched)visible++;});status.textContent=visible+(visible===1?' match':' matches');noMatch.classList.toggle('show',visible===0);} \n"
        "function syncSerialHeight(){const denomPanel=document.querySelector('.panel-denom');const snPanel=document.querySelector('.panel-sn');if(!denomPanel||!snPanel)return;const h=denomPanel.offsetHeight;snPanel.style.height=h+'px';snPanel.style.minHeight=h+'px';}\n"
        "function renderRejectSection(){const container=document.getElementById('rejectContent');if(!container)return;const hasReject=(reportData.rejectCount>0)||(reportData.rejectDetails&&reportData.rejectDetails.length>0);if(!hasReject){container.innerHTML='<div class=\"reject-empty\"><div class=\"reject-card\"><div class=\"tag\">Excellent</div><h3>No rejected notes</h3><p>All notes passed validation successfully.</p></div><div class=\"empty-points\"><div class=\"empty-item\"><span>Suspect Notes</span><strong>'+reportData.suspectNotes+'</strong></div><div class=\"empty-item\"><span>Damaged Notes</span><strong>'+reportData.damagedNotes+'</strong></div></div></div>';return;}const rowsHtml=(reportData.rejectDetails||[]).map(item=>'<tr><td>'+item.no+'</td><td class=\"num-font\">'+item.pcs+'</td><td>'+item.reason+'</td></tr>').join('');container.innerHTML='<div class=\"reject-detail\"><div class=\"reject-stats\"><div class=\"reject-stat\"><strong>Suspect Notes</strong><span>'+reportData.suspectNotes+'</span></div><div class=\"reject-stat\"><strong>Damaged Notes</strong><span>'+reportData.damagedNotes+'</span></div></div><div class=\"reject-table-wrap\"><table><thead><tr><th>No</th><th>PCS</th><th>Reason</th></tr></thead><tbody>'+rowsHtml+'</tbody></table></div></div>';}\n"
        "if(input){input.addEventListener('input',applySearch);}applySearch();renderRejectSection();window.addEventListener('load',()=>{syncSerialHeight();});window.addEventListener('resize',()=>{syncSerialHeight();});requestAnimationFrame(()=>{syncSerialHeight();});\n"
        "})();</script>\n"
        "</body></html>\n");

    return history_export_flush_and_verify(fp, file_path);
}

static bool history_export_write_selected_record(const ui_history_record_t *rec,
                                                 history_export_output_t *output)
{
    history_export_denom_entry_t *denoms = NULL;
    history_export_sn_entry_t *sns = NULL;
    history_export_reject_entry_t *rejects = NULL;
    int denom_count = 0;
    int sn_count = 0;
    int reject_count = 0;
    char export_name[96] = {0};
    char csv_tmp_path[UI_HISTORY_EXPORT_PATH_SIZE + 5U] = {0};
    char html_tmp_path[UI_HISTORY_EXPORT_PATH_SIZE + 5U] = {0};
    int written;
    bool ok = false;

    if (rec == NULL || !rec->valid || output == NULL) {
        return false;
    }
    memset(output, 0, sizeof(*output));

    if (!history_export_parse_denom_entries(rec, &denoms, &denom_count) ||
        !history_export_parse_sn_entries(rec, &sns, &sn_count) ||
        !history_export_parse_reject_entries(rec, &rejects, &reject_count)) {
        goto cleanup;
    }

    history_export_build_name_for_record(export_name, sizeof(export_name), rec);
    if (!usb_storage_make_unique_file_pair(export_name,
                                           ".csv", output->csv_path,
                                           sizeof(output->csv_path),
                                           ".html", output->html_path,
                                           sizeof(output->html_path))) {
        goto cleanup;
    }
    written = lv_snprintf(csv_tmp_path, sizeof(csv_tmp_path), "%s.tmp",
                          output->csv_path);
    if (written < 0 || (size_t)written >= sizeof(csv_tmp_path)) {
        goto cleanup;
    }
    written = lv_snprintf(html_tmp_path, sizeof(html_tmp_path), "%s.tmp",
                          output->html_path);
    if (written < 0 || (size_t)written >= sizeof(html_tmp_path)) {
        goto cleanup;
    }

    if (!history_export_write_csv_file(csv_tmp_path, rec, denoms, denom_count,
                                       sns, sn_count, rejects, reject_count) ||
        !history_export_write_html_file(html_tmp_path, rec, denoms, denom_count,
                                        sns, sn_count, rejects, reject_count)) {
        goto cleanup;
    }
    if (!usb_storage_commit_file_pair(csv_tmp_path, output->csv_path,
                                      html_tmp_path, output->html_path)) {
        goto cleanup;
    }
    ok = true;

cleanup:
    if (csv_tmp_path[0] != '\0') {
        unlink(csv_tmp_path);
    }
    if (html_tmp_path[0] != '\0') {
        unlink(html_tmp_path);
    }
    free(denoms);
    free(sns);
    free(rejects);
    if (!ok) memset(output, 0, sizeof(*output));
    return ok;
}

static void history_export_rollback_outputs(history_export_output_t *outputs,
                                            size_t output_count)
{
    size_t i;

    if (outputs == NULL) return;

    for (i = 0; i < output_count; i++) {
        if (outputs[i].csv_path[0] != '\0') {
            (void)unlink(outputs[i].csv_path);
        }
        if (outputs[i].html_path[0] != '\0') {
            (void)unlink(outputs[i].html_path);
        }
    }
}

bool ui_history_export_data_request(void)
{
    const ui_history_store_t *store;
    history_export_output_t *outputs = NULL;
    size_t output_count = 0U;
    int i;
    int selected_count = 0;

    if (g_history_export_lock) {
        history_export_show_toast(UI_HISTORY_EXPORT_TEXT_EXPORTING, false);
        return false;
    }

    if (!usb_storage_prepare()) {
        history_export_show_toast(UI_HISTORY_EXPORT_TEXT_FAILED, true);
        return false;
    }

    store = ui_history_data_get();
    if (store == NULL || store->record_count == 0) {
        history_export_show_toast(UI_HISTORY_EXPORT_TEXT_COUNT_FIRST, true);
        return false;
    }
    if (store->record_count > UI_HISTORY_MAX_RECORDS) {
        history_export_show_toast(UI_HISTORY_EXPORT_TEXT_FAILED, true);
        return false;
    }

    for (i = 0; i < store->record_count; i++) {
        if (store->records[i].selected) {
            selected_count++;
        }
    }
    if (selected_count <= 0) {
        history_export_show_toast(UI_HISTORY_EXPORT_TEXT_COUNT_FIRST, true);
        return false;
    }

    outputs = calloc((size_t)selected_count, sizeof(*outputs));
    if (outputs == NULL) {
        history_export_show_toast(UI_HISTORY_EXPORT_TEXT_FAILED, true);
        return false;
    }

    history_export_start_lock();
    history_export_show_toast(UI_HISTORY_EXPORT_TEXT_EXPORTING, false);

    for (i = 0; i < store->record_count; i++) {
        if (!store->records[i].selected) {
            continue;
        }
        if (output_count >= (size_t)selected_count ||
            !history_export_write_selected_record(&store->records[i],
                                                  &outputs[output_count])) {
            history_export_rollback_outputs(outputs, output_count);
            free(outputs);
            history_export_show_toast(UI_HISTORY_EXPORT_TEXT_FAILED, true);
            return false;
        }
        output_count++;
    }

    free(outputs);
    return true;
}
