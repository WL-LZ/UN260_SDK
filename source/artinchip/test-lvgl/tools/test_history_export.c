#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#define fsync _commit
#endif

#include "un260/lv_system/ui_history_export_data.c"

static ui_history_store_t test_store;
static lv_timer_t test_timer;
static char test_directory[192];
static char test_paths[UI_HISTORY_MAX_RECORDS][2][256];
static int prepare_calls;
static int pair_calls;
static int commit_calls;
static int fail_commit;
static bool mutate_on_prepare;
static bool prepare_ok = true;

const ui_history_store_t *ui_history_data_get(void) { return &test_store; }
lv_timer_t *lv_timer_create(void (*callback)(lv_timer_t *), uint32_t period, void *user_data)
{
    (void)period; (void)user_data;
    test_timer.callback = callback;
    return &test_timer;
}
void lv_timer_del(lv_timer_t *timer) { (void)timer; }
void lv_timer_set_repeat_count(lv_timer_t *timer, int32_t count) { (void)timer; (void)count; }
lv_print_toast_config_t lv_print_toast_get_default_config(void)
{
    lv_print_toast_config_t config = {0};
    return config;
}
void lv_print_toast_show_with_config(const lv_print_toast_config_t *config) { (void)config; }
bool usb_storage_prepare(void)
{
    ++prepare_calls;
    if (mutate_on_prepare) {
        test_store.records[0].pcs = 1;
        test_store.records[0].amount = 2;
    }
    return prepare_ok;
}
bool usb_storage_make_unique_file_pair(const char *name, const char *ext1,
                                       char *path1, size_t size1, const char *ext2,
                                       char *path2, size_t size2)
{
    (void)name;
    assert(pair_calls < UI_HISTORY_MAX_RECORDS);
    int first = snprintf(path1, size1, "%s/export-%d%s", test_directory, pair_calls, ext1);
    int second = snprintf(path2, size2, "%s/export-%d%s", test_directory, pair_calls, ext2);
    assert(first > 0 && (size_t)first < size1);
    assert(second > 0 && (size_t)second < size2);
    strcpy(test_paths[pair_calls][0], path1);
    strcpy(test_paths[pair_calls][1], path2);
    ++pair_calls;
    return true;
}
bool usb_storage_commit_file_pair(const char *tmp1, const char *final1,
                                  const char *tmp2, const char *final2)
{
    ++commit_calls;
    if (commit_calls == fail_commit) return false;
    assert(rename(tmp1, final1) == 0);
    assert(rename(tmp2, final2) == 0);
    return true;
}

static char *read_all(const char *path)
{
    FILE *file = fopen(path, "rb");
    assert(file);
    assert(fseek(file, 0, SEEK_END) == 0);
    long size = ftell(file);
    assert(size >= 0);
    rewind(file);
    char *text = calloc((size_t)size + 1U, 1U);
    assert(text && fread(text, 1, (size_t)size, file) == (size_t)size);
    assert(fclose(file) == 0);
    size_t used = 0;
    for (size_t i = 0; i < (size_t)size; i++) {
        if (text[i] != '\r') text[used++] = text[i];
    }
    text[used] = '\0';
    return text;
}

static void unlock_and_reset(void)
{
    history_export_unlock_timer_cb(&test_timer);
    prepare_calls = pair_calls = commit_calls = fail_commit = 0;
    mutate_on_prepare = false;
    prepare_ok = true;
    memset(test_paths, 0, sizeof(test_paths));
}

static void cleanup_outputs(void)
{
    for (int i = 0; i < pair_calls; ++i) {
        unlink(test_paths[i][0]);
        unlink(test_paths[i][1]);
    }
}

