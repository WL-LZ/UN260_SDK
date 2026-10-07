from pathlib import Path
import tempfile,subprocess,csv
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='cash-export-') as directory:
 p=Path(directory);usb=p/'usb';usb.mkdir()
 for name,text in {
 'un260/lv_components/ui_notice.h':'typedef enum {UI_NOTICE_SUCCESS,UI_NOTICE_ERROR} ui_notice_kind_t;void ui_notice_post_text(ui_notice_kind_t,const char*,const char*,const char*);',
 'un260/lv_system/ui_i18n.h':'#define UI_N_(s) s\n',
 }.items():
  h=p/name;h.parent.mkdir(parents=True,exist_ok=True);h.write_text(text)
 source=p/'test.c';source.write_text(r'''
#include "un260/storage/cash_journal_export.h"
#include "un260/storage/cash_journal_store.h"
#include "un260/storage/storage_worker.h"
#include "un260/lv_components/ui_notice.h"
#include <assert.h>
#include <unistd.h>
#include <string.h>
static cash_journal_t journal;
static bool usb=true;
static unsigned notices;static ui_notice_kind_t kind;
const cash_journal_t *cash_journal_store_get(void){return &journal;}
bool usb_storage_prepare(void){return usb;}
void ui_notice_post_text(ui_notice_kind_t k,const char *a,const char *b,const char *c){(void)a;(void)b;(void)c;kind=k;++notices;}
static void drain(unsigned expected){for(unsigned i=0;i<5000&&notices<expected;++i){usleep(1000);cash_journal_export_poll();}assert(notices==expected);assert(storage_worker_pending_status()==STORAGE_JOB_SUCCEEDED);}
int main(void){assert(storage_worker_init());journal.count=2;journal.entries[0]=(cash_entry_t){.id=1,.kind=CASH_RECEIPT,.cash={.code="USD",.amount=1230000}};strcpy(journal.entries[0].operator_name,"=1+1");strcpy(journal.entries[0].note,"a,\"b\"\nline");journal.entries[1].id=2;uint32_t ids[]={1};assert(cash_journal_export_start(ids,1));assert(!cash_journal_export_start(ids,1));strcpy(journal.entries[0].note,"later mutation");drain(1);assert(kind==UI_NOTICE_SUCCESS);usb=false;assert(cash_journal_export_start(ids,1));drain(2);assert(kind==UI_NOTICE_ERROR);usb=true;assert(cash_journal_export_start(ids,1));drain(3);assert(kind==UI_NOTICE_SUCCESS);assert(storage_worker_shutdown());}
''')
 exe=p/'test';subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-no-pie',f'-I{p}',f'-I{r}',f'-DCASH_EXPORT_DIRECTORY="{usb}"',str(source),str(r/'un260/storage/cash_journal_export.c'),str(r/'un260/storage/storage_worker.c'),str(r/'un260/workspace/cash_tools.c'),'-pthread','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
 files=sorted(usb.glob('*.csv'));assert len(files)==2
 records=[list(csv.DictReader(f.open(encoding='utf-8-sig',newline=''))) for f in files]
 assert all(len(rows)==1 and rows[0]['Operator']=="'=1+1" for rows in records)
 assert any(rows[0]['Note']=='a,"b"\nline' for rows in records),records
 print('PASS cash export: immutable filtered snapshot, escaped CSV/formula text, double-submit guard, USB failure releases storage lane, retry')
