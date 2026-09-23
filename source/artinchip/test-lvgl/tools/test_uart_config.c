/* Real Linux termios2 on a PTY: accepted settings, NOT electrical baud. */
#define _GNU_SOURCE
#include <asm/termbits.h>
#include <asm/ioctls.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "un260/lv_drivers/lv_drivers.h"

static unsigned long fail_request;
static unsigned fail_at, matching_calls;
static unsigned read_count;
static int mismatch_readback;
int __real_ioctl(int fd, unsigned long request, ...);

int __wrap_ioctl(int fd, unsigned long request, ...)
{
    va_list args;
    va_start(args, request);
    void *value = NULL;
    int queue = 0;
    if (request == TCFLSH) queue = va_arg(args, int);
    else value = va_arg(args, void *);
    va_end(args);
    if (request == fail_request && ++matching_calls == fail_at) {
        errno = EIO;
        return -1;
    }
    int result = request == TCFLSH ? __real_ioctl(fd, request, queue) :
                                    __real_ioctl(fd, request, value);
    if (request == TCGETS2 && ++read_count == 2 && mismatch_readback && result == 0)
        ((struct termios2 *)value)->c_ospeed = 500000;
    return result;
}

static void verify(int fd, unsigned rate, tcflag_t speed)
{
    struct termios2 tty;
    assert(ioctl(fd, TCGETS2, &tty) == 0);
    assert(tty.c_ispeed == rate && tty.c_ospeed == rate);
    assert((tty.c_cflag & CBAUD) == speed);
    assert(((tty.c_cflag >> IBSHIFT) & CBAUD) == speed);
    assert((tty.c_cflag & CSIZE) == CS8);
    assert(!(tty.c_cflag & (PARENB | CSTOPB)));
    assert((tty.c_cflag & (CLOCAL | CREAD)) == (CLOCAL | CREAD));
    assert(!(tty.c_iflag & (IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON)));
    assert(!(tty.c_oflag & OPOST));
    assert(!(tty.c_lflag & (ECHO | ECHONL | ICANON | ISIG | IEXTEN)));
    assert(tty.c_cc[VMIN] == 1 && tty.c_cc[VTIME] == 1);
}

int main(void)
{
    int master = posix_openpt(O_RDWR | O_NOCTTY);
    assert(master >= 0 && grantpt(master) == 0 && unlockpt(master) == 0);
    char *name = ptsname(master);
    assert(name);
    int slave = open(name, O_RDWR | O_NOCTTY);
    assert(slave >= 0);
    const int rates[] = {9600, 115200, 500000, 512000, 921600, 115200, 512000};
    const tcflag_t speeds[] = {B9600, B115200, B500000, BOTHER, B921600, B115200, BOTHER};
    for (unsigned i = 0; i < sizeof(rates) / sizeof(rates[0]); ++i) {
        assert(uart_config(slave, rates[i], 8, 'N', 1) == 0);
        verify(slave, (unsigned)rates[i], speeds[i]);
    }
    assert(uart_config(slave, 12345, 8, 'N', 1) < 0);
    assert(uart_config(slave, 512000, 6, 'N', 1) < 0);
    assert(uart_config(slave, 512000, 8, 'X', 1) < 0);
    assert(uart_config(slave, 512000, 8, 'N', 3) < 0);
    assert(uart_config(-1, 512000, 8, 'N', 1) < 0);
    verify(slave, 512000, BOTHER);

    const unsigned long failures[] = {TCGETS2, TCFLSH, TCSETS2, TCGETS2};
    for (unsigned i = 0; i < sizeof(failures) / sizeof(failures[0]); ++i) {
        fail_request = failures[i];
        fail_at = i == 3 ? 2 : 1;
        matching_calls = 0;
        assert(uart_config(slave, 512000, 8, 'N', 1) < 0);
        fail_request = 0;
        assert(uart_config(slave, 115200, 8, 'N', 1) == 0);
        verify(slave, 115200, B115200);
    }
    read_count = 0;
    mismatch_readback = 1;
    assert(uart_config(slave, 512000, 8, 'N', 1) < 0);
    mismatch_readback = 0;
    assert(uart_config(slave, 512000, 8, 'N', 1) == 0);
    verify(slave, 512000, BOTHER);
    close(master);
    close(slave);
    puts("PASS UART termios2: requested 512000 8N1, standard/custom transitions, invalid parameters, ioctl failures and mismatched readback");
    return 0;
}
