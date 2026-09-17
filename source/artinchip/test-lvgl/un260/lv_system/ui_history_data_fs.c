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
#include "un260/counting/counting_data_store.h"

#ifndef UI_HISTORY_STORE_DIR
#define UI_HISTORY_STORE_DIR         "/etc/ui_state/count_history"
#endif
#define UI_HISTORY_INDEX_PATH        UI_HISTORY_STORE_DIR "/index.cfg"
#define UI_HISTORY_META_PATH         UI_HISTORY_STORE_DIR "/meta.cfg"
#define UI_HISTORY_SLOT_PATH_FMT     UI_HISTORY_STORE_DIR "/%02u.rec"
#define UI_HISTORY_MAGIC             0x48495354u
#define UI_HISTORY_VERSION           3u
#define UI_HISTORY_LINE_BUFFER_SIZE  8192u

static ui_history_store_t g_history_store;
static bool g_history_loaded = false;
static bool g_history_load_attempted = false;
static bool g_history_initialized;
static storage_job_id_t g_history_load_job;
/* Worker-only until its job is SUCCEEDED. storage_worker_status/wait provide
 * the mutex handoff; only the UI poll publishes and frees this private result. */
static ui_history_store_t *g_history_load_result;
static bool g_history_load_valid;
static ui_history_store_t g_history_accepted;
static ui_history_store_t g_history_durable;
static bool g_history_failed_view;
/* Worker-only mirror cache; the self-contained v2 index remains authoritative. */
static ui_history_record_t g_mirror_records[UI_HISTORY_MAX_RECORDS];
static bool g_mirror_valid[UI_HISTORY_MAX_RECORDS];
static bool g_index_needs_directory_sync;
static bool g_history_parent_synced;
static bool g_meta_valid;
static uint32_t g_meta_total;
static uint32_t g_meta_next_record;
static uint8_t g_meta_next_slot;
static storage_job_id_t g_history_jobs[STORAGE_WORKER_CAPACITY];
static unsigned g_history_job_count;
static storage_job_id_t g_last_commit;
static storage_job_id_t g_last_durable_commit;
static uint32_t g_history_retry_tick;

_Static_assert(sizeof(ui_history_store_t) <= STORAGE_WORKER_MAX_JOB_BYTES,
               "history snapshot exceeds bounded storage job payload");
/* v2 indexes record00..record99; physical slots are 1..100. Increasing this
 * further requires reviewing the two-digit index parser and uint8_t fields. */
_Static_assert(UI_HISTORY_MAX_RECORDS > 0 && UI_HISTORY_MAX_RECORDS <= 100,
               "history capacity exceeds v2 index range");

static void history_ensure_loaded(void);

static void history_store_reset(ui_history_store_t *store)
{
    memset(store, 0, sizeof(*store));
    store->next_record_no = 1;
    store->next_slot_no = 1;
}

static int history_sync_store_dir(void)
{
    int fd = open(UI_HISTORY_STORE_DIR, O_RDONLY | O_DIRECTORY);
    int result;
    if (fd < 0) return -1;
    result = fsync(fd);
    if (close(fd) != 0) result = -1;
    return result;
}

