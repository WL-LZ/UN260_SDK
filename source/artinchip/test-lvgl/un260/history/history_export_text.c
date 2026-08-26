#include "history_export_text.h"

#include <string.h>

static bool history_export_append(char *dst,
                                  size_t dst_size,
                                  size_t *used,
                                  const char *text)
{
    size_t len;

    if (dst == NULL || used == NULL || text == NULL || *used >= dst_size) {
        return false;
    }
    len = strlen(text);
    if (len >= dst_size - *used) {
        return false;
    }
    memcpy(dst + *used, text, len);
    *used += len;
    dst[*used] = '\0';
    return true;
}

bool history_export_html_escape(char *dst,
                                size_t dst_size,
                                const char *src)
{
    size_t used = 0;

    if (dst == NULL || dst_size == 0U) {
        return false;
    }
    dst[0] = '\0';
    if (src == NULL) {
        return true;
    }

    for (size_t i = 0; src[i] != '\0'; i++) {
        const char *replacement;
        char plain[2] = {src[i], '\0'};

        switch (src[i]) {
        case '&': replacement = "&amp;"; break;
        case '<': replacement = "&lt;"; break;
        case '>': replacement = "&gt;"; break;
        case '"': replacement = "&quot;"; break;
        case '\'': replacement = "&#39;"; break;
        case '\n':
        case '\r': replacement = " "; break;
        default: replacement = plain; break;
        }
        if (!history_export_append(dst, dst_size, &used, replacement)) {
            return false;
        }
    }
    return true;
}

bool history_export_csv_escape(char *dst,
                               size_t dst_size,
                               const char *src)
{
    size_t used = 0;
    size_t first = 0;

    if (dst == NULL || dst_size == 0U) {
        return false;
    }
    dst[0] = '\0';
    if (src == NULL) {
        return true;
    }

    while (src[first] == ' ' || src[first] == '\t') {
        first++;
    }
    if (src[first] == '=' || src[first] == '+' ||
        src[first] == '-' || src[first] == '@') {
        if (!history_export_append(dst, dst_size, &used, "'")) {
            return false;
        }
    }

    for (size_t i = 0; src[i] != '\0'; i++) {
        char plain[2] = {src[i], '\0'};
        const char *replacement = plain;

        if (src[i] == '"') {
            replacement = "\"\"";
        } else if (src[i] == '\n' || src[i] == '\r') {
            replacement = " ";
        }
        if (!history_export_append(dst, dst_size, &used, replacement)) {
            return false;
        }
    }
    return true;
}

bool history_export_js_escape(char *dst,
                              size_t dst_size,
                              const char *src)
{
    size_t used = 0;

    if (dst == NULL || dst_size == 0U) {
        return false;
    }
    dst[0] = '\0';
    if (src == NULL) {
        return true;
    }

    for (size_t i = 0; src[i] != '\0'; i++) {
        char plain[2] = {src[i], '\0'};
        const char *replacement;

        switch (src[i]) {
        case '\\': replacement = "\\\\"; break;
        case '"': replacement = "\\\""; break;
        case '\n':
        case '\r': replacement = " "; break;
        default: replacement = plain; break;
        }
        if (!history_export_append(dst, dst_size, &used, replacement)) {
            return false;
        }
    }
    return true;
}
