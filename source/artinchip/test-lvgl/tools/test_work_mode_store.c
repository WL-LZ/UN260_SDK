#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "un260/storage/work_mode_store.c"

static bool complete(work_mode_record_t *record, bool expected_load)
{
    bool load = false, ok = false;
    for (unsigned i = 0; i < 5000; ++i) {
        if (work_mode_store_poll(record, &load, &ok)) {
            assert(load == expected_load && !work_mode_store_busy());
            return ok;
        }
        usleep(1000);
    }
    assert(!"storage worker did not finish");
    return false;
}
int main(void)
{
    work_mode_record_t record = {0}, saved = {true, 0, true};
    assert(work_mode_store_begin_load());
    assert(complete(&record, true) && !record.preferred_valid);
    assert(work_mode_store_begin_save(&saved));
    assert(!work_mode_store_begin_load());
    assert(complete(&record, false));
    assert(access(TEMP_PATH, F_OK) != 0);
    assert(work_mode_store_begin_load() && complete(&record, true));
    assert(record.preferred_valid && record.preferred == 0 && record.restore_pending);
    work_mode_record_t invalid = {false, 0, true};
    assert(!work_mode_store_begin_save(&invalid));

    FILE *file = fopen(STORE_PATH, "wb");
    assert(file && fwrite("broken", 1, 6, file) == 6 && fclose(file) == 0);
    assert(work_mode_store_begin_load() && !complete(&record, true));
    /* A failed operation does not stall the shared storage lane. */
    assert(work_mode_store_begin_save(&saved) && complete(&record, false));
    assert(mkdir(TEMP_PATH, 0700) == 0);
    saved.preferred = 1;
    assert(work_mode_store_begin_save(&saved) && !complete(&record, false));
    assert(rmdir(TEMP_PATH) == 0);
    assert(work_mode_store_begin_load() && complete(&record, true));
    assert(record.preferred == 0 && record.restore_pending);
    assert(work_mode_store_begin_save(&saved) && complete(&record, false));
    assert(work_mode_store_begin_load() && complete(&record, true));
    assert(record.preferred == 1);
    assert(storage_worker_shutdown());
    puts("work_mode_store: real async atomic save/load/corruption/failure isolation PASS");
    return 0;
}
