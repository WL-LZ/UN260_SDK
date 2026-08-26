#ifndef HISTORY_EXPORT_TEXT_H
#define HISTORY_EXPORT_TEXT_H

#include <stdbool.h>
#include <stddef.h>

bool history_export_html_escape(char *dst,
                                size_t dst_size,
                                const char *src);
bool history_export_csv_escape(char *dst,
                               size_t dst_size,
                               const char *src);
bool history_export_js_escape(char *dst,
                              size_t dst_size,
                              const char *src);

#endif
