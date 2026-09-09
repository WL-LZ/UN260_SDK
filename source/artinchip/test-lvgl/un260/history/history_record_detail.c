#include "history_record_detail.h"
#include "un260/protocol/protocol_frame.h"

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define HISTORY_SAVED_TEXT_LIMIT 4096U

static bool next_line(const char **cursor, char *out, size_t capacity, bool *cut)
{
    const char *start = *cursor;
    size_t length = 0;
    while (*start == '\r' || *start == '\n') ++start;
    if (!*start) { *cursor = start; return false; }
    while (start[length] && start[length] != '\r' && start[length] != '\n') ++length;
    *cut = length >= capacity;
    size_t copied = *cut ? capacity - 1 : length;
    memcpy(out, start, copied);
    out[copied] = '\0';
    *cursor = start + length;
    return true;
}

/* Truncated persisted text can end in the middle of a valid-looking serial.
 * Never expose that tail as an exact serial match. */
static bool copy_saved_text(char *out, const char *source, bool cut)
{
    size_t length = 0;
    if (!source) source = "";
    while (length < HISTORY_SAVED_TEXT_LIMIT && source[length]) ++length;
    if (length == HISTORY_SAVED_TEXT_LIMIT) cut = true;
    memcpy(out, source, length);
    out[length] = '\0';
    if (cut && length && out[length - 1] != '\n' && out[length - 1] != '\r') {
        while (length && out[length - 1] != '\n' && out[length - 1] != '\r') --length;
        out[length] = '\0';
    }
    return cut;
}

static void skip_space(const char **text)
{
    while (isspace((unsigned char)**text)) ++*text;
}

static bool parse_u32(const char **text, uint32_t *out)
{
    uint32_t value = 0;
    if (!isdigit((unsigned char)**text)) return false;
    while (isdigit((unsigned char)**text)) {
        unsigned digit = (unsigned)(*(*text)++ - '0');
        if (value > (UINT32_MAX - digit) / 10U) return false;
        value = value * 10U + digit;
    }
    *out = value;
    return true;
}

static void parse_denominations(const char *text, history_record_detail_t *out,
                                bool *malformed)
{
    char line[128];
    bool cut;
    while (next_line(&text, line, sizeof(line), &cut)) {
        const char *cursor = line;
        uint32_t value, pcs;
        skip_space(&cursor);
        if (cut || !parse_u32(&cursor, &value) || !value) {
            *malformed = true; continue;
        }
        skip_space(&cursor);
        if (*cursor != 'x' && *cursor != 'X') { *malformed = true; continue; }
        ++cursor;
        skip_space(&cursor);
        if (!parse_u32(&cursor, &pcs)) { *malformed = true; continue; }
        skip_space(&cursor);
        if (*cursor || out->denom_count >= HISTORY_DETAIL_MAX_DENOMS) {
            *malformed = true; continue;
        }
        history_detail_denom_t *row = &out->denoms[out->denom_count++];
        row->value = value;
        row->pcs = pcs;
        row->amount = (uint64_t)value * pcs;
        out->denomination_pcs += pcs;
        if (UINT64_MAX - out->denomination_amount < row->amount) {
            out->denomination_amount = UINT64_MAX;
            *malformed = true;
        } else out->denomination_amount += row->amount;
    }
}

