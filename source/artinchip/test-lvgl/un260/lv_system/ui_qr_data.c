#include "ui_qr_data.h"
#include "un260/lv_system/machine_time.h"
#include "un260/currency/currency_state.h"
#include "un260/counting/counting_data_store.h"
#include "un260/counting/counting_reject_reason.h"
#include <stdarg.h>
#include <stdio.h>

#define QR_LINE_BREAK "\r\n"

static bool ui_qr_data_append(char* buf, size_t buf_size, size_t* used,
    const char* fmt, ...) //向二维码文本缓冲区追加内容
{
    int written;
    va_list args;

    if (buf == NULL || used == NULL || fmt == NULL || *used >= buf_size) {
        return false;
    }

    va_start(args, fmt);
    written = vsnprintf(buf + *used, buf_size - *used, fmt, args);
    va_end(args);

    if (written < 0 || (size_t)written >= (buf_size - *used)) {
        return false;
    }

    *used += (size_t)written;
    return true;
}

bool ui_qr_data_is_ready(void) //判断当前是否有有效点钞数据
{
    if (counting_data_current()->total_pcs > 0) return true;
    if (counting_data_current()->total_amount > 0.0f) return true;
    if (counting_data_reject_pcs_count(counting_data_current()) > 0) return true;
    return false;
}

bool ui_qr_data_build(char* buf, size_t buf_size) //组装当前点钞结果二维码文本
{
    size_t used = 0;
    const size_t qr_safe_payload_limit = 2500; //保守上限，避免二维码编码阶段“too large”
    machine_time_value_t now;
    int sn_count = 0;
    bool has_err = false;
    int i;
    int sn_col = 0;
    int emitted_sn = 0;
    int omitted_sn = 0;
    int error_count;
    char curr_code[4];

    if (buf == NULL || buf_size == 0 || !ui_qr_data_is_ready()) {
        return false;
    }

    buf[0] = '\0';
    machine_time_get(&now);
    currency_state_get_active_code(curr_code);
    error_count = counting_data_error_detail_count(counting_data_current());

    if (!ui_qr_data_append(buf, buf_size, &used,
        "Total Amount: %.2f %s;" QR_LINE_BREAK,
        counting_data_current()->total_amount,
        (curr_code[0] != '\0') ? curr_code : "---")) {
        return false;
    }

    if (!ui_qr_data_append(buf, buf_size, &used, "Total Pieces: %d pcs;" QR_LINE_BREAK, counting_data_current()->total_pcs)) {
        return false;
    }

    if (!ui_qr_data_append(buf, buf_size, &used,
        "Transaction Time: %04u-%02u-%02u %02u:%02u:%02u;" QR_LINE_BREAK,
        (unsigned)now.year, (unsigned)now.month, (unsigned)now.day,
        (unsigned)now.hour, (unsigned)now.minute, (unsigned)now.second)) {
        return false;
    }

    if (!ui_qr_data_append(buf, buf_size, &used, "Error Notes: ")) {
        return false;
    }

    for (i = 0; i < error_count; i++) {

        if (has_err) {
            if (!ui_qr_data_append(buf, buf_size, &used, ",")) {
                return false;
            }
        }

        if (!ui_qr_data_append(buf, buf_size, &used, "%s",
                               counting_reject_reason_get(counting_data_current()->err_code[i]))) {
            return false;
        }

        if (counting_data_current()->err_pcs != NULL) {
            if (!ui_qr_data_append(buf, buf_size, &used, "(%u)", (unsigned)counting_data_current()->err_pcs[i])) {
                return false;
            }
        }

        has_err = true;
    }

    if (!has_err) {
        if (!ui_qr_data_append(buf, buf_size, &used, "None")) {
            return false;
        }
    }
    if (!ui_qr_data_append(buf, buf_size, &used, ";")) {
        return false;
    }

    if (!ui_qr_data_append(buf, buf_size, &used,
        QR_LINE_BREAK "--------------------" QR_LINE_BREAK)) {
        return false;
    }

    for (i = 0; i < counting_data_serial_scan_limit(counting_data_current()); i++) {
        if (counting_data_current()->sn_str[i] == NULL || counting_data_current()->sn_str[i][0] == '\0') {
            continue;
        }
        sn_count++;
    }

    if (!ui_qr_data_append(buf, buf_size, &used, "SN List (%d pcs):" QR_LINE_BREAK, sn_count)) {
        return false;
    }
    if (!ui_qr_data_append(buf, buf_size, &used,
        "Note: Read SN list left to right." QR_LINE_BREAK)) {
        return false;
    }

    if (sn_count == 0) {
        if (!ui_qr_data_append(buf, buf_size, &used, "-")) {
            return false;
        }
        return true;
    }

    for (i = 0; i < counting_data_serial_scan_limit(counting_data_current()); i++) {
        char sn_item[32];
        int sn_item_len;
        const char* tail = NULL;
        size_t tail_len = 0;
        size_t remain = 0;
        const size_t reserve_for_omitted_note = 48; //预留尾部空间，用于写入省略提示

        if (counting_data_current()->sn_str[i] == NULL || counting_data_current()->sn_str[i][0] == '\0') {
            continue;
        }

        sn_item_len = snprintf(sn_item, sizeof(sn_item), "%d.%s", emitted_sn + 1, counting_data_current()->sn_str[i]);
        if (sn_item_len < 0) {
            continue;
        }
        if ((size_t)sn_item_len >= sizeof(sn_item)) {
            omitted_sn = sn_count - emitted_sn;
            break;
        }

        tail = (((emitted_sn + 1) % 3) == 0) ? QR_LINE_BREAK : "   ";
        tail_len = (((emitted_sn + 1) % 3) == 0) ? 2U : 3U;
        remain = (used < qr_safe_payload_limit) ? (qr_safe_payload_limit - used) : 0U;

        if ((size_t)sn_item_len + tail_len + reserve_for_omitted_note >= remain) {
            omitted_sn = sn_count - emitted_sn;
            break;
        }

        if (!ui_qr_data_append(buf, buf_size, &used, "%s", sn_item)) {
            return false;
        }

        emitted_sn++;
        sn_col++;
        if ((sn_col % 3) == 0) {
            if (!ui_qr_data_append(buf, buf_size, &used, "%s", tail)) {
                return false;
            }
        } else {
            if (!ui_qr_data_append(buf, buf_size, &used, "%s", tail)) {
                return false;
            }
        }
    }

    if ((sn_col % 3) != 0) {
        if (!ui_qr_data_append(buf, buf_size, &used, QR_LINE_BREAK)) {
            return false;
        }
    }

    if (omitted_sn > 0) {
        if (!ui_qr_data_append(buf, buf_size, &used, "SN Omitted: %d;" QR_LINE_BREAK, omitted_sn)) {
            return false;
        }
    }

    return true;
}
