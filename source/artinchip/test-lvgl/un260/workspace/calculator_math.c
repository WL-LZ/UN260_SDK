#include "calculator_math.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t magnitude(int64_t v){return v<0?(uint64_t)(-(v+1))+1:(uint64_t)v;}
bool calculator_parse(const char *s,int64_t *out)
{
    if(!s||!out||!*s)return false;
    bool negative=*s=='-';if(negative)++s;
    uint64_t n=0;unsigned digits=0,fraction=0;bool dot=false;
    for(;*s;++s){
        if(*s=='.'&&!dot){dot=true;continue;}
        if(*s<'0'||*s>'9'||(dot&&fraction==6))return false;
        unsigned d=(unsigned)(*s-'0');
        if(n>((uint64_t)CALCULATOR_LIMIT-d)/10)return false;
        n=n*10+d;++digits;if(dot)++fraction;
    }
    if(!digits)return false;
    while(fraction++<6){if(n>(uint64_t)CALCULATOR_LIMIT/10)return false;n*=10;}
    *out=negative?-(int64_t)n:(int64_t)n;return true;
}
void calculator_format(int64_t value,unsigned decimals,char *out,size_t size)
{
    if(decimals>6)decimals=6;
    uint64_t n=magnitude(value),fraction=n%CALCULATOR_SCALE;
    char tail[8];snprintf(tail,sizeof(tail),"%06u",(unsigned)fraction);tail[decimals]=0;
    snprintf(out,size,decimals?"%s%llu.%s":"%s%llu",value<0?"-":"",(unsigned long long)(n/CALCULATOR_SCALE),tail);
}
static bool muldiv(uint64_t a,uint64_t b,uint64_t d,uint64_t *out)
{
    if(!d)return false;
    uint64_t q=0,r=0;
    for(int bit=63;bit>=0;--bit){
        r=r*2+((a>>bit)&1U)*b;
        uint64_t add=r/d;r%=d;
        if(add>(uint64_t)CALCULATOR_LIMIT||q>((uint64_t)CALCULATOR_LIMIT-add)/2)return false;
        q=q*2+add;
    }
    if(r>=(d+1)/2){if(q==(uint64_t)CALCULATOR_LIMIT)return false;++q;}
    *out=q;return true;
}
calculator_error_t calculator_calculate(int64_t a,char op,int64_t b,int64_t *out)
{
    if(!out||magnitude(a)>CALCULATOR_LIMIT||magnitude(b)>CALCULATOR_LIMIT)return CALCULATOR_INVALID;
    int64_t result;
    if(op=='+'||op=='-')result=op=='+'?a+b:a-b;
    else if(op=='*'||op=='/'){
        if(op=='/'&&!b)return CALCULATOR_DIV_ZERO;
        uint64_t n;
        if(!muldiv(magnitude(a),op=='*'?magnitude(b):CALCULATOR_SCALE,
                   op=='*'?CALCULATOR_SCALE:magnitude(b),&n))return CALCULATOR_OVERFLOW;
        result=(a<0)!=(b<0)?-(int64_t)n:(int64_t)n;
    }else return CALCULATOR_INVALID;
    if(magnitude(result)>CALCULATOR_LIMIT)return CALCULATOR_OVERFLOW;
    *out=result;return CALCULATOR_OK;
}
int calculator_currency_decimals(const char *code)
{
    if(!code||strlen(code)!=3)return -1;
    static const char *const zero[]={"JPY","KRW","ISK","XOF","XAF"};
    static const char *const three[]={"OMR","IQD","BHD","KWD","JOD","TND","LYD"};
    static const char *const two[]={"EUR","USD","CNY","RUB","TRY","GBP","MXN","CAD","ILS","AED","SAR","IRR","HKD","AUD","SGD","CHF","ZAR","IDR","MOP","PKR","QAR","THB","TJS","UAH","AMD","AZN","LBP","EGP","PHP","INR","SOS","MAD","DZD","PLN"};
    for(unsigned i=0;i<sizeof(zero)/sizeof(*zero);++i)if(!strcmp(code,zero[i]))return 0;
    for(unsigned i=0;i<sizeof(three)/sizeof(*three);++i)if(!strcmp(code,three[i]))return 3;
    for(unsigned i=0;i<sizeof(two)/sizeof(*two);++i)if(!strcmp(code,two[i]))return 2;
    return -1;
}
bool calculator_amount_precision(const char code[4],int64_t amount)
{
    if(!code||code[3])return false;
    int decimals=calculator_currency_decimals(code);if(decimals<0)return false;
    int64_t unit=1;for(int i=decimals;i<6;++i)unit*=10;
    return amount>=0&&amount<=CALCULATOR_LIMIT&&amount%unit==0;
}
static bool precise(const calculator_snapshot_t *s){return calculator_amount_precision(s->code,s->amount);}
calculator_error_t calculator_snapshot_total(calculator_snapshot_t *s)
{
    if(!s||s->count>CALCULATOR_DENOMS)return CALCULATOR_INVALID;
    int64_t total=0;
    for(unsigned i=0;i<s->count;++i){
        const calculator_denom_t *d=&s->denoms[i];
        if(d->value<=0||d->value>CALCULATOR_LIMIT||d->quantity>99999)return CALCULATOR_INVALID;
        calculator_snapshot_t face={0};memcpy(face.code,s->code,4);face.amount=d->value;
        if(!precise(&face))return CALCULATOR_PRECISION;
        for(unsigned j=0;j<i;++j)if(s->denoms[j].value==d->value)return CALCULATOR_INVALID;
        if(d->quantity&&(d->value>(CALCULATOR_LIMIT-total)/d->quantity))return CALCULATOR_OVERFLOW;
        total+=d->value*d->quantity;
    }
    s->amount=total;return precise(s)?CALCULATOR_OK:CALCULATOR_PRECISION;
}
calculator_error_t calculator_convert(int64_t amount,int64_t rate,const char *target,int64_t *out)
{
    int decimals=calculator_currency_decimals(target);if(!out||amount<=0||rate<=0||rate>999999999999LL||decimals<0)return CALCULATOR_INVALID;
    uint64_t unit=1,n;for(int i=decimals;i<6;++i)unit*=10;
    /* Round only once, to the target currency quantum, not twice via micros. */
    if(!muldiv((uint64_t)amount,(uint64_t)rate,CALCULATOR_SCALE*unit,&n)||n>(uint64_t)CALCULATOR_LIMIT/unit)return CALCULATOR_OVERFLOW;
    *out=(int64_t)(n*unit);return *out>0?CALCULATOR_OK:CALCULATOR_PRECISION;
}
