#include "un260/workspace/cashbook.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static cashbook_t book;
static char reason[160];
static cashbook_result_t result(unsigned source,unsigned pcs,unsigned amount,const char *code)
{
    cashbook_result_t r={.source=source,.day=20260923,.operator_id=1,.operator_name="Alex",.pcs=pcs,.hour=10,.currencies=1,.complete=true};
    strcpy(r.money[0].code,code);r.money[0].pcs=pcs;r.money[0].amount=amount;return r;
}
static bool op(cashbook_operation_t operation,unsigned group,unsigned run,unsigned day)
{
    cashbook_command_t c={.operation=operation,.group=group,.run=run,.day=day,.operator_id=1};
    cashbook_t *draft=malloc(sizeof(*draft));assert(draft);*draft=book;
    bool ok=cashbook_apply(draft,&c,reason,sizeof(reason));if(ok){assert(cashbook_valid(draft));book=*draft;}free(draft);return ok;
}
static void ingest(cashbook_result_t r)
{cashbook_command_t c={.operation=CASHBOOK_INGEST,.result=r};assert(cashbook_apply(&book,&c,reason,sizeof(reason)));assert(cashbook_valid(&book));}
static uint64_t total(const char *code,unsigned *pending)
{
    cashbook_money_t money[CASHBOOK_CURRENCIES];unsigned n,confirmed;
    assert(cashbook_totals(&book,20260923,money,&n,&confirmed,pending));
    for(unsigned i=0;i<n;i++)if(!strcmp(code,money[i].code))return money[i].amount;return 0;
}
int main(void)
{
    assert(cashbook_day_valid(20240229));assert(!cashbook_day_valid(20250229));assert(!cashbook_day_valid(19700101));
    cashbook_defaults(&book);unsigned pending;
    cashbook_result_t a=result(1,100,10000,"CNY");a.sample_count=4;for(unsigned i=0;i<4;i++)a.samples[i]=i+1;
    ingest(a);assert(total("CNY",&pending)==0&&pending==1);assert(!op(CASHBOOK_CLOSE,0,0,20260923));
    assert(op(CASHBOOK_CONFIRM,1,1,0));assert(total("CNY",&pending)==10000&&!pending);
    ingest(a);assert(book.run_count==1); /* exact retry is idempotent */
    cashbook_result_t b=a;b.source=2;b.pcs=99;b.money[0].pcs=99;b.money[0].amount=9900;ingest(b);
    assert(book.runs[1].candidate==1);assert(total("CNY",&pending)==10000&&pending==1);
    assert(op(CASHBOOK_MERGE,1,2,0));assert(book.runs[1].group==1);assert(total("CNY",&pending)==10000&&pending==1);
    assert(op(CASHBOOK_SELECT,1,2,0));assert(total("CNY",&pending)==9900&&!pending);
    b.source=3;b.pcs=100;b.money[0].pcs=100;b.money[0].amount=10000;b.recount_group=1;ingest(b);
    assert(book.runs[2].group==1&&book.groups[0].selected==2&&book.groups[0].review);
    assert(op(CASHBOOK_SELECT,1,3,0));assert(total("CNY",&pending)==10000);
    assert(op(CASHBOOK_SPLIT,1,2,0));assert(book.runs[1].group==3&&total("CNY",&pending)==10000&&pending==2);
    assert(op(CASHBOOK_EXCLUDE,3,0,0));assert(op(CASHBOOK_SELECT,1,3,0));
    ingest(result(4,12,240,"USD"));assert(op(CASHBOOK_CONFIRM,4,4,0));assert(total("CNY",&pending)==10000&&total("USD",&pending)==240);
    assert(op(CASHBOOK_CLOSE,0,0,20260923));assert(book.closes[0].groups==2&&book.closes[0].currencies==2);
    assert(!op(CASHBOOK_SELECT,1,1,0));
    b.source=3;b.money[0].amount=9999;ingest(b);assert(book.groups[4].day==0); /* late update never changes closed selection */
    assert(book.closes[0].money[0].amount==10000);
    assert(op(CASHBOOK_REOPEN,0,0,20260923));assert(!book.closes[0].current);
    assert(op(CASHBOOK_SELECT,1,1,0));assert(op(CASHBOOK_CLOSE,0,0,20260923));assert(book.closes[1].revision==2);
    assert(op(CASHBOOK_SET_DAY,0,0,20260924));assert(book.groups[4].day==20260924);
    cashbook_defaults(&book);
    a=result(10,100,500,"CNY");a.day=0;a.sample_count=0;ingest(a);assert(!op(CASHBOOK_CONFIRM,1,1,0));
    assert(op(CASHBOOK_SET_DAY,0,0,20260923));assert(op(CASHBOOK_CONFIRM,1,1,0));
    a.pcs=200;a.money[0].pcs=200;a.money[0].amount=1000;a.cumulative=true;ingest(a);
    assert(book.group_count==1&&book.run_count==2&&total("CNY",&pending)==500&&pending==1);
    assert(op(CASHBOOK_SELECT,1,2,0));assert(total("CNY",&pending)==1000); /* cumulative is not 1500 */
    assert(op(CASHBOOK_EXCLUDE,1,0,0));assert(total("CNY",&pending)==0);assert(op(CASHBOOK_RESTORE,1,0,0));assert(total("CNY",&pending)==0&&pending==1);
    b=result(11,1,0,"CNY");b.complete=false;ingest(b);assert(!op(CASHBOOK_CONFIRM,2,3,0));
    b=result(12,10,500,"CNY");ingest(b);
    assert(op(CASHBOOK_CONFIRM_SINGLES,0,0,20260923));
    assert(!book.groups[0].confirmed&&!book.groups[1].confirmed&&book.groups[2].confirmed);
    assert(total("CNY",&pending)==500&&pending==2);
    printf("PASS cashbook: immutable attempts, idempotency, suggested/explicit recount, selection, split/exclude, per-currency totals, closed revisions, invalid clock, cumulative replacement (%zu bytes model)\n",sizeof(book));
}
