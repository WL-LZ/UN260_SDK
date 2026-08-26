#include "ui_export_data.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include "un260/lv_components/lv_print_toast.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/lv_system/machine_time.h"
#include "un260/machine_state/machine_state.h"
#include "un260/currency/currency_state.h"
#include "un260/storage/usb_storage.h"
#include "un260/counting/counting_data_store.h"
#include "un260/counting/counting_reject_reason.h"

#define UI_EXPORT_LOCK_MS                  2000U
#define UI_EXPORT_TOAST_TEXT_EXPORTING     "Exporting..."
#define UI_EXPORT_TOAST_TEXT_COUNT_FIRST   "Please Count First"
#define UI_EXPORT_TOAST_TEXT_EXPORT_FAILED "Export Failed"

static bool g_ui_export_data_lock = false;
static lv_timer_t *g_ui_export_data_unlock_timer = NULL;

static void ui_export_data_show_alarm_toast(const char *text)
{
    lv_print_toast_config_t toast_cfg = lv_print_toast_get_default_config();

    toast_cfg.w = 320;
    toast_cfg.h = 101;
    toast_cfg.text = text ? text : UI_EXPORT_TOAST_TEXT_COUNT_FIRST;
    toast_cfg.show_loader = true;
    toast_cfg.align_center = true;
    toast_cfg.use_text_area = false;
    toast_cfg.loader_color = lv_color_hex(0xC0392B);
    toast_cfg.auto_hide_ms = UI_EXPORT_LOCK_MS;

    lv_print_toast_show_with_config(&toast_cfg);
}

static void ui_export_data_show_normal_toast(const char *text)
{
    lv_print_toast_config_t toast_cfg = lv_print_toast_get_default_config();

    toast_cfg.w = 320;
    toast_cfg.h = 101;
    toast_cfg.text = text ? text : UI_EXPORT_TOAST_TEXT_EXPORTING;
    toast_cfg.show_loader = true;
    toast_cfg.align_center = true;
    toast_cfg.use_text_area = false;
    toast_cfg.loader_color = LV_PRINT_TOAST_DEFAULT_LOADER_COLOR;
    toast_cfg.auto_hide_ms = UI_EXPORT_LOCK_MS;

    lv_print_toast_show_with_config(&toast_cfg);
}

static int ui_export_data_get_reject_count(void)
{
    return counting_data_reject_pcs_count(counting_data_current());
}

static bool ui_export_data_is_empty(void)
{
    return (counting_data_current()->total_pcs == 0 &&
            counting_data_current()->total_amount <= 0.0f &&
            ui_export_data_get_reject_count() == 0);
}

static void ui_export_data_get_currency_code(char *buf, size_t size)
{
    char curr_code[4];
    char selected_code[4];
    uint8_t selected_index;

    if (buf == NULL || size == 0U) {
        return;
    }

    currency_state_get_active_code(curr_code);
    selected_index = currency_state_active_index();
    if (curr_code[0] != '\0') {
        lv_snprintf(buf, size, "%s", curr_code);
    } else if (currency_state_get_code(selected_index, selected_code) && selected_code[0] != '\0') {
        lv_snprintf(buf, size, "%s", selected_code);
    } else {
        lv_snprintf(buf, size, "%s", "CUR");
    }
}

static void ui_export_data_sanitize_token(char *dst, size_t dst_size, const char *src)
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

static void ui_export_data_build_export_name(char *buf, size_t size)
{
    char curr_raw[8] = {0};
    char curr[8] = {0};
    machine_time_value_t now;

    if (buf == NULL || size == 0U) {
        return;
    }

    ui_export_data_get_currency_code(curr_raw, sizeof(curr_raw));
    ui_export_data_sanitize_token(curr, sizeof(curr), curr_raw);
    machine_time_get(&now);

    lv_snprintf(buf, size, "%s_%04u-%02u-%02u_%02u-%02u-%02u",
                curr,
                (unsigned)now.year,
                (unsigned)now.month,
                (unsigned)now.day,
                (unsigned)now.hour,
                (unsigned)now.minute,
                (unsigned)now.second);
}

