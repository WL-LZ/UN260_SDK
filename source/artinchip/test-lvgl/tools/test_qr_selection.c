#include "un260/lv_system/ui_qr_data.h"
#include "un260/lv_system/machine_time.h"
#include "un260/counting/counting_data_store.h"
#include "un260/counting/counting_multi_extra.h"
#include "un260/lv_components/qrcodegen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static counting_sim_t data;
static counting_multi_t multi;
static multi_serial_cache_t serial;
static multi_reject_cache_t errors;
static bool options[3];
const char *ui_tr(const char *s){return s;}
bool user_cfg_qr_option(unsigned i){return i<3&&options[i];}
void machine_time_get(machine_time_value_t *t){*t=(machine_time_value_t){2026,10,14,12,30,0};}
void currency_state_get_effective_code(char c[4]){memcpy(c,"USD",4);}
bool currency_state_is_special_code(const char c[4]){(void)c;return false;}
const counting_sim_t *counting_data_current(void){return &data;}
bool counting_data_monetary_result_supported(const counting_sim_t *s){return !s->multi_currency_result;}
int counting_data_reject_pcs_count(const counting_sim_t *s){return s->err_expected;}
int counting_data_error_detail_count(const counting_sim_t *s){return s->err_num;}
int counting_data_serial_scan_limit(const counting_sim_t *s){return s->sn_capacity;}
const char *counting_reject_reason_get(uint8_t n){(void)n;return "Double note";}
const counting_multi_t *counting_multi_current(void){return &multi;}
const multi_serial_cache_t *counting_multi_serials(unsigned n){(void)n;return &serial;}
const multi_reject_cache_t *counting_multi_rejects(void){return &errors;}
static void encode(const char *text){uint8_t work[qrcodegen_BUFFER_LEN_MAX],qr[qrcodegen_BUFFER_LEN_MAX];assert(qrcodegen_encodeText(text,work,qr,qrcodegen_Ecc_LOW,qrcodegen_VERSION_MIN,qrcodegen_VERSION_MAX,qrcodegen_Mask_AUTO,true));int modules=qrcodegen_getSize(qr);assert(260/(modules+8)>=2);}
int main(void){
 char out[3072],other[3072];char *sn[]={"AA001",NULL,"ZZ003"};uint8_t ep[]={2},ec[]={1};
 data=(counting_sim_t){.total_pcs=3,.total_amount=300,.denom_number=1,.denom={{100,3,300}},.sn_str=sn,.sn_capacity=3,.err_num=1,.err_expected=2,.err_code=ec,.err_pcs=ep};
 for(unsigned mask=0;mask<8;mask++){
  for(unsigned i=0;i<3;i++)options[i]=(mask>>i)&1;
  assert(ui_qr_data_build(out,sizeof(out)));assert(ui_qr_data_build_summary(other,sizeof(other)));assert(!strcmp(out,other));
  assert(!!strstr(out,"Serial numbers")==options[0]);assert(!!strstr(out,"AA001")==options[0]);assert(!!strstr(out,"100 x 3")==options[1]);assert(!!strstr(out,"Double note")==options[2]);
  if(options[0]){assert(strstr(out,"3  ZZ003"));assert(!strstr(out,"2  ZZ003"));}encode(out);
 }
 char *many[400];for(unsigned i=0;i<400;i++)many[i]="Z12345678901234567890";data.sn_str=many;data.sn_capacity=400;assert(ui_qr_data_build(out,sizeof(out)));assert(strlen(out)<=1350&&strstr(out,"Some content omitted"));encode(out);
 data.multi_currency_result=true;multi.count=2;multi.total_pcs=4;
 for(unsigned i=0;i<2;i++){multi.currencies[i]=(multi_currency_t){.amount=200,.pcs=2,.status=MULTI_DETAIL_READY,.denom_count=1,.denom={{100,2}}};memcpy(multi.currencies[i].code,i?"EUR":"USD",4);}
 serial.status=MULTI_DETAIL_NONE;assert(ui_qr_data_build(out,sizeof(out)));assert(strstr(out,"USD")&&strstr(out,"EUR")&&strstr(out,"Details not available"));encode(out);
 for(unsigned i=0;i<3;i++)options[i]=false;
 multi.count=32;multi.total_pcs=32*65535U;
 for(unsigned i=0;i<32;i++){multi.currencies[i].amount=4294967295U;multi.currencies[i].pcs=65535;snprintf(multi.currencies[i].code,4,"%03u",i);}
 assert(ui_qr_data_build(out,sizeof(out)));encode(out);multi.total_pcs++;assert(!ui_qr_data_build(out,sizeof(out)));
 assert(!ui_qr_data_build(out,8));
 puts("PASS QR: all eight option combinations, preserved serial indices, bounded complete lines, explicit omission, MULTI separation/unavailable details, 32-currency summary, encoded QR >=2px/module");
}
