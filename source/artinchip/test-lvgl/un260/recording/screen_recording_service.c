#include "screen_recording_service.h"

#include "dma_allocator.h"
#include "lv_fbdev.h"
#include "mpp_ge.h"
#include "un260/lv_drivers/uart_io.h"
#include "un260/storage/usb_storage.h"

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <time.h>
#include <unistd.h>
#include <video/mpp_types.h>

#define SCREEN_RECORDING_JPEG_QUALITY        75
#define SCREEN_RECORDING_MAX_SECONDS         (30U * 60U)
#define SCREEN_RECORDING_MAX_FILE_BYTES      (0x70000000UL)
#define SCREEN_RECORDING_MIN_FREE_BYTES      (32ULL * 1024ULL * 1024ULL)
#define SCREEN_RECORDING_JPEG_BYTES_PER_PIXEL 2U

#define AVI_FLAG_HAS_INDEX 0x00000010U
#define AVI_INDEX_KEYFRAME 0x00000010U

/* Public SDK symbol; this SDK installs the library but not mpp_encoder.h. */
int mpp_encode_jpeg(struct mpp_frame *frame, int quality,
                    int dma_buf_fd, int buf_len, int *len);

typedef struct {
    uint32_t offset;
    uint32_t size;
} avi_index_entry_t;

typedef struct {
    FILE *fp;
    avi_index_entry_t *index;
    size_t index_count;
    size_t index_capacity;
    long riff_size_pos;
    long total_frames_pos;
    long stream_length_pos;
    long max_bytes_per_sec_pos;
    long suggested_buffer_pos;
    long stream_buffer_pos;
    long movi_size_pos;
    long movi_data_pos;
    uint32_t width;
    uint32_t height;
    uint32_t max_frame_size;
    uint32_t fps;
    uint32_t max_frames;
} avi_writer_t;

typedef enum {
    AVI_APPEND_OK = 0,
    AVI_APPEND_LIMIT_REACHED,
    AVI_APPEND_FAILED,
} avi_append_result_t;

typedef struct {
    pthread_mutex_t lock;
    pthread_t thread;
    bool thread_joinable;
    bool stop_requested;
    bool completion_pending;
    screen_recording_state_t state;
    screen_recording_completion_info_t completion;
    char final_path[256];
    char temp_path[272];
    uint32_t fps;
} screen_recording_service_t;

static screen_recording_service_t g_recording = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .state = SCREEN_RECORDING_IDLE,
};

static bool write_bytes(FILE *fp, const void *data, size_t size)
{
    return fp != NULL && fwrite(data, 1, size, fp) == size;
}

static bool write_u16(FILE *fp, uint16_t value)
{
    uint8_t data[2] = {
        (uint8_t)(value & 0xffU),
        (uint8_t)((value >> 8) & 0xffU),
    };

    return write_bytes(fp, data, sizeof(data));
}

static bool write_u32(FILE *fp, uint32_t value)
{
    uint8_t data[4] = {
        (uint8_t)(value & 0xffU),
        (uint8_t)((value >> 8) & 0xffU),
        (uint8_t)((value >> 16) & 0xffU),
        (uint8_t)((value >> 24) & 0xffU),
    };

    return write_bytes(fp, data, sizeof(data));
}

static bool write_fourcc(FILE *fp, const char code[4])
{
    return write_bytes(fp, code, 4U);
}

static bool patch_u32(FILE *fp, long position, uint32_t value)
{
    long current;

    if (fp == NULL || position < 0) {
        return false;
    }
    current = ftell(fp);
    if (current < 0 || fseek(fp, position, SEEK_SET) != 0 ||
        !write_u32(fp, value) || fseek(fp, current, SEEK_SET) != 0) {
        return false;
    }
    return true;
}

