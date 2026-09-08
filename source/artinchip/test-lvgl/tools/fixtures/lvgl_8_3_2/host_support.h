/* Host types/platform hooks for the unmodified board LVGL 8.3.2 timer,
 * async and linked-list sources in this directory (see LICENSE.txt).
 * Memory is quarantined until reset so a legacy double free is reported
 * deterministically even on hosts without AddressSanitizer. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LV_ARCH_64 1
#define LV_LOG_TRACE_TIMER 0
#define LV_ATTRIBUTE_TIMER_HANDLER
#define LV_NO_TIMER_READY 0xffffffffU
#define LV_LOG_WARN(...) ((void)0)
#define LV_ASSERT_MALLOC(value) ((void)(value))
#define LV_ASSERT_MEM_INTEGRITY() ((void)0)
#define LV_GC_ROOT(name) name
#define LV_RES_OK 0
#define LV_RES_INV 1
typedef int lv_res_t;
typedef void (*lv_async_cb_t)(void *);
typedef uint8_t lv_ll_node_t;
typedef struct {uint32_t n_size; lv_ll_node_t *head,*tail;} lv_ll_t;
struct _lv_timer_t;
typedef void (*lv_timer_cb_t)(struct _lv_timer_t *);
typedef struct _lv_timer_t {
    uint32_t period,last_run;
    lv_timer_cb_t timer_cb;
    void *user_data;
    int32_t repeat_count;
    uint32_t paused : 1;
} lv_timer_t;
static lv_ll_t _lv_timer_ll;
static lv_timer_t *_lv_timer_act;
static uint32_t tick;
static bool fail_timer,fail_async,expect_double_free;
static unsigned double_frees,allocation_count;
static struct {void *pointer; bool freed;} allocations[8192];
static uint32_t lv_tick_get(void) {return tick;}
static uint32_t lv_tick_elaps(uint32_t from) {return tick-from;}
static void *lv_mem_alloc(size_t size) {
    if(fail_timer || fail_async) return NULL;
    assert(allocation_count<8192);
    void *pointer=malloc(size);assert(pointer);
    allocations[allocation_count].pointer=pointer;
    allocations[allocation_count++].freed=false;
    return pointer;
}
static void lv_mem_free(void *pointer) {
    if(!pointer) return;
    for(unsigned n=0;n<allocation_count;n++) {
        if(allocations[n].pointer!=pointer) continue;
        if(allocations[n].freed) {double_frees++;assert(expect_double_free);}
        else allocations[n].freed=true;
        return;
    }
    assert(!"free of unowned LVGL allocation");
}
void *_lv_ll_get_head(const lv_ll_t *);
void *_lv_ll_get_tail(const lv_ll_t *);
void *_lv_ll_get_prev(const lv_ll_t *,const void *);
void *_lv_ll_get_next(const lv_ll_t *,const void *);
void lv_timer_enable(bool);
void lv_timer_del(lv_timer_t *);
lv_timer_t *lv_timer_create(lv_timer_cb_t,uint32_t,void *);
static void test_lvgl_reset(void);
