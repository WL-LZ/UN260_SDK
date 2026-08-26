#include "ui_history_data.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "un260/lv_system/machine_time.h"
#include "un260/currency/currency_state.h"

#define UI_HISTORY_STORE_DIR         "/etc/ui_state/count_history"
#define UI_HISTORY_INDEX_PATH        "/etc/ui_state/count_history/index.cfg"
#define UI_HISTORY_META_PATH         "/etc/ui_state/count_history/meta.cfg"
#define UI_HISTORY_SLOT_PATH_FMT     "/etc/ui_state/count_history/%02u.rec"
#define UI_HISTORY_MAGIC             0x48495354u
#define UI_HISTORY_VERSION           2u
#define UI_HISTORY_LINE_BUFFER_SIZE  8192u

static ui_history_store_t g_history_store;
static bool g_history_loaded = false;

static void history_ensure_loaded(void);

static void history_store_reset(void)
{
    memset(&g_history_store, 0, sizeof(g_history_store));
    g_history_store.next_record_no = 1;
    g_history_store.next_slot_no = 1;
}

static int history_ensure_dir(void)
{
    int fd;

    if (mkdir(UI_HISTORY_STORE_DIR, 0755) != 0 && errno != EEXIST) {
        return -1;
    }

    fd = open(UI_HISTORY_STORE_DIR, O_RDONLY | O_DIRECTORY);
    if (fd >= 0) {
        fsync(fd);
        close(fd);
    }

    return 0;
}

static void history_sync_store_dir(void)
{
    int fd = open(UI_HISTORY_STORE_DIR, O_RDONLY | O_DIRECTORY);

    if (fd < 0) {
        return;
    }
    (void)fsync(fd);
    (void)close(fd);
}

static int history_commit_atomic_file(FILE *fp, int fd,
                                      const char *tmp_path, const char *path,
                                      bool sync_directory)
{
    bool write_failed;

    if (fp == NULL || fd < 0 || tmp_path == NULL || path == NULL) {
        return -1;
    }

    write_failed = ferror(fp) != 0;
    if (fflush(fp) != 0) {
        write_failed = true;
    }
    if (!write_failed && fsync(fd) != 0) {
        write_failed = true;
    }
    if (fclose(fp) != 0) {
        write_failed = true;
    }
    if (write_failed) {
        (void)unlink(tmp_path);
        return -1;
    }
    if (rename(tmp_path, path) != 0) {
        (void)unlink(tmp_path);
        return -1;
    }

    /* rename 已经是权威提交点；目录同步失败不能触发业务重试和重复记录。 */
    if (sync_directory) {
        history_sync_store_dir();
    }
    return 0;
}

static void history_write_escaped_field(FILE *fp,
                                        const char *prefix,
                                        const char *key,
                                        const char *value)
{
    size_t i;

    if (fp == NULL || prefix == NULL || key == NULL) {
        return;
    }

    if (fprintf(fp, "%s%s=", prefix, key) < 0) {
        return;
    }

    if (value == NULL) {
        value = "";
    }

    for (i = 0; value[i] != '\0'; i++) {
        char ch = value[i];

        if (ch == '\\') {
            if (fputs("\\\\", fp) == EOF) return;
        } else if (ch == '\n') {
            if (fputs("\\n", fp) == EOF) return;
        } else if (ch == '\r') {
            if (fputs("\\r", fp) == EOF) return;
        } else {
            if (fputc((unsigned char)ch, fp) == EOF) return;
        }
    }

    (void)fputc('\n', fp);
}

static void history_unescape_text(char *dst, size_t dst_size, const char *src)
{
    size_t i;
    size_t j = 0;

    if (dst == NULL || dst_size == 0) {
        return;
    }

    dst[0] = '\0';
    if (src == NULL) {
        return;
    }

    for (i = 0; src[i] != '\0' && j + 1 < dst_size; i++) {
        if (src[i] == '\\' && src[i + 1] != '\0') {
            i++;
            if (src[i] == 'n') {
                dst[j++] = '\n';
            } else if (src[i] == 'r') {
                dst[j++] = '\r';
            } else {
                dst[j++] = src[i];
            }
        } else {
            dst[j++] = src[i];
        }
    }

    dst[j] = '\0';
}

