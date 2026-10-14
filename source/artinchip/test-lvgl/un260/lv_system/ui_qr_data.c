#include "ui_qr_data.h"
#include "ui_i18n.h"
#include "user_cfg.h"
#include "machine_time.h"
#include "un260/currency/currency_state.h"
#include "un260/counting/counting_data_store.h"
#include "un260/counting/counting_reject_reason.h"
#include "un260/counting/counting_multi.h"
#include "un260/counting/counting_multi_extra.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
/* One formatter for manual, automatic and island entry points. Append only
 * complete lines, keeping space for an explicit incomplete-report notice. */
typedef struct {char *text;size_t size,used;bool omitted;} report_t;
static bool append(report_t *r,const char *fmt,...)
{
 char line[768];va_list ap;va_start(ap,fmt);int n=vsnprintf(line,sizeof(line),fmt,ap);va_end(ap);
 if(n<0||(size_t)n>=sizeof(line)||r->used+(size_t)n>=r->size){r->omitted=true;return false;}
 memcpy(r->text+r->used,line,(size_t)n+1);r->used+=(size_t)n;return true;
}
bool ui_qr_data_is_ready(void)
{
 const counting_sim_t *d=counting_data_current();
 if(d->multi_currency_result){const counting_multi_t *m=counting_multi_current();return !m->counting&&!m->overflow&&m->count&&m->count<=COUNTING_MULTI_MAX&&m->total_pcs;}
 return counting_data_monetary_result_supported(d)&&(d->total_pcs>0||counting_data_reject_pcs_count(d)>0);
}
static void section(report_t *r,const char *title){append(r,"\n%s\n",ui_tr(title));}
bool ui_qr_data_build(char *buf,size_t capacity)
{
 if(!buf||capacity<512||!ui_qr_data_is_ready())return false;
 if(capacity>1351)capacity=1351;
 buf[0]=0;report_t r={buf,capacity-180,0,false};const counting_sim_t *d=counting_data_current();
 bool serials=user_cfg_qr_option(0),details=user_cfg_qr_option(1),errors=user_cfg_qr_option(2);
 machine_time_value_t now;machine_time_get(&now);
 append(&r,"UN260\n%04u-%02u-%02u %02u:%02u:%02u\n",now.year,now.month,now.day,now.hour,now.minute,now.second);
 if(d->multi_currency_result){
   const counting_multi_t *m=counting_multi_current();uint32_t total=0;
   for(unsigned i=0;i<m->count;i++)total+=m->currencies[i].pcs;if(total!=m->total_pcs)return false;
   for(unsigned i=0;i<m->count;i++){const multi_currency_t *c=&m->currencies[i];append(&r,"%s  %u.00 | %s: %u\n",c->code,c->amount,ui_tr("Notes"),c->pcs);}
   if(r.omitted)return false;
   if(details){section(&r,UI_N_("Denomination details"));for(unsigned i=0;i<m->count;i++){
     const multi_currency_t *c=&m->currencies[i];append(&r,"%s\n",c->code);
     if(c->status!=MULTI_DETAIL_READY&&c->status!=MULTI_DETAIL_EMPTY){append(&r,"%s\n",ui_tr("Details not available"));continue;}
     for(unsigned j=0;j<c->denom_count&&j<COUNTING_MULTI_DENOMS;j++)if(c->denom[j].pcs)append(&r,"%u x %u\n",c->denom[j].value,c->denom[j].pcs);
   }}
   if(errors){section(&r,UI_N_("Error details"));append(&r,"%s: %u\n",ui_tr("Rejected"),m->reject);const multi_reject_cache_t *e=counting_multi_rejects();
     if(m->reject&&e->status!=MULTI_DETAIL_READY)append(&r,"%s\n",ui_tr("Details not available"));
     else for(unsigned i=0;i<e->count;i++)append(&r,"%s x %u\n",counting_reject_reason_get(e->rows[i].code),e->rows[i].pcs);
   }
   if(serials){section(&r,UI_N_("Serial numbers"));for(unsigned i=0;i<m->count;i++){
     append(&r,"%s\n",m->currencies[i].code);const multi_serial_cache_t *s=counting_multi_serials(i);
     if(!s||s->status!=MULTI_DETAIL_READY){append(&r,"%s\n",ui_tr("Details not available"));continue;}
     for(unsigned j=0;j<s->count;j++)if(!append(&r,"%u  %s\n",s->rows[j].number,s->rows[j].text))break;
   }}
 }else{
   char currency[4];currency_state_get_effective_code(currency);if(!currency[0]||currency_state_is_special_code(currency))return false;
   append(&r,"%s  %.2f | %s: %d\n",currency,d->total_amount,ui_tr("Notes"),d->total_pcs);
   if(r.omitted)return false;
   if(details){section(&r,UI_N_("Denomination details"));
     if(!d->denom_number)append(&r,"%s\n",ui_tr("Details not available"));
     for(unsigned i=0;i<d->denom_number&&i<COUNTING_DENOM_MAX_ITEMS;i++)if(d->denom[i].pcs)append(&r,"%d x %u\n",d->denom[i].value,d->denom[i].pcs);
   }
   if(errors){section(&r,UI_N_("Error details"));int count=counting_data_error_detail_count(d);append(&r,"%s: %d\n",ui_tr("Rejected"),counting_data_reject_pcs_count(d));
     if(!count&&counting_data_reject_pcs_count(d))append(&r,"%s\n",ui_tr("Details not available"));
     for(int i=0;i<count;i++)append(&r,"%s x %u\n",counting_reject_reason_get(d->err_code[i]),d->err_pcs?d->err_pcs[i]:0);
   }
   if(serials){section(&r,UI_N_("Serial numbers"));int n=counting_data_serial_scan_limit(d),valid=0;
     for(int i=0;i<n;i++)if(d->sn_str&&d->sn_str[i]&&d->sn_str[i][0]){valid++;if(!append(&r,"%d  %s\n",i+1,d->sn_str[i]))break;}
     if(!valid)append(&r,"%s\n",ui_tr("Details not available"));
   }
 }
 if(r.omitted){r.size=capacity;if(!append(&r,"\n%s\n",ui_tr("Some content omitted. Export to USB for the full report.")))return false;}
 return true;
}
bool ui_qr_data_build_summary(char *buf,size_t capacity){return ui_qr_data_build(buf,capacity);}
