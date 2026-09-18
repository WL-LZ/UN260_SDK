#include "counting_history_service.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "un260/lv_system/ui_history_data.h"
#include "counting_data_store.h"
#include "counting_multi.h"

#define COUNTING_HISTORY_FRAME_TEXT_SIZE 160
#define COUNTING_HISTORY_SESSION_LOG_SIZE 4096
#define COUNTING_HISTORY_HEX_BUFFER_SIZE (UINT8_MAX * 3U + 1U)
#define COUNTING_HISTORY_PENDING_CAPACITY 8U

static char g_last_error_frame_text[COUNTING_HISTORY_FRAME_TEXT_SIZE];
static char g_last_start_frame_text[COUNTING_HISTORY_FRAME_TEXT_SIZE];
static char g_last_end_frame_text[COUNTING_HISTORY_FRAME_TEXT_SIZE];
static char g_session_log_text[COUNTING_HISTORY_SESSION_LOG_SIZE];
static size_t g_session_log_len;
typedef struct {
    ui_history_record_t record;
    storage_job_id_t job;
    uint32_t multi_group;
} counting_history_snapshot_t;
static counting_history_snapshot_t g_snapshots[COUNTING_HISTORY_PENDING_CAPACITY];
static unsigned g_snapshot_count;
static counting_history_snapshot_t g_overflow_snapshot;
static bool g_overflow_valid;
static bool g_uncaptured_pending;
static bool g_failure_reported;
static uint32_t g_multi_generation, g_submitted_group, g_submitted_id, g_submitted_pcs;
static ui_history_record_t g_multi_snapshot;

static void counting_history_frame_to_hex(const uint8_t *buf,
                                          uint8_t len,
                                          char *out,
                                          size_t out_size)
{
    size_t pos = 0;

    if (out == NULL || out_size == 0U) {
        return;
    }
    out[0] = '\0';
    if (buf == NULL || len == 0U) {
        return;
    }

    for (uint16_t i = 0; i < len && pos + 3U < out_size; i++) {
        int written = snprintf(out + pos, out_size - pos, "%02X ", buf[i]);

        if (written != 3) {
            break;
        }
        pos += 3U;
    }
    if (pos > 0U) {
        out[pos - 1U] = '\0';
    }
}

static void counting_history_session_reset(void)
{
    g_last_start_frame_text[0] = '\0';
    g_last_end_frame_text[0] = '\0';
    g_session_log_text[0] = '\0';
    g_session_log_len = 0;
}

void counting_history_append_frame(const char *tag,
                                   const uint8_t *buf,
                                   uint8_t len)
{
    char hex[COUNTING_HISTORY_HEX_BUFFER_SIZE];
    int written;

    if (tag == NULL || buf == NULL || len == 0U ||
        g_session_log_len >= sizeof(g_session_log_text) - 1U) {
        return;
    }

    counting_history_frame_to_hex(buf, len, hex, sizeof(hex));
    written = snprintf(g_session_log_text + g_session_log_len,
                       sizeof(g_session_log_text) - g_session_log_len,
                       "%s %s\n", tag, hex);
    if (written <= 0) {
        return;
    }

    g_session_log_len += (size_t)written;
    if (g_session_log_len >= sizeof(g_session_log_text)) {
        g_session_log_len = sizeof(g_session_log_text) - 1U;
        g_session_log_text[g_session_log_len] = '\0';
    }
}

void counting_history_session_start(const uint8_t *buf, uint8_t len)
{
    g_last_error_frame_text[0] = '\0';
    counting_history_session_reset();
    counting_history_frame_to_hex(buf, len,
                                  g_last_start_frame_text,
                                  sizeof(g_last_start_frame_text));
    counting_history_append_frame("0x0A", buf, len);
}

void counting_history_capture_error(const char *tag,
                                    const uint8_t *buf,
                                    uint8_t len)
{
    counting_history_frame_to_hex(buf, len,
                                  g_last_error_frame_text,
                                  sizeof(g_last_error_frame_text));
    counting_history_append_frame(tag, buf, len);
}

void counting_history_capture_end(const uint8_t *buf, uint8_t len)
{
    counting_history_append_frame("0x0E", buf, len);
    counting_history_frame_to_hex(buf, len,
                                  g_last_end_frame_text,
                                  sizeof(g_last_end_frame_text));
}

static void counting_history_clear_pending(counting_session_state_t *session)
{
    session->history_record.valid = false;
    session->history_record.end_seen = false;
    session->history_record.save_attempts = 0;
    session->history_record.retry_tick = 0;
    session->history_record.pcs = 0;
    session->history_record.total_after = 0;
    session->history_record.amount = 0.0f;
    g_last_error_frame_text[0] = '\0';
    counting_history_session_reset();
}

static void counting_history_promote_overflow(void)
{
    if (g_overflow_valid && g_snapshot_count < COUNTING_HISTORY_PENDING_CAPACITY) {
        g_snapshots[g_snapshot_count++] = g_overflow_snapshot;
        g_overflow_valid = false;
    }
}