static bool avi_writer_write_main_header(avi_writer_t *writer)
{
    FILE *fp = writer->fp;

    if (!write_fourcc(fp, "avih") || !write_u32(fp, 56U) ||
        !write_u32(fp, 1000000U / writer->fps)) {
        return false;
    }
    writer->max_bytes_per_sec_pos = ftell(fp);
    if (!write_u32(fp, 0U) || !write_u32(fp, 0U) ||
        !write_u32(fp, AVI_FLAG_HAS_INDEX)) {
        return false;
    }
    writer->total_frames_pos = ftell(fp);
    return write_u32(fp, 0U) &&
           write_u32(fp, 0U) &&
           write_u32(fp, 1U) &&
           (writer->suggested_buffer_pos = ftell(fp)) >= 0 &&
           write_u32(fp, 0U) &&
           write_u32(fp, writer->width) &&
           write_u32(fp, writer->height) &&
           write_u32(fp, 0U) && write_u32(fp, 0U) &&
           write_u32(fp, 0U) && write_u32(fp, 0U);
}

static bool avi_writer_write_stream_header(avi_writer_t *writer)
{
    FILE *fp = writer->fp;

    if (!write_fourcc(fp, "strh") || !write_u32(fp, 56U) ||
        !write_fourcc(fp, "vids") || !write_fourcc(fp, "MJPG") ||
        !write_u32(fp, 0U) || !write_u16(fp, 0U) || !write_u16(fp, 0U) ||
        !write_u32(fp, 0U) || !write_u32(fp, 1U) ||
        !write_u32(fp, writer->fps) || !write_u32(fp, 0U)) {
        return false;
    }
    writer->stream_length_pos = ftell(fp);
    if (!write_u32(fp, 0U)) {
        return false;
    }
    writer->stream_buffer_pos = ftell(fp);
    return write_u32(fp, 0U) &&
           write_u32(fp, UINT32_MAX) &&
           write_u32(fp, 0U) &&
           write_u16(fp, 0U) && write_u16(fp, 0U) &&
           write_u16(fp, (uint16_t)writer->width) &&
           write_u16(fp, (uint16_t)writer->height);
}

static bool avi_writer_write_format(avi_writer_t *writer)
{
    uint64_t image_size = (uint64_t)writer->width * writer->height * 3U;

    if (image_size > UINT32_MAX) {
        return false;
    }
    return write_fourcc(writer->fp, "strf") &&
           write_u32(writer->fp, 40U) &&
           write_u32(writer->fp, 40U) &&
           write_u32(writer->fp, writer->width) &&
           write_u32(writer->fp, writer->height) &&
           write_u16(writer->fp, 1U) &&
           write_u16(writer->fp, 24U) &&
           write_fourcc(writer->fp, "MJPG") &&
           write_u32(writer->fp, (uint32_t)image_size) &&
           write_u32(writer->fp, 0U) && write_u32(writer->fp, 0U) &&
           write_u32(writer->fp, 0U) && write_u32(writer->fp, 0U);
}

static bool avi_writer_open(avi_writer_t *writer, const char *path,
                            uint32_t width, uint32_t height, uint32_t fps)
{
    long hdrl_size_pos;
    long hdrl_data_pos;
    long strl_size_pos;
    long strl_data_pos;
    long end_pos;

    memset(writer, 0, sizeof(*writer));
    writer->width = width;
    writer->height = height;
    writer->fps = fps;
    writer->max_frames = fps * SCREEN_RECORDING_MAX_SECONDS;
    writer->fp = fopen(path, "wb+");
    if (writer->fp == NULL) {
        return false;
    }

    if (!write_fourcc(writer->fp, "RIFF")) goto failed;
    writer->riff_size_pos = ftell(writer->fp);
    if (!write_u32(writer->fp, 0U) || !write_fourcc(writer->fp, "AVI ") ||
        !write_fourcc(writer->fp, "LIST")) goto failed;
    hdrl_size_pos = ftell(writer->fp);
    if (!write_u32(writer->fp, 0U) || !write_fourcc(writer->fp, "hdrl")) goto failed;
    hdrl_data_pos = hdrl_size_pos + 4L;

    if (!avi_writer_write_main_header(writer) ||
        !write_fourcc(writer->fp, "LIST")) goto failed;
    strl_size_pos = ftell(writer->fp);
    if (!write_u32(writer->fp, 0U) || !write_fourcc(writer->fp, "strl")) goto failed;
    strl_data_pos = strl_size_pos + 4L;
    if (!avi_writer_write_stream_header(writer) ||
        !avi_writer_write_format(writer)) goto failed;
    end_pos = ftell(writer->fp);
    if (end_pos < 0 ||
        !patch_u32(writer->fp, strl_size_pos,
                   (uint32_t)(end_pos - strl_data_pos)) ||
        !patch_u32(writer->fp, hdrl_size_pos,
                   (uint32_t)(end_pos - hdrl_data_pos)) ||
        !write_fourcc(writer->fp, "LIST")) goto failed;
    writer->movi_size_pos = ftell(writer->fp);
    if (!write_u32(writer->fp, 0U) || !write_fourcc(writer->fp, "movi")) goto failed;
    writer->movi_data_pos = ftell(writer->fp);
    return writer->movi_data_pos >= 0;

failed:
    fclose(writer->fp);
    writer->fp = NULL;
    return false;
}

