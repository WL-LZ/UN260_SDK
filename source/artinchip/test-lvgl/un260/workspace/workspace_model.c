#include "workspace_model.h"
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <stddef.h>
_Static_assert(offsetof(workspace_user_t,profiles)==44,"workspace header layout changed");
bool workspace_name_valid(const char *s)
{
    if (!s || !*s) return false;
    unsigned n = 0;
    for (; n <= WORKSPACE_NAME && s[n]; ++n)
        if ((unsigned char)s[n] < 32 || (unsigned char)s[n] > 126) return false;
    return n && n <= WORKSPACE_NAME && s[0] != ' ' && s[n-1] != ' ';
}
static bool optional_valid(const char *s,unsigned limit)
{
    unsigned n=0;
    while(n<=limit&&s[n]){if((unsigned char)s[n]<32||(unsigned char)s[n]>126)return false;n++;}
    return n<=limit&&(!n||(s[0]!=' '&&s[n-1]!=' '));
}
static bool employee_valid(const char *s)
{
    if(!optional_valid(s,WORKSPACE_EMPLOYEE_ID))return false;
    for(unsigned i=0;s[i];i++)if(!((s[i]>='A'&&s[i]<='Z')||(s[i]>='a'&&s[i]<='z')||(s[i]>='0'&&s[i]<='9')||s[i]=='-'||s[i]=='_'))return false;
    return true;
}
workspace_user_t *workspace_find(workspace_model_t *m, uint32_t id)
{
    if (!m || m->user_count > WORKSPACE_USERS) return NULL;
    for (unsigned i=0; i<m->user_count; ++i) if (m->users[i].id==id) return &m->users[i];
    return NULL;
}
const workspace_user_t *workspace_active(const workspace_model_t *m)
{
    if (!m || m->user_count > WORKSPACE_USERS) return NULL;
    for (unsigned i=0; i<m->user_count; ++i) if (m->users[i].id==m->active_id) return &m->users[i];
    return NULL;
}
static void seed(workspace_user_t *u)
{
    u->quick_enabled=1;u->batch_count=5;
    const uint8_t batches[]={0,10,50,100,150};memcpy(u->batches,batches,sizeof(batches));
    u->profile_count=1;
    u->profiles[0]=(workspace_profile_t){.id=1,.name="Everyday count",.speed=1,.beep=1,.batch=50,.mode=1};
}
void workspace_defaults(workspace_model_t *m)
{
    memset(m,0,sizeof(*m));m->version=3;m->next_id=2;m->active_id=1;m->user_count=1;
    m->users[0].id=1;strcpy(m->users[0].name,"Local operator");seed(&m->users[0]);
}
bool workspace_model_valid(const workspace_model_t *m)
{
    if (!m || m->version!=3 || !m->next_id || !m->user_count || m->user_count>WORKSPACE_USERS || !workspace_active(m))return false;
    for(unsigned i=0;i<m->user_count;i++) {
        const workspace_user_t *u=&m->users[i];
        if(!u->id||u->id>=m->next_id||!workspace_name_valid(u->name)||!employee_valid(u->employee_id)||!optional_valid(u->team,WORKSPACE_TEAM)||u->quick_enabled>1||u->qr_after_count>1||u->avatar.present>1||
           !u->profile_count||u->profile_count>WORKSPACE_PROFILES||u->batch_count<2||u->batch_count>WORKSPACE_BATCHES||u->batches[0])return false;
        for(unsigned j=0;j<i;j++)if(m->users[j].id==u->id||!strcasecmp(m->users[j].name,u->name)||(*u->employee_id&&!strcasecmp(m->users[j].employee_id,u->employee_id)))return false;
        for(unsigned j=1;j<u->batch_count;j++) {
            if(!u->batches[j]||u->batches[j]>200)return false;
            for(unsigned k=0;k<j;k++)if(u->batches[j]==u->batches[k])return false;
        }
        for(unsigned j=0;j<u->profile_count;j++) {
            const workspace_profile_t *p=&u->profiles[j];
            if(!p->id||!workspace_name_valid(p->name)||p->mode>3||p->speed>2||p->work>1||p->add>1||p->sort>3||p->beep>1||p->batch_enabled>1||p->batch>200||(p->batch_enabled&&!p->batch))return false;
            for(unsigned k=0;k<j;k++)if(u->profiles[k].id==p->id||!strcasecmp(u->profiles[k].name,p->name))return false;
        }
    }return true;
}
bool workspace_add_user(workspace_model_t *m,const char *name,uint32_t *id)
{
    if(!workspace_model_valid(m)||!workspace_name_valid(name)||m->user_count==WORKSPACE_USERS||m->next_id==UINT32_MAX)return false;
    for(unsigned i=0;i<m->user_count;i++)if(!strcasecmp(name,m->users[i].name))return false;
    workspace_user_t *u=&m->users[m->user_count++];memset(u,0,sizeof(*u));
    u->id=m->next_id++;strcpy(u->name,name);seed(u);if(id)*id=u->id;return true;
}
bool workspace_add_profile(workspace_model_t *m,uint32_t user,const workspace_profile_t *profile)
{
    workspace_user_t *u=workspace_find(m,user);
    if(!u||!profile||!workspace_name_valid(profile->name)||u->profile_count>=WORKSPACE_PROFILES)return false;
    uint32_t next=1;
    for(unsigned i=0;i<u->profile_count;i++) {
        if(!strcasecmp(u->profiles[i].name,profile->name)||u->profiles[i].id==UINT32_MAX)return false;
        if(u->profiles[i].id>=next)next=u->profiles[i].id+1;
    }
    workspace_profile_t p=*profile;p.id=next;
    u->profiles[u->profile_count++]=p;
    if(workspace_model_valid(m))return true;
    memset(&u->profiles[--u->profile_count],0,sizeof(p));return false;
}
bool workspace_delete_profile(workspace_model_t *m,uint32_t user,uint32_t profile)
{
    workspace_user_t *u=workspace_find(m,user);
    if(!u||u->profile_count<=1)return false;
    for(unsigned i=0;i<u->profile_count;i++)if(u->profiles[i].id==profile) {
        memmove(&u->profiles[i],&u->profiles[i+1],(u->profile_count-i-1)*sizeof(u->profiles[0]));
        memset(&u->profiles[--u->profile_count],0,sizeof(u->profiles[0]));return true;
    }return false;
}
uint8_t workspace_next_batch(const workspace_user_t *u,uint8_t current)
{
    if(!u||u->batch_count<2||u->batch_count>WORKSPACE_BATCHES)return 0;
    unsigned start=1;
    for(unsigned i=0;i<u->batch_count;i++)if(u->batches[i]==current){start=i+1;break;}
    /* Legacy drafts may contain 200. Preserve them for editing, but never
     * send that OFF sentinel as an enabled preset or strand Main's cycle. */
    for(unsigned i=0;i<u->batch_count;i++){uint8_t value=u->batches[(start+i)%u->batch_count];if(value<200)return value;}
    return 0;
}
