#include "un260/lv_components/ui_notice_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool post(ui_notice_state_t *s, ui_notice_kind_t kind, const char *key,
                 const char *title, const char *detail)
{
    ui_notice_config_t c = { .kind=kind, .key=key, .title=title, .detail=detail };
    return ui_notice_state_post(s, &c);
}

static void test_task_result_and_dismissal(void)
{
    ui_notice_state_t s;
    ui_notice_state_init(&s);
    assert(post(&s, UI_NOTICE_PROGRESS, "receipt", "Applying", "Waiting for reply"));
    assert(!ui_notice_state_elapse(&s, 600000));
    assert(s.has_active && s.active.remaining_ms == 0);
    assert(ui_notice_state_remove(&s, NULL, true));
    assert(!s.has_active);
    assert(!post(&s, UI_NOTICE_PROGRESS, "receipt", "Applying", "Waiting for reply"));
    assert(post(&s, UI_NOTICE_ERROR, "receipt", "Not applied", "Previous value retained"));
    assert(s.has_active && s.active.kind == UI_NOTICE_ERROR && s.dismissed_count == 0);
    assert(ui_notice_state_remove(&s, NULL, true));
    assert(post(&s, UI_NOTICE_PROGRESS, "receipt", "Applying", NULL));
    assert(post(&s, UI_NOTICE_SUCCESS, "receipt", "Saved", NULL));
    assert(s.active.kind == UI_NOTICE_SUCCESS && s.queued_count == 0);
}

static void test_priority_resume_and_coalesce(void)
{
    ui_notice_state_t s;
    ui_notice_state_init(&s);
    assert(post(&s, UI_NOTICE_INFO, "one", "Information", NULL));
    assert(!ui_notice_state_elapse(&s, 1200));
    assert(post(&s, UI_NOTICE_ERROR, "error", "Failed", "Check connection"));
    assert(s.active.kind == UI_NOTICE_ERROR && s.queued_count == 1);
    assert(s.queued[0].remaining_ms == 1050);
    assert(post(&s, UI_NOTICE_ERROR, "error", "Failed", "Check connection"));
    assert(s.active.repeats == 2 && s.queued_count == 1);
    assert(post(&s, UI_NOTICE_ERROR, "error", "New failure", NULL));
    assert(s.active.repeats == 1);
    assert(ui_notice_state_remove(&s, NULL, true));
    assert(strcmp(s.active.key, "one") == 0 && s.active.remaining_ms == 1050);
    assert(!ui_notice_state_elapse(&s, 1049));
    assert(ui_notice_state_elapse(&s, 1));
    assert(ui_notice_state_remove(&s, NULL, true));
    assert(!s.has_active);
}

static void test_queued_result_and_clear(void)
{
    ui_notice_state_t s;
    ui_notice_state_init(&s);
    post(&s, UI_NOTICE_ERROR, "blocking", "Error", NULL);
    post(&s, UI_NOTICE_PROGRESS, "export", "Exporting", NULL);
    post(&s, UI_NOTICE_SUCCESS, "export", "Exported", "USB drive");
    assert(s.queued_count == 1 && s.queued[0].kind == UI_NOTICE_SUCCESS);
    assert(!ui_notice_state_remove(&s, "export", false));
    assert(s.queued_count == 0 && strcmp(s.active.key, "blocking") == 0);
    ui_notice_state_remove(&s, NULL, false);
    post(&s, UI_NOTICE_PROGRESS, "cancel", "Working", NULL);
    ui_notice_state_remove(&s, "cancel", true);
    assert(!post(&s, UI_NOTICE_PROGRESS, "cancel", "Working", NULL));
    ui_notice_state_remove(&s, "cancel", false);
    assert(post(&s, UI_NOTICE_PROGRESS, "cancel", "Working again", NULL));
}

static void test_bounded_queue_and_input_ownership(void)
{
    ui_notice_state_t s;
    char key[80], title[400], detail[400];
    unsigned i;
    ui_notice_state_init(&s);
    post(&s, UI_NOTICE_ERROR, "active", "Critical", NULL);
    for (i = 0; i < UI_NOTICE_QUEUE_CAPACITY + 5; ++i) {
        snprintf(key, sizeof(key), "q%u", i);
        assert(post(&s, UI_NOTICE_INFO, key, "Notice", NULL));
    }
    assert(s.queued_count == UI_NOTICE_QUEUE_CAPACITY);
    assert(strcmp(s.queued[0].key, "q5") == 0);
    for (i = 0; i < UI_NOTICE_QUEUE_CAPACITY; ++i) {
        snprintf(key, sizeof(key), "e%u", i);
        assert(post(&s, UI_NOTICE_ERROR, key, "Failure", NULL));
    }
    assert(!post(&s, UI_NOTICE_INFO, "low", "Low priority", NULL));
    memset(key, 'K', sizeof(key)); key[sizeof(key)-1] = '\0';
    assert(!post(&s, UI_NOTICE_ERROR, key, "No key truncation", NULL));
    ui_notice_state_init(&s);
    strcpy(key, "stack"); strcpy(title, "Title"); strcpy(detail, "Detail");
    assert(post(&s, UI_NOTICE_INFO, key, title, detail));
    memset(key, 0, sizeof(key)); memset(title, 0, sizeof(title)); memset(detail, 0, sizeof(detail));
    assert(strcmp(s.active.key, "stack") == 0 && strcmp(s.active.title, "Title") == 0 &&
           strcmp(s.active.detail, "Detail") == 0);
    assert(!post(&s, UI_NOTICE_INFO, "invalid", "", NULL));
    memset(title, 'a', sizeof(title));
    title[318] = (char)0xE4; title[319] = (char)0xB8; title[320] = (char)0xAD;
    title[sizeof(title)-1] = '\0';
    assert(post(&s, UI_NOTICE_INFO, "stack", title, NULL));
    assert(strlen(s.active.title) == 318);
}

int main(void)
{
    const uint32_t expected[]={1800,3750,3250,0,2250};
    for(unsigned i=0;i<5;++i){
        ui_notice_state_t s;ui_notice_state_init(&s);
        assert(post(&s,(ui_notice_kind_t)i,"duration","Duration",NULL));
        assert(s.active.remaining_ms==expected[i]);
    }
    test_task_result_and_dismissal();
    test_priority_resume_and_coalesce();
    test_queued_result_and_clear();
    test_bounded_queue_and_input_ownership();
    puts("ui_notice_state: PASS (tasks, dismissal, priority, queue, lifetime, ownership, UTF-8)");
    return 0;
}
