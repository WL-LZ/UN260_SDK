#!/usr/bin/env python3
"""Host test of the actual scratch pool/budget and snapshot_render transaction.

Allocator faults and CPU pressure are deterministic; no target build or hardware
is required. Pixel checks exercise the CPU-to-DMA commit boundary, not LVGL's
software rasterizer or the board DMA cache-coherency implementation.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def without_includes(source):
    return "\n".join(line for line in source.splitlines()
                     if not line.startswith("#include"))


def function(source, name):
    signature = re.search(r"^(?:static )?[\w *]+\b" + re.escape(name)
                          + r"\([^;]*?\)\s*\{", source, re.M)
    assert signature, name
    start = signature.start()
    opening = source.index("{", start)
    depth = 0
    tokens = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*|[{}]', re.S)
    for token in tokens.finditer(source, opening):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if depth == 0:
                return source[start:token.end()]
    raise AssertionError("Unterminated function: " + name)


memory_h = (ROOT / "aic_ui/image_memory.h").read_text(encoding="utf-8")
memory_c = (ROOT / "aic_ui/image_memory.c").read_text(encoding="utf-8")
scratch_h = (ROOT / "aic_ui/render_scratch.h").read_text(encoding="utf-8")
scratch_c = (ROOT / "aic_ui/render_scratch.c").read_text(encoding="utf-8")
snapshot_c = (ROOT / "un260/lv_components/lv_dma_snapshot_cache.c").read_text(encoding="utf-8")
snapshot_render = function(snapshot_c, "snapshot_render")
assert "render_scratch_acquire" in snapshot_render
assert "lv_mem_alloc" not in snapshot_render
assert "lv_mem_free" not in snapshot_render
assert "failures_before == image_recovery_failure_serial()" in snapshot_render

allocator = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {void *pointer;size_t bytes;} allocation_t;
static allocation_t allocations[16];
static unsigned malloc_calls,free_calls,fail_malloc_call;
static uint32_t allocated_bytes,allocated_peak;
static void *test_malloc(size_t bytes) {
    malloc_calls++;
    if(fail_malloc_call==malloc_calls) return NULL;
    void *pointer=malloc(bytes);assert(pointer);
    for(unsigned n=0;n<16;n++) if(!allocations[n].pointer) {
        allocations[n]=(allocation_t){pointer,bytes};
        allocated_bytes+=(uint32_t)bytes;
        if(allocated_bytes>allocated_peak) allocated_peak=allocated_bytes;
        return pointer;
    }
    assert(!"test allocation registry exhausted");return NULL;
}
static void test_free(void *pointer) {
    assert(pointer);
    for(unsigned n=0;n<16;n++) if(allocations[n].pointer==pointer) {
        allocated_bytes-=(uint32_t)allocations[n].bytes;
        allocations[n]=(allocation_t){0};free_calls++;free(pointer);return;
    }
    assert(!"double free or foreign pointer");
}
'''

snapshot_stubs = r'''
#define LV_IMG_CF_TRUE_COLOR_ALPHA 1
#define LV_RES_OK 0
#define CACHE_CLEAN 1
typedef struct {unsigned w,h;} lv_obj_t;
typedef struct {struct {uint16_t w,h;} header;} lv_img_dsc_t;
struct mpp_frame {struct {struct {unsigned width,height;} size;uint32_t stride[1];int fd[1];} buf;};
typedef struct {struct mpp_frame frame;} lv_dma_snapshot_t;
static struct {uint32_t errors,capture_count,capture_max_us;uint64_t capture_total_us;} g_dma_snapshot_stats;
static uint8_t dma_pixels_fixture[256];
static bool fail_map,fail_render,fail_offscreen,fail_image,fail_geometry,fail_clean;
static unsigned maps,unmaps,cleans,draws,cache_drops;
static uint32_t clock_ms,failure_serial;
static bool perf_profile_is_enabled(void) {return false;}
static void uart_debug_printf(const char *fmt,...) {(void)fmt;}
static uint32_t lv_tick_get(void) {return clock_ms;}
static uint64_t app_clock_monotonic_us(void) {return (uint64_t)clock_ms*1000U;}
static uint32_t app_clock_elapsed_us32(uint64_t from,uint64_t to) {return (uint32_t)(to-from);}
static uint32_t image_recovery_failure_serial(void) {return failure_serial;}
static uint32_t lv_snapshot_buf_size_needed(lv_obj_t *o,int format) {
    assert(format==LV_IMG_CF_TRUE_COLOR_ALPHA);return o->w*o->h*4U;
}
static uint8_t *dmabuf_mmap(int fd,int bytes) {
    assert(fd==9 && bytes==192);maps++;return fail_map?NULL:dma_pixels_fixture;
}
static void dmabuf_munmap(uint8_t *pixels,int bytes) {
    assert(pixels==dma_pixels_fixture && bytes==192);unmaps++;
}
static int dmabuf_sync(int fd,int mode) {assert(fd==9 && mode==CACHE_CLEAN);cleans++;return fail_clean?-1:0;}
static void lv_ge2d_offscreen_capture_begin(void) {}
static bool lv_ge2d_offscreen_capture_end(void) {return !fail_offscreen;}
static int lv_snapshot_take_to_buf(lv_obj_t *o,int format,lv_img_dsc_t *image,
                                   uint8_t *pixels,uint32_t bytes) {
    assert(format==LV_IMG_CF_TRUE_COLOR_ALPHA && bytes==o->w*o->h*4U);
    assert(pixels!=dma_pixels_fixture); /* transaction cannot target live DMA */
    for(unsigned n=0;n<192;n++) assert(dma_pixels_fixture[n]==0x11);
    draws++;memset(pixels,0x66,bytes);clock_ms+=10;
    image->header.w=o->w+(fail_geometry?1U:0U);image->header.h=o->h;
    if(fail_image) failure_serial++;
    return fail_render?1:LV_RES_OK;
}
static void lv_ge2d_scaled_cache_drop_source(struct mpp_frame *frame) {assert(frame);cache_drops++;}
'''

tests = r'''
#define MIB (1024U*1024U)
static render_scratch_stats_t stats(void) {
    render_scratch_stats_t s;render_scratch_take_stats(&s);return s;
}
static void fixture(void) {
    render_scratch_trim();
    assert(!g_owner && !g_temporary_bytes && !g_total_bytes);
    assert(used[IMAGE_MEM_CPU]==0 && !allocated_bytes);
    (void)stats();malloc_calls=free_calls=fail_malloc_call=allocated_peak=0;
}
static void budget_ok(void) {
    assert(used[IMAGE_MEM_CPU]<=4U*MIB);
    assert(allocated_bytes==g_total_bytes);
}
static void snapshot_fixture(void) {
    fail_map=fail_render=fail_offscreen=fail_image=fail_geometry=fail_clean=false;
    maps=unmaps=cleans=draws=cache_drops=0;clock_ms+=100;
    memset(dma_pixels_fixture,0x11,sizeof(dma_pixels_fixture));
}
static void assert_dma_unchanged(void) {
    for(unsigned n=0;n<192;n++) assert(dma_pixels_fixture[n]==0x11);
    assert(!cleans && !cache_drops && !g_owner);
}
int main(void) {
    render_scratch_lease_t a={0},b={0},c={0},d={0},e={0};
    fixture();assert(render_scratch_acquire(&a,1000,0));
    assert(a.capacity==4096 && a.pooled && used[IMAGE_MEM_CPU]==4096);
    memset(a.data,0x52,1000);void *original=a.data;
    assert(!render_scratch_acquire(&a,2000,1) && a.data==original);
    render_scratch_release(&a,10);assert(!a.data && !free_calls);
    assert(render_scratch_acquire(&a,500,20) && a.data==original && malloc_calls==1);
    assert(((uint8_t *)a.data)[499]==0x52);render_scratch_release(&a,30);
    render_scratch_release(&a,40);budget_ok();
    render_scratch_stats_t s=stats();assert(s.hits==1 && s.misses==1 && s.failures==1);
    assert(s.retained_bytes==4096 && s.total_bytes==4096 && s.in_use_bytes==0);
    s=stats();assert(!s.hits && !s.misses && s.peak_bytes==4096);
    render_scratch_take_stats(NULL);

    /* Growth malloc failure retains the idle pool; budget is fully rolled back. */
    fixture();assert(render_scratch_acquire(&a,MIB,0));original=a.data;
    render_scratch_release(&a,0);fail_malloc_call=malloc_calls+1;
    assert(!render_scratch_acquire(&b,2U*MIB,1));
    assert(!b.data && used[IMAGE_MEM_CPU]==MIB && allocated_bytes==MIB);
    assert(render_scratch_acquire(&a,MIB,2) && a.data==original);
    render_scratch_release(&a,3);fail_malloc_call=0;
    assert(render_scratch_acquire(&a,2U*MIB,4));
    assert(a.capacity==RENDER_SCRATCH_RETAIN_LIMIT && allocated_peak<=4U*MIB);
    render_scratch_release(&a,5);budget_ok();

    /* Nested borrows are distinct, never overwrite/reallocate a live pool. */
    fixture();assert(render_scratch_acquire(&a,2U*MIB,0));
    memset(a.data,0xAB,a.capacity);
    assert(render_scratch_acquire(&b,2U*MIB,1) && !b.pooled && b.data!=a.data);
    memset(b.data,0xCD,b.capacity);assert(used[IMAGE_MEM_CPU]==4U*MIB);
    assert(!render_scratch_acquire(&c,1,2));budget_ok();
    render_scratch_trim();render_scratch_poll(10000);
    assert(((uint8_t *)a.data)[2U*MIB-1]==0xAB && ((uint8_t *)b.data)[2U*MIB-1]==0xCD);
    assert(!free_calls);render_scratch_release(&b,3);assert(used[IMAGE_MEM_CPU]==2U*MIB);
    render_scratch_release(&a,4);assert(used[IMAGE_MEM_CPU]==0);

    fixture();assert(render_scratch_acquire(&a,MIB,0));
    assert(render_scratch_acquire(&b,MIB,1));assert(render_scratch_acquire(&c,MIB,2));
    assert(render_scratch_acquire(&d,MIB,3));assert(!render_scratch_acquire(&e,1,4));
    render_scratch_release(&c,5);render_scratch_release(&d,6);
    render_scratch_release(&b,7);render_scratch_release(&a,8);budget_ok();

    /* Releasing outer before inner is legal; all overlapping loans remain
     * temporary until the last active borrower returns. */
    fixture();assert(render_scratch_acquire(&a,MIB,0));original=a.data;
    assert(render_scratch_acquire(&b,MIB,1));void *inner=b.data;
    render_scratch_release(&a,2);assert(render_scratch_acquire(&c,MIB,3));
    assert(!c.pooled && c.data!=original && c.data!=inner);render_scratch_trim();
    render_scratch_release(&c,4);assert(used[IMAGE_MEM_CPU]==MIB && b.data==inner);
    render_scratch_release(&b,5);assert(!used[IMAGE_MEM_CPU]);

    fixture();assert(render_scratch_acquire(&a,3U*MIB,0) && !a.pooled);
    assert(render_scratch_acquire(&b,MIB,1) && !b.pooled && b.data!=a.data);
    assert(!g_owner && !g_retained);render_scratch_trim();
    assert(used[IMAGE_MEM_CPU]==4U*MIB);render_scratch_release(&b,2);
    render_scratch_release(&a,3);assert(!used[IMAGE_MEM_CPU]);

    fixture();assert(render_scratch_acquire(&a,MIB,0));
    fail_malloc_call=malloc_calls+1;assert(!render_scratch_acquire(&b,MIB,1));
    assert(used[IMAGE_MEM_CPU]==MIB && g_owner==&a);
    render_scratch_lease_t copied=a;render_scratch_release(&copied,2);
    assert(g_owner==&a && a.data==copied.data);render_scratch_release(&a,3);

    /* Idle expiry uses release time and remains correct through uint32 wrap. */
    fixture();assert(render_scratch_acquire(&a,1000,100));
    render_scratch_poll(100000);assert(g_owner==&a && !free_calls);
    render_scratch_release(&a,100000);
    render_scratch_poll(104999);assert(used[IMAGE_MEM_CPU]==4096);
    render_scratch_poll(105000);assert(!used[IMAGE_MEM_CPU]);
    assert(render_scratch_acquire(&a,1000,UINT32_MAX-2500U));
    render_scratch_release(&a,UINT32_MAX-2500U);
    render_scratch_poll((uint32_t)(UINT32_MAX-2500U+4999U));assert(used[IMAGE_MEM_CPU]==4096);
    render_scratch_poll((uint32_t)(UINT32_MAX-2500U+5000U));assert(!used[IMAGE_MEM_CPU]);

    /* CPU pressure may evict only idle cached data, including growth retry. */
    fixture();assert(render_scratch_acquire(&a,MIB,0));render_scratch_release(&a,0);
    assert(image_mem_acquire(IMAGE_MEM_CPU,2U*MIB));
    assert(render_scratch_acquire(&a,2U*MIB,1));budget_ok();
    assert(used[IMAGE_MEM_CPU]==4U*MIB);render_scratch_release(&a,2);
    image_mem_release(IMAGE_MEM_CPU,2U*MIB);
    assert(reclaim[IMAGE_MEM_CPU]);reclaim[IMAGE_MEM_CPU](1);assert(!used[IMAGE_MEM_CPU]);

    fixture();assert(render_scratch_acquire(&a,MIB,0));render_scratch_release(&a,0);
    assert(image_mem_acquire(IMAGE_MEM_CPU,3U*MIB));
    assert(!render_scratch_acquire(&a,2U*MIB,1));
    assert(!a.data && used[IMAGE_MEM_CPU]==3U*MIB && !allocated_bytes);
    image_mem_release(IMAGE_MEM_CPU,3U*MIB);
    assert(render_scratch_acquire(&a,2U*MIB+1U,2) && !a.pooled);
    assert(a.capacity==2U*MIB+4096U);render_scratch_release(&a,3);assert(!used[IMAGE_MEM_CPU]);
    assert(!render_scratch_acquire(&a,4U*MIB+1U,4));
    assert(!render_scratch_acquire(&a,UINT32_MAX,4));
    assert(!render_scratch_acquire(&a,0,4));assert(!render_scratch_acquire(NULL,1,4));
    render_scratch_release(NULL,4);assert(!used[IMAGE_MEM_CPU]);

    fixture();fail_malloc_call=1;assert(!render_scratch_acquire(&a,4096,0));
    assert(!a.data && !used[IMAGE_MEM_CPU] && !allocated_bytes);
    fail_malloc_call=0;assert(render_scratch_acquire(&a,4096,1));
    reclaim[IMAGE_MEM_CPU](1);assert(g_owner==&a && allocated_bytes==4096);
    render_scratch_release(&a,2);assert(!used[IMAGE_MEM_CPU]);

    /* Exercise production snapshot_render against the real pool: capture and
     * image-recovery checks must succeed before any old DMA pixel is changed. */
    lv_obj_t obj={8,4};lv_dma_snapshot_t snap={.frame.buf={.size={8,4},.stride={48},.fd={9}}};
    fixture();snapshot_fixture();assert(snapshot_render(&snap,&obj,"TEST"));
    assert(maps==1 && unmaps==1 && cleans==1 && draws==1 && cache_drops==1 && !g_owner);
    for(unsigned y=0;y<4;y++) {
        for(unsigned x=0;x<32;x++) assert(dma_pixels_fixture[y*48+x]==0x66);
        for(unsigned x=32;x<48;x++) assert(dma_pixels_fixture[y*48+x]==0);
    }
    assert(used[IMAGE_MEM_CPU]==4096 && malloc_calls==1);
    snapshot_fixture();assert(snapshot_render(&snap,&obj,"TEST"));
    assert(malloc_calls==1);s=stats();assert(s.hits==1 && s.misses==1);

    for(unsigned failure=0;failure<4;failure++) {
        snapshot_fixture();
        if(failure==0) fail_render=true;
        if(failure==1) fail_offscreen=true;
        if(failure==2) fail_image=true;
        if(failure==3) fail_geometry=true;
        assert(!snapshot_render(&snap,&obj,"TEST"));assert_dma_unchanged();
        assert(maps==1 && unmaps==1 && draws==1 && used[IMAGE_MEM_CPU]==4096);
    }
    snapshot_fixture();fail_map=true;assert(!snapshot_render(&snap,&obj,"TEST"));
    assert_dma_unchanged();assert(maps==1 && !unmaps && !draws);
    fixture();snapshot_fixture();fail_malloc_call=1;
    assert(!snapshot_render(&snap,&obj,"TEST"));assert_dma_unchanged();
    assert(maps==1 && unmaps==1 && !draws && !used[IMAGE_MEM_CPU]);
    fixture();snapshot_fixture();assert(image_mem_acquire(IMAGE_MEM_CPU,4U*MIB));
    assert(!snapshot_render(&snap,&obj,"TEST"));assert_dma_unchanged();
    assert(maps==1 && unmaps==1 && !draws);image_mem_release(IMAGE_MEM_CPU,4U*MIB);
    fixture();snapshot_fixture();fail_clean=true;
    assert(!snapshot_render(&snap,&obj,"TEST"));
    assert(draws==1 && cleans==1 && unmaps==1 && !cache_drops && !g_owner);
    fixture();budget_ok();
    puts("render scratch: PASS reuse, bounded/nested allocations, malloc/growth failures, pressure/idle/wrap, transactional snapshot capture");
    return 0;
}
'''

combined = (allocator + without_includes(memory_h) + "\n" + without_includes(scratch_h)
            + "\n" + without_includes(memory_c)
            + "\n#define malloc test_malloc\n#define free test_free\n"
            + without_includes(scratch_c) + "\n#undef malloc\n#undef free\n"
            + snapshot_stubs + function(snapshot_c, "snapshot_create_error")
            + "\n" + snapshot_render + "\n" + tests)

with tempfile.TemporaryDirectory(prefix="un260-render-scratch-") as directory:
    work = Path(directory)
    source = work / "test.c"
    binary = work / ("test.exe" if os.name == "nt" else "test")
    source.write_text(combined, encoding="utf-8")
    flags = ["-std=c11", "-O2", "-Wall", "-Wextra", "-Werror"]
    if os.environ.get("UN260_TEST_SANITIZE", "0" if os.name == "nt" else "1") == "1":
        flags.append("-fsanitize=address,undefined")
    subprocess.run([os.environ.get("CC", "cc"), *flags, str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
