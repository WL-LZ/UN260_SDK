#include "image_memory.h"
#include <stdio.h>
#include <limits.h>
/* 16 MiB MPP pool: at most 12 MiB managed persistent+temporary DMA claims.
 * The remaining 4 MiB is headroom, NOT a claim that all SDK allocations are
 * measured here. Reserved-heap fallbacks conservatively consume this budget too.
 * CPU capture scratch has a separate cap and never counts as DMA capacity. */
#define DMA_LIMIT (12U * 1024U * 1024U)
#define CPU_LIMIT (4U * 1024U * 1024U)
static uint32_t used[IMAGE_MEM_COUNT], peak, denied;
static image_mem_reclaim_fn reclaim[IMAGE_MEM_COUNT];
static bool reclaiming;
static uint32_t aligned(uint32_t n) { return n > UINT32_MAX-4095U ? UINT32_MAX : (n+4095U)&~4095U; }
uint32_t image_mem_used(void)
{
    uint64_t sum=0;
    for(unsigned i=0;i<IMAGE_MEM_CPU;i++) sum+=used[i];
    return sum>UINT32_MAX ? UINT32_MAX : (uint32_t)sum;
}
void image_mem_register(image_mem_kind_t kind, image_mem_reclaim_fn fn)
{ if(kind<IMAGE_MEM_COUNT) reclaim[kind]=fn; }
bool image_mem_acquire(image_mem_kind_t kind, uint32_t bytes)
{
    if(kind>=IMAGE_MEM_COUNT || !bytes) return false;
    bytes=aligned(bytes);
    uint32_t limit=kind==IMAGE_MEM_CPU ? CPU_LIMIT : DMA_LIMIT;
    uint32_t total=kind==IMAGE_MEM_CPU ? used[kind] : image_mem_used();
    if(bytes>limit) { denied++; return false; }
    if(total>limit-bytes && !reclaiming && kind!=IMAGE_MEM_CPU) {
        reclaiming=true;
        /* Cheap derivatives first, then unreferenced snapshots, finally decoded
         * images. Each owner protects live users and updates claims on release. */
        const unsigned order[]={IMAGE_MEM_SCALE,IMAGE_MEM_SNAPSHOT,IMAGE_MEM_IMAGE};
        for(unsigned i=0;i<3 && image_mem_used()>limit-bytes;i++)
            if(reclaim[order[i]]) reclaim[order[i]](image_mem_used()-(limit-bytes));
        reclaiming=false;
        total=image_mem_used();
    }
    if(total>limit-bytes) { denied++; return false; }
    used[kind]+=bytes;
    if(image_mem_used()>peak) peak=image_mem_used();
    return true;
}
void image_mem_release(image_mem_kind_t kind, uint32_t bytes)
{
    if(kind>=IMAGE_MEM_COUNT || !bytes) return;
    bytes=aligned(bytes);
    if(bytes>used[kind]) { fprintf(stderr,"IMG_MEM unbalanced kind=%u bytes=%u used=%u\n",kind,bytes,used[kind]); return; }
    used[kind]-=bytes;
}
bool image_mem_prewarm_allowed(void) { return image_mem_used() <= DMA_LIMIT-2U*1024U*1024U; }
void image_mem_report(void)
{
    fprintf(stderr,"IMG_MEM image=%u snapshot=%u scale=%u decode=%u cpu=%u total=%u peak=%u limit=%u denied=%u\n",
        used[0],used[1],used[2],used[3],used[4],image_mem_used(),peak,DMA_LIMIT,denied);
}
