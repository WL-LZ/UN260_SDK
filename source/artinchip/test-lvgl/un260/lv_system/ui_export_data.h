#ifndef UI_EXPORT_DATA_H
#define UI_EXPORT_DATA_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    UI_EXPORT_TEXT_OK = 0,
    UI_EXPORT_TEXT_EMPTY,
    UI_EXPORT_TEXT_USB_NOT_READY,
    UI_EXPORT_TEXT_FAILED
} ui_export_text_result_t;

bool ui_export_data_request(void); //导出当前点钞数据到U盘CSV+HTML
ui_export_text_result_t ui_export_text_lines(const char *file_prefix,
                                             const char *const *lines,
                                             size_t line_count); //导出文本行到U盘

#endif // UI_EXPORT_DATA_H