static void counting_history_submit_snapshots(void)
{
    unsigned i;
    counting_history_promote_overflow();
    for (i = 0; i < g_snapshot_count; i++) {
        uint32_t total, increment;
        if (g_snapshots[i].job != 0) continue;
        if (!ui_history_data_can_accept()) break;
        total = ui_history_total_notes_counted_get();
        counting_history_snapshot_t *s=&g_snapshots[i];
        bool update=s->multi_group && s->multi_group==g_submitted_group;
        increment=update ? (s->record.pcs>g_submitted_pcs ? s->record.pcs-g_submitted_pcs : 0) : s->record.pcs;
        total=increment>UINT32_MAX-total ? UINT32_MAX : total+increment;
        if(update) {
            s->record.record_no=g_submitted_id;
            bool exists=false;
            const ui_history_store_t *store=ui_history_data_get();
            for(unsigned j=0;j<store->record_count;j++)
                if(store->records[j].record_no==g_submitted_id) exists=true;
            if(exists) {
                if(!ui_history_record_update_snapshot(&s->record,total)) break;
            } else {
                /* Deletion wins over late detail; lifetime accounting remains independent. */
                ui_history_total_notes_counted_set(total);
                if(ui_history_total_notes_counted_get()!=total)break;
            }
        } else {
            uint32_t id=ui_history_data_get()->next_record_no;
            if(!ui_history_record_append_snapshot(&s->record,total)) break;
            g_submitted_id=id;g_submitted_group=s->multi_group;
        }
        g_submitted_pcs=s->record.pcs;
        g_snapshots[i].job = ui_history_last_commit_id();
    }
}

static counting_history_commit_result_t capture_multi(counting_session_state_t *session,
                                                       const counting_sim_t *sim, bool force)
{
    const counting_multi_t *m=counting_multi_current();
    if(m->counting || !m->passes || !m->total_pcs) return COUNTING_HISTORY_COMMIT_NOT_READY;
    bool waiting=false,blocked=false;
    for(unsigned i=0;i<m->count;i++) {
        multi_detail_status_t status=m->currencies[i].status;
        /* On-demand details must not prevent saving an unvisited result. */
        if(status==MULTI_DETAIL_LOADING)waiting=true;
        if(status==MULTI_DETAIL_TIMEOUT)blocked=true;
    }
    /* Persist summary promptly, then coalesce any user-requested details into one
     * detail snapshot. Reset/start may force a partial snapshot before reuse. */
    if(!force && g_multi_generation==m->generation && waiting && !blocked)
        return COUNTING_HISTORY_COMMIT_NOT_READY;
    history_multi_t data={0};
    data.enabled=true;data.count=m->count;data.rejects=m->reject;
    data.overflow=m->overflow;data.add=m->add;data.passes=m->passes;
    uint32_t grouped_pcs=0;
    for(unsigned i=0;i<m->count;i++) {
        const multi_currency_t *src=&m->currencies[i];
        history_multi_currency_t *dst=&data.currencies[i];
        memcpy(dst->code,src->code,4);dst->pcs=src->pcs;dst->amount=src->amount;
        grouped_pcs+=src->pcs;
        dst->complete=src->status==MULTI_DETAIL_READY ||
            (src->status==MULTI_DETAIL_EMPTY && !src->pcs && !src->amount);
        if(dst->complete) {
            dst->count=src->denom_count;
            for(unsigned j=0;j<src->denom_count;j++)
                dst->denoms[j]=(history_multi_denom_t){src->denom[j].value,src->denom[j].pcs};
        }
    }
    if(grouped_pcs!=m->total_pcs)data.overflow=true;
    if(g_multi_generation==m->generation && g_multi_snapshot.pcs==m->total_pcs &&
       !memcmp(&g_multi_snapshot.multi,&data,sizeof(data))) return COUNTING_HISTORY_COMMIT_NOT_READY;
    counting_history_promote_overflow();
    bool overflow=g_snapshot_count>=COUNTING_HISTORY_PENDING_CAPACITY;
    if(overflow && g_overflow_valid) {g_uncaptured_pending=true;return COUNTING_HISTORY_COMMIT_FAILED;}
    counting_history_snapshot_t *s=overflow?&g_overflow_snapshot:&g_snapshots[g_snapshot_count];
    memset(s,0,sizeof(*s));
    if(g_multi_generation!=m->generation) {
        if(!ui_history_record_build_multi_base(sim,m->total_pcs,g_last_error_frame_text,
            g_last_start_frame_text,g_last_end_frame_text,g_session_log_text,&s->record))
            return COUNTING_HISTORY_COMMIT_FAILED;
    } else s->record=g_multi_snapshot;
    s->record.multi=data;s->record.pcs=m->total_pcs;s->record.amount=0;
    memcpy(s->record.currency,"MUL",4);
    s->multi_group=m->group_generation;
    g_multi_snapshot=s->record;g_multi_generation=m->generation;
    if(overflow)g_overflow_valid=true;else g_snapshot_count++;
    g_uncaptured_pending=false;
    counting_history_clear_pending(session);
    counting_history_submit_snapshots();
    return COUNTING_HISTORY_COMMIT_PENDING;
}