static bool history_trim_line_end(char *line)
{
    size_t len;

    if (line == NULL) {
        return false;
    }

    len = strlen(line);
    if (len == 0 || line[len - 1] != '\n') {
        return false;
    }

    line[--len] = '\0';
    if (len > 0 && line[len - 1] == '\r') {
        line[len - 1] = '\0';
    }
    return true;
}

static bool history_parse_u32(const char *value, uint32_t *out)
{
    unsigned long parsed;
    char *end;

    if (value == NULL || out == NULL || value[0] == '\0' || value[0] == '-') {
        return false;
    }

    errno = 0;
    parsed = strtoul(value, &end, 0);
    if (errno == ERANGE || end == value || *end != '\0' || parsed > UINT32_MAX) {
        return false;
    }

    *out = (uint32_t)parsed;
    return true;
}

static bool history_parse_bool(const char *value, bool *out)
{
    uint32_t parsed;

    if (out == NULL || !history_parse_u32(value, &parsed) || parsed > 1) {
        return false;
    }

    *out = parsed != 0;
    return true;
}

static uint32_t history_amount_to_u32(float amount)
{
    if (!(amount > 0.0f)) {
        return 0;
    }
    if (amount >= (float)UINT32_MAX) {
        return UINT32_MAX;
    }
    return (uint32_t)amount;
}

static void history_format_denom(const counting_sim_t *sim_data, char *dst, size_t size)
{
    int i;
    size_t pos = 0;

    if (dst == NULL || size == 0) {
        return;
    }

    dst[0] = '\0';
    if (sim_data == NULL) {
        return;
    }

    for (i = 0; i < sim_data->denom_number && i < (int)(sizeof(sim_data->denom) / sizeof(sim_data->denom[0])); i++) {
        int written;

        if (sim_data->denom[i].value <= 0) {
            continue;
        }
        written = snprintf(dst + pos, size - pos, "%s%d x %u",
                              (pos > 0) ? "\n" : "",
                              sim_data->denom[i].value,
                              (unsigned)sim_data->denom[i].pcs);
        if (written < 0) {
            break;
        }
        if ((size_t)written >= size - pos) {
            break;
        }
        pos += (size_t)written;
        if (pos + 1U >= size) {
            break;
        }
    }
}

static void history_format_sn(const counting_sim_t *sim_data, char *dst, size_t size)
{
    int i;
    size_t pos = 0;

    if (dst == NULL || size == 0) {
        return;
    }

    dst[0] = '\0';
    if (sim_data == NULL || sim_data->sn_str == NULL) {
        return;
    }

    for (i = 0; i < sim_data->sn_capacity; i++) {
        int written;

        if (sim_data->sn_str[i] == NULL || sim_data->sn_str[i][0] == '\0') {
            continue;
        }
        written = snprintf(dst + pos, size - pos, "%s%s",
                              (pos > 0) ? "\n" : "",
                              sim_data->sn_str[i]);
        if (written < 0) {
            break;
        }
        if ((size_t)written >= size - pos) {
            break;
        }
        pos += (size_t)written;
        if (pos + 1U >= size) {
            break;
        }
    }
}

static void history_format_sn_detail(const counting_sim_t *sim_data, char *dst, size_t size)
{
    int i;
    size_t pos = 0;

    if (dst == NULL || size == 0) {
        return;
    }

    dst[0] = '\0';
    if (sim_data == NULL || sim_data->sn_str == NULL) {
        return;
    }

    for (i = 0; i < sim_data->sn_capacity; i++) {
        int written;
        const char *sn = sim_data->sn_str[i];
        int denom = sim_data->denom_mix[i];

        if (sn == NULL || sn[0] == '\0') {
            continue;
        }

        written = snprintf(dst + pos, size - pos, "%s%02d\t%d\t%s",
                              (pos > 0) ? "\n" : "",
                              i + 1, denom, sn);
        if (written < 0) {
            break;
        }
        if ((size_t)written >= size - pos) {
            break;
        }
        pos += (size_t)written;
        if (pos + 1U >= size) {
            break;
        }
    }
}

