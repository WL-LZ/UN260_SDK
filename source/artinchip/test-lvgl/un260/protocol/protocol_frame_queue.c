#include "protocol_frame_queue.h"

#include <pthread.h>
#include <string.h>

#include "protocol_frame.h"

static protocol_frame_t g_frames[PROTOCOL_FRAME_QUEUE_CAPACITY];
static int g_head;
static int g_tail;
static int g_count;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

bool protocol_frame_queue_push(const uint8_t *data, int len)
{
    bool pushed = false;

    if (len < 0 || !protocol_frame_is_valid(data, (size_t)len)) {
        return false;
    }

    pthread_mutex_lock(&g_mutex);
    if (g_count < PROTOCOL_FRAME_QUEUE_CAPACITY) {
        memcpy(g_frames[g_tail].data, data, (size_t)len);
        g_frames[g_tail].len = (uint8_t)len;
        g_tail = (g_tail + 1) % PROTOCOL_FRAME_QUEUE_CAPACITY;
        g_count++;
        pushed = true;
    }
    pthread_mutex_unlock(&g_mutex);

    return pushed;
}

bool protocol_frame_queue_pop(protocol_frame_t *frame)
{
    bool popped = false;

    if (frame == NULL) {
        return false;
    }

    pthread_mutex_lock(&g_mutex);
    if (g_count > 0) {
        *frame = g_frames[g_head];
        g_head = (g_head + 1) % PROTOCOL_FRAME_QUEUE_CAPACITY;
        g_count--;
        popped = true;
    }
    pthread_mutex_unlock(&g_mutex);

    return popped;
}

void protocol_frame_queue_clear(void)
{
    pthread_mutex_lock(&g_mutex);
    g_head = 0;
    g_tail = 0;
    g_count = 0;
    pthread_mutex_unlock(&g_mutex);
}
