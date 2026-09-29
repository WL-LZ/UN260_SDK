#include "ui_message.h"
#include "ui_i18n.h"
#include <stdio.h>
#include <string.h>

static void copy(char *out, size_t capacity, const char *value)
{
    if (!value) value = "";
    size_t length = strlen(value);
    if (length >= capacity) {
        length = capacity - 1;
        while (length && ((unsigned char)value[length] & 0xc0) == 0x80) --length;
    }
    memcpy(out, value, length); out[length] = 0;
}
void ui_message_key(ui_message_t *m, const char *key)
{ if (m) { memset(m, 0, sizeof(*m)); copy(m->key, sizeof(m->key), key); } }
void ui_message_literal(ui_message_t *m, const char *text)
{ ui_message_key(m,text); if(m)m->format=UI_MESSAGE_LITERAL; }
void ui_message_string(ui_message_t *m, const char *key, const char *value)
{ ui_message_key(m,key); if(m){m->format=UI_MESSAGE_STRING;copy(m->argument,sizeof(m->argument),value);} }
void ui_message_uint(ui_message_t *m, const char *key, uint32_t value)
{ ui_message_key(m,key); if(m){m->format=UI_MESSAGE_UINT;m->numbers[0]=value;} }
void ui_message_uint3(ui_message_t *m, const char *key, uint32_t a, uint32_t b, uint32_t c)
{ ui_message_key(m,key); if(m){m->format=UI_MESSAGE_UINT3;m->numbers[0]=a;m->numbers[1]=b;m->numbers[2]=c;} }
void ui_message_uint3_int64(ui_message_t *m, const char *key, uint32_t a, uint32_t b, uint32_t c, int64_t amount)
{ ui_message_uint3(m,key,a,b,c); if(m){m->format=UI_MESSAGE_UINT3_INT64;m->amount=amount;} }
void ui_message_string_uint(ui_message_t *m, const char *key, const char *value, uint32_t count)
{ ui_message_string(m,key,value); if(m){m->format=UI_MESSAGE_STRING_UINT;m->numbers[0]=count;} }
void ui_message_text(ui_message_t *m,const char *key,const char *argument)
{ui_message_string(m,key,argument);if(m)m->argument_is_key=true;}
void ui_message_text_uint(ui_message_t *m,const char *key,const char *argument,uint32_t count)
{ui_message_string_uint(m,key,argument,count);if(m)m->argument_is_key=true;}
bool ui_message_equal(const ui_message_t *a,const ui_message_t *b)
{ return a&&b&&a->format==b->format&&a->argument_is_key==b->argument_is_key&&a->amount==b->amount&&!strcmp(a->key,b->key)&&!strcmp(a->argument,b->argument)&&!memcmp(a->numbers,b->numbers,sizeof(a->numbers)); }

static bool render(const ui_message_t *m,char *out,size_t size,bool localize)
{
    if(!out||!size)return false;
    out[0]=0;if(!m)return true;
    const char *format=localize&&m->format!=UI_MESSAGE_LITERAL?ui_tr(m->key):m->key;
    const char *argument=localize&&m->argument_is_key?ui_tr(m->argument):m->argument;
    int n;
    switch(m->format){
    case UI_MESSAGE_STRING:n=snprintf(out,size,format,argument);break;
    case UI_MESSAGE_UINT:n=snprintf(out,size,format,(unsigned)m->numbers[0]);break;
    case UI_MESSAGE_UINT3:n=snprintf(out,size,format,(unsigned)m->numbers[0],(unsigned)m->numbers[1],(unsigned)m->numbers[2]);break;
    case UI_MESSAGE_UINT3_INT64:n=snprintf(out,size,format,(unsigned)m->numbers[0],(unsigned)m->numbers[1],(unsigned)m->numbers[2],(long long)m->amount);break;
    case UI_MESSAGE_STRING_UINT:n=snprintf(out,size,format,argument,(unsigned)m->numbers[0]);break;
    default:n=snprintf(out,size,"%s",format);break;
    }
    if(n<0){out[0]=0;return false;}
    if((size_t)n<size)return true;
    /* snprintf may cut inside a multibyte sequence; drop its incomplete tail. */
    size_t end=size-1,start=end;
    while(start&&((unsigned char)out[start-1]&0xc0)==0x80)--start;
    if(start){unsigned char c=(unsigned char)out[start-1];unsigned need=c>=0xf0?4:c>=0xe0?3:c>=0xc0?2:1;if(end-(start-1)<need)out[start-1]=0;}
    return false;
}
bool ui_message_render(const ui_message_t *m,char *out,size_t size){return render(m,out,size,true);}
bool ui_message_source(const ui_message_t *m,char *out,size_t size){return render(m,out,size,false);}