static int history_ensure_dir(void)
{
    char parent[512];
    char *slash;
    int fd;
    int result;
    bool created = mkdir(UI_HISTORY_STORE_DIR, 0755) == 0;
    if (!created && errno != EEXIST) return -1;
    if (created || !g_history_parent_synced) {
        if (snprintf(parent, sizeof(parent), "%s", UI_HISTORY_STORE_DIR) >= (int)sizeof(parent))
            return -1;
        slash = strrchr(parent, '/');
        if (slash == NULL) snprintf(parent, sizeof(parent), ".");
        else if (slash == parent) slash[1] = '\0';
        else *slash = '\0';
        fd = open(parent, O_RDONLY | O_DIRECTORY);
        if (fd < 0) return -1;
        result = fsync(fd);
        if (close(fd) != 0) result = -1;
        if (result != 0) return -1;
        g_history_parent_synced = true;
    }
    return 0;
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

    /* -2 means rename happened but durability is unconfirmed. Retry directory
     * sync on the SAME immutable job; never append a second business record. */
    if (sync_directory && history_sync_store_dir() != 0) return -2;
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

static uint8_t history_next_available_slot(void)
{
    bool occupied[UI_HISTORY_MAX_RECORDS + 1] = { false };
    uint8_t candidate = g_history_store.next_slot_no;
    int i;

    /* Records remain newest-first even when the machine clock is adjusted.
     * A full store replaces its oldest record, never an arbitrary cursor slot. */
    if (g_history_store.record_count >= UI_HISTORY_MAX_RECORDS) {
        return g_history_store.records[g_history_store.record_count - 1].slot_no;
    }
    for (i = 0; i < g_history_store.record_count; i++) {
        occupied[g_history_store.records[i].slot_no] = true;
    }
    if (candidate == 0 || candidate > UI_HISTORY_MAX_RECORDS) candidate = 1;
    for (i = 0; i < UI_HISTORY_MAX_RECORDS; i++) {
        if (!occupied[candidate]) return candidate;
        candidate = (uint8_t)(candidate % UI_HISTORY_MAX_RECORDS + 1);
    }
    return 0;
}

static void history_record_defaults(ui_history_record_t *rec)
{
    if (rec == NULL) {
        return;
    }
    memset(rec, 0, sizeof(*rec));
}

static void history_write_multi(FILE *fp, const char *prefix, const history_multi_t *m)
{
    if (!m->enabled) return;
    fprintf(fp,"%smulti=%u,%u,%u,%u,%u\n",prefix,m->count,m->rejects,
        m->passes,m->add,m->overflow);
    for (unsigned i=0;i<m->count;i++) {
        const history_multi_currency_t *c=&m->currencies[i];
        fprintf(fp,"%smg%02u=%s,%u,%u,%u,%u",prefix,i,c->code,c->pcs,
            c->amount,c->complete,c->count);
        for(unsigned j=0;j<c->count;j++)
            fprintf(fp,",%u:%u",c->denoms[j].value,c->denoms[j].pcs);
        fputc('\n',fp);
    }
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
    history_write_multi(fp,prefix,&rec->multi);
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
        history_write_multi(fp,prefix,&item->multi);
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

static int history_write_meta_file(const ui_history_store_t *store)
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
    fprintf(fp, "total_notes_counted=%u\n", (unsigned)store->total_notes_counted);
    fprintf(fp, "next_record_no=%u\n", (unsigned)store->next_record_no);
    fprintf(fp, "next_slot_no=%u\n", (unsigned)store->next_slot_no);
    return history_commit_atomic_file(fp, fd, tmp, UI_HISTORY_META_PATH, false);
}

static bool history_save_snapshot(const void *snapshot, size_t size)
{
    const ui_history_store_t *store = snapshot;
    int i;
    int result;
    bool slot_present[UI_HISTORY_MAX_RECORDS + 1] = { false };

    if (size != sizeof(*store) || history_ensure_dir() != 0) {
        return false;
    }

    if (g_index_needs_directory_sync) {
        if (history_sync_store_dir() != 0) return false;
        g_index_needs_directory_sync = false;
    } else {
        result = history_write_file(UI_HISTORY_INDEX_PATH, store->records,
                                    store->total_notes_counted, store->next_record_no,
                                    store->next_slot_no, store->record_count);
        if (result != 0) {
            g_index_needs_directory_sync = result == -2;
            return false;
        }
    }

    /* Only dirty mirrors are rewritten. Mirrors are diagnostic exports, not a
     * second authority: an interrupted mirror update cannot invalidate index. */
    for (i = 0; i < store->record_count; i++) {
        const ui_history_record_t *record = &store->records[i];
        uint8_t slot_no = record->slot_no;

        if (slot_no > 0 && slot_no <= UI_HISTORY_MAX_RECORDS) {
            slot_present[slot_no] = true;
            if ((!g_mirror_valid[slot_no - 1] ||
                 memcmp(&g_mirror_records[slot_no - 1], record, sizeof(*record)) != 0) &&
                history_write_slot_file(record) == 0) {
                g_mirror_records[slot_no - 1] = *record;
                g_mirror_valid[slot_no - 1] = true;
            }
        }
    }

    for (i = 1; i <= UI_HISTORY_MAX_RECORDS; i++) {
        if (!slot_present[i]) {
            char slot_path[128];

            snprintf(slot_path, sizeof(slot_path), UI_HISTORY_SLOT_PATH_FMT, (unsigned)i);
            (void)unlink(slot_path);
            g_mirror_valid[i - 1] = false;
        }
    }

    if ((!g_meta_valid || g_meta_total != store->total_notes_counted ||
         g_meta_next_record != store->next_record_no ||
         g_meta_next_slot != store->next_slot_no) && history_write_meta_file(store) == 0) {
        g_meta_total = store->total_notes_counted;
        g_meta_next_record = store->next_record_no;
        g_meta_next_slot = store->next_slot_no;
        g_meta_valid = true;
    }
    (void)history_sync_store_dir();
    return true;
}

static bool history_submit_or_restore(void)
{
    storage_job_id_t id;
    if (g_history_loaded && !g_history_failed_view && g_history_job_count < STORAGE_WORKER_CAPACITY &&
        storage_worker_submit(history_save_snapshot, &g_history_store,
                               sizeof(g_history_store), &id)) {
        g_history_jobs[g_history_job_count++] = id;
        g_last_commit = id;
        g_history_accepted = g_history_store;
        return true;
    }

    /* Rejected input was never accepted: restore the last accepted UI view.
     * Accepted failures stay owned by the worker and are retried in order. */
    g_history_store = g_history_failed_view ? g_history_durable : g_history_accepted;
    return false;
}

static bool history_multi_numbers_valid(const char *p)
{
    while(*p) {
        if(*p>='0'&&*p<='9') {
            uint64_t value=0;
            do {value=value*10+(unsigned)(*p++-'0');if(value>UINT32_MAX)return false;}
            while(*p>='0'&&*p<='9');
        } else if((*p>='A'&&*p<='Z')||*p==','||*p==':')p++;
        else return false;
    }
    return true;
}

static bool history_parse_key_value(ui_history_store_t *store, int record_index,
                                    const char *key, const char *value)
{
    ui_history_record_t *rec;
    uint32_t parsed;

    if (record_index < 0 || record_index >= UI_HISTORY_MAX_RECORDS) {
        return false;
    }

    rec = &store->records[record_index];
    if (key == NULL || value == NULL) {
        return false;
    }

    if (strcmp(key, "multi") == 0) {
        if(!history_multi_numbers_valid(value))return false;
        unsigned count,rejects,passes,add,overflow; char extra;
        if (sscanf(value,"%u,%u,%u,%u,%u%c",&count,&rejects,&passes,&add,&overflow,&extra)!=5 ||
            count>HISTORY_MULTI_CURRENCIES || rejects>255 || !passes || add>1 || overflow>1)
            return false;
        rec->multi.enabled=true;rec->multi.count=count;rec->multi.rejects=rejects;
        rec->multi.passes=passes;rec->multi.add=add;rec->multi.overflow=overflow;
    } else if (strncmp(key,"mg",2)==0) {
        if(!history_multi_numbers_valid(value))return false;
        unsigned index,pcs,amount,complete,count;int used=0;char extra;
        if(sscanf(key,"mg%u%c",&index,&extra)!=1 || index>=HISTORY_MULTI_CURRENCIES) return false;
        history_multi_currency_t *c=&rec->multi.currencies[index];
        if(sscanf(value,"%3[A-Z],%u,%u,%u,%u%n",c->code,&pcs,&amount,&complete,&count,&used)!=5 ||
            strlen(c->code)!=3 || complete>1 || count>HISTORY_MULTI_DENOMS) return false;
        c->pcs=pcs;c->amount=amount;c->complete=complete;c->count=count;
        const char *p=value+used;
        for(unsigned j=0;j<count;j++) {
            unsigned d,n;used=0;
            if(sscanf(p,",%u:%u%n",&d,&n,&used)!=2 || !used || !d) return false;
            c->denoms[j]=(history_multi_denom_t){d,n};p+=used;
        }
        if(*p) return false;
    } else if (strcmp(key, "valid") == 0) {
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

static bool history_loaded_records_valid(const ui_history_store_t *store)
{
    bool slots_seen[UI_HISTORY_MAX_RECORDS + 1] = { false };
    uint32_t max_record_no = 0;
    int i;
    int j;

    for (i = 0; i < store->record_count; i++) {
        const ui_history_record_t *rec = &store->records[i];
        machine_time_value_t time_value = {
            rec->year, rec->month, rec->day,
            rec->hour, rec->minute, rec->second
        };

        if (!rec->valid || rec->record_no == 0 || rec->slot_no == 0 ||
            rec->slot_no > UI_HISTORY_MAX_RECORDS || slots_seen[rec->slot_no] ||
            !machine_time_is_valid(&time_value)) {
            return false;
        }
        if(rec->multi.enabled) {
            if(rec->multi.count>HISTORY_MULTI_CURRENCIES || !rec->multi.passes ||
                strcmp(rec->currency,"MUL") || rec->amount) return false;
            for(unsigned g=0;g<rec->multi.count;g++) {
                const history_multi_currency_t *c=&rec->multi.currencies[g];
                if(c->code[3] || c->count>HISTORY_MULTI_DENOMS || (!c->complete && c->count))return false;
                for(unsigned k=0;k<3;k++)if(c->code[k]<'A'||c->code[k]>'Z')return false;
                for(unsigned k=0;k<g;k++)if(!strcmp(c->code,rec->multi.currencies[k].code))return false;
                uint64_t pcs=0,amount=0;
                for(unsigned k=0;k<c->count;k++) {
                    if(!c->denoms[k].value)return false;
                    for(unsigned n=0;n<k;n++)if(c->denoms[n].value==c->denoms[k].value)return false;
                    pcs+=c->denoms[k].pcs;amount+=(uint64_t)c->denoms[k].value*c->denoms[k].pcs;
                }
                if(c->complete && (pcs!=c->pcs || amount!=c->amount))return false;
            }
        }
        for (j = 0; j < i; j++) {
            if (store->records[j].record_no == rec->record_no) {
                return false;
            }
        }
        slots_seen[rec->slot_no] = true;
        if (rec->record_no > max_record_no) {
            max_record_no = rec->record_no;
        }
    }

    return store->record_count == 0 ||
           store->next_record_no > max_record_no;
}

static bool history_load_from_file(ui_history_store_t *store)
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

    history_store_reset(store);

    fp = fopen(UI_HISTORY_INDEX_PATH, "r");
    if (fp == NULL) {
        /* Only a genuinely absent index is a new empty store. Treating a
         * permission/device error as empty would overwrite saved history. */
        return errno == ENOENT;
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

            if (!history_parse_u32(eq, &parsed) || (parsed != 2u && parsed != UI_HISTORY_VERSION)) {
                file_valid = false;
                break;
            }
            version_seen = true;
            continue;
        }
        if (strcmp(line, "total_notes_counted") == 0) {
            if (!history_parse_u32(eq, &store->total_notes_counted)) {
                file_valid = false;
                break;
            }
            total_seen = true;
            continue;
        }
        if (strcmp(line, "next_record_no") == 0) {
            if (!history_parse_u32(eq, &store->next_record_no) ||
                store->next_record_no == 0) {
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
            store->next_slot_no = (uint8_t)parsed;
            next_slot_seen = true;
            continue;
        }
        if (strcmp(line, "record_count") == 0) {
            uint32_t parsed;

            if (!history_parse_u32(eq, &parsed) || parsed > UI_HISTORY_MAX_RECORDS) {
                file_valid = false;
                break;
            }
            store->record_count = (uint8_t)parsed;
            record_count_seen = true;
            continue;
        }

        if (strncmp(line, "record", 6) == 0) {
            int record_index = -1;
            char key[64];
            unsigned idx;
            if (sscanf(line, "record%02u_%63[^=]", &idx, key) == 2 && idx < UI_HISTORY_MAX_RECORDS) {
                record_index = (int)idx;
                if (!history_parse_key_value(store, record_index, key, eq)) {
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
        !next_slot_seen || !record_count_seen || !history_loaded_records_valid(store)) {
        file_valid = false;
    }

    if (!file_valid) {
        history_store_reset(store);
    }

    return file_valid;
}

static bool history_load_job(const void *snapshot, size_t size)
{
    (void)snapshot;
    (void)size;
    g_history_load_result = malloc(sizeof(*g_history_load_result));
    g_history_load_valid = g_history_load_result != NULL &&
                          history_load_from_file(g_history_load_result);
    /* The read attempt completed; availability is reported separately. Keep
     * corrupt/unreadable history from pinning the shared worker's FIFO lane. */
    return true;
}

void ui_history_data_init_async(void)
{
    const char load_request = 0;
    if (g_history_load_attempted) return;
    g_history_load_attempted = true;
    /* No filesystem work and no wait on the UI thread. Failure is terminal for
     * this process, matching the protected old boot-read failure semantics. */
    if (storage_worker_init() && storage_worker_submit(history_load_job,
            &load_request, sizeof(load_request), &g_history_load_job)) return;
    history_store_reset(&g_history_store);
    g_history_accepted = g_history_store;
    g_history_durable = g_history_store;
    g_history_initialized = true;
}

bool ui_history_data_init_poll(void)
{
    if (g_history_initialized) return true;
    if (!g_history_load_attempted || g_history_load_job == 0) return false;
    if (storage_worker_status(g_history_load_job) != STORAGE_JOB_SUCCEEDED)
        return false;

    if (g_history_load_result != NULL) {
        g_history_store = *g_history_load_result;
        free(g_history_load_result);
        g_history_load_result = NULL;
    } else {
        history_store_reset(&g_history_store);
    }
    g_history_loaded = g_history_load_valid;
    g_history_accepted = g_history_store;
    g_history_durable = g_history_store;
    (void)storage_worker_release(g_history_load_job);
    g_history_load_job = 0;
    g_history_initialized = true;
    return true;
}

bool ui_history_data_is_initialized(void)
{
    return g_history_initialized;
}

void ui_history_data_init(void)
{
    ui_history_data_init_async();
    if (!g_history_initialized && g_history_load_job != 0) {
        (void)storage_worker_wait(g_history_load_job);
        (void)ui_history_data_init_poll();
    }
}

static void history_ensure_loaded(void)
{
    /* Preserve lazy synchronous initialization for existing non-async callers.
     * Once startup opted in to async, getters stay RAM-only and cannot wait. */
    if (!g_history_load_attempted) ui_history_data_init();
}

bool ui_history_data_poll(uint32_t now_ms)
{
    bool changed = !g_history_initialized && ui_history_data_init_poll();
    while (g_history_job_count > 0) {
        storage_job_id_t id = g_history_jobs[0];
        storage_job_status_t status = storage_worker_status(id);
        if (status == STORAGE_JOB_SUCCEEDED) {
            (void)storage_worker_copy_completed(id, &g_history_durable,
                                                 sizeof(g_history_durable));
            g_last_durable_commit = id;
            (void)storage_worker_release(id);
            memmove(g_history_jobs, g_history_jobs + 1,
                    (--g_history_job_count) * sizeof(g_history_jobs[0]));
            changed = true;
            continue;
        }
        if (status == STORAGE_JOB_FAILED) {
            if (!g_history_failed_view) {
                g_history_failed_view = true;
                g_history_store = g_history_durable;
                changed = true;
            }
            if ((uint32_t)(now_ms - g_history_retry_tick) >= 1000U) {
                g_history_retry_tick = now_ms;
                (void)storage_worker_retry(id);
            }
        }
        break;
    }
    if (g_history_failed_view && g_history_job_count == 0) {
        g_history_failed_view = false;
        g_history_store = g_history_accepted;
        changed = true;
    }
    return changed;
}

storage_job_id_t ui_history_last_commit_id(void) { return g_last_commit; }

storage_job_status_t ui_history_commit_status(storage_job_id_t id)
{
    if (id != 0 && id <= g_last_durable_commit) return STORAGE_JOB_SUCCEEDED;
    return storage_worker_status(id);
}

storage_job_status_t ui_history_data_status(void)
{
    unsigned i;
    bool pending = false;
    if (!g_history_initialized) return STORAGE_JOB_PENDING;
    if (g_history_failed_view) return STORAGE_JOB_FAILED;
    for (i = 0; i < g_history_job_count; i++) {
        storage_job_status_t status = storage_worker_status(g_history_jobs[i]);
        if (status == STORAGE_JOB_FAILED || status == STORAGE_JOB_UNKNOWN) return STORAGE_JOB_FAILED;
        if (status == STORAGE_JOB_PENDING) pending = true;
    }
    if (pending) return STORAGE_JOB_PENDING;
    return g_history_loaded ? STORAGE_JOB_SUCCEEDED : STORAGE_JOB_FAILED;
}

bool ui_history_data_can_accept(void)
{
    return g_history_loaded && !g_history_failed_view &&
           g_history_job_count < STORAGE_WORKER_CAPACITY && storage_worker_has_capacity();
}

bool ui_history_data_is_available(void)
{
    history_ensure_loaded();
    return g_history_loaded;
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
    if (!g_history_loaded) return;
    if (g_history_store.total_notes_counted == total) return;
    g_history_store.total_notes_counted = total;
    (void)history_submit_or_restore();
}

void ui_history_total_notes_counted_clear(void)
{
    ui_history_total_notes_counted_set(0);
}

static bool history_record_build(const counting_sim_t *sim_data, uint32_t pcs_total,
                                           float amount_total,
                                           const char *error_frame_text,
                                           const char *start_frame_text, const char *end_frame_text,
                                           const char *session_log_text, ui_history_record_t *out, bool multi)
{
    ui_history_record_t rec;
    machine_time_value_t now;
    char curr_code[4];

    if (out == NULL || sim_data == NULL ||
        (!multi && !counting_data_monetary_result_supported(sim_data)) ||
        sim_data->sn_capacity < 0 ||
        sim_data->sn_capacity > COUNTING_DATA_MAX_ITEMS ||
        (sim_data->sn_capacity > 0 && sim_data->sn_str == NULL)) {
        return false;
    }

    history_record_defaults(&rec);
    rec.valid = true;
    rec.selected = false;
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
    if (!sim_data->multi_currency_result) {
        history_format_denom(sim_data, rec.denom_text, sizeof(rec.denom_text));
        history_format_sn(sim_data, rec.sn_text, sizeof(rec.sn_text));
        history_format_sn_detail(sim_data, rec.sn_detail_text, sizeof(rec.sn_detail_text));
    }
    snprintf(rec.error_frame_text, sizeof(rec.error_frame_text), "%s",
                error_frame_text ? error_frame_text : "");
    snprintf(rec.start_frame_text, sizeof(rec.start_frame_text), "%s",
                start_frame_text ? start_frame_text : "");
    snprintf(rec.end_frame_text, sizeof(rec.end_frame_text), "%s",
                end_frame_text ? end_frame_text : "");
    snprintf(rec.session_log, sizeof(rec.session_log), "%s",
                session_log_text ? session_log_text : "");

    *out = rec;
    return true;
}

bool ui_history_record_build_from_session(const counting_sim_t *sim,uint32_t pcs,float amount,
    const char *error,const char *start,const char *end,const char *log,ui_history_record_t *out)
{
    return history_record_build(sim,pcs,amount,error,start,end,log,out,false);
}
bool ui_history_record_build_multi_base(const counting_sim_t *sim,uint32_t pcs,
    const char *error,const char *start,const char *end,const char *log,ui_history_record_t *out)
{
    return sim && sim->multi_currency_result && history_record_build(sim,pcs,0,error,start,end,log,out,true);
}

bool ui_history_record_append_snapshot(const ui_history_record_t *record,
                                       uint32_t total_notes_after)
{
    ui_history_record_t rec;
    uint8_t slot_no;
    history_ensure_loaded();
    if (record == NULL || !record->valid || !ui_history_data_can_accept() ||
        g_history_store.next_record_no == UINT32_MAX) return false;
    rec = *record;
    slot_no = history_next_available_slot();
    if (slot_no == 0 || slot_no > UI_HISTORY_MAX_RECORDS) return false;
    rec.slot_no = slot_no;
    rec.record_no = g_history_store.next_record_no;
    history_insert_front(&rec);

    g_history_store.total_notes_counted = total_notes_after;
    g_history_store.next_record_no++;
    g_history_store.next_slot_no = (uint8_t)(slot_no % UI_HISTORY_MAX_RECORDS + 1);
    if (g_history_store.record_count > UI_HISTORY_MAX_RECORDS) {
        g_history_store.record_count = UI_HISTORY_MAX_RECORDS;
    }

    return history_submit_or_restore();
}

bool ui_history_record_update_snapshot(const ui_history_record_t *record, uint32_t total_notes_after)
{
    history_ensure_loaded();
    if (!record || !record->valid || !record->record_no || !ui_history_data_can_accept()) return false;
    for (unsigned i=0;i<g_history_store.record_count;i++) {
        ui_history_record_t *dst=&g_history_store.records[i];
        if(dst->record_no!=record->record_no) continue;
        uint8_t slot=dst->slot_no;bool selected=dst->selected;
        *dst=*record;dst->slot_no=slot;dst->selected=selected;
        g_history_store.total_notes_counted=total_notes_after;
        return history_submit_or_restore();
    }
    return false;
}

bool ui_history_record_append_from_session(const counting_sim_t *sim_data, uint32_t pcs_total,
                                           float amount_total, uint32_t total_notes_after,
                                           const char *error_frame_text,
                                           const char *start_frame_text, const char *end_frame_text,
                                           const char *session_log_text)
{
    ui_history_record_t rec;
    return ui_history_record_build_from_session(sim_data, pcs_total, amount_total,
        error_frame_text, start_frame_text, end_frame_text, session_log_text, &rec) &&
        ui_history_record_append_snapshot(&rec, total_notes_after);
}

bool ui_history_record_toggle_selected(uint8_t index)
{
    history_ensure_loaded();
    if (!g_history_loaded || index >= g_history_store.record_count) {
        return false;
    }
    g_history_store.records[index].selected = !g_history_store.records[index].selected;
    return history_submit_or_restore();
}

bool ui_history_record_set_selected(uint8_t index, bool selected)
{
    history_ensure_loaded();
    if (!g_history_loaded || index >= g_history_store.record_count) {
        return false;
    }
    if (g_history_store.records[index].selected == selected) return true;
    g_history_store.records[index].selected = selected;
    return history_submit_or_restore();
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
    bool changed = false;

    history_ensure_loaded();
    if (!g_history_loaded) return;
    for (i = 0; i < g_history_store.record_count; i++) {
        changed |= g_history_store.records[i].selected;
        g_history_store.records[i].selected = false;
    }
    if (changed) (void)history_submit_or_restore();
}

void ui_history_record_set_all_selected(bool selected)
{
    int i;
    bool changed = false;

    history_ensure_loaded();
    if (!g_history_loaded) return;
    for (i = 0; i < g_history_store.record_count; i++) {
        changed |= g_history_store.records[i].selected != selected;
        g_history_store.records[i].selected = selected;
    }
    if (changed) (void)history_submit_or_restore();
}

static bool history_delete_marked(const bool remove[UI_HISTORY_MAX_RECORDS])
{
    int kept_count = 0;
    int i;

    for (i = 0; i < g_history_store.record_count; i++) {
        if (remove[i]) {
            continue;
        }
        if (kept_count != i)
            g_history_store.records[kept_count] = g_history_store.records[i];
        kept_count++;
    }

    if (kept_count == (int)g_history_store.record_count) {
        return false;
    }

    memset(&g_history_store.records[kept_count], 0,
           (UI_HISTORY_MAX_RECORDS - kept_count) * sizeof(g_history_store.records[0]));
    g_history_store.record_count = (uint8_t)kept_count;
    g_history_store.next_slot_no = history_next_available_slot();
    /* Keep surviving slot identities and the monotonic record number, even
     * after deleting every record. Reusing a deleted ID targets the wrong item
     * in an open detail view or in a saved selection/search result. */

    return history_submit_or_restore();
}

bool ui_history_record_delete_selected(void)
{
    bool remove[UI_HISTORY_MAX_RECORDS] = { false };
    history_ensure_loaded();
    if (!ui_history_data_can_accept()) return false;
    for (size_t i = 0; i < g_history_store.record_count; ++i)
        remove[i] = g_history_store.records[i].selected;
    return history_delete_marked(remove);
}

bool ui_history_record_delete_records(const uint32_t *record_nos, size_t count)
{
    bool remove[UI_HISTORY_MAX_RECORDS] = { false };
    history_ensure_loaded();
    if (!record_nos || count == 0 || count > UI_HISTORY_MAX_RECORDS ||
        !ui_history_data_can_accept()) return false;
    for (size_t requested = 0; requested < count; ++requested) {
        if (record_nos[requested] == 0) return false;
        size_t index = 0;
        while (index < g_history_store.record_count &&
               g_history_store.records[index].record_no != record_nos[requested]) ++index;
        if (index == g_history_store.record_count || remove[index]) return false;
        remove[index] = true;
    }
    return history_delete_marked(remove);
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