static bool avi_writer_reserve_index(avi_writer_t *writer)
{
    size_t next_capacity;
    avi_index_entry_t *next;

    if (writer->index_count < writer->index_capacity) {
        return true;
    }
    next_capacity = writer->index_capacity == 0U ? 256U : writer->index_capacity * 2U;
    if (next_capacity > writer->max_frames) {
        next_capacity = writer->max_frames;
    }
    if (next_capacity <= writer->index_capacity) {
        return false;
    }
    next = realloc(writer->index, next_capacity * sizeof(*next));
    if (next == NULL) {
        return false;
    }
    writer->index = next;
    writer->index_capacity = next_capacity;
    return true;
}

static avi_append_result_t avi_writer_append_frame(avi_writer_t *writer,
                                                   const uint8_t *jpeg,
                                                   uint32_t jpeg_size)
{
    long chunk_pos;
    uint64_t projected_size;
    uint32_t offset;
    uint8_t padding = 0U;

    if (writer->fp == NULL || jpeg == NULL || jpeg_size == 0U) {
        return AVI_APPEND_FAILED;
    }
    chunk_pos = ftell(writer->fp);
    if (chunk_pos < 0 || chunk_pos < writer->movi_data_pos - 4L) {
        return AVI_APPEND_FAILED;
    }
    projected_size = (uint64_t)chunk_pos + 8U + jpeg_size +
                     (jpeg_size & 1U) + 8U +
                     (writer->index_count + 1U) * 16U;
    if (projected_size >= SCREEN_RECORDING_MAX_FILE_BYTES) {
        return AVI_APPEND_LIMIT_REACHED;
    }
    if (!avi_writer_reserve_index(writer)) {
        return AVI_APPEND_FAILED;
    }
    offset = (uint32_t)(chunk_pos - (writer->movi_data_pos - 4L));
    if (!write_fourcc(writer->fp, "00dc") ||
        !write_u32(writer->fp, jpeg_size) ||
        !write_bytes(writer->fp, jpeg, jpeg_size) ||
        ((jpeg_size & 1U) != 0U && !write_bytes(writer->fp, &padding, 1U))) {
        return AVI_APPEND_FAILED;
    }
    writer->index[writer->index_count].offset = offset;
    writer->index[writer->index_count].size = jpeg_size;
    writer->index_count++;
    if (jpeg_size > writer->max_frame_size) {
        writer->max_frame_size = jpeg_size;
    }
    return AVI_APPEND_OK;
}