static void history_shift_left(int from)
{
    int i;

    for (i = from; i < (int)g_history_store.record_count - 1; i++) {
        g_history_store.records[i] = g_history_store.records[i + 1];
    }
    if (g_history_store.record_count > 0) {
        memset(&g_history_store.records[g_history_store.record_count - 1], 0,
               sizeof(g_history_store.records[0]));
    }
}

static void history_insert_front(const ui_history_record_t *rec)
{
    int i;

    if (rec == NULL) {
        return;
    }

    for (i = 0; i < g_history_store.record_count; i++) {
        if (g_history_store.records[i].slot_no == rec->slot_no) {
            history_shift_left(i);
            g_history_store.record_count--;
            break;
        }
    }

    if (g_history_store.record_count >= UI_HISTORY_MAX_RECORDS) {
        g_history_store.record_count = UI_HISTORY_MAX_RECORDS - 1;
    }

    for (i = (int)g_history_store.record_count; i > 0; i--) {
        g_history_store.records[i] = g_history_store.records[i - 1];
    }
    g_history_store.records[0] = *rec;
    g_history_store.record_count++;
}

static void history_record_defaults(ui_history_record_t *rec)
{
    if (rec == NULL) {
        return;
    }
    memset(rec, 0, sizeof(*rec));
}

static void history_write_kv(FILE *fp, uint8_t slot_no, const ui_history_record_t *rec)
{
    char buf[128];
    char prefix[32];

    if (fp == NULL || rec == NULL) {
        return;
    }

    snprintf(buf, sizeof(buf), "slot%02u_valid=%d\n", (unsigned)slot_no, rec->valid ? 1 : 0);
    fputs(buf, fp);
    snprintf(buf, sizeof(buf), "slot%02u_selected=%d\n", (unsigned)slot_no, rec->selected ? 1 : 0);
    fputs(buf, fp);
    snprintf(buf, sizeof(buf), "slot%02u_slot_no=%u\n", (unsigned)slot_no, (unsigned)rec->slot_no);
    fputs(buf, fp);
    snprintf(buf, sizeof(buf), "slot%02u_record_no=%u\n", (unsigned)slot_no, (unsigned)rec->record_no);
    fputs(buf, fp);
    snprintf(buf, sizeof(buf), "slot%02u_pcs=%u\n", (unsigned)slot_no, (unsigned)rec->pcs);
    fputs(buf, fp);
    snprintf(buf, sizeof(buf), "slot%02u_amount=%u\n", (unsigned)slot_no, (unsigned)rec->amount);
    fputs(buf, fp);
    snprintf(buf, sizeof(buf), "slot%02u_currency=%s\n", (unsigned)slot_no, rec->currency);
    fputs(buf, fp);
    snprintf(buf, sizeof(buf), "slot%02u_time=%04u-%02u-%02u %02u:%02u:%02u\n",
                (unsigned)slot_no,
                (unsigned)rec->year, (unsigned)rec->month, (unsigned)rec->day,
                (unsigned)rec->hour, (unsigned)rec->minute, (unsigned)rec->second);
    fputs(buf, fp);

    snprintf(prefix, sizeof(prefix), "slot%02u_", (unsigned)slot_no);
    history_write_escaped_field(fp, prefix, "denom", rec->denom_text);
    history_write_escaped_field(fp, prefix, "sn", rec->sn_text);
    history_write_escaped_field(fp, prefix, "sn_detail", rec->sn_detail_text);
    history_write_escaped_field(fp, prefix, "err", rec->error_frame_text);
    history_write_escaped_field(fp, prefix, "start", rec->start_frame_text);
    history_write_escaped_field(fp, prefix, "end", rec->end_frame_text);
    history_write_escaped_field(fp, prefix, "log", rec->session_log);
}

