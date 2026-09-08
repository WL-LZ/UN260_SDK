#define _GNU_SOURCE
/* Strict ustar/gzip extractor for a private upgrade staging directory.
 * No links, devices, extensions, traversal, overwrite or shell execution.
 * Validation and extraction consume the compressed stream once; extracted
 * files remain untrusted until the installer validates all manifest hashes. */
#include <zlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#define MAX_TOTAL (48U*1024U*1024U)
static int read_exact(gzFile g, void *p, unsigned n) {
    unsigned done=0;
    while(done<n) { int r=gzread(g,(char*)p+done,n-done);if(r<=0)return -1;done+=(unsigned)r; }
    return 0;
}
static int octal(const unsigned char *p,unsigned n,uint64_t *out) {
    uint64_t v=0;
    unsigned i=0;while(i<n && p[i]==' ')i++;
    int digits=0;
    for(;i<n && p[i]>='0' && p[i]<='7';i++) { if(v>UINT64_MAX/8)return -1;v=v*8+p[i]-'0';digits=1; }
    for(;i<n;i++)if(p[i]!=0 && p[i]!=' ')return -1;
    *out=v;return digits?0:-1;
}
static int safe_path(const char *s) {
    if(!*s || *s=='/' || strstr(s,"..") || strstr(s,"//"))return 0;
    for(const unsigned char *p=(const unsigned char*)s;*p;p++)
        if(!isalnum(*p) && *p!='_' && *p!='-' && *p!='.' && *p!='/')return 0;
    return !strcmp(s,"manifest.ini") || !strcmp(s,"checksums.sha256") || !strcmp(s,"install.tsv") ||
        !strcmp(s,"baseline.tsv") || !strcmp(s,"target.tsv") || !strcmp(s,"payload") || !strncmp(s,"payload/",8);
}
static int destination(int root,char *path,int dir) {
    int fd=dup(root);if(fd<0)return -1;
    char *save=NULL,*part=strtok_r(path,"/",&save);
    while(part) {
        char *next=strtok_r(NULL,"/",&save);
        if(next || dir) {
            if(mkdirat(fd,part,0700)<0 && errno!=EEXIST){close(fd);return -1;}
            int n=openat(fd,part,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
            close(fd);if(n<0)return -1;fd=n;
        } else {
            int n=openat(fd,part,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
            close(fd);return n;
        }
        part=next;
    }
    return fd;
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--probe")){puts("UN260_UNPACK_1");return 0;}
    if(argc!=3){fprintf(stderr,"usage: un260_unpack archive stage\n");return 2;}
    gzFile g=gzopen(argv[1],"rb");int root=open(argv[2],O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(!g || root<0){if(g)gzclose(g);if(root>=0)close(root);return 2;}
    unsigned char h[512],buf[32768];uint64_t total=0;unsigned entries=0,zeros=0;int rc=1;
    for(;;) {
        if(read_exact(g,h,512)<0)goto done;
        int zero=1;for(unsigned i=0;i<512;i++)if(h[i]){zero=0;break;}
        if(zero) { if(++zeros==2)break;continue; }
        if(zeros || ++entries>8192)goto done;
        uint64_t expected,size;unsigned sum=0;
        if(octal(h+148,8,&expected)<0 || octal(h+124,12,&size)<0 || size>24U*1024U*1024U)goto done;
        for(unsigned i=0;i<512;i++)sum+=(i>=148 && i<156)?32:h[i];
        if(sum!=expected || memcmp(h+257,"ustar",5) || h[157])goto done;
        if(h[156]!=0 && h[156]!='0' && h[156]!='5')goto done;
        char name[257],path[257];size_t a=strnlen((char*)h,100),b=strnlen((char*)h+345,155);
        if(!a || a==100 || b==155 || a+b+2>sizeof(name))goto done;
        if(b)snprintf(name,sizeof(name),"%.*s/%.*s",(int)b,h+345,(int)a,h);
        else snprintf(name,sizeof(name),"%.*s",(int)a,h);
        size_t len=strlen(name);if(len && name[len-1]=='/')name[--len]=0;
        if(!safe_path(name) || (h[156]=='5' && size))goto done;
        total+=size;if(total>MAX_TOTAL)goto done;
        strcpy(path,name);int fd=destination(root,path,h[156]=='5');if(fd<0)goto done;
        uint64_t remaining=size;
        while(remaining) {
            unsigned n=remaining>sizeof(buf)?sizeof(buf):(unsigned)remaining;
            if(read_exact(g,buf,n)<0){close(fd);goto done;}
            unsigned off=0;while(off<n){ssize_t w=write(fd,buf+off,n-off);if(w<=0){close(fd);goto done;}off+=(unsigned)w;}
            remaining-=n;
        }
        if(close(fd)<0)goto done;
        unsigned pad=(512U-(unsigned)(size%512U))%512U;
        if(pad && read_exact(g,buf,pad)<0)goto done;
    }
    /* Consume to EOF to verify gzip CRC. Only zero tar padding is accepted. */
    for(;;){int n=gzread(g,buf,sizeof(buf));if(n<0)goto done;if(!n)break;
        for(int i=0;i<n;i++)if(buf[i])goto done;
        total+=(unsigned)n;if(total>MAX_TOTAL)goto done;}
    rc=0;
done:
    if(gzclose(g)!=Z_OK)rc=1;
    close(root);
    if(rc)fprintf(stderr,"UNPACK_FAIL invalid/truncated archive or staging I/O; no rootfs files installed\n");
    else fprintf(stderr,"UNPACK_OK entries=%u bytes=%llu passes=1\n",entries,(unsigned long long)total);
    return rc;
}