static bool avi_writer_finish(avi_writer_t *writer)
{
    long movi_end;
    long final_size;
    uint32_t frame_count;
    bool ok = true;

    if (writer->fp == NULL || writer->index_count == 0U ||
        writer->index_count > UINT32_MAX) {
        return false;
    }
    frame_count = (uint32_t)writer->index_count;
    movi_end = ftell(writer->fp);
    if (movi_end < 0 ||
        !write_fourcc(writer->fp, "idx1") ||
        !write_u32(writer->fp, frame_count * 16U)) {
        return false;
    }
    for (size_t i = 0; i < writer->index_count; i++) {
        if (!write_fourcc(writer->fp, "00dc") ||
            !write_u32(writer->fp, AVI_INDEX_KEYFRAME) ||
            !write_u32(writer->fp, writer->index[i].offset) ||
            !write_u32(writer->fp, writer->index[i].size)) {
            ok = false;
            break;
        }
    }
    final_size = ftell(writer->fp);
    if (!ok || final_size < 8L || movi_end < writer->movi_size_pos + 4L ||
        (unsigned long)final_size >= SCREEN_RECORDING_MAX_FILE_BYTES) {
        return false;
    }
    ok = patch_u32(writer->fp, writer->riff_size_pos,
                   (uint32_t)(final_size - 8L)) &&
         patch_u32(writer->fp, writer->movi_size_pos,
                   (uint32_t)(movi_end - (writer->movi_size_pos + 4L))) &&
         patch_u32(writer->fp, writer->total_frames_pos, frame_count) &&
         patch_u32(writer->fp, writer->stream_length_pos, frame_count) &&
         patch_u32(writer->fp, writer->max_bytes_per_sec_pos,
                   writer->max_frame_size * writer->fps) &&
         patch_u32(writer->fp, writer->suggested_buffer_pos,
                   writer->max_frame_size) &&
         patch_u32(writer->fp, writer->stream_buffer_pos,
                   writer->max_frame_size);
    if (ok && (fflush(writer->fp) != 0 || fsync(fileno(writer->fp)) != 0)) {
        ok = false;
    }
    if (fclose(writer->fp) != 0) {
        ok = false;
    }
    writer->fp = NULL;
    return ok;
}

static void avi_writer_abort(avi_writer_t *writer)
{
    if (writer->fp != NULL) {
        fclose(writer->fp);
        writer->fp = NULL;
    }
    free(writer->index);
    writer->index = NULL;
    writer->index_count = 0U;
    writer->index_capacity = 0U;
}

static bool screen_recording_has_space(void)
{
    struct statvfs info;
    uint64_t available;

    if (statvfs(USB_STORAGE_MOUNT_POINT, &info) != 0) {
        return false;
    }
    available = (uint64_t)info.f_bavail * (uint64_t)info.f_frsize;
    return available >= SCREEN_RECORDING_MIN_FREE_BYTES;
}

static bool screen_recording_make_paths(char *final_path, size_t final_size,
                                        char *temp_path, size_t temp_size)
{
    char timestamp[32];
    struct tm local_tm;
    time_t now = time(NULL);

    if (localtime_r(&now, &local_tm) == NULL ||
        strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &local_tm) == 0U) {
        return false;
    }
    for (int suffix = 0; suffix < 100; suffix++) {
        int written;

        if (suffix == 0) {
            written = snprintf(final_path, final_size, "%s/screen_record_%s.avi",
                               USB_STORAGE_MOUNT_POINT, timestamp);
        } else {
            written = snprintf(final_path, final_size, "%s/screen_record_%s_%02d.avi",
                               USB_STORAGE_MOUNT_POINT, timestamp, suffix);
        }
        if (written < 0 || (size_t)written >= final_size) {
            return false;
        }
        written = snprintf(temp_path, temp_size, "%s.part", final_path);
        if (written < 0 || (size_t)written >= temp_size) {
            return false;
        }
        if (access(final_path, F_OK) != 0 && access(temp_path, F_OK) != 0) {
            return true;
        }
    }
    return false;
}

static bool screen_recording_stop_requested(void)
{
    bool requested;

    pthread_mutex_lock(&g_recording.lock);
    requested = g_recording.stop_requested;
    pthread_mutex_unlock(&g_recording.lock);
    return requested;
}

static bool screen_recording_convert_frame(struct mpp_ge *ge,
                                           struct mpp_frame *frame)
{
    struct ge_bitblt blt;

