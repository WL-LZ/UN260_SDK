#include "touch_calibration_store.h"
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef TOUCH_CALIBRATION_STORE_DIR
#define TOUCH_CALIBRATION_STORE_DIR "/etc/ui_state"
#endif
#define CAL_FILE TOUCH_CALIBRATION_STORE_DIR "/touch_calibration.cfg"
#define CAL_TEMP CAL_FILE ".tmp"
#define CAL_MAGIC 0x554E5443U
#define CAL_VERSION 1U
typedef struct {
    uint32_t magic, version, enabled;
    touch_calibration_source_t source;
    touch_calibration_matrix_t matrix;
    uint32_t checksum;
} disk_calibration_t;
_Static_assert(sizeof(disk_calibration_t) == 68, "Calibration wire layout changed");

static uint32_t checksum(const disk_calibration_t *record)
{
    const unsigned char *bytes = (const unsigned char *)record;
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < offsetof(disk_calibration_t, checksum); ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = crc & 1 ? (crc >> 1) ^ 0xEDB88320U : crc >> 1;
    }
    return crc ^ 0xFFFFFFFFU;
}

touch_calibration_load_result_t touch_calibration_store_load(
    const touch_calibration_source_t *source, touch_calibration_config_t *config)
{
    if (!touch_calibration_source_valid(source) || !config) return TOUCH_CAL_STORE_INVALID;
    int fd = open(CAL_FILE, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return errno == ENOENT ? TOUCH_CAL_STORE_MISSING : TOUCH_CAL_STORE_IO_ERROR;
    struct stat info;
    bool io_error = fstat(fd, &info) != 0;
    bool valid = !io_error && S_ISREG(info.st_mode) &&
        info.st_size == (off_t)sizeof(disk_calibration_t);
    disk_calibration_t record;
    size_t got = 0;
    while (valid && got < sizeof(record)) {
        ssize_t bytes = read(fd, (unsigned char *)&record + got, sizeof(record) - got);
        if (bytes < 0 && errno == EINTR) continue;
        if (bytes < 0) io_error = true;
        if (bytes <= 0) { valid = false; break; }
        got += (size_t)bytes;
    }
    if (close(fd)) io_error = true;
    if (io_error) return TOUCH_CAL_STORE_IO_ERROR;
    if (!valid || record.magic != CAL_MAGIC || record.version != CAL_VERSION ||
        record.enabled > 1 || record.checksum != checksum(&record) ||
        memcmp(&record.source, source, sizeof(*source)) ||
        !touch_calibration_matrix_valid(source, &record.matrix)) return TOUCH_CAL_STORE_INVALID;
    *config = (touch_calibration_config_t){record.source, record.matrix, record.enabled != 0};
    return TOUCH_CAL_STORE_LOADED;
}

bool touch_calibration_store_save(const touch_calibration_config_t *config)
{
    if (!config || !touch_calibration_matrix_valid(&config->source, &config->matrix)) return false;
    if (mkdir(TOUCH_CALIBRATION_STORE_DIR, 0755) && errno != EEXIST) return false;
    /* Open the directory before mutation, so inaccessible storage cannot
     * replace a previously working calibration. */
    int dir = open(TOUCH_CALIBRATION_STORE_DIR, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dir < 0) return false;
    int fd = open(CAL_TEMP, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC, 0600);
    if (fd < 0) { close(dir); return false; }
    struct stat info;
    if (fstat(fd, &info) || !S_ISREG(info.st_mode)) {
        close(fd); close(dir); return false;
    }
    disk_calibration_t record = {CAL_MAGIC, CAL_VERSION, config->enabled ? 1U : 0U,
                                 config->source, config->matrix, 0};
    record.checksum = checksum(&record);
    bool ok = true;
    size_t written = 0;
    while (written < sizeof(record)) {
        ssize_t bytes = write(fd, (const unsigned char *)&record + written, sizeof(record) - written);
        if (bytes < 0 && errno == EINTR) continue;
        if (bytes <= 0) { ok = false; break; }
        written += (size_t)bytes;
    }
    if (ok && fsync(fd)) ok = false;
    if (close(fd)) ok = false;
    if (ok && rename(CAL_TEMP, CAL_FILE)) ok = false;
    if (ok && fsync(dir)) ok = false;
    if (close(dir)) ok = false;
    if (!ok) unlink(CAL_TEMP);
    return ok;
}
