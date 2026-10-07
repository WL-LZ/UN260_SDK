#ifndef UN260_OPERATOR_INITIAL_H
#define UN260_OPERATOR_INITIAL_H
#include <stddef.h>
static inline void operator_initial(const char *name,char out[5])
{
    unsigned char c=name?(unsigned char)name[0]:0;
    unsigned n=c<0x80?1:(c&0xE0)==0xC0?2:(c&0xF0)==0xE0?3:(c&0xF8)==0xF0?4:0;
    if(!c||!n){out[0]='?';out[1]=0;return;}
    for(unsigned i=1;i<n;++i)if(!name[i]||((unsigned char)name[i]&0xC0)!=0x80){out[0]='?';out[1]=0;return;}
    for(unsigned i=0;i<n;++i)out[i]=name[i];out[n]=0;
    if(n==1&&c>='a'&&c<='z')out[0]=(char)(c-'a'+'A');
}
#endif
