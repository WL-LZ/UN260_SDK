#define _GNU_SOURCE
#include "un260/lv_drivers/uart_io.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
static long ms(void) { struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return t.tv_sec*1000+t.tv_nsec/1000000; }
static void alarm_handler(int n) { (void)n; }
static void pair(int fd[2]) { assert(!socketpair(AF_UNIX,SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC,0,fd));int size=1024;assert(!setsockopt(fd[0],SOL_SOCKET,SO_SNDBUF,&size,sizeof(size)));uart_tx_reset(fd[0]); }
static char large[1024*1024];
static void *sender(void *arg) { int fd=*(int *)arg;assert(uart_send(fd,large,sizeof(large))==-1);return NULL; }
int main(void)
{
    signal(SIGPIPE,SIG_IGN);
    int fd[2];char buf[4096];pair(fd);
    assert(uart_send(fd[0],"hello",5)==5);
    assert(uart_recv(fd[1],buf,sizeof(buf),50)==5&&!memcmp(buf,"hello",5));
    long begin=ms();assert(uart_recv(fd[1],buf,sizeof(buf),30)==0);assert(ms()-begin<250);
    memset(large,'A',sizeof(large));begin=ms();
    assert(uart_send(fd[0],large,sizeof(large))==-1&&errno==ETIMEDOUT);
    assert(ms()-begin<250&&!uart_tx_is_healthy(fd[0]));
    unsigned got=0;int n;while((n=read(fd[1],buf,sizeof(buf)))>0)got+=(unsigned)n;
    assert(got>0&&got<sizeof(large));
    assert(uart_send(fd[0],"must not append",15)==-1);
    assert(read(fd[1],buf,sizeof(buf))==-1&&errno==EAGAIN);
    close(fd[0]);close(fd[1]);pair(fd);
    pthread_t thread;assert(!pthread_create(&thread,NULL,sender,&fd[0]));
    usleep(10000);begin=ms();assert(uart_send(fd[0],"B",1)==-1);assert(ms()-begin<250);
    assert(!pthread_join(thread,NULL));
    while((n=read(fd[1],buf,sizeof(buf)))>0)for(int i=0;i<n;i++)assert(buf[i]=='A');
    close(fd[0]);close(fd[1]);pair(fd);
    struct sigaction action={0};action.sa_handler=alarm_handler;assert(!sigaction(SIGALRM,&action,NULL));
    struct itimerval timer={{0,1000},{0,1000}};assert(!setitimer(ITIMER_REAL,&timer,NULL));
    begin=ms();assert(uart_recv(fd[1],buf,sizeof(buf),40)==0);assert(ms()-begin<250);
    timer=(struct itimerval){{0,0},{0,0}};assert(!setitimer(ITIMER_REAL,&timer,NULL));
    close(fd[1]);begin=ms();assert(uart_recv(fd[0],buf,sizeof(buf),40)==-1);assert(ms()-begin<100);
    assert(uart_send(fd[0],"A",1)==-1&&!uart_tx_is_healthy(fd[0]));close(fd[0]);
    /* Diagnostic backpressure has a deadline and never poisons controller TX. */
    pair(fd);begin=ms();assert(uart_write_all(fd[0],large,sizeof(large))==-1);assert(ms()-begin<250);assert(uart_tx_is_healthy(fd[0]));
    close(fd[0]);close(fd[1]);
    puts("PASS UART: normal bytes, partial write deadline, no retry after uncertainty, concurrent serialization, EINTR total receive deadline, disconnect, log isolation");
}
