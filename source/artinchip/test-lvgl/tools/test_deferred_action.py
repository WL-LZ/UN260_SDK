#!/usr/bin/env python3
"""Exercise the real LVGL 8.3.2 callback lifetime and production deferred owner."""
from pathlib import Path
import os
import re
import subprocess
import tempfile
from lvgl_timer_fixture import code as lvgl_timer_code

ROOT = Path(__file__).resolve().parents[1]
helper = '\n'.join((ROOT / ('un260/lv_core/ui_deferred_action.' + suffix)).read_text(encoding='utf-8')
                   for suffix in ('h', 'c'))
helper = re.sub(r'^#include[^\n]*', '', helper, flags=re.M)
test = r'''
static unsigned calls;
static ui_deferred_action_t slot;
static lv_timer_t *dispatched_timer;
static void assert_detached(void) {
    assert(!slot.timer && !slot.callback && !slot.user_data);
    for(lv_timer_t *timer=lv_timer_get_next(NULL);timer;timer=lv_timer_get_next(timer))
        assert(timer!=dispatched_timer);
}
static void legacy_self_cancel(void *data) {
    assert(data==&calls);calls++;
    assert(lv_async_call_cancel(legacy_self_cancel,data)==LV_RES_OK);
}
static void count(void *data) {assert(data==&calls);calls++;}
static void cancel_from_callback(void *data) {
    assert(data==&calls);assert_detached();calls++;
    ui_deferred_action_cancel(&slot);ui_deferred_action_cancel(&slot);
}
static void requeue_cancel(void *data) {
    assert_detached();calls++;
    assert(ui_deferred_action_schedule(&slot,count,data));
    ui_deferred_action_cancel(&slot);
}
static void requeue_run(void *data) {
    assert_detached();calls++;
    assert(ui_deferred_action_schedule(&slot,count,data));
}
static void destroy_owner(void *data) {
    ui_deferred_action_t *owner=data;
    assert(!owner->timer && !owner->callback && !owner->user_data);
    ui_deferred_action_cancel(owner);
    lv_mem_free(owner);calls++;
}
static void schedule(lv_async_cb_t callback) {
    assert(ui_deferred_action_schedule(&slot,callback,&calls));
    dispatched_timer=slot.timer;
}
int main(void) {
    test_lvgl_reset();tick=100;
    /* Baseline proof: actual board LVGL retains the running async timer in
     * its list and frees info again after a self-cancelling callback returns. */
    expect_double_free=true;
    assert(lv_async_call(legacy_self_cancel,&calls)==LV_RES_OK);
    lv_timer_handler();
    assert(calls==1 && double_frees==1 && !lv_timer_get_next(NULL));
    puts("LVGL 8.3.2 baseline: reproduced running-async double free");
    test_lvgl_reset();calls=0;

    schedule(cancel_from_callback);lv_timer_handler();assert(calls==1);
    schedule(count);ui_deferred_action_cancel(&slot);ui_deferred_action_cancel(&slot);
    lv_timer_handler();assert(calls==1 && !slot.timer);
    schedule(count);schedule(cancel_from_callback);lv_timer_handler();assert(calls==2);
    schedule(requeue_cancel);lv_timer_handler();assert(calls==3 && !slot.timer);
    schedule(requeue_run);lv_timer_handler();assert(calls==5 && !slot.timer);

    schedule(count);fail_timer=true;
    assert(!ui_deferred_action_schedule(&slot,count,&calls));
    assert(!slot.timer && !slot.callback && !slot.user_data);
    fail_timer=false;lv_timer_handler();assert(calls==5);
    assert(!ui_deferred_action_schedule(NULL,count,&calls));
    assert(!ui_deferred_action_schedule(&slot,NULL,&calls));
    ui_deferred_action_cancel(NULL);

    ui_deferred_action_t *dynamic=lv_mem_alloc(sizeof(*dynamic));
    *dynamic=(ui_deferred_action_t){0};
    assert(ui_deferred_action_schedule(dynamic,destroy_owner,dynamic));
    lv_timer_handler();assert(calls==6);
    for(unsigned n=0;n<1000;n++) {
        schedule(count);schedule(requeue_cancel);lv_timer_handler();
        assert(!slot.timer && !double_frees);
    }
    assert(calls==1006 && !lv_timer_get_next(NULL));
    test_lvgl_reset();
    puts("Deferred action: PASS detach before dispatch, replacement, repeated cancel, OOM, requeue and owner destruction");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-deferred-') as directory:
    work = Path(directory)
    source = work / 'test.c'
    binary = work / ('test.exe' if os.name == 'nt' else 'test')
    source.write_text(lvgl_timer_code() + helper + test, encoding='utf-8')
    flags = ['-std=c11', '-O2', '-Wall', '-Wextra', '-Werror']
    if os.environ.get('UN260_TEST_SANITIZE', '0' if os.name == 'nt' else '1') == '1':
        flags.append('-fsanitize=address,undefined')
    subprocess.run([os.environ.get('CC', 'cc'), *flags, str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
