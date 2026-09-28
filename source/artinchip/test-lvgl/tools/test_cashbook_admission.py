"""Archive admission uses actual queued-source validation, not only queue emptiness."""
from pathlib import Path
import subprocess,tempfile
from test_multi_result_safety import function
root=Path(__file__).resolve().parents[1]
actual=function((root/'un260/counting/counting_history_service.c').read_text(),'counting_history_can_archive')
code=r'''
#include <assert.h>
#include <stdio.h>
#include "un260/storage/cashbook_store.h"
#define STORAGE_JOB_SUCCEEDED 1
static bool g_uncaptured_pending,g_overflow_valid;
static unsigned g_snapshot_count;
typedef struct {unsigned job;cashbook_result_t cashbook;} counting_history_snapshot_t;
static counting_history_snapshot_t g_snapshots[8];
static cashbook_t book;
const cashbook_t *cashbook_store_get(void){return &book;}
static int ui_history_commit_status(unsigned job){return job==1?1:0;}
'''
main=r'''
int main(void){
 cashbook_defaults(&book);book.group_count=book.run_count=1;
 book.groups[0].id=1;book.runs[0].id=1;book.runs[0].group=1;book.runs[0].result.source=41;
 assert(counting_history_can_archive());g_snapshot_count=1;
 g_snapshots[0].cashbook.source=42;assert(!counting_history_can_archive());
 g_snapshots[0].job=1;assert(counting_history_can_archive());
 g_snapshots[0].cashbook.source=41;assert(!counting_history_can_archive());
 book.groups[0].excluded=true;assert(counting_history_can_archive());
 g_overflow_valid=true;assert(!counting_history_can_archive());g_overflow_valid=false;
 g_uncaptured_pending=true;assert(!counting_history_can_archive());
 puts("PASS archive admission: no full-ledger deadlock for independent pending counts; included cumulative revisions cannot cross periods");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-archive-admission-') as tmp:
    tmp=Path(tmp);src=tmp/'test.c';exe=tmp/'test';src.write_text(code+actual+main)
    subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie',f'-I{root}',str(src),str(root/'un260/workspace/cashbook.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
