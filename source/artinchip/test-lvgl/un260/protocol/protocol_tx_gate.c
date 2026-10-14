#include "protocol_tx_gate.h"
#include "un260/lv_system/app_clock.h"
#include <pthread.h>
#include <string.h>

/* Optional feedback expires before this fallback. A missing reply must not
 * keep the serial lane muted forever, or change business timeout ownership. */
#define REPLY_WINDOW_MS 300U
#define SETTLE_MS 8U
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static uint64_t pending_until[256], quiet_until;

int protocol_tx_gate_send(const uint8_t *frame, uint16_t len, bool optional,
                          protocol_tx_write_t write_frame)
{
    if(!frame || len < 5 || !write_frame) return -1;
    if(optional) {
        if(pthread_mutex_trylock(&mutex)) return 0;
        uint64_t now = app_clock_monotonic_ms();
        bool busy = now < quiet_until;
        for(unsigned i = 0; i < 256 && !busy; ++i) busy = now < pending_until[i];
        if(busy) { pthread_mutex_unlock(&mutex); return 0; }
    } else pthread_mutex_lock(&mutex);
    /* Check and write share one lock: another normal command cannot slip
     * between an idle check and the optional frame reaching UART. */
    int result = write_frame(frame,len);
    uint64_t now = app_clock_monotonic_ms();
    quiet_until = now + SETTLE_MS;
    if(!optional && result == len) pending_until[frame[3]] = now + REPLY_WINDOW_MS;
    pthread_mutex_unlock(&mutex);
    return result;
}
void protocol_tx_gate_on_reply(uint8_t command)
{
    pthread_mutex_lock(&mutex);
    pending_until[command] = 0;
    quiet_until = app_clock_monotonic_ms() + SETTLE_MS;
    pthread_mutex_unlock(&mutex);
}
void protocol_tx_gate_reset(void)
{
    pthread_mutex_lock(&mutex);
    memset(pending_until,0,sizeof(pending_until));
    quiet_until = 0;
    pthread_mutex_unlock(&mutex);
}