counting_history_commit_result_t counting_history_try_commit(
    counting_session_state_t *session,
    const counting_sim_t *sim_data,
    uint32_t now_ms)
{
    counting_history_snapshot_t *snapshot;
    bool overflow;
    (void)now_ms;

    if(session && sim_data && sim_data->multi_currency_result)
        return capture_multi(session,sim_data,true);
    if (session == NULL || sim_data == NULL ||
        !session->history_record.valid || !session->history_record.end_seen) {
        return COUNTING_HISTORY_COMMIT_NOT_READY;
    }
    counting_history_promote_overflow();
    overflow = g_snapshot_count >= COUNTING_HISTORY_PENDING_CAPACITY;
    if (overflow && g_overflow_valid) {
        g_uncaptured_pending = true;
        return COUNTING_HISTORY_COMMIT_FAILED;
    }
    snapshot = overflow ? &g_overflow_snapshot : &g_snapshots[g_snapshot_count];
    if (!ui_history_record_build_from_session(
        sim_data,
        session->history_record.pcs,
        session->history_record.amount,
        g_last_error_frame_text,
        g_last_start_frame_text,
        g_last_end_frame_text,
        g_session_log_text, &snapshot->record)) {
        g_uncaptured_pending = true;
        return COUNTING_HISTORY_COMMIT_FAILED;
    }
    snapshot->job = 0;
    snapshot->multi_group = 0;
    if (overflow) g_overflow_valid = true;
    else g_snapshot_count++;
    g_uncaptured_pending = false;
    /* Ownership moves to bounded immutable storage, not to "saved". Neither
     * subsequent starts nor clear replies may destroy these snapshots. */
    counting_history_clear_pending(session);
    counting_history_submit_snapshots();
    return COUNTING_HISTORY_COMMIT_PENDING;
}

counting_history_commit_result_t counting_history_poll_commit(
    counting_session_state_t *session,
    const counting_sim_t *sim_data,
    uint32_t now_ms)
{
    bool saved = false;
    (void)now_ms;
    while (g_snapshot_count > 0 && g_snapshots[0].job != 0 &&
           ui_history_commit_status(g_snapshots[0].job) == STORAGE_JOB_SUCCEEDED) {
        memmove(g_snapshots, g_snapshots + 1,
                (--g_snapshot_count) * sizeof(g_snapshots[0]));
        saved = true;
    }
    counting_history_submit_snapshots();
    if (g_uncaptured_pending && session != NULL && sim_data != NULL &&
        session->history_record.valid && session->history_record.end_seen)
        (void)counting_history_try_commit(session, sim_data, now_ms);
    if (ui_history_data_status() == STORAGE_JOB_FAILED) {
        if (!g_failure_reported) {
            g_failure_reported = true;
            return COUNTING_HISTORY_COMMIT_FAILED;
        }
        return COUNTING_HISTORY_COMMIT_PENDING;
    }
    g_failure_reported = false;
    if(session && sim_data && sim_data->multi_currency_result)
        (void)capture_multi(session,sim_data,false);
    if (saved) return COUNTING_HISTORY_COMMIT_SAVED;
    return g_snapshot_count != 0 || g_overflow_valid ? COUNTING_HISTORY_COMMIT_PENDING
                                 : COUNTING_HISTORY_COMMIT_NOT_READY;
}

bool counting_history_discard_pending(counting_session_state_t *session)
{
    if (session == NULL || !session->history_record.valid) {
        return false;
    }
    /* A complete, not-yet-copied record is backpressure, not disposable data. */
    if (session->history_record.end_seen) {
        g_uncaptured_pending = true;
        return false;
    }
    counting_history_clear_pending(session);
    return true;
}

bool counting_history_can_start(void)
{
    return !g_uncaptured_pending && !g_overflow_valid &&
           g_snapshot_count < COUNTING_HISTORY_PENDING_CAPACITY &&
           ui_history_data_can_accept() && ui_history_data_status() != STORAGE_JOB_FAILED;
}

bool counting_history_prepare_reset(counting_session_state_t *session,
    const counting_sim_t *sim_data, uint32_t now_ms)
{
    bool end_seen;
    if (session == NULL) return false;
    if(sim_data && sim_data->multi_currency_result) {
        (void)capture_multi(session,sim_data,true);
        return !g_uncaptured_pending;
    }
    if (!session->history_record.valid) return true;
    end_seen = session->history_record.end_seen;
    /* A new start/reset ends the opportunity for optional detail frames. Save
     * the complete available snapshot now, including a not-yet-settled count. */
    session->history_record.end_seen = true;
    (void)counting_history_try_commit(session, sim_data, now_ms);
    if (session->history_record.valid) {
        session->history_record.end_seen = end_seen;
        return false;
    }
    return true;
}

bool counting_history_prepare_start(counting_session_state_t *session,
    const counting_sim_t *sim_data, uint32_t now_ms)
{
    return counting_history_prepare_reset(session, sim_data, now_ms) &&
           counting_history_can_start();
}