int main(int argc, char **argv)
{
    assert(argc == 2 && strlen(argv[1]) < sizeof(test_directory));
    strcpy(test_directory, argv[1]);
    ui_history_record_t *rec = &test_store.records[0];
    *rec = (ui_history_record_t){ .valid = true, .record_no = 91,
        .pcs = 300, .amount = 12510, .year = 2026, .month = 9, .day = 9,
        .hour = 12, .minute = 34, .second = 56 };
    strcpy(rec->currency, "USD");
    strcpy(rec->denom_text, "50 x 1\n100 x 2\n");
    strcpy(rec->sn_detail_text, "1\t100\tAB123456\n2\t100\tA<&\"123\n");
    for (int i = 0; i < 40; ++i) strcat(rec->session_log, "0x05 FD DF 05 05 0A\n");
    strcat(rec->session_log, "0x0C FD DF 07 0C 15 04 0A\n");
    strcpy(rec->error_frame_text, "FD DF 07 0C 15 04 0A");
    test_store.record_count = 1;
    uint32_t ids[] = {91, 92};

    assert(!ui_history_export_data_request_records(NULL, 1));
    assert(!ui_history_export_data_request_records(ids, 0));
    assert(!ui_history_export_data_request_records(ids, UI_HISTORY_MAX_RECORDS + 1U));
    assert(!ui_history_export_data_request_records(ids + 1, 1));
    assert(!ui_history_export_data_request_records(ids, 2));
    uint32_t duplicate_ids[] = {91, 91};
    assert(!ui_history_export_data_request_records(duplicate_ids, 2));
    assert(prepare_calls == 0 && pair_calls == 0);

    mutate_on_prepare = true;
    assert(ui_history_export_data_request_records(ids, 1));
    assert(prepare_calls == 1 && pair_calls == 1);
    assert(!ui_history_export_data_request_records(ids, 1));
    assert(prepare_calls == 1);
    FILE *read_only = fopen(test_paths[0][0], "rb");
    assert(read_only && fputc('X', read_only) == EOF);
    assert(!history_export_flush_and_verify(read_only, test_paths[0][0]));
    char *csv = read_all(test_paths[0][0]);
    char *html = read_all(test_paths[0][1]);
    assert(strstr(csv, "Total Pcs,300\n"));
    assert(strstr(csv, "Total Amount,12510\n"));
    assert(strstr(csv, "Machine Mode,Not recorded\n"));
    assert(strstr(csv, "Record Time,2026-09-09 12:34:56\n"));
    assert(strstr(csv, "Saved Reject Pcs,4\n"));
    assert(strstr(csv, "1,4,\"UV\"\n"));
    assert(strstr(csv, "1,\"AB123456\",100\n"));
    assert(strstr(csv, "2,\"A<&\"\"123\",100\n"));
    assert(strstr(html, "data-target=\"12510\""));
    assert(strstr(html, "data-target=\"300\""));
    assert(strstr(html, "data-sn=\"A&lt;&amp;&quot;123\""));
    assert(strstr(html, "savedRejectPcs:4,rejectDetails:[{no:1,pcs:4,reason:\"UV\"}]"));
    assert(!strstr(html, "Suspect Notes") && !strstr(html, "Damaged Notes"));
    assert(!strstr(html, "All notes passed validation"));
    assert(!strstr(html, "BAT:OFF"));
    free(csv); free(html);
    cleanup_outputs(); unlock_and_reset();

    rec->session_log[0] = rec->error_frame_text[0] = rec->sn_detail_text[0] = '\0';
    assert(ui_history_export_data_request_records(ids, 1));
    html = read_all(test_paths[0][1]);
    assert(strstr(html, "No saved reject detail"));
    assert(!strstr(html, "data-sn=\"NONE\""));
    free(html);
    cleanup_outputs(); unlock_and_reset();

    /* Legacy serial-only records do not establish a zero face value. */
    strcpy(rec->sn_text, "LEGACY001");
    assert(ui_history_export_data_request_records(ids, 1));
    csv = read_all(test_paths[0][0]);
    html = read_all(test_paths[0][1]);
    assert(strstr(csv, "1,\"LEGACY001\",\n"));
    assert(!strstr(csv, "1,\"LEGACY001\",0\n"));
    assert(strstr(html, "LEGACY001</td><td class=\"text-right num-font\">--</td>"));
    free(csv); free(html);
    cleanup_outputs(); unlock_and_reset();
    rec->sn_text[0] = '\0';

    test_store.records[1] = *rec;
    test_store.records[1].record_no = 92;
    test_store.record_count = 2;
    fail_commit = 2;
    assert(!ui_history_export_data_request_records(ids, 2));
    assert(pair_calls == 2 && commit_calls == 2);
    for (int i = 0; i < pair_calls; ++i) {
        assert(access(test_paths[i][0], F_OK) != 0);
        assert(access(test_paths[i][1], F_OK) != 0);
        char tmp[264];
        snprintf(tmp, sizeof(tmp), "%.255s.tmp", test_paths[i][0]);
        assert(access(tmp, F_OK) != 0);
        snprintf(tmp, sizeof(tmp), "%.255s.tmp", test_paths[i][1]);
        assert(access(tmp, F_OK) != 0);
    }
    unlock_and_reset();
    prepare_ok = false;
    assert(!ui_history_export_data_request_records(ids, 1));
    assert(prepare_calls == 1 && pair_calls == 0);
    unlock_and_reset();
    assert(!ui_history_export_data_request());
    assert(prepare_calls == 0);
    test_store.records[1].selected = true;
    assert(ui_history_export_data_request());
    assert(pair_calls == 1);
    cleanup_outputs();
    puts("PASS: saved totals/metadata, all-log reject parsing, empty detail, stable IDs, snapshots, busy/no USB and paired rollback");
    return 0;
}
