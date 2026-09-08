#ifndef UN260_IMAGE_MEMORY_H
#define UN260_IMAGE_MEMORY_H
#include <stdint.h>
#include <stdbool.h>
/* UI-thread only. Claims include in-flight allocations, not just cache entries. */
typedef enum { IMAGE_MEM_IMAGE, IMAGE_MEM_SNAPSHOT, IMAGE_MEM_SCALE,
               IMAGE_MEM_DECODE, IMAGE_MEM_CPU, IMAGE_MEM_COUNT } image_mem_kind_t;
typedef void (*image_mem_reclaim_fn)(uint32_t bytes);
void image_mem_register(image_mem_kind_t kind, image_mem_reclaim_fn fn);
bool image_mem_acquire(image_mem_kind_t kind, uint32_t bytes);
void image_mem_release(image_mem_kind_t kind, uint32_t bytes);
uint32_t image_mem_used(void);
bool image_mem_prewarm_allowed(void);
void image_mem_report(void);
#endif