static int history_write_file(const char *path, const ui_history_record_t *rec, uint32_t total_notes,
                              uint32_t next_record_no, uint8_t next_slot_no, uint8_t record_count)
{
    FILE *fp;
    int fd;
    char tmp[160];
    int i;

    if (history_ensure_dir() != 0 || path == NULL || rec == NULL) {
        return -1;
    }

    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return -1;
    }

    fp = fdopen(fd, "w");
    if (fp == NULL) {
        close(fd);
        (void)unlink(tmp);
        return -1;
    }

    fprintf(fp, "magic=%u\nversion=%u\n", UI_HISTORY_MAGIC, UI_HISTORY_VERSION);
    fprintf(fp, "total_notes_counted=%u\n", (unsigned)total_notes);
    fprintf(fp, "next_record_no=%u\n", (unsigned)next_record_no);
    fprintf(fp, "next_slot_no=%u\n", (unsigned)next_slot_no);
    fprintf(fp, "record_count=%u\n", (unsigned)record_count);

    for (i = 0; i < record_count; i++) {
        char prefix[32];
        const ui_history_record_t *item = &rec[i];

        snprintf(prefix, sizeof(prefix), "record%02d_", i);
        fprintf(fp, "%svalid=%d\n", prefix, item->valid ? 1 : 0);
        fprintf(fp, "%sselected=%d\n", prefix, item->selected ? 1 : 0);
        fprintf(fp, "%sslot_no=%u\n", prefix, (unsigned)item->slot_no);
        fprintf(fp, "%srecord_no=%u\n", prefix, (unsigned)item->record_no);
        fprintf(fp, "%spcs=%u\n", prefix, (unsigned)item->pcs);
        fprintf(fp, "%samount=%u\n", prefix, (unsigned)item->amount);
        fprintf(fp, "%scurrency=%s\n", prefix, item->currency);
        fprintf(fp, "%stime=%04u-%02u-%02u %02u:%02u:%02u\n", prefix,
                (unsigned)item->year, (unsigned)item->month, (unsigned)item->day,
                (unsigned)item->hour, (unsigned)item->minute, (unsigned)item->second);
        history_write_escaped_field(fp, prefix, "denom", item->denom_text);
        history_write_escaped_field(fp, prefix, "sn", item->sn_text);
        history_write_escaped_field(fp, prefix, "sn_detail", item->sn_detail_text);
        history_write_escaped_field(fp, prefix, "err", item->error_frame_text);
        history_write_escaped_field(fp, prefix, "start", item->start_frame_text);
        history_write_escaped_field(fp, prefix, "end", item->end_frame_text);
        history_write_escaped_field(fp, prefix, "log", item->session_log);
    }

    return history_commit_atomic_file(fp, fd, tmp, path, true);
}

static int history_write_slot_file(const ui_history_record_t *rec)
{
    char path[128];
    FILE *fp;
    int fd;
    char tmp[160];

    if (rec == NULL || rec->slot_no == 0 || rec->slot_no > UI_HISTORY_MAX_RECORDS) {
        return -1;
    }

    snprintf(path, sizeof(path), UI_HISTORY_SLOT_PATH_FMT, (unsigned)rec->slot_no);
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return -1;
    }
    fp = fdopen(fd, "w");
    if (fp == NULL) {
        close(fd);
        (void)unlink(tmp);
        return -1;
    }

    history_write_kv(fp, rec->slot_no, rec);
    return history_commit_atomic_file(fp, fd, tmp, path, false);
}

static int history_write_meta_file(void)
{
    FILE *fp;
    int fd;
    char tmp[160];

    snprintf(tmp, sizeof(tmp), "%s.tmp", UI_HISTORY_META_PATH);
    fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return -1;
    }
    fp = fdopen(fd, "w");
    if (fp == NULL) {
        close(fd);
        (void)unlink(tmp);
        return -1;
    }

    fprintf(fp, "magic=%u\nversion=%u\n", UI_HISTORY_MAGIC, UI_HISTORY_VERSION);
    fprintf(fp, "total_notes_counted=%u\n", (unsigned)g_history_store.total_notes_counted);
    fprintf(fp, "next_record_no=%u\n", (unsigned)g_history_store.next_record_no);
    fprintf(fp, "next_slot_no=%u\n", (unsigned)g_history_store.next_slot_no);
    return history_commit_atomic_file(fp, fd, tmp, UI_HISTORY_META_PATH, false);
}

