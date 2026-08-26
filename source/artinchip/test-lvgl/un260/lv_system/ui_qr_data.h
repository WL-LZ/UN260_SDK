#ifndef UI_QR_DATA_H
#define UI_QR_DATA_H

#include <stdbool.h>
#include <stddef.h>

bool ui_qr_data_is_ready(void); //当前是否有可导出的点钞数据
bool ui_qr_data_build(char* buf, size_t buf_size); //组装二维码文本内容

#endif