    memset(&blt, 0, sizeof(blt));
    if (fbdev_get_visible_buffer(&blt.src_buf) != 0) {
        return false;
    }
    blt.dst_buf = frame->buf;
    if (mpp_ge_bitblt(ge, &blt) < 0 ||
        mpp_ge_emit(ge) < 0 || mpp_ge_sync(ge) < 0) {
        return false;
    }
    return true;
}

static void screen_recording_set_active(void)
{
    pthread_mutex_lock(&g_recording.lock);
    if (!g_recording.stop_requested) {
        g_recording.state = SCREEN_RECORDING_ACTIVE;
    } else {
        g_recording.state = SCREEN_RECORDING_STOPPING;
    }
    pthread_mutex_unlock(&g_recording.lock);
}

static void screen_recording_complete(screen_recording_completion_t result,
                                      const char *path, uint32_t frames)
{
    pthread_mutex_lock(&g_recording.lock);
    g_recording.state = SCREEN_RECORDING_IDLE;
    g_recording.completion.result = result;
    g_recording.completion.frames = frames;
    snprintf(g_recording.completion.path, sizeof(g_recording.completion.path),
             "%s", path != NULL ? path : "");
    g_recording.completion_pending = true;
    pthread_mutex_unlock(&g_recording.lock);
}

static void screen_recording_sleep_until(struct timespec *deadline, uint32_t fps)
{
    const long frame_ns = 1000000000L / (long)fps;

    deadline->tv_nsec += frame_ns;
    while (deadline->tv_nsec >= 1000000000L) {
        deadline->tv_nsec -= 1000000000L;
        deadline->tv_sec++;
    }
    while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, deadline, NULL) == EINTR) {
    }
}