static int hex_value(char ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

static bool parse_frame(const char *text, uint8_t *raw, size_t *length)
{
    size_t count = 0;
    while (*text) {
        skip_space(&text);
        if (!*text) break;
        int high = hex_value(*text++);
        int low = *text ? hex_value(*text++) : -1;
        if (high < 0 || low < 0 || count >= PROTOCOL_FRAME_MAX_SIZE) return false;
        raw[count++] = (uint8_t)(high * 16 + low);
        if (*text && !isspace((unsigned char)*text)) return false;
    }
    *length = count;
    return protocol_frame_is_valid(raw, count);
}

static void append_reject(history_record_detail_t *out, const uint8_t *raw,
                           size_t length, bool *malformed)
{
    if (length < PROTOCOL_FRAME_OVERHEAD + 2U) { *malformed = true; return; }
    if (raw[4] == 0 || raw[4] == 0xFF) return;
    if (out->reject_count >= HISTORY_DETAIL_MAX_REJECTS) { *malformed = true; return; }
    history_detail_reject_t *row = &out->rejects[out->reject_count++];
    row->no = (uint32_t)out->reject_count;
    row->code = raw[4];
    row->pcs = raw[5];
    out->saved_reject_pcs += row->pcs;
}

/* Reuse the established serial parser only after validating saved frames.
 * There is no CRC/trailer requirement: protocol_frame_is_valid is the actual
 * receive contract. Its command must also agree with the saved log tag. */
static void parse_session(const char *text, char *serial_log,
                           history_record_detail_t *out, bool *malformed)
{
    char line[1024];
    size_t used = 0;
    bool cut;
    serial_log[0] = '\0';
    while (next_line(&text, line, sizeof(line), &cut)) {
        if (strncmp(line, "0x0C", 4) && strncmp(line, "0x0D", 4)) continue;
        uint8_t raw[PROTOCOL_FRAME_MAX_SIZE];
        size_t length;
        if (cut || !isspace((unsigned char)line[4]) ||
            !parse_frame(line + 4, raw, &length) ||
            raw[3] != (line[3] == 'C' ? 0x0C : 0x0D)) {
            *malformed = true; continue;
        }
        if (raw[3] == 0x0C) append_reject(out, raw, length, malformed);
        else {
            size_t line_length = strlen(line);
            if (line_length + used + 1U > HISTORY_SAVED_TEXT_LIMIT) {
                *malformed = true; continue;
            }
            memcpy(serial_log + used, line, line_length);
            used += line_length;
            serial_log[used++] = '\n';
            serial_log[used] = '\0';
        }
    }
}

static void parse_last_error(const char *text, history_record_detail_t *out,
                              bool *malformed)
{
    uint8_t raw[PROTOCOL_FRAME_MAX_SIZE];
    size_t length;
    if (!text || !*text) return;
    if (!parse_frame(text, raw, &length)) { *malformed = true; return; }
    if (raw[3] != 0x0C) return;
    if (length < PROTOCOL_FRAME_OVERHEAD + 2U) { *malformed = true; return; }
    /* error_frame is the last captured frame, often already present in log. */
    for (size_t i = 0; i < out->reject_count; ++i)
        if (out->rejects[i].code == raw[4] && out->rejects[i].pcs == raw[5]) return;
    append_reject(out, raw, length, malformed);
}

bool history_record_detail_build(const history_detail_input_t *input,
                                 history_record_detail_t *out)
{
    char denom[HISTORY_SAVED_TEXT_LIMIT + 1U];
    char detail[HISTORY_SAVED_TEXT_LIMIT + 1U];
    char serial[HISTORY_SAVED_TEXT_LIMIT + 1U];
    char log[HISTORY_SAVED_TEXT_LIMIT + 1U];
    char serial_log[HISTORY_SAVED_TEXT_LIMIT + 1U];
    bool bad_denom = false, bad_log = false;
    int serial_count = 0;
    if (!input || !out) return false;
    memset(out, 0, sizeof(*out));
    bool denom_cut = copy_saved_text(denom, input->denom_text, input->denoms_truncated);
    bool detail_cut = copy_saved_text(detail, input->sn_detail_text, input->serials_truncated);
    bool legacy_cut = copy_saved_text(serial, input->sn_text, input->legacy_serials_truncated);
    bool log_cut = copy_saved_text(log, input->session_log, input->log_truncated);
    parse_denominations(denom, out, &bad_denom);
    parse_session(log, serial_log, out, &bad_log);
    bool serial_log_incomplete = log_cut || bad_log;
    parse_last_error(input->error_frame_text, out, &bad_log);
    bool serial_cut = detail_cut;
    /* A nonempty but malformed detail field can still fall back. Parse each
     * source through the existing parser so provenance follows actual rows,
     * not a nonempty string heuristic or an unused backup's truncation flag. */
    if (!history_export_sn_parse(detail, NULL, NULL, &out->serials, &serial_count))
        goto failed;
    if (!serial_count) {
        serial_cut = serial_log_incomplete;
        if (!history_export_sn_parse(NULL, serial_log, NULL, &out->serials, &serial_count))
            goto failed;
    }
    if (!serial_count) {
        serial_cut = legacy_cut;
        if (!history_export_sn_parse(NULL, NULL, serial, &out->serials, &serial_count))
            goto failed;
    }
    out->serial_count = serial_count > 0 ? (size_t)serial_count : 0;
    out->denoms_available = out->denom_count > 0;
    out->serials_available = out->serial_count > 0;
    out->rejects_available = out->reject_count > 0 || input->reject_log_complete;
    out->denoms_complete = out->denoms_available && !denom_cut && !bad_denom &&
        out->denomination_pcs == input->total_pcs;
    out->serials_complete = out->serials_available && !serial_cut &&
        out->serial_count == input->total_pcs &&
        out->serial_count < HISTORY_EXPORT_SN_MAX_ENTRIES;
    for (size_t i = 0; i < out->serial_count; ++i)
        if (strlen(out->serials[i].sn) >= HISTORY_EXPORT_SN_TEXT_SIZE - 1U)
            out->serials_complete = false;
    out->rejects_complete = input->reject_log_complete && !log_cut && !bad_log;
    out->malformed = bad_denom || bad_log;
    return true;
failed:
    history_record_detail_release(out);
    return false;
}

void history_record_detail_release(history_record_detail_t *detail)
{
    if (!detail) return;
    free(detail->serials);
    memset(detail, 0, sizeof(*detail));
}
