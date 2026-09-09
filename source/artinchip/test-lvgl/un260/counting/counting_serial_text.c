#include "counting_serial_text.h"

#include <string.h>

static unsigned char ascii_fold(unsigned char value)
{
    return value >= 'a' && value <= 'z'
        ? (unsigned char)(value - 'a' + 'A') : value;
}

static bool text_prefix(const char *serial, const char *text, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        if (!serial[i] || ascii_fold((unsigned char)serial[i]) !=
            ascii_fold((unsigned char)text[i])) return false;
    }
    return true;
}

bool counting_serial_text_matches(const char *serial, const char *text,
    size_t text_length, counting_serial_text_match_t mode)
{
    if (!serial || (!text && text_length)) return false;
    if (!text_length) return true;
    if (mode == COUNTING_SERIAL_TEXT_EXACT)
        return text_prefix(serial, text, text_length) && serial[text_length] == '\0';
    if (mode == COUNTING_SERIAL_TEXT_PREFIX) return text_prefix(serial, text, text_length);
    if (mode == COUNTING_SERIAL_TEXT_SUFFIX) {
        size_t length = strlen(serial);
        return length >= text_length && text_prefix(serial + length - text_length, text, text_length);
    }
    for (; *serial; ++serial) if (text_prefix(serial, text, text_length)) return true;
    return false;
}

static bool ascii_space(unsigned char value)
{
    return value == ' ' || (value >= '\t' && value <= '\r');
}

bool counting_serial_text_trim(char *out, size_t capacity,
                                const char *text, size_t length)
{
    size_t first = 0;
    if (!out || !capacity || (!text && length)) return false;
    while (first < length && ascii_space((unsigned char)text[first])) ++first;
    while (length > first && ascii_space((unsigned char)text[length - 1])) --length;
    if (length - first >= capacity) return false;
    if (length > first) memmove(out, text + first, length - first);
    out[length - first] = '\0';
    return true;
}