static void *screen_recording_thread(void *arg)
{
    struct mpp_frame frame;
    struct mpp_ge *ge = NULL;
    avi_writer_t writer;
    unsigned char *jpeg_data = NULL;
    char final_path[sizeof(g_recording.final_path)];
    char temp_path[sizeof(g_recording.temp_path)];
    struct timespec deadline;
    int dma_device = -1;
    int jpeg_fd = -1;
    int jpeg_capacity = 0;
    int width = 0;
    int height = 0;
    uint32_t frames = 0U;
    uint32_t fps;
    const char *failure_stage = "initialization";
    int failure_errno = 0;
    int ge_mode = -1;
    int ge_registered_count = 0;
    bool writer_open = false;
    bool frame_allocated = false;
    bool saved = false;

    (void)arg;
    memset(&frame, 0, sizeof(frame));
    memset(&writer, 0, sizeof(writer));
    pthread_mutex_lock(&g_recording.lock);
    snprintf(final_path, sizeof(final_path), "%s", g_recording.final_path);
    snprintf(temp_path, sizeof(temp_path), "%s", g_recording.temp_path);
    fps = g_recording.fps;
    pthread_mutex_unlock(&g_recording.lock);

    if (fbdev_get_size(&width, &height) != 0 || width <= 0 || height <= 0 ||
        (width & 1) != 0 || (height & 1) != 0 ||
        width > UINT16_MAX || height > UINT16_MAX) {
        failure_stage = "display-size";
        goto done;
    }
    dma_device = dmabuf_device_open();
    if (dma_device < 0) {
        failure_stage = "dma-device-open";
        goto done;
    }

    frame.buf.size.width = width;
    frame.buf.size.height = height;
    frame.buf.stride[0] = (unsigned int)width;
    frame.buf.stride[1] = frame.buf.stride[2] = (unsigned int)width / 2U;
    frame.buf.format = MPP_FMT_YUV420P;
    frame.buf.flags = MPP_COLOR_SPACE_BT601;
    if (mpp_buf_alloc(dma_device, &frame.buf) < 0) {
        failure_stage = "yuv-buffer-alloc";
        goto done;
    }
    frame_allocated = true;

    jpeg_capacity = width * height * (int)SCREEN_RECORDING_JPEG_BYTES_PER_PIXEL;
    jpeg_fd = dmabuf_alloc(dma_device, jpeg_capacity);
    if (jpeg_fd < 0) {
        failure_stage = "jpeg-buffer-alloc";
        goto done;
    }
    jpeg_data = dmabuf_mmap(jpeg_fd, jpeg_capacity);
    if (jpeg_data == NULL) {
        failure_stage = "jpeg-buffer-map";
        goto done;
    }

    ge = mpp_ge_open();
    if (ge == NULL) {
        failure_stage = "ge-open";
        goto done;
    }
    ge_mode = (int)mpp_ge_get_mode(ge);
    if (ge_mode == GE_MODE_CMDQ) {
        for (int i = 0; i < 3; i++) {
            int add_result;

            errno = 0;
            add_result = mpp_ge_add_dmabuf(ge, frame.buf.fd[i]);
            if (add_result < 0) {
                failure_errno = errno;
                uart_debug_printf("screen recording GE add failed plane=%d fd=%d ret=%d errno=%d mode=%d\n",
                                  i, frame.buf.fd[i], add_result,
                                  failure_errno, ge_mode);
                failure_stage = "ge-add-yuv-buffer";
                goto done;
            }
            ge_registered_count++;
        }
    }
    if (!avi_writer_open(&writer, temp_path, (uint32_t)width,
                         (uint32_t)height, fps)) {
        failure_stage = "avi-open";
        goto done;
    }
    writer_open = true;
    screen_recording_set_active();
    clock_gettime(CLOCK_MONOTONIC, &deadline);

    while (!screen_recording_stop_requested() && frames < writer.max_frames) {
        avi_append_result_t append_result;
        int jpeg_len = 0;

        if (!screen_recording_convert_frame(ge, &frame)) {
            failure_stage = "ge-rgb-to-yuv";
            goto done;
        }
        if (mpp_encode_jpeg(&frame, SCREEN_RECORDING_JPEG_QUALITY,
                            jpeg_fd, jpeg_capacity, &jpeg_len) < 0) {
            failure_stage = "jpeg-encode";
            goto done;
        }
        if (jpeg_len <= 0 || jpeg_len > jpeg_capacity) {
            failure_stage = "jpeg-size";
            goto done;
        }
        append_result = avi_writer_append_frame(&writer, jpeg_data,
                                                (uint32_t)jpeg_len);
        if (append_result == AVI_APPEND_LIMIT_REACHED) {
            break;
        }
        if (append_result != AVI_APPEND_OK) {
            failure_stage = "avi-frame-write";
            goto done;
        }
        frames++;
        if ((frames % fps) == 0U &&
            !screen_recording_has_space()) {
            break;
        }
        screen_recording_sleep_until(&deadline, fps);
    }

    failure_stage = frames > 0U ? "avi-finalize" : "no-frame";
    if (frames > 0U && avi_writer_finish(&writer)) {
        writer_open = false;
        if (rename(temp_path, final_path) == 0) {
            saved = true;
        } else {
            failure_stage = "avi-rename";
        }
    }

done:
    if (saved) {
        uart_debug_printf("screen recording saved frames=%u path=%s\n",
                          frames, final_path);
    } else {
        uart_debug_printf("screen recording failed stage=%s frames=%u errno=%d\n",
                          failure_stage, frames,
                          failure_errno != 0 ? failure_errno : errno);
    }
    if (writer_open) {
        avi_writer_abort(&writer);
    } else {
        free(writer.index);
    }
    if (!saved) {
        unlink(temp_path);
    }
    if (ge != NULL) {
        for (int i = 0; i < ge_registered_count; i++) {
            mpp_ge_rm_dmabuf(ge, frame.buf.fd[i]);
        }
        mpp_ge_close(ge);
    }
    if (jpeg_data != NULL) dmabuf_munmap(jpeg_data, jpeg_capacity);
    if (jpeg_fd >= 0) dmabuf_free(jpeg_fd);
    if (frame_allocated) mpp_buf_free(&frame.buf);
    if (dma_device >= 0) dmabuf_device_close(dma_device);
    screen_recording_complete(saved ? SCREEN_RECORDING_COMPLETION_SAVED
                                    : SCREEN_RECORDING_COMPLETION_FAILED,
                              saved ? final_path : "", frames);
    return NULL;
}

