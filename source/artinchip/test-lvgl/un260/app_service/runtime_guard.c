#define _GNU_SOURCE
#include "runtime_guard.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <ucontext.h>

#ifndef RUNTIME_GUARD_DIR
#define RUNTIME_GUARD_DIR "/etc/un260-runtime-fault"
#endif
#ifndef RUNTIME_GUARD_LOCK
#define RUNTIME_GUARD_LOCK "/var/run/un260-runtime.lock"
#endif
#ifndef RUNTIME_GUARD_DISPLAY
#define RUNTIME_GUARD_DISPLAY "/usr/local/bin/un260_upgrade_display"
#endif
#ifndef RUNTIME_GUARD_STALL_MS
#define RUNTIME_GUARD_STALL_MS 10000U
#endif
#ifndef RUNTIME_GUARD_START_MS
#define RUNTIME_GUARD_START_MS 60000U
#endif
#define RX_SLOTS 2048U
#define TX_SLOTS 256U
typedef struct { uint32_t sequence, length; int32_t completed; uint8_t bytes[256]; } record_t;
typedef struct { int pcs; float amount; unsigned phase; bool busy, pending, multi; } count_t;
typedef struct {
    uint32_t heartbeat, fault, rx_count, tx_count, count_slot, count_valid;
    volatile sig_atomic_t signal_number, signal_code;
    uintptr_t signal_address, signal_pc;
    count_t count[2];
    record_t rx[RX_SLOTS], tx[TX_SLOTS];
} shared_t;
static shared_t *shared;
static bool previous_fault;
bool runtime_guard_previous_fault(void) { return previous_fault; }
static volatile sig_atomic_t stopping;
static unsigned char signal_stack[65536];
static uint64_t now_ms(void)
{
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t)) return 0;
    return (uint64_t)t.tv_sec * 1000U + (unsigned)t.tv_nsec / 1000000U;
}
static void pause_monitor(void) { struct timespec t = {0, 100000000}; nanosleep(&t, NULL); }
static void stop_handler(int number) { stopping = number; }
static void fatal_handler(int number, siginfo_t *info, void *context)
{
#if defined(__riscv)
    shared->signal_pc = ((ucontext_t *)context)->uc_mcontext.__gregs[REG_PC];
#elif defined(__x86_64__)
    shared->signal_pc = ((ucontext_t *)context)->uc_mcontext.gregs[REG_RIP];
#else
    (void)context;
#endif
    shared->signal_code = info->si_code;
    shared->signal_address = (uintptr_t)info->si_addr;
    shared->signal_number = number;
    /* SA_RESETHAND makes the pending signal fatal. No heap, stdio, locks or
     * attempt to resume the application after corrupted process state. */
    kill(getpid(), number);
}
void runtime_guard_heartbeat(void)
{
    if (shared) __atomic_add_fetch(&shared->heartbeat, 1U, __ATOMIC_RELEASE);
}
void runtime_guard_fault(unsigned reason)
{
    if (shared) { uint32_t zero = 0; __atomic_compare_exchange_n(&shared->fault, &zero, reason,
                         false, __ATOMIC_RELEASE, __ATOMIC_RELAXED); }
}
bool runtime_guard_has_fault(void)
{ return shared && __atomic_load_n(&shared->fault,__ATOMIC_ACQUIRE)!=0; }
static void record(record_t *records, unsigned capacity, uint32_t *count,
                   const void *data, size_t length, int completed)
{
    if (!data || length > 256) return;
    /* Each ring has one writer: RX thread, or TX mutex owner. The parent only
     * reads after process death, including a possibly interrupted last slot. */
    unsigned next = *count + 1U;
    record_t *r = &records[(next - 1U) % capacity];
    r->sequence = 0;
    r->length = (uint32_t)length;
    r->completed = completed;
    memcpy(r->bytes, data, length);
    __atomic_store_n(&r->sequence, next, __ATOMIC_RELEASE);
    __atomic_store_n(count, next, __ATOMIC_RELEASE);
}
void runtime_guard_rx(const void *data, size_t length)
{ if (shared) record(shared->rx, RX_SLOTS, &shared->rx_count, data, length, 1); }
void runtime_guard_tx(const void *data, size_t length, int completed)
{ if (shared) record(shared->tx, TX_SLOTS, &shared->tx_count, data, length, completed); }
void runtime_guard_count(int pcs,float amount,unsigned phase,bool busy,bool pending,bool multi)
{
    if (!shared) return;
    unsigned next = shared->count_slot ^ 1U;
    shared->count[next] = (count_t){pcs,amount,phase,busy,pending,multi};
    __atomic_store_n(&shared->count_slot, next, __ATOMIC_RELEASE);
    __atomic_store_n(&shared->count_valid, 1U, __ATOMIC_RELEASE);
}
static int directory(void)
{
    if (mkdir(RUNTIME_GUARD_DIR, 0700) && errno != EEXIST) return -1;
    int fd = open(RUNTIME_GUARD_DIR, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    struct stat st;
    if (fd >= 0 && (fstat(fd, &st) || st.st_uid != geteuid() || (st.st_mode & 0022))) {
        close(fd); errno = EPERM; return -1;
    }
    return fd;
}
static int lock_guard(void)
{
    int fd = open(RUNTIME_GUARD_LOCK, O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd >= 0 && flock(fd, LOCK_EX | LOCK_NB)) { close(fd); return -1; }
    return fd;
}
static bool write_all(int fd, const void *data, size_t length)
{
    const char *bytes = data;
    while (length) {
        ssize_t n = write(fd, bytes, length);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        bytes += n; length -= (size_t)n;
    }
    return true;
}
static bool marker(int dir)
{
    int fd = openat(dir, "pending", O_CREAT | O_EXCL | O_WRONLY | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) return errno == EEXIST;
    const char text[] = "Runtime fault. Physical notes and mainboard state must be reconciled.\n";
    bool ok = write_all(fd, text, sizeof(text)-1) && fsync(fd) == 0;
    if (close(fd)) ok = false;
    if (fsync(dir)) ok = false;
    return ok;
}
static void dump_ring(FILE *out, const char *name, record_t *ring, unsigned size, unsigned count)
{
    fprintf(out,"%s received=%u retained=%u overwritten=%u\n",name,count,
            count < size ? count : size,count > size ? count-size : 0);
    unsigned first = count > size ? count-size+1 : 1;
    for (unsigned seq=first; seq && seq<=count; seq++) {
        record_t *r=&ring[(seq-1)%size];
        if (r->sequence!=seq || r->length>256) { fprintf(out,"%s %u INCOMPLETE\n",name,seq); continue; }
        fprintf(out,"%s %u completed=%d ",name,seq,r->completed);
        for(unsigned i=0;i<r->length;i++)fprintf(out,"%02X",r->bytes[i]);
        fputc('\n',out);
    }
}
static bool save_evidence(int dir, int status, const char *maps)
{
    int fd=openat(dir,"evidence.tmp",O_CREAT|O_EXCL|O_WRONLY|O_NOFOLLOW|O_CLOEXEC,0600);
    if(fd<0)return false;
    FILE *out=fdopen(fd,"w");
    if(!out){close(fd);return false;}
    unsigned slot=__atomic_load_n(&shared->count_slot,__ATOMIC_ACQUIRE);
    bool valid=shared->count_valid==1U && slot<2U;
    count_t c={0};
    /* The failed process is untrusted, including its shared-memory metadata. */
    if(valid)memcpy(&c,&shared->count[slot],sizeof(c));
    fprintf(out,"UN260 runtime evidence v1\nuptime_ms=%llu wait_status=%d fault=%u\n"
        "signal=%d code=%d address=0x%llx pc=0x%llx\n"
        "LAST UI SNAPSHOT ONLY - NOT FINAL PHYSICAL COUNT\nsnapshot_valid=%u\npcs=%d amount=%.9g phase=%u busy=%u pending=%u multi=%u\n"
        "RX contains received frames, not proof of application processing. TX completion means host write only, not mainboard ACK.\n"
        "Journal is bounded; overwritten/incomplete records require manual reconciliation.\n",
        (unsigned long long)now_ms(),status,shared->fault,shared->signal_number,
        shared->signal_code,(unsigned long long)shared->signal_address,
        (unsigned long long)shared->signal_pc,(unsigned)valid,c.pcs,(double)c.amount,
        c.phase,c.busy,c.pending,c.multi);
    dump_ring(out,"RX",shared->rx,RX_SLOTS,shared->rx_count);
    dump_ring(out,"TX",shared->tx,TX_SLOTS,shared->tx_count);
    fprintf(out,"\nSTARTUP MODULE MAPS\n%s",maps);
    bool ok=fflush(out)==0 && !ferror(out) && fsync(fd)==0;
    if(fclose(out))ok=false;
    if(ok && renameat(dir,"evidence.tmp",dir,"evidence.txt"))ok=false;
    if(fsync(dir))ok=false;
    return ok;
}
static pid_t start_protected_display(void)
{
#ifndef RUNTIME_GUARD_TEST
    pid_t parent=getpid();
    pid_t display=fork();
    if(!display){
        signal(SIGTERM,SIG_DFL);signal(SIGINT,SIG_DFL);
        prctl(PR_SET_PDEATHSIG,SIGKILL);
        if(getppid()!=parent)_exit(1);
        setenv("UN260_RUNTIME_FAULT_DISPLAY","1",1);
        execl(RUNTIME_GUARD_DISPLAY,RUNTIME_GUARD_DISPLAY,(char*)NULL);
        _exit(127);
    }
    return display;
#else
    return -1;
#endif
}
static void wait_protected_display(pid_t display)
{
#ifndef RUNTIME_GUARD_TEST
    /* Never launch the counting application here. A stopped maintenance
     * screen is not permission to resume RUN or erase any count/history. */
    uint64_t retry=now_ms();
    while(!stopping) {
        if(display>0 && waitpid(display,NULL,WNOHANG)==display) {
            display=-1;retry=now_ms()+5000U;
        }
        if(display<0 && now_ms()>=retry) {
            display=start_protected_display();retry=now_ms()+5000U;
        }
        pause_monitor();
    }
    if(display>0){kill(display,SIGTERM);waitpid(display,NULL,0);}
#else
    (void)display;
#endif
}
/* Archiving records a restart, not a physical reconciliation. Keep original
 * evidence until the archive and its directory entry are durable. */
static bool archive_pending(int dir, const char *prefix, bool require_evidence)
{
    struct stat st;
    if(fstatat(dir,"pending",&st,AT_SYMLINK_NOFOLLOW) || !S_ISREG(st.st_mode)) return false;
    bool evidence=fstatat(dir,"evidence.txt",&st,AT_SYMLINK_NOFOLLOW)==0;
    if((evidence && (!S_ISREG(st.st_mode)||st.st_size<=0)) ||
       (!evidence && (errno!=ENOENT || require_evidence))) return false;
    char archive[96]; int saved=-1;
    for(unsigned attempt=0;attempt<32;attempt++) {
        snprintf(archive,sizeof(archive),"%s-%lld-%ld-%u",prefix,(long long)time(NULL),(long)getpid(),attempt);
        if(!mkdirat(dir,archive,0700)) {
            saved=openat(dir,archive,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);break;
        }
        if(errno!=EEXIST)return false;
    }
    if(saved<0)return false;
    bool ok=!evidence || linkat(dir,"evidence.txt",saved,"evidence.txt",0)==0;
    if(ok && linkat(dir,"pending",saved,"pending",0))ok=false;
    /* A failed evidence write may have left a partial file. Preserve it too. */
    bool partial=fstatat(dir,"evidence.tmp",&st,AT_SYMLINK_NOFOLLOW)==0;
    if(partial && (!S_ISREG(st.st_mode) || linkat(dir,"evidence.tmp",saved,"evidence.partial",0)))ok=false;
    if(ok && fsync(saved))ok=false;
    if(ok && fsync(dir))ok=false;
    if(ok && unlinkat(dir,"pending",0))ok=false;
    if(ok && fsync(dir))ok=false;
    if(ok && evidence && unlinkat(dir,"evidence.txt",0))ok=false;
    if(ok && partial && unlinkat(dir,"evidence.tmp",0))ok=false;
    if(ok && fsync(dir))ok=false;
    if(!ok)(void)marker(dir);
    close(saved);
    return ok;
}
int runtime_guard_reconcile(void)
{
    int lock=lock_guard();
    if(lock<0){fprintf(stderr,"Stop the UI service before reconciliation.\n");return 1;}
    int dir=directory();
    if(dir<0){close(lock);return 1;}
    bool ok=archive_pending(dir,"reconciled",true);
    close(dir);close(lock);
    if(ok)puts("Reconciliation recorded. Evidence retained. Start the UI service manually.");
    return ok?0:1;
}
int runtime_guard_run(int (*application)(void))
{
    stopping=0;previous_fault=false;
    signal(SIGTERM,stop_handler);signal(SIGINT,stop_handler);signal(SIGPIPE,SIG_IGN);
    int lock=lock_guard(),dir=directory();
    if(lock<0||dir<0){fprintf(stderr,"Runtime protection unavailable; counting not started.\n");
        if(lock>=0)close(lock);
        if(dir>=0)close(dir);
        return 1;}
    struct stat st;
    if(!fstatat(dir,"pending",&st,AT_SYMLINK_NOFOLLOW)||errno!=ENOENT){
        if(!archive_pending(dir,"restart",false)) {
            fprintf(stderr,"Fault log cannot be preserved; startup requires storage repair.\n");
            wait_protected_display(start_protected_display());close(dir);close(lock);return 2;
        }
        previous_fault=true;
        fprintf(stderr,"Previous runtime fault archived, not reconciled; starting normal self-check.\n");
    }
    shared=mmap(NULL,sizeof(*shared),PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS,-1,0);
    if(shared==MAP_FAILED){shared=NULL;close(dir);close(lock);return 1;}
    /* Prefault the bounded journal before any device or UI threads exist. */
    memset(shared,0,sizeof(*shared));
    pid_t parent=getpid(),child=fork();
    if(!child){
        close(lock);close(dir);
        prctl(PR_SET_PDEATHSIG,SIGKILL);
        if(getppid()!=parent)_exit(1);
        signal(SIGTERM,SIG_DFL);signal(SIGINT,SIG_DFL);
        stack_t stack={.ss_sp=signal_stack,.ss_size=sizeof(signal_stack)};
        if(sigaltstack(&stack,NULL))_exit(1);
        struct sigaction action={0};action.sa_sigaction=fatal_handler;
        action.sa_flags=SA_SIGINFO|SA_RESETHAND|SA_ONSTACK;sigemptyset(&action.sa_mask);
        const int signals[]={SIGSEGV,SIGBUS,SIGILL,SIGFPE,SIGABRT};
        for(unsigned i=0;i<sizeof(signals)/sizeof(signals[0]);i++)
            if(sigaction(signals[i],&action,NULL))_exit(1);
        int result=application();_exit(result);
    }
    if(child<0){munmap(shared,sizeof(*shared));shared=NULL;close(dir);close(lock);return 1;}
    char maps[65536]={0},path[64];snprintf(path,sizeof(path),"/proc/%ld/maps",(long)child);
    int mapfd=open(path,O_RDONLY|O_CLOEXEC);
    if(mapfd>=0){ssize_t n=read(mapfd,maps,sizeof(maps)-1);if(n>0)maps[n]=0;close(mapfd);}
    unsigned beat=0;uint64_t changed=now_ms();int status=0;bool terminated=false;
    for(;;){
        pid_t result=waitpid(child,&status,WNOHANG);
        if(result==child)break;
        if(result<0&&errno!=EINTR){status=-1;break;}
        unsigned current=__atomic_load_n(&shared->heartbeat,__ATOMIC_ACQUIRE);
        if(current!=beat){beat=current;changed=now_ms();}
        bool stalled=now_ms()-changed>(beat?RUNTIME_GUARD_STALL_MS:RUNTIME_GUARD_START_MS);
        if(stopping || __atomic_load_n(&shared->fault,__ATOMIC_ACQUIRE) || stalled){
            if(stalled&&!stopping)runtime_guard_fault(RUNTIME_FAULT_STALL);
            if(!terminated){kill(child,stopping?SIGTERM:SIGKILL);terminated=true;changed=now_ms();}
            else if(now_ms()-changed>1000)kill(child,SIGKILL);
        }
        pause_monitor();
    }
    bool expected=stopping || (WIFEXITED(status)&&WEXITSTATUS(status)==0&&!shared->fault);
    if(!expected){
        /* The maintenance renderer is independent of storage. A failed or
         * stalled evidence write must not leave the dead UI on screen. */
        pid_t display=start_protected_display();
        bool held=marker(dir),saved=save_evidence(dir,status,maps);
        fprintf(stderr,"Runtime fault: fault_record=%d evidence=%d; restart runs normal self-check; verify unfinished counts.\n",held,saved);
        if(!saved){
            int ram=open("/tmp",O_RDONLY|O_DIRECTORY|O_CLOEXEC);
            if(ram>=0){(void)save_evidence(ram,status,maps);close(ram);}
        }
        wait_protected_display(display);
    }
    munmap(shared,sizeof(*shared));shared=NULL;close(dir);close(lock);
    return expected?0:2;
}