static bool history_save_all(void)
{
    int i;
    bool slot_present[UI_HISTORY_MAX_RECORDS + 1] = { false };

    if (history_ensure_dir() != 0) {
        return false;
    }

    /* index.cfg 是唯一权威数据源；只有它提交成功，内存修改才算成功。 */
    if (history_write_file(UI_HISTORY_INDEX_PATH, g_history_store.records,
                           g_history_store.total_notes_counted,
                           g_history_store.next_record_no,
                           g_history_store.next_slot_no,
                           g_history_store.record_count) != 0) {
        return false;
    }

    for (i = 0; i < g_history_store.record_count; i++) {
        uint8_t slot_no = g_history_store.records[i].slot_no;

        if (slot_no > 0 && slot_no <= UI_HISTORY_MAX_RECORDS) {
            slot_present[slot_no] = true;
        }
        (void)history_write_slot_file(&g_history_store.records[i]);
    }

    for (i = 1; i <= UI_HISTORY_MAX_RECORDS; i++) {
        if (!slot_present[i]) {
            char slot_path[128];

            snprintf(slot_path, sizeof(slot_path), UI_HISTORY_SLOT_PATH_FMT, (unsigned)i);
            (void)unlink(slot_path);
        }
    }

    (void)history_write_meta_file();
    history_sync_store_dir();
    return true;
}

static bool history_save_or_reload(void)
{
    if (history_save_all()) {
        return true;
    }

    /* 丢弃未落盘的内存修改，避免 UI 与重启后状态不一致。 */
    g_history_loaded = false;
    history_ensure_loaded();
    return false;
}

static bool history_parse_key_value(int record_index, const char *key, const char *value)
{
    ui_history_record_t *rec;
    uint32_t parsed;

    if (record_index < 0 || record_index >= UI_HISTORY_MAX_RECORDS) {
        return false;
    }

    rec = &g_history_store.records[record_index];
    if (key == NULL || value == NULL) {
        return false;
    }

    if (strcmp(key, "valid") == 0) {
        return history_parse_bool(value, &rec->valid);
    } else if (strcmp(key, "selected") == 0) {
        return history_parse_bool(value, &rec->selected);
    } else if (strcmp(key, "slot_no") == 0) {
        if (!history_parse_u32(value, &parsed) || parsed == 0 || parsed > UI_HISTORY_MAX_RECORDS) {
            return false;
        }
        rec->slot_no = (uint8_t)parsed;
    } else if (strcmp(key, "record_no") == 0) {
        if (!history_parse_u32(value, &parsed) || parsed == 0) {
            return false;
        }
        rec->record_no = parsed;
    } else if (strcmp(key, "pcs") == 0) {
        if (!history_parse_u32(value, &rec->pcs)) {
            return false;
        }
    } else if (strcmp(key, "amount") == 0) {
        if (!history_parse_u32(value, &rec->amount)) {
            return false;
        }
    } else if (strcmp(key, "currency") == 0) {
        snprintf(rec->currency, sizeof(rec->currency), "%s", value);
    } else if (strcmp(key, "time") == 0) {
        unsigned y, m, d, h, mi, s;
        char trailing;
        machine_time_value_t time_value;

        if (sscanf(value, "%u-%u-%u %u:%u:%u%c", &y, &m, &d, &h, &mi, &s, &trailing) != 6 ||
            y > UINT16_MAX || m > UINT8_MAX || d > UINT8_MAX || h > UINT8_MAX ||
            mi > UINT8_MAX || s > UINT8_MAX) {
            return false;
        }
        time_value.year = (uint16_t)y;
        time_value.month = (uint8_t)m;
        time_value.day = (uint8_t)d;
        time_value.hour = (uint8_t)h;
        time_value.minute = (uint8_t)mi;
        time_value.second = (uint8_t)s;
        if (!machine_time_is_valid(&time_value)) {
            return false;
        }
        rec->year = time_value.year;
        rec->month = time_value.month;
        rec->day = time_value.day;
        rec->hour = time_value.hour;
        rec->minute = time_value.minute;
        rec->second = time_value.second;
    } else if (strcmp(key, "denom") == 0) {
        history_unescape_text(rec->denom_text, sizeof(rec->denom_text), value);
    } else if (strcmp(key, "sn") == 0) {
        history_unescape_text(rec->sn_text, sizeof(rec->sn_text), value);
    } else if (strcmp(key, "sn_detail") == 0) {
        history_unescape_text(rec->sn_detail_text, sizeof(rec->sn_detail_text), value);
    } else if (strcmp(key, "err") == 0) {
        history_unescape_text(rec->error_frame_text, sizeof(rec->error_frame_text), value);
    } else if (strcmp(key, "start") == 0) {
        history_unescape_text(rec->start_frame_text, sizeof(rec->start_frame_text), value);
    } else if (strcmp(key, "end") == 0) {
        history_unescape_text(rec->end_frame_text, sizeof(rec->end_frame_text), value);
    } else if (strcmp(key, "log") == 0) {
        history_unescape_text(rec->session_log, sizeof(rec->session_log), value);
    }

    return true;
}

