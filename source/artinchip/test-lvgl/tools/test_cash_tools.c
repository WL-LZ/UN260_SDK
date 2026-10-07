#include "un260/workspace/cash_tools.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static cash_entry_t entry(unsigned kind,int amount){cash_entry_t e={0};e.kind=kind;e.time=1791244800;e.cash.amount=(int64_t)amount*CASH_SCALE;strcpy(e.cash.code,"USD");return e;}
int main(void){
 int64_t n;assert(cash_parse("0.1",&n)&&n==100000);assert(!cash_parse("1.0000001",&n));assert(!cash_parse("--1",&n));
 assert(cash_calculate(100000,'+',200000,&n)==CASH_OK&&n==300000);
 assert(cash_calculate(1000000,'/',3000000,&n)==CASH_OK&&n==333333);
 assert(cash_calculate(-1000000,'/',6000000,&n)==CASH_OK&&n==-166667);
 assert(cash_calculate(CASH_LIMIT,'*',CASH_SCALE,&n)==CASH_OK&&n==CASH_LIMIT);
 assert(cash_calculate(CASH_LIMIT,'*',2*CASH_SCALE,&n)==CASH_OVERFLOW);
 assert(cash_calculate(1,'/',0,&n)==CASH_DIV_ZERO);
 cash_journal_t *j=calloc(1,sizeof(*j));cash_entry_t e=entry(CASH_RECEIPT,50);
 assert(cash_journal_append(j,&e)==CASH_NEEDS_OPENING);e=entry(CASH_OPENING,0);assert(cash_journal_append(j,&e)==CASH_OK);
 assert(cash_journal_append(j,&e)==CASH_ALREADY_OPEN);e=entry(CASH_RECEIPT,100);e.cash.record=42;e.cash.recorded_at=20261006100000ULL;
 assert(cash_journal_append(j,&e)==CASH_OK);assert(cash_journal_append(j,&e)==CASH_DUPLICATE);
 e=entry(CASH_PAYMENT,101);assert(cash_journal_append(j,&e)==CASH_BALANCE);e.cash.amount=80*CASH_SCALE;assert(cash_journal_append(j,&e)==CASH_OK);
 e=j->entries[1];e.cash.amount=90*CASH_SCALE;assert(cash_journal_correct(j,2,&e,e.time)==CASH_OK);assert(cash_journal_balance(j,"USD")==10*CASH_SCALE);
 assert(cash_journal_valid(j));assert(cash_journal_correct(j,2,&e,e.time)==CASH_REVERSED);
 cash_entry_t r=j->entries[4];r.kind=CASH_REVERSAL;r.reverse_of=5;r.corrects=0;assert(cash_journal_append(j,&r)==CASH_BALANCE);
 e=entry(CASH_OPENING,20);strcpy(e.cash.code,"EUR");assert(cash_journal_append(j,&e)==CASH_OK);assert(cash_journal_balance(j,"USD")==10*CASH_SCALE);
 e=entry(CASH_RECEIPT,0);assert(cash_journal_append(j,&e)==CASH_INVALID);e.cash.amount=1;assert(cash_journal_append(j,&e)==CASH_PRECISION);
 assert(cash_journal_valid(j));
 e=j->entries[0];e.cash.amount=10*CASH_SCALE;assert(cash_journal_correct(j,1,&e,e.time)==CASH_OK);assert(cash_journal_valid(j));assert(cash_journal_balance(j,"USD")==20*CASH_SCALE);
 j->entries[j->count-1].kind=CASH_REVERSAL;j->entries[j->count-1].reverse_of=UINT32_MAX;assert(!cash_journal_valid(j));
 memset(j,0,sizeof(*j));e=entry(CASH_OPENING,0);assert(cash_journal_append(j,&e)==CASH_OK);
 e=entry(CASH_RECEIPT,1);while(j->count<CASH_NEW_LIMIT)assert(cash_journal_append(j,&e)==CASH_OK);assert(cash_journal_append(j,&e)==CASH_FULL);
 e=j->entries[1];e.kind=CASH_REVERSAL;e.reverse_of=2;assert(cash_journal_append(j,&e)==CASH_OK);assert(cash_journal_valid(j));
 memset(j,0,sizeof(*j));e=entry(CASH_OPENING,100);assert(!cash_journal_append(j,&e));strcpy(e.cash.code,"CNY");e.cash.amount=0;assert(!cash_journal_append(j,&e));
 e=entry(CASH_EXCHANGE,10);strcpy(e.peer.code,"CNY");e.peer.amount=70*CASH_SCALE;e.rate=7*CASH_SCALE;assert(!cash_journal_append(j,&e));assert(cash_journal_balance(j,"USD")==90*CASH_SCALE&&cash_journal_balance(j,"CNY")==70*CASH_SCALE);
 e.cash.amount=200*CASH_SCALE;assert(cash_journal_append(j,&e)==CASH_BALANCE);assert(j->count==3&&cash_journal_balance(j,"CNY")==70*CASH_SCALE);
 e=j->entries[2];e.peer.amount=71*CASH_SCALE;assert(!cash_journal_correct(j,3,&e,e.time));assert(cash_journal_balance(j,"CNY")==71*CASH_SCALE&&cash_journal_valid(j));
 e=entry(CASH_AUDIT,89);strcpy(e.peer.code,"USD");e.peer.amount=90*CASH_SCALE;assert(!cash_journal_append(j,&e));assert(cash_journal_balance(j,"USD")==90*CASH_SCALE&&cash_journal_valid(j));
 e=j->entries[4];e.kind=CASH_REVERSAL;e.reverse_of=5;e.corrects=0;assert(!cash_journal_append(j,&e));assert(cash_journal_balance(j,"USD")==100*CASH_SCALE&&cash_journal_balance(j,"CNY")==0&&cash_journal_valid(j));
 assert(!cash_convert(1005000,CASH_SCALE,"USD",&n)&&n==1010000);assert(!cash_convert(1499999,CASH_SCALE,"JPY",&n)&&n==CASH_SCALE);
 memset(j->entries[1].cash.code,'X',4);assert(!cash_journal_valid(j));
 memset(j,0,sizeof(*j));e=entry(CASH_OPENING,100);assert(cash_journal_append(j,&e)==CASH_OK);assert(cash_journal_entry_actions(j,1)==CASH_ACTION_CORRECT);
 e=entry(CASH_AUDIT,99);strcpy(e.peer.code,"USD");e.peer.amount=100*CASH_SCALE;assert(cash_journal_append(j,&e)==CASH_OK);assert(cash_journal_entry_actions(j,2)==CASH_ACTION_VOID);
 e=j->entries[1];e.kind=CASH_REVERSAL;e.reverse_of=2;assert(cash_journal_append(j,&e)==CASH_OK);assert(cash_journal_valid(j)&&cash_journal_balance(j,"USD")==100*CASH_SCALE);assert(cash_entry_is_audit(j,&j->entries[2]));assert(!cash_journal_entry_actions(j,2)&&!cash_journal_entry_actions(j,3));
 free(j);puts("PASS arithmetic boundaries, precision, opening, currencies, duplicate, insufficient funds, atomic correction, audit");
}
