/* Real termios driver on a PTY: verifies configuration, not electrical baud. */
#include <assert.h>
#include <pty.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include "un260/lv_drivers/lv_drivers.h"

int main(void)
{
    int master, slave;
    struct termios tty;
    assert(openpty(&master, &slave, NULL, NULL, NULL) == 0);
    const int rates[] = {9600, 115200, 500000, 921600, 115200, 500000};
    const speed_t speeds[] = {B9600, B115200, B500000, B921600, B115200, B500000};
    for (unsigned i = 0; i < sizeof(rates) / sizeof(rates[0]); ++i) {
        assert(uart_config(slave, rates[i], 8, 'N', 1) == 0);
        assert(tcgetattr(slave, &tty) == 0);
        assert(cfgetispeed(&tty) == speeds[i] && cfgetospeed(&tty) == speeds[i]);
        assert((tty.c_cflag & CSIZE) == CS8);
        assert(!(tty.c_cflag & (PARENB | CSTOPB)));
        assert((tty.c_cflag & (CLOCAL | CREAD)) == (CLOCAL | CREAD));
        assert(!(tty.c_lflag & (ICANON | ECHO)));
        assert(tty.c_cc[VMIN] == 1 && tty.c_cc[VTIME] == 1);
    }
    assert(uart_config(slave, 12345, 8, 'N', 1) < 0);
    assert(uart_config(slave, 500000, 6, 'N', 1) < 0);
    assert(uart_config(slave, 500000, 8, 'X', 1) < 0);
    assert(uart_config(slave, 500000, 8, 'N', 3) < 0);
    assert(uart_config(-1, 500000, 8, 'N', 1) < 0);
    assert(tcgetattr(slave, &tty) == 0);
    assert(cfgetispeed(&tty) == B500000 && cfgetospeed(&tty) == B500000);
    close(master);
    close(slave);
    puts("PASS UART termios: 500000 8N1, existing baud rates and invalid configuration rejection");
    return 0;
}
