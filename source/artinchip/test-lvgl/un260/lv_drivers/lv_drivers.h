#ifndef UART_DRV_H
#define UART_DRV_H

#ifdef __cplusplus
extern "C" {
#endif

#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>

#include "uart_io.h"

/* 打开串口设备 */
int uart_open(const char *device);

/* 配置串口
 * fd      : 串口文件描述符
 * baud    : 波特率 (9600, 115200, 921600 ...)
 * dataBit : 数据位 (7 或 8)
 * parity  : 校验位 ('N' 无, 'E' 偶校验, 'O' 奇校验)
 * stopBit : 停止位 (1 或 2)
 */
int uart_config(int fd, int baud, int dataBit, char parity, int stopBit);

/* 关闭串口 */
void uart_close(int fd);

#ifdef __cplusplus
}
#endif

#endif