static void ui_export_data_get_sort_text(char *buf, size_t size)
{
    if (buf == NULL || size == 0U) {
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

static void ui_export_data_get_add_text(char *buf, size_t size)
{
    if (buf == NULL || size == 0U) {
        return;
    }

    lv_snprintf(buf, size, "%s", machine_state_add_enabled() ? "ADD:ON" : "ADD:OFF");
}

static void ui_export_data_get_work_text(char *buf, size_t size)
{
    if (buf == NULL || size == 0U) {
        return;
    }

    lv_snprintf(buf, size, "%s", machine_state_work_mode() ? "MANUAL" : "AUTO");
}

static void ui_export_data_get_speed_text(char *buf, size_t size)
{
    if (buf == NULL || size == 0U) {
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

static bool ui_export_data_flush_and_verify(FILE *fp, const char *file_path)
{
    struct stat st;

    if (fp == NULL || file_path == NULL || file_path[0] == '\0') {
        return false;
    }

    if (fflush(fp) != 0) {
        fclose(fp);
        return false;
    }

    if (ferror(fp) != 0) {
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

static void ui_export_data_escape_js_str(char *dst, size_t dst_size, const char *src)
{
    size_t i;
    size_t j = 0;

    if (dst == NULL || dst_size == 0U) {
        return;
    }

    dst[0] = '\0';
    if (src == NULL) {
        return;
    }

    for (i = 0; src[i] != '\0' && j + 1 < dst_size; i++) {
        char ch = src[i];

        if (ch == '\\' || ch == '"') {
            if (j + 2 >= dst_size) {
                break;
            }
            dst[j++] = '\\';
            dst[j++] = ch;
        } else if (ch == '\n' || ch == '\r') {
            dst[j++] = ' ';
        } else {
            dst[j++] = ch;
        }
    }

    dst[j] = '\0';
}

static bool ui_export_data_write_html_file(const char *file_path)
{
    FILE *fp;
    int i;
    int sn_no = 0;
    int reject_no = 0;
    int total_pcs = counting_data_current()->total_pcs > 0 ? counting_data_current()->total_pcs : 0;
    int reject_total = ui_export_data_get_reject_count();
    float total_amount = counting_data_current()->total_amount > 0.0f ? counting_data_current()->total_amount : 0.0f;
    char mode_buf[8] = "NONE";
    char sort_buf[16] = "SORT:OFF";
    char add_buf[16] = "ADD:OFF";
    char work_buf[16] = "AUTO";
    char batch_buf[24] = "BAT:OFF";
    char speed_buf[16] = "SPD:HIGH";
    char curr_buf[8] = "CUR";
    char reason_buf[96];
    char reason_js[160];
    machine_time_value_t now;

    if (file_path == NULL || file_path[0] == '\0') {
        return false;
    }

    if (machine_state_mode() == MODE_MDC) {
        lv_snprintf(mode_buf, sizeof(mode_buf), "%s", "MDC");
    } else if (machine_state_mode() == MODE_SDC) {
        lv_snprintf(mode_buf, sizeof(mode_buf), "%s", "SDC");
    } else if (machine_state_mode() == MODE_CNT) {
        lv_snprintf(mode_buf, sizeof(mode_buf), "%s", "CNT");
    }

    ui_export_data_get_sort_text(sort_buf, sizeof(sort_buf));
    ui_export_data_get_add_text(add_buf, sizeof(add_buf));
    ui_export_data_get_work_text(work_buf, sizeof(work_buf));
    ui_export_data_get_speed_text(speed_buf, sizeof(speed_buf));
    if (machine_state_batch_enabled()) {
        lv_snprintf(batch_buf, sizeof(batch_buf), "BAT:%d", (int)machine_state_batch_num());
    } else {
        lv_snprintf(batch_buf, sizeof(batch_buf), "%s", "BAT:OFF");
    }
    ui_export_data_get_currency_code(curr_buf, sizeof(curr_buf));
    machine_time_get(&now);

    fp = fopen(file_path, "w");
    if (fp == NULL) {
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
        ":root{--bg-page:#e2e8f0;--bg-container:#f8fafc;--bg-card:#ffffff;--text-main:#0f172a;--text-sub:#475569;--text-muted:#94a3b8;--border-light:#edf2f7;--indigo:#4f46e5;--indigo-bg:#e0e7ff;--emerald:#059669;--emerald-bg:#d1fae5;--amber:#d97706;--amber-bg:#ffedd5;--slate:#64748b;--slate-bg:#f1f5f9;--rose:#e11d48;--rose-bg:#ffe4e6;--shadow-card:0 10px 30px rgba(15,23,42,.06);--shadow-container:0 24px 60px rgba(15,23,42,.12);}*{box-sizing:border-box;margin:0;padding:0;}body{background:linear-gradient(180deg,#dbe4ef 0%%,#eef4f8 100%%);font-family:Inter,system-ui,-apple-system,BlinkMacSystemFont,\"Segoe UI\",sans-serif;display:flex;justify-content:center;align-items:flex-start;min-height:100vh;padding:24px 0;color:var(--text-main);} .dashboard-container{width:1280px;min-height:400px;background:rgba(248,250,252,.95);border-radius:28px;box-shadow:var(--shadow-container);overflow:hidden;border:1px solid rgba(255,255,255,.8);} .header-section{background:rgba(255,255,255,.92);padding:18px 28px 16px;border-bottom:1px solid var(--border-light);} .header-top{display:grid;grid-template-columns:230px 1fr 230px;align-items:start;gap:24px;} .header-left h1{font-size:20px;font-weight:750;letter-spacing:.2px;} .header-left .meta{font-size:12px;color:var(--text-sub);margin-top:6px;} .hero-summary{display:flex;flex-direction:column;align-items:center;justify-content:center;margin-top:-2px;} .total-inline{display:flex;align-items:baseline;justify-content:center;gap:16px;width:100%%;} .value-inline{font-size:44px;line-height:1;font-weight:800;color:var(--text-main);letter-spacing:-1px;} .label-inline{font-size:13px;line-height:1;font-weight:500;color:var(--text-sub);} .hero-summary .sub{margin-top:10px;font-size:13px;color:var(--text-sub);display:flex;flex-direction:column;gap:2px;align-items:center;} .hero-summary .sub strong{color:var(--text-main);} .settings-bar{display:flex;gap:10px;flex-wrap:wrap;margin-top:16px;justify-content:center;} .badge{padding:7px 14px;border-radius:99px;font-size:12px;font-weight:700;display:inline-flex;align-items:center;gap:7px;} .badge::before{content:'';width:7px;height:7px;border-radius:50%%;} .badge-indigo{background:var(--indigo-bg);color:var(--indigo);} .badge-indigo::before{background:var(--indigo);} .badge-emerald{background:var(--emerald-bg);color:var(--emerald);} .badge-emerald::before{background:#10b981;} .badge-amber{background:var(--amber-bg);color:var(--amber);} .badge-amber::before{background:#fb923c;} .badge-slate{background:var(--slate-bg);color:var(--slate);} .badge-slate::before{background:var(--slate);opacity:.6;} .content-section{padding:18px 28px 20px;display:grid;grid-template-columns:1.1fr 1.45fr .9fr;gap:18px;align-items:start;} .panel{background:var(--bg-card);border-radius:18px;border:1px solid rgba(15,23,42,.04);box-shadow:var(--shadow-card);overflow:hidden;display:flex;flex-direction:column;} .panel-header{padding:16px 18px 14px;display:flex;justify-content:space-between;align-items:center;border-bottom:1px solid var(--border-light);} .panel-header h2{font-size:15px;font-weight:700;} .panel-note{font-size:12px;color:var(--text-muted);} .table-wrap{padding:0 10px 8px;} table{width:100%%;border-collapse:collapse;text-align:left;} th{font-size:11px;font-weight:700;color:var(--text-muted);text-transform:uppercase;letter-spacing:.6px;padding:11px 10px;border-bottom:1px solid var(--border-light);background:#fff;position:sticky;top:0;z-index:1;} td{padding:11px 10px;font-size:13px;color:var(--text-sub);border-bottom:1px solid var(--border-light);} tr:last-child td{border-bottom:none;} .num-font{font-variant-numeric:tabular-nums;color:var(--text-main);} .text-right{text-align:right;} .panel-denom .graph-area{padding:8px 18px 16px;border-top:1px solid var(--border-light);background:linear-gradient(180deg,#ffffff 0%%,#fafcff 100%%);} .graph-title{font-size:12px;font-weight:700;color:var(--text-sub);margin-bottom:10px;display:flex;justify-content:space-between;align-items:center;} .graph-title span{font-size:11px;color:var(--text-muted);font-weight:600;} .bar-row{display:grid;grid-template-columns:48px 1fr 82px;gap:10px;align-items:center;margin:10px 0;} .bar-label{font-size:12px;font-weight:700;color:var(--text-main);} .bar-track{height:10px;background:#eef2ff;border-radius:999px;overflow:hidden;position:relative;} .bar-fill{height:100%%;border-radius:999px;background:linear-gradient(90deg,#6366f1 0%%,#8b5cf6 100%%);} .bar-fill-green{background:linear-gradient(90deg,#22c55e 0%%,#16a34a 100%%);} .bar-values{font-size:12px;font-weight:700;color:var(--text-main);display:grid;grid-template-columns:48px 52px;gap:10px;text-align:left;} .bar-values .percent-value{color:var(--text-muted);font-weight:600;position:relative;left:-10px;} .search-box{display:flex;align-items:center;gap:10px;} .search-box input{width:180px;padding:10px 12px;border-radius:12px;border:1px solid var(--border-light);background:#f8fafc;color:var(--text-main);font-size:12px;outline:none;transition:.2s ease;} .search-box input:focus{border-color:#c7d2fe;box-shadow:0 0 0 4px rgba(99,102,241,.08);background:#fff;} .search-status{padding:0 18px 10px;color:var(--text-muted);font-size:12px;} .sn-table-wrap{flex:1;overflow:auto;padding:0 10px 0;} .highlight-row td{background:#eef2ff;} .no-match{display:none;padding:18px;text-align:center;color:var(--text-muted);font-size:13px;} .no-match.show{display:block;} .reject-empty{padding:16px 18px 14px;display:flex;flex-direction:column;gap:12px;} .reject-card{border-radius:16px;padding:16px;background:linear-gradient(135deg,#ecfdf5 0%%,#f8fafc 100%%);border:1px solid #d1fae5;} .reject-card .tag{display:inline-flex;align-items:center;gap:8px;padding:7px 12px;border-radius:999px;background:#d1fae5;color:var(--emerald);font-size:12px;font-weight:800;} .reject-card h3{font-size:18px;font-weight:800;margin-top:14px;color:#065f46;} .reject-card p{margin-top:6px;font-size:13px;line-height:1.5;color:#4b5563;} .empty-points{display:grid;grid-template-columns:1fr;gap:10px;} .empty-item{display:flex;justify-content:space-between;align-items:center;padding:10px 12px;border-radius:12px;background:#fff;border:1px solid var(--border-light);font-size:12px;} .empty-item strong{font-size:12px;color:var(--text-main);} .reject-detail{padding:14px 18px 16px;display:flex;flex-direction:column;gap:12px;} .reject-stats{display:grid;grid-template-columns:1fr 1fr;gap:10px;} .reject-stat{padding:12px 14px;border-radius:14px;background:#fff7ed;border:1px solid #fed7aa;} .reject-stat strong{display:block;font-size:12px;color:#9a3412;margin-bottom:6px;} .reject-stat span{font-size:22px;font-weight:800;color:#7c2d12;} .reject-table-wrap{border:1px solid var(--border-light);border-radius:14px;overflow:hidden;background:#fff;} .reject-table-wrap table th,.reject-table-wrap table td{position:static;} @media (max-width:1320px){body{padding:16px}.dashboard-container{width:100%%}}\n"
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
        "<div class=\"sub\"><span><strong><span class=\"counter\" data-target=\"%d\">0</span></strong> Notes Counted</span><span><strong><span class=\"counter\" data-target=\"%d\">0</span></strong> Reject</span></div>\n"
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
        (unsigned)now.year, (unsigned)now.month, (unsigned)now.day,
        (unsigned)now.hour, (unsigned)now.minute, (unsigned)now.second,
        curr_buf, total_amount, total_pcs, reject_total,
        mode_buf, sort_buf, work_buf, add_buf, batch_buf, speed_buf);

    for (i = 0; i < counting_data_current()->denom_number && i < (int)(sizeof(counting_data_current()->denom) / sizeof(counting_data_current()->denom[0])); i++) {
        if (counting_data_current()->denom[i].value <= 0) {
            continue;
        }
        fprintf(fp,
                "<tr><td class=\"num-font\">%d</td><td class=\"text-right num-font\">%u</td><td class=\"text-right num-font\">%.0f</td></tr>\n",
                counting_data_current()->denom[i].value, (unsigned)counting_data_current()->denom[i].pcs, counting_data_current()->denom[i].amount);
    }

    fprintf(fp,
        "</tbody></table></div>\n"
        "<div class=\"graph-area\">\n"
        "<div class=\"graph-title\"><div>Amount Distribution</div><span>Horizontal overview</span></div>\n");

    for (i = 0; i < counting_data_current()->denom_number && i < (int)(sizeof(counting_data_current()->denom) / sizeof(counting_data_current()->denom[0])); i++) {
        float amount = counting_data_current()->denom[i].amount;
        float pct = (total_amount > 0.0f) ? (amount * 100.0f / total_amount) : 0.0f;

        if (counting_data_current()->denom[i].value <= 0) {
            continue;
        }

        fprintf(fp,
                "<div class=\"bar-row\"><div class=\"bar-label\">%d</div><div class=\"bar-track\"><div class=\"bar-fill\" style=\"width:%.1f%%\"></div></div><div class=\"bar-values\"><span class=\"amount-value\">%.0f</span><span class=\"percent-value\">%.1f%%</span></div></div>\n",
                counting_data_current()->denom[i].value, pct, amount, pct);
    }

    fprintf(fp, "<div class=\"graph-title\" style=\"margin-top:16px;\"><div>PCS Distribution</div><span>Count overview</span></div>\n");

    for (i = 0; i < counting_data_current()->denom_number && i < (int)(sizeof(counting_data_current()->denom) / sizeof(counting_data_current()->denom[0])); i++) {
        float pcs = (float)counting_data_current()->denom[i].pcs;
        float pct = (total_pcs > 0) ? (pcs * 100.0f / (float)total_pcs) : 0.0f;

        if (counting_data_current()->denom[i].value <= 0) {
            continue;
        }

        fprintf(fp,
                "<div class=\"bar-row\"><div class=\"bar-label\">%d</div><div class=\"bar-track\"><div class=\"bar-fill bar-fill-green\" style=\"width:%.1f%%\"></div></div><div class=\"bar-values\"><span class=\"amount-value\">%u</span><span class=\"percent-value\">%.1f%%</span></div></div>\n",
                counting_data_current()->denom[i].value, pct, (unsigned)counting_data_current()->denom[i].pcs, pct);
    }

    fprintf(fp,
        "</div></section>\n"
        "<section class=\"panel panel-sn\">\n"
        "<div class=\"panel-header\"><h2>Serial Numbers</h2><div class=\"search-box\"><input id=\"snSearch\" type=\"text\" placeholder=\"Search Serial Number\" /></div></div>\n"
        "<div class=\"search-status\" id=\"searchStatus\">%d matches</div>\n"
        "<div class=\"sn-table-wrap\"><table><thead><tr><th>No.</th><th>Serial Number</th><th class=\"text-right\">Value</th></tr></thead><tbody id=\"snTableBody\">\n",
        total_pcs);

    {
        int sn_limit = counting_data_serial_scan_limit(counting_data_current());

        for (i = 0; i < sn_limit; i++) {
            if (counting_data_current()->sn_str[i] == NULL || counting_data_current()->sn_str[i][0] == '\0') {
                continue;
            }
            if (counting_data_current()->denom_mix[i] <= 0) {
                continue;
            }
            sn_no++;
            fprintf(fp,
                    "<tr data-sn=\"%s\"><td>%02d</td><td class=\"num-font sn-cell\">%s</td><td class=\"text-right num-font\">%d</td></tr>\n",
                    counting_data_current()->sn_str[i], sn_no, counting_data_current()->sn_str[i], counting_data_current()->denom_mix[i]);
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
        "const reportData={totalAmount:%.0f,totalNotes:%d,rejectCount:%d,suspectNotes:%d,damagedNotes:0,rejectDetails:[",
        total_amount, total_pcs, reject_total, reject_total);

    {
        int error_count = counting_data_error_detail_count(counting_data_current());

        for (i = 0; i < error_count; i++) {
            unsigned int pcs = 0;

            lv_snprintf(reason_buf, sizeof(reason_buf), "%s",
                        counting_reject_reason_get(counting_data_current()->err_code[i]));
            if (strcmp(reason_buf, "Size Unknow") == 0) {
                lv_snprintf(reason_buf, sizeof(reason_buf), "%s", "Size Unknown");
            } else if (strcmp(reason_buf, "Ort Unknow") == 0) {
                lv_snprintf(reason_buf, sizeof(reason_buf), "%s", "Ort Unknown");
            } else if (strcmp(reason_buf, "Version Unknow") == 0) {
                lv_snprintf(reason_buf, sizeof(reason_buf), "%s", "Version Unknown");
            }
            ui_export_data_escape_js_str(reason_js, sizeof(reason_js), reason_buf);

            if (counting_data_current()->err_pcs != NULL) {
                pcs = (unsigned int)counting_data_current()->err_pcs[i];
            }

            reject_no++;
            fprintf(fp, "%s{no:%d,pcs:%u,reason:\"%s\"}",
                    reject_no > 1 ? "," : "", reject_no, pcs, reason_js);
        }
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

    return ui_export_data_flush_and_verify(fp, file_path);
}

static bool ui_export_data_write_csv_file(const char *file_path)
{
    FILE *fp;
    int i;
    int no = 0;
    bool has_sn = false;
    bool has_error = false;
    char mode_buf[8] = "NONE";
    char reason_buf[96];
    machine_time_value_t now;
    char curr_code[4];

    if (machine_state_mode() == MODE_MDC) {
        lv_snprintf(mode_buf, sizeof(mode_buf), "%s", "MDC");
    } else if (machine_state_mode() == MODE_SDC) {
        lv_snprintf(mode_buf, sizeof(mode_buf), "%s", "SDC");
    } else if (machine_state_mode() == MODE_CNT) {
        lv_snprintf(mode_buf, sizeof(mode_buf), "%s", "CNT");
    }

    if (file_path == NULL || file_path[0] == '\0') {
        return false;
    }

    machine_time_get(&now);
    currency_state_get_active_code(curr_code);

    fp = fopen(file_path, "w");
    if (fp == NULL) {
        return false;
    }

    fprintf(fp, "Un260 Intelligent Cash Counter Report\n");
    fprintf(fp, "Machine Mode,%s\n", mode_buf);
    fprintf(fp, "Export Time,%02u:%02u:%02u\n",
            (unsigned)now.hour,
            (unsigned)now.minute,
            (unsigned)now.second);
    fprintf(fp, "Currency,%s\n", curr_code);
    fprintf(fp, "Total Pcs,%d\n", counting_data_current()->total_pcs);
    fprintf(fp, "Total Amount,%.0f\n", counting_data_current()->total_amount);
    fprintf(fp, "Reject Pcs,%d\n\n", ui_export_data_get_reject_count());

    fprintf(fp, "DENOMINATION SUMMARY\n");
    fprintf(fp, "DENOM,PCS,AMOUNT\n");
    for (i = 0; i < counting_data_current()->denom_number && i < (int)(sizeof(counting_data_current()->denom) / sizeof(counting_data_current()->denom[0])); i++) {
        if (counting_data_current()->denom[i].value <= 0) {
            continue;
        }

        fprintf(fp, "%d,%u,%.0f\n",
                counting_data_current()->denom[i].value,
                (unsigned)counting_data_current()->denom[i].pcs,
                counting_data_current()->denom[i].amount);
    }

    fprintf(fp, "\nSERIAL NUMBER LIST\n");
    fprintf(fp, "NO,SN,DENOM\n");
    {
        int sn_limit = counting_data_serial_scan_limit(counting_data_current());

        for (i = 0; i < sn_limit; i++) {
            if (counting_data_current()->sn_str[i] == NULL || counting_data_current()->sn_str[i][0] == '\0') {
                continue;
            }
            if (counting_data_current()->denom_mix[i] <= 0) {
                continue;
            }

            no++;
            fprintf(fp, "%d,%s,%d\n", no, counting_data_current()->sn_str[i], counting_data_current()->denom_mix[i]);
            has_sn = true;
        }
    }
    if (!has_sn) {
        fprintf(fp, "None\n");
    }

    fprintf(fp, "\nREJECT REPORT\n");
    fprintf(fp, "NO,PCS,REASON\n");
    {
        int error_count = counting_data_error_detail_count(counting_data_current());

        no = 0;
        for (i = 0; i < error_count; i++) {
            no++;
            lv_snprintf(reason_buf, sizeof(reason_buf), "%s",
                        counting_reject_reason_get(counting_data_current()->err_code[i]));
            if (strcmp(reason_buf, "Size Unknow") == 0) {
                lv_snprintf(reason_buf, sizeof(reason_buf), "%s", "Size Unknown");
            } else if (strcmp(reason_buf, "Ort Unknow") == 0) {
                lv_snprintf(reason_buf, sizeof(reason_buf), "%s", "Ort Unknown");
            } else if (strcmp(reason_buf, "Version Unknow") == 0) {
                lv_snprintf(reason_buf, sizeof(reason_buf), "%s", "Version Unknown");
            }

            if (counting_data_current()->err_pcs != NULL && counting_data_current()->err_pcs[i] > 0) {
                fprintf(fp, "%d,%u,%s\n", no, (unsigned)counting_data_current()->err_pcs[i], reason_buf);
            } else {
                fprintf(fp, "%d,0,%s\n", no, reason_buf);
            }
            has_error = true;
        }
    }
    if (!has_error) {
        fprintf(fp, "None\n");
    }

    return ui_export_data_flush_and_verify(fp, file_path);
}

static void ui_export_data_unlock_timer_cb(lv_timer_t *timer)
{
    if (timer == g_ui_export_data_unlock_timer) {
        g_ui_export_data_unlock_timer = NULL;
    }
    g_ui_export_data_lock = false;
}

static void ui_export_data_start_lock(void)
{
    if (g_ui_export_data_unlock_timer) {
        lv_timer_del(g_ui_export_data_unlock_timer);
        g_ui_export_data_unlock_timer = NULL;
    }

    g_ui_export_data_lock = true;
    g_ui_export_data_unlock_timer = lv_timer_create(ui_export_data_unlock_timer_cb, UI_EXPORT_LOCK_MS, NULL);
    if (g_ui_export_data_unlock_timer) {
        lv_timer_set_repeat_count(g_ui_export_data_unlock_timer, 1);
    } else {
        g_ui_export_data_lock = false;
    }
}

bool ui_export_data_request(void)
{
    char export_name[96] = {0};
    char csv_path[256] = {0};
    char html_path[256] = {0};
    char csv_tmp_path[sizeof(csv_path) + 5] = {0};
    char html_tmp_path[sizeof(html_path) + 5] = {0};
    int written;
    bool ok = false;

    if (g_ui_export_data_lock) {
        ui_export_data_show_normal_toast(UI_EXPORT_TOAST_TEXT_EXPORTING);
        return false;
    }

    if (ui_export_data_is_empty()) {
        ui_export_data_show_alarm_toast(UI_EXPORT_TOAST_TEXT_COUNT_FIRST);
        return false;
    }

    if (!usb_storage_prepare()) {
        ui_export_data_show_alarm_toast(UI_EXPORT_TOAST_TEXT_EXPORT_FAILED);
        return false;
    }

    ui_export_data_start_lock();
    ui_export_data_show_normal_toast(UI_EXPORT_TOAST_TEXT_EXPORTING);

    ui_export_data_build_export_name(export_name, sizeof(export_name));
    if (!usb_storage_make_unique_file_pair(export_name,
                                           ".csv", csv_path, sizeof(csv_path),
                                           ".html", html_path, sizeof(html_path))) {
        goto cleanup;
    }
    written = lv_snprintf(csv_tmp_path, sizeof(csv_tmp_path), "%s.tmp", csv_path);
    if (written < 0 || (size_t)written >= sizeof(csv_tmp_path)) {
        goto cleanup;
    }
    written = lv_snprintf(html_tmp_path, sizeof(html_tmp_path), "%s.tmp", html_path);
    if (written < 0 || (size_t)written >= sizeof(html_tmp_path)) {
        goto cleanup;
    }

    if (!ui_export_data_write_csv_file(csv_tmp_path) ||
        !ui_export_data_write_html_file(html_tmp_path)) {
        goto cleanup;
    }
    if (!usb_storage_commit_file_pair(csv_tmp_path, csv_path,
                                      html_tmp_path, html_path)) {
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
    if (!ok) {
        ui_export_data_show_alarm_toast(UI_EXPORT_TOAST_TEXT_EXPORT_FAILED);
    }
    return ok;
}

ui_export_text_result_t ui_export_text_lines(const char *file_prefix,
                                             const char *const *lines,
                                             size_t line_count)
{
    char safe_prefix[48];
    char timestamp[32];
    char file_path[256];
    struct tm local_tm;
    time_t now;
    FILE *fp;
    size_t prefix_pos = 0;
    int suffix = 0;
    int written;

    if (lines == NULL || line_count == 0U) {
        return UI_EXPORT_TEXT_EMPTY;
    }

    if (!usb_storage_prepare()) {
        return UI_EXPORT_TEXT_USB_NOT_READY;
    }

    if (file_prefix != NULL) {
        for (size_t i = 0; file_prefix[i] != '\0' &&
             prefix_pos + 1U < sizeof(safe_prefix); i++) {
            unsigned char ch = (unsigned char)file_prefix[i];
            if (isalnum(ch) || ch == '_' || ch == '-') {
                safe_prefix[prefix_pos++] = (char)ch;
            }
        }
    }
    if (prefix_pos == 0U) {
        lv_snprintf(safe_prefix, sizeof(safe_prefix), "%s", "text");
    } else {
        safe_prefix[prefix_pos] = '\0';
    }

    now = time(NULL);
    if (localtime_r(&now, &local_tm) == NULL ||
        strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &local_tm) == 0U) {
        return UI_EXPORT_TEXT_FAILED;
    }

    for (suffix = 0; suffix <= 99; suffix++) {
        if (suffix == 0) {
            written = lv_snprintf(file_path, sizeof(file_path), "%s/%s_%s.txt",
                                  USB_STORAGE_MOUNT_POINT, safe_prefix, timestamp);
        } else {
            written = lv_snprintf(file_path, sizeof(file_path), "%s/%s_%s_%02d.txt",
                                  USB_STORAGE_MOUNT_POINT, safe_prefix, timestamp, suffix);
        }
        if (written < 0 || (size_t)written >= sizeof(file_path)) {
            return UI_EXPORT_TEXT_FAILED;
        }
        if (access(file_path, F_OK) != 0) {
            break;
        }
    }

    if (suffix > 99) {
        return UI_EXPORT_TEXT_FAILED;
    }

    fp = fopen(file_path, "w");
    if (fp == NULL) {
        return UI_EXPORT_TEXT_FAILED;
    }

    for (size_t i = 0; i < line_count; i++) {
        if (lines[i] != NULL && fprintf(fp, "%s\n", lines[i]) < 0) {
            fclose(fp);
            unlink(file_path);
            return UI_EXPORT_TEXT_FAILED;
        }
    }

    if (!ui_export_data_flush_and_verify(fp, file_path)) {
        unlink(file_path);
        return UI_EXPORT_TEXT_FAILED;
    }

    return UI_EXPORT_TEXT_OK;
}