static bool history_loaded_records_valid(void)
{
    bool slots_seen[UI_HISTORY_MAX_RECORDS + 1] = { false };
    uint32_t max_record_no = 0;
    int i;
    int j;

    for (i = 0; i < g_history_store.record_count; i++) {
        const ui_history_record_t *rec = &g_history_store.records[i];
        machine_time_value_t time_value = {
            rec->year, rec->month, rec->day,
            rec->hour, rec->minute, rec->second
        };

        if (!rec->valid || rec->record_no == 0 || rec->slot_no == 0 ||
            rec->slot_no > UI_HISTORY_MAX_RECORDS || slots_seen[rec->slot_no] ||
            !machine_time_is_valid(&time_value)) {
            return false;
        }
        for (j = 0; j < i; j++) {
            if (g_history_store.records[j].record_no == rec->record_no) {
                return false;
            }
        }
        slots_seen[rec->slot_no] = true;
        if (rec->record_no > max_record_no) {
            max_record_no = rec->record_no;
        }
    }

    return g_history_store.record_count == 0 ||
           g_history_store.next_record_no > max_record_no;
}

static void history_load_from_file(void)
{
    FILE *fp;
    char line[UI_HISTORY_LINE_BUFFER_SIZE];
    bool read_failed;
    bool file_valid = true;
    bool magic_seen = false;
    bool version_seen = false;
    bool total_seen = false;
    bool next_record_seen = false;
    bool next_slot_seen = false;
    bool record_count_seen = false;

    history_store_reset();

    fp = fopen(UI_HISTORY_INDEX_PATH, "r");
    if (fp == NULL) {
        g_history_loaded = true;
        return;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *eq = strchr(line, '=');

        if (!history_trim_line_end(line)) {
            file_valid = false;
            break;
        }
        if (eq == NULL) {
            file_valid = false;
            break;
        }
        *eq++ = '\0';

        if (strcmp(line, "magic") == 0) {
            uint32_t parsed;

            if (!history_parse_u32(eq, &parsed) || parsed != UI_HISTORY_MAGIC) {
                file_valid = false;
                break;
            }
            magic_seen = true;
            continue;
        }
        if (strcmp(line, "version") == 0) {
            uint32_t parsed;

            if (!history_parse_u32(eq, &parsed) || parsed != UI_HISTORY_VERSION) {
                file_valid = false;
                break;
            }
            version_seen = true;
            continue;
        }
        if (strcmp(line, "total_notes_counted") == 0) {
            if (!history_parse_u32(eq, &g_history_store.total_notes_counted)) {
                file_valid = false;
                break;
            }
            total_seen = true;
            continue;
        }
        if (strcmp(line, "next_record_no") == 0) {
            if (!history_parse_u32(eq, &g_history_store.next_record_no) ||
                g_history_store.next_record_no == 0) {
                file_valid = false;
                break;
            }
            next_record_seen = true;
            continue;
        }
        if (strcmp(line, "next_slot_no") == 0) {
            uint32_t parsed;

            if (!history_parse_u32(eq, &parsed) || parsed == 0 ||
                parsed > UI_HISTORY_MAX_RECORDS) {
                file_valid = false;
                break;
            }
            g_history_store.next_slot_no = (uint8_t)parsed;
            next_slot_seen = true;
            continue;
        }
        if (strcmp(line, "record_count") == 0) {
            uint32_t parsed;

            if (!history_parse_u32(eq, &parsed) || parsed > UI_HISTORY_MAX_RECORDS) {
                file_valid = false;
                break;
            }
            g_history_store.record_count = (uint8_t)parsed;
            record_count_seen = true;
            continue;
        }

        if (strncmp(line, "record", 6) == 0) {
            int record_index = -1;
            char key[64];
            unsigned idx;
            if (sscanf(line, "record%02u_%63[^=]", &idx, key) == 2 && idx < UI_HISTORY_MAX_RECORDS) {
                record_index = (int)idx;
                if (!history_parse_key_value(record_index, key, eq)) {
                    file_valid = false;
                    break;
                }
            }
        }
    }

    read_failed = ferror(fp) != 0;
    if (fclose(fp) != 0) {
        read_failed = true;
    }
    if (read_failed) {
        file_valid = false;
    }

    if (!magic_seen || !version_seen || !total_seen || !next_record_seen ||
        !next_slot_seen || !record_count_seen || !history_loaded_records_valid()) {
        file_valid = false;
    }

    if (!file_valid) {
        history_store_reset();
    }

    g_history_loaded = true;
}