screen_recording_start_result_t screen_recording_service_start(uint32_t fps)
{
    char final_path[sizeof(g_recording.final_path)];
    char temp_path[sizeof(g_recording.temp_path)];
    int create_result;

    if (fps != 8U && fps != 24U && fps != 60U) {
        return SCREEN_RECORDING_START_FAILED;
    }
    pthread_mutex_lock(&g_recording.lock);
    if (g_recording.state != SCREEN_RECORDING_IDLE ||
        g_recording.thread_joinable || g_recording.completion_pending) {
        pthread_mutex_unlock(&g_recording.lock);
        return SCREEN_RECORDING_START_BUSY;
    }
    pthread_mutex_unlock(&g_recording.lock);

    if (!usb_storage_prepare()) {
        uart_debug_printf("screen recording start failed: USB not ready\n");
        return SCREEN_RECORDING_START_USB_NOT_READY;
    }
    if (!screen_recording_has_space()) {
        uart_debug_printf("screen recording start failed: free space check errno=%d\n",
                          errno);
        return SCREEN_RECORDING_START_FAILED;
    }
    if (!screen_recording_make_paths(final_path, sizeof(final_path),
                                     temp_path, sizeof(temp_path))) {
        uart_debug_printf("screen recording start failed: output path errno=%d\n",
                          errno);
        return SCREEN_RECORDING_START_FAILED;
    }

    pthread_mutex_lock(&g_recording.lock);
    if (g_recording.state != SCREEN_RECORDING_IDLE ||
        g_recording.thread_joinable || g_recording.completion_pending) {
        pthread_mutex_unlock(&g_recording.lock);
        return SCREEN_RECORDING_START_BUSY;
    }
    snprintf(g_recording.final_path, sizeof(g_recording.final_path), "%s", final_path);
    snprintf(g_recording.temp_path, sizeof(g_recording.temp_path), "%s", temp_path);
    g_recording.stop_requested = false;
    g_recording.fps = fps;
    g_recording.state = SCREEN_RECORDING_STARTING;
    create_result = pthread_create(&g_recording.thread, NULL,
                                   screen_recording_thread, NULL);
    if (create_result != 0) {
        g_recording.state = SCREEN_RECORDING_IDLE;
        pthread_mutex_unlock(&g_recording.lock);
        uart_debug_printf("screen recording start failed: pthread=%d\n",
                          create_result);
        return SCREEN_RECORDING_START_FAILED;
    }
    g_recording.thread_joinable = true;
    pthread_mutex_unlock(&g_recording.lock);
    return SCREEN_RECORDING_START_OK;
}

void screen_recording_service_request_stop(void)
{
    pthread_mutex_lock(&g_recording.lock);
    if (g_recording.state == SCREEN_RECORDING_STARTING ||
        g_recording.state == SCREEN_RECORDING_ACTIVE) {
        g_recording.stop_requested = true;
        g_recording.state = SCREEN_RECORDING_STOPPING;
    }
    pthread_mutex_unlock(&g_recording.lock);
}

screen_recording_state_t screen_recording_service_state(void)
{
    screen_recording_state_t state;

    pthread_mutex_lock(&g_recording.lock);
    state = g_recording.state;
    pthread_mutex_unlock(&g_recording.lock);
    return state;
}

bool screen_recording_service_poll_completion(
    screen_recording_completion_info_t *completion)
{
    pthread_t thread;
    bool should_join = false;

    if (completion == NULL) {
        return false;
    }
    pthread_mutex_lock(&g_recording.lock);
    if (!g_recording.completion_pending) {
        pthread_mutex_unlock(&g_recording.lock);
        return false;
    }
    *completion = g_recording.completion;
    g_recording.completion_pending = false;
    if (g_recording.thread_joinable) {
        thread = g_recording.thread;
        g_recording.thread_joinable = false;
        should_join = true;
    }
    pthread_mutex_unlock(&g_recording.lock);
    if (should_join) {
        pthread_join(thread, NULL);
    }
    return true;
}
