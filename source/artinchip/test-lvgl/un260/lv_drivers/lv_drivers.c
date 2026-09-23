#include "lv_drivers.h"
#include "uart_io.h"
#include <asm/termbits.h>
#include <asm/ioctls.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>


/* 打开串口 */
int uart_open(const char *device)
{
    int fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd < 0) {
        perror("uart_open");
        return -1;
    }
    if (fcntl(fd, F_SETFL, 0) < 0) {  // 阻塞模式
        perror("uart_open fcntl");
        close(fd);
        return -1;
    }
    return fd;
}

/* 配置串口 */
int uart_config(int fd, int baud, int dataBit, char parity, int stopBit)
{
    struct termios2 tty;
    tcflag_t speed;
    switch (baud) {
        case 9600: speed = B9600; break;
        case 115200: speed = B115200; break;
        case 500000: speed = B500000; break;
        case 512000: speed = BOTHER; break;
        case 921600: speed = B921600; break;
        default: errno = EINVAL; return -1;
    }
    if ((dataBit != 7 && dataBit != 8) || (stopBit != 1 && stopBit != 2) ||
        (parity != 'N' && parity != 'n' && parity != 'E' && parity != 'e' &&
         parity != 'O' && parity != 'o')) {
        errno = EINVAL;
        return -1;
    }
    if (ioctl(fd, TCGETS2, &tty) != 0) {
        perror("uart_config TCGETS2");
        return -1;
    }

    /* cfmakeraw 的等效配置，使用内核 termios2 ABI 支持非标准速率。 */
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_oflag &= ~OPOST;
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_cflag &= ~(CSIZE | PARENB);
    tty.c_cflag |= (CLOCAL | CREAD);

    // 数据位
    if (dataBit == 7) tty.c_cflag |= CS7;
    else tty.c_cflag |= CS8;

    // 校验位
    switch (parity) {
        case 'N': case 'n': tty.c_cflag &= ~PARENB; break;
        case 'E': case 'e': tty.c_cflag |= PARENB; tty.c_cflag &= ~PARODD; break;
        case 'O': case 'o': tty.c_cflag |= PARENB | PARODD; break;
        default: return -1;
    }


    if (stopBit == 1) tty.c_cflag &= ~CSTOPB;
    else tty.c_cflag |= CSTOPB;

    tty.c_cflag &= ~(CBAUD | (CBAUD << IBSHIFT));
    tty.c_cflag |= speed | (speed << IBSHIFT);
    tty.c_ispeed = (speed_t)baud;
    tty.c_ospeed = (speed_t)baud;

    tty.c_cc[VTIME] = 1; // 0.1s
    tty.c_cc[VMIN]  = 1;

    if (ioctl(fd, TCFLSH, TCIFLUSH) != 0) {
        perror("uart_config TCFLSH");
        return -1;
    }
    if (ioctl(fd, TCSETS2, &tty) != 0) {
        perror("uart_config TCSETS2");
        return -1;
    }
    if (ioctl(fd, TCGETS2, &tty) != 0) {
        perror("uart_config readback");
        return -1;
    }
    /* 回读仅验证驱动接受的配置；实际线速仍取决于硬件时钟与分频。 */
    if (tty.c_ispeed != (speed_t)baud || tty.c_ospeed != (speed_t)baud) {
        errno = EINVAL;
        perror("uart_config baud readback");
        return -1;
    }
    return 0;
}

void uart_close(int fd)
{
    if (fd >= 0) close(fd);
}