static void history_ensure_loaded(void)
{
    if (!g_history_loaded) {
        history_load_from_file();
    }
}

void ui_history_data_init(void)
{
    history_ensure_loaded();
}

const ui_history_store_t *ui_history_data_get(void)
{
    history_ensure_loaded();
    return &g_history_store;
}

uint32_t ui_history_total_notes_counted_get(void)
{
    history_ensure_loaded();
    return g_history_store.total_notes_counted;
}

void ui_history_total_notes_counted_set(uint32_t total)
{
    history_ensure_loaded();
    g_history_store.total_notes_counted = total;
    (void)history_save_or_reload();
}

void ui_history_total_notes_counted_clear(void)
{
    ui_history_total_notes_counted_set(0);
}

bool ui_history_record_append_from_session(const counting_sim_t *sim_data, uint32_t pcs_total,
                                           float amount_total, uint32_t total_notes_after,
                                           const char *error_frame_text,
                                           const char *start_frame_text, const char *end_frame_text,
                                           const char *session_log_text)
{
    ui_history_record_t rec;
    uint8_t slot_no;
    machine_time_value_t now;
    char curr_code[4];

    history_ensure_loaded();
    if (sim_data == NULL || sim_data->sn_capacity < 0 ||
        sim_data->sn_capacity > COUNTING_DATA_MAX_ITEMS ||
        (sim_data->sn_capacity > 0 && sim_data->sn_str == NULL)) {
        return false;
    }

    history_record_defaults(&rec);
    slot_no = g_history_store.next_slot_no;
    if (slot_no == 0 || slot_no > UI_HISTORY_MAX_RECORDS) {
        slot_no = 1;
    }

    rec.valid = true;
    rec.selected = false;
    rec.slot_no = slot_no;
    rec.record_no = g_history_store.next_record_no;
    rec.pcs = pcs_total;
    rec.amount = history_amount_to_u32(amount_total);
    currency_state_get_active_code(curr_code);
    snprintf(rec.currency, sizeof(rec.currency), "%s", curr_code);
    machine_time_get(&now);
    rec.year = now.year;
    rec.month = now.month;
    rec.day = now.day;
    rec.hour = now.hour;
    rec.minute = now.minute;
    rec.second = now.second;
    history_format_denom(sim_data, rec.denom_text, sizeof(rec.denom_text));
    history_format_sn(sim_data, rec.sn_text, sizeof(rec.sn_text));
    history_format_sn_detail(sim_data, rec.sn_detail_text, sizeof(rec.sn_detail_text));
    snprintf(rec.error_frame_text, sizeof(rec.error_frame_text), "%s",
                error_frame_text ? error_frame_text : "");
    snprintf(rec.start_frame_text, sizeof(rec.start_frame_text), "%s",
                start_frame_text ? start_frame_text : "");
    snprintf(rec.end_frame_text, sizeof(rec.end_frame_text), "%s",
                end_frame_text ? end_frame_text : "");
    snprintf(rec.session_log, sizeof(rec.session_log), "%s",
                session_log_text ? session_log_text : "");

    history_insert_front(&rec);

    g_history_store.total_notes_counted = total_notes_after;
    g_history_store.next_record_no++;
    g_history_store.next_slot_no = (uint8_t)(slot_no % UI_HISTORY_MAX_RECORDS + 1);
    if (g_history_store.record_count > UI_HISTORY_MAX_RECORDS) {
        g_history_store.record_count = UI_HISTORY_MAX_RECORDS;
    }

    return history_save_or_reload();
}

