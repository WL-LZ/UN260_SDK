#define _GNU_SOURCE
#include "boot_bundle.h"
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct { uint32_t h[8]; uint64_t bytes; unsigned used; unsigned char block[64]; } sha256_t;
static uint32_t rotate(uint32_t value, unsigned count) { return (value >> count) | (value << (32U-count)); }
static void sha_block(sha256_t *s, const unsigned char *p)
{
    static const uint32_t k[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };
    uint32_t w[64];
    for(unsigned i=0;i<16;i++)w[i]=((uint32_t)p[i*4]<<24)|((uint32_t)p[i*4+1]<<16)|((uint32_t)p[i*4+2]<<8)|p[i*4+3];
    for(unsigned i=16;i<64;i++) {
        uint32_t a=w[i-15],b=w[i-2];
        w[i]=w[i-16]+(rotate(a,7)^rotate(a,18)^(a>>3))+w[i-7]+(rotate(b,17)^rotate(b,19)^(b>>10));
    }
    uint32_t a=s->h[0],b=s->h[1],c=s->h[2],d=s->h[3],e=s->h[4],f=s->h[5],g=s->h[6],h=s->h[7];
    for(unsigned i=0;i<64;i++) {
        uint32_t t1=h+(rotate(e,6)^rotate(e,11)^rotate(e,25))+((e&f)^((~e)&g))+k[i]+w[i];
        uint32_t t2=(rotate(a,2)^rotate(a,13)^rotate(a,22))+((a&b)^(a&c)^(b&c));
        h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    s->h[0]+=a;s->h[1]+=b;s->h[2]+=c;s->h[3]+=d;s->h[4]+=e;s->h[5]+=f;s->h[6]+=g;s->h[7]+=h;
}
static void sha_update(sha256_t *s,const unsigned char *p,size_t length)
{
    s->bytes+=length;
    while(length) {
        size_t take=64U-s->used;if(take>length)take=length;
        memcpy(s->block+s->used,p,take);s->used+=(unsigned)take;p+=take;length-=take;
        if(s->used==64U){sha_block(s,s->block);s->used=0;}
    }
}
static sha256_t sha_initial(void)
{
    sha256_t s={{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19},0,0,{0}};
    return s;
}
static void sha_finish(sha256_t *s,char digest[65])
{
    uint64_t bits=s->bytes*8U;s->block[s->used++]=0x80;
    if(s->used>56U){memset(s->block+s->used,0,64U-s->used);sha_block(s,s->block);s->used=0;}
    memset(s->block+s->used,0,56U-s->used);
    for(unsigned i=0;i<8;i++)s->block[63U-i]=(unsigned char)(bits>>(i*8U));
    sha_block(s,s->block);
    for(unsigned i=0;i<8;i++)snprintf(digest+i*8,9,"%08x",s->h[i]);
}
bool boot_bundle_sha256_fd(int fd,char digest[65])
{
    sha256_t s=sha_initial();unsigned char data[8192];off_t offset=0;
    for(;;) {
        ssize_t n=pread(fd,data,sizeof(data),offset);
        if(n<0){if(errno==EINTR)continue;return false;}
        if(!n)break;
        sha_update(&s,data,(size_t)n);offset+=n;
    }
    sha_finish(&s,digest);
    return true;
}
static bool hex64(const char *value)
{
    if(strlen(value)!=64U)return false;
    for(unsigned i=0;i<64;i++)if(!((value[i]>='0'&&value[i]<='9')||(value[i]>='a'&&value[i]<='f')))return false;
    return true;
}
static bool safe_file(int fd,uint64_t maximum,uint64_t *size,bool executable)
{
    struct stat st;
    if(fd<0||fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size<=0||(uint64_t)st.st_size>maximum||
       st.st_uid!=geteuid()||(st.st_mode&(0022|S_ISUID|S_ISGID))||
       (executable&&!(st.st_mode&0100)))return false;
    if(size)*size=(uint64_t)st.st_size;
    return true;
}
static bool safe_directory(int fd)
{
    struct stat st;
    return fd>=0&&!fstat(fd,&st)&&S_ISDIR(st.st_mode)&&st.st_uid==geteuid()&&!(st.st_mode&0022);
}
static bool read_text(int fd,char *text,size_t capacity)
{
    uint64_t length;
    if(!safe_file(fd,capacity-1U,&length,false))return false;
    size_t done=0;
    while(done<length) {
        ssize_t n=pread(fd,text+done,(size_t)length-done,(off_t)done);
        if(n<0&&errno==EINTR)continue;
        if(n<=0)return false;
        done+=(size_t)n;
    }
    if(memchr(text,0,done)||text[done-1]!='\n')return false;
    text[done]=0;return true;
}
static char *line(char **cursor)
{
    char *p=*cursor,*end=strchr(p,'\n');
    if(!end)return NULL;
    *end=0;*cursor=end+1;return p;
}
static bool decimal(const char *value,uint64_t maximum,uint64_t *result)
{
    if(!value[0])return false;
    uint64_t n=0;
    for(const char *p=value;*p;p++) {
        if(*p<'0'||*p>'9'||n>(maximum-(unsigned)(*p-'0'))/10U)return false;
        n=n*10U+(unsigned)(*p-'0');
    }
    if(!n||n>maximum)return false;
    *result=n;return true;
}
static bool static_elf(int fd,uint64_t size)
{
    Elf64_Ehdr eh;
    if(pread(fd,&eh,sizeof(eh),0)!=(ssize_t)sizeof(eh)||memcmp(eh.e_ident,ELFMAG,SELFMAG)||
       eh.e_ident[EI_CLASS]!=ELFCLASS64||eh.e_ident[EI_DATA]!=ELFDATA2LSB||eh.e_ident[EI_VERSION]!=EV_CURRENT||
       eh.e_version!=EV_CURRENT||eh.e_ehsize!=sizeof(eh)||
       (eh.e_type!=ET_EXEC&&eh.e_type!=ET_DYN)||eh.e_phentsize!=sizeof(Elf64_Phdr)||
       !eh.e_phnum||eh.e_phnum>128||eh.e_phoff<sizeof(eh)||eh.e_phoff>size||
       (uint64_t)eh.e_phnum*sizeof(Elf64_Phdr)>size-eh.e_phoff)return false;
#ifdef __riscv
    if(eh.e_machine!=EM_RISCV)return false;
#endif
    bool executable_entry=false;
    for(unsigned i=0;i<eh.e_phnum;i++) {
        Elf64_Phdr ph;
        if(pread(fd,&ph,sizeof(ph),(off_t)(eh.e_phoff+(uint64_t)i*sizeof(ph)))!=(ssize_t)sizeof(ph)||ph.p_type==PT_INTERP)return false;
        if(ph.p_type==PT_LOAD) {
            if(ph.p_offset>size||ph.p_filesz>size-ph.p_offset||ph.p_memsz<ph.p_filesz||
               ph.p_memsz>UINT64_MAX-ph.p_vaddr||
               (ph.p_align>1&&((ph.p_align&(ph.p_align-1))||ph.p_offset%ph.p_align!=ph.p_vaddr%ph.p_align)))return false;
            if((ph.p_flags&PF_X)&&eh.e_entry>=ph.p_vaddr&&eh.e_entry-ph.p_vaddr<ph.p_memsz)executable_entry=true;
        }
    }
    return executable_entry;
}
static bool open_version(int versions,const char *id,const char *manifest_hash,boot_bundle_t *out)
{
    int directory=-1,manifest=-1;bool ok=false;char text[1536],hash[65],player_hash[65]="",theme_hash[65]="",mode[10]="";
    uint64_t player_bytes=0,theme_bytes=0,actual=0;unsigned fields=0;
    directory=openat(versions,id,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(!safe_directory(directory))goto done;
    manifest=openat(directory,"manifest",O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    if(!read_text(manifest,text,sizeof(text))||!boot_bundle_sha256_fd(manifest,hash)||strcmp(hash,manifest_hash))goto done;
    char *cursor=text,*entry;
    while((entry=line(&cursor))!=NULL) {
        char *equal=strchr(entry,'=');if(!equal||equal==entry)goto done;
        *equal++=0;unsigned bit=0;
        if(!strcmp(entry,"schema")){bit=1U;if(strcmp(equal,"un260-boot-bundle-v1"))goto done;}
        else if(!strcmp(entry,"id")){bit=2U;if(strcmp(equal,id))goto done;}
        else if(!strcmp(entry,"protocol")){bit=4U;if(strcmp(equal,"1"))goto done;}
        else if(!strcmp(entry,"player_sha256")){bit=8U;if(!hex64(equal))goto done;strcpy(player_hash,equal);}
        else if(!strcmp(entry,"player_bytes")){bit=16U;if(!decimal(equal,BOOT_BUNDLE_MAX_PLAYER_BYTES,&player_bytes))goto done;}
        else if(!strcmp(entry,"theme_sha256")){bit=32U;if(!hex64(equal))goto done;strcpy(theme_hash,equal);}
        else if(!strcmp(entry,"theme_bytes")){bit=64U;if(!decimal(equal,BOOT_BUNDLE_MAX_THEME_BYTES,&theme_bytes))goto done;}
        else if(!strcmp(entry,"mode")){bit=128U;if(strcmp(equal,"animation")&&strcmp(equal,"logo"))goto done;strcpy(mode,equal);}
        else goto done;
        if(fields&bit)goto done;
        fields|=bit;
    }
    if(*cursor||fields!=255U)goto done;
    char identity[192];
    int identity_length=snprintf(identity,sizeof(identity),"player_sha256=%s\ntheme_sha256=%s\nmode=%s\n",player_hash,theme_hash,mode);
    sha256_t identity_sha=sha_initial();
    sha_update(&identity_sha,(const unsigned char *)identity,(size_t)identity_length);
    sha_finish(&identity_sha,hash);
    if(strcmp(hash,id))goto done;
    out->player_fd=openat(directory,"player",O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    out->theme_fd=openat(directory,"theme.pack",O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    if(!safe_file(out->player_fd,BOOT_BUNDLE_MAX_PLAYER_BYTES,&actual,true)||actual!=player_bytes||
       !static_elf(out->player_fd,actual)||!boot_bundle_sha256_fd(out->player_fd,hash)||strcmp(hash,player_hash)||
       !safe_file(out->theme_fd,BOOT_BUNDLE_MAX_THEME_BYTES,&actual,false)||actual!=theme_bytes||
       !boot_bundle_sha256_fd(out->theme_fd,hash)||strcmp(hash,theme_hash))goto done;
    strcpy(out->id,id);strcpy(out->manifest_sha256,manifest_hash);
    strcpy(out->theme_sha256,theme_hash);strcpy(out->mode,mode);ok=true;
done:
    if(manifest>=0)close(manifest);
    if(directory>=0)close(directory);
    if(!ok)boot_bundle_close(out);
    return ok;
}
static bool open_record(const char *active_path,const char *versions_path,bool previous_only,boot_bundle_t *out)
{
    memset(out,0,sizeof(*out));out->player_fd=out->theme_fd=-1;
    int active=open(active_path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC),versions=-1;
    char text[512];bool ok=false;
    if(!read_text(active,text,sizeof(text)))goto done;
    char *cursor=text,*magic=line(&cursor),*id=line(&cursor),*hash=line(&cursor),*previous=line(&cursor),*previous_hash=line(&cursor);
    if(!magic||!id||!hash||!previous||!previous_hash||*cursor||strcmp(magic,BOOT_BUNDLE_ACTIVE_MAGIC)||
       !hex64(id)||!hex64(hash)||
       !((!strcmp(previous,"none")&&!strcmp(previous_hash,"none"))||(hex64(previous)&&hex64(previous_hash)&&strcmp(id,previous))))goto done;
    versions=open(versions_path,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(!safe_directory(versions))goto done;
    if(!previous_only)ok=open_version(versions,id,hash,out);
    if(!ok&&hex64(previous)){ok=open_version(versions,previous,previous_hash,out);out->previous=ok;}
done:
    if(active>=0)close(active);
    if(versions>=0)close(versions);
    return ok;
}
bool boot_bundle_open(const char *active_path,const char *versions_path,bool previous_only,boot_bundle_t *out)
{
    if(open_record(active_path,versions_path,previous_only,out))return true;
    char previous_path[512];
    int count=snprintf(previous_path,sizeof(previous_path),"%s.previous",active_path);
    if(count<0||(size_t)count>=sizeof(previous_path))return false;
    if(open_record(previous_path,versions_path,false,out)){out->previous=true;return true;}
    return false;
}
void boot_bundle_close(boot_bundle_t *bundle)
{
    if(bundle->player_fd>=0)close(bundle->player_fd);
    if(bundle->theme_fd>=0)close(bundle->theme_fd);
    bundle->player_fd=bundle->theme_fd=-1;
}