bool ui_history_record_toggle_selected(uint8_t index)
{
    history_ensure_loaded();
    if (index >= g_history_store.record_count) {
        return false;
    }
    g_history_store.records[index].selected = !g_history_store.records[index].selected;
    return history_save_or_reload();
}

bool ui_history_record_set_selected(uint8_t index, bool selected)
{
    history_ensure_loaded();
    if (index >= g_history_store.record_count) {
        return false;
    }
    g_history_store.records[index].selected = selected;
    return history_save_or_reload();
}

int ui_history_record_selected_first_index_get(void)
{
    int i;

    history_ensure_loaded();
    for (i = 0; i < g_history_store.record_count; i++) {
        if (g_history_store.records[i].selected) {
            return i;
        }
    }
    return -1;
}

int ui_history_record_selected_count_get(void)
{
    int i;
    int count = 0;

    history_ensure_loaded();
    for (i = 0; i < g_history_store.record_count; i++) {
        if (g_history_store.records[i].selected) {
            count++;
        }
    }
    return count;
}

void ui_history_record_clear_selected(void)
{
    int i;

    history_ensure_loaded();
    for (i = 0; i < g_history_store.record_count; i++) {
        g_history_store.records[i].selected = false;
    }
    (void)history_save_or_reload();
}

void ui_history_record_set_all_selected(bool selected)
{
    int i;

    history_ensure_loaded();
    for (i = 0; i < g_history_store.record_count; i++) {
        g_history_store.records[i].selected = selected;
    }
    (void)history_save_or_reload();
}

bool ui_history_record_delete_selected(void)
{
    ui_history_record_t kept[UI_HISTORY_MAX_RECORDS];
    int kept_count = 0;
    int i;
    uint32_t next_record_no = 1;

    history_ensure_loaded();

    for (i = 0; i < g_history_store.record_count; i++) {
        if (g_history_store.records[i].selected) {
            continue;
        }
        if (kept_count < UI_HISTORY_MAX_RECORDS) {
            kept[kept_count++] = g_history_store.records[i];
        }
    }

    if (kept_count == (int)g_history_store.record_count) {
        return false;
    }

    for (i = 0; i < kept_count; i++) {
        kept[i].selected = false;
        kept[i].slot_no = (uint8_t)(i + 1);
        if (kept[i].record_no >= next_record_no) {
            next_record_no = kept[i].record_no + 1;
        }
    }

    memset(g_history_store.records, 0, sizeof(g_history_store.records));
    for (i = 0; i < kept_count; i++) {
        g_history_store.records[i] = kept[i];
    }
    g_history_store.record_count = (uint8_t)kept_count;
    g_history_store.next_slot_no = (uint8_t)((kept_count >= UI_HISTORY_MAX_RECORDS) ? 1 : (kept_count + 1));
    g_history_store.next_record_no = next_record_no;

    return history_save_or_reload();
}

bool ui_history_record_get(uint8_t index, ui_history_record_t *out)
{
    history_ensure_loaded();
    if (out == NULL || index >= g_history_store.record_count) {
        return false;
    }

    *out = g_history_store.records[index];
    return true;
}

bool ui_history_record_get_by_no(uint32_t record_no, ui_history_record_t *out)
{
    int i;

    history_ensure_loaded();
    if (out == NULL) {
        return false;
    }

    for (i = 0; i < g_history_store.record_count; i++) {
        if (g_history_store.records[i].record_no == record_no) {
            *out = g_history_store.records[i];
            return true;
        }
    }
    return false;
}
