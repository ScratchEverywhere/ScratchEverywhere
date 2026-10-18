/*
    nxdk -> lwmem memory management overrides.

    nxdk's libc (pdclib) ships a dlmalloc based allocator. When SE_USE_LWMEM is
    enabled, we define malloc/calloc/realloc/free/aligned_alloc ourselves and
    forward them to lwmem. Since this object is linked before libpdclib.lib,
    the linker resolves every libc, libc++ (operator new/delete go through
    malloc), SDL and third party allocation to the definitions below and never
    pulls the dlmalloc object out of the archive.

    lwmem manages one fixed region that is reserved lazily on the first
    allocation. Its size is controlled by SE_LWMEM_HEAP_SIZE_MB (CMake cache
    variable, see cmake/platforms/xbox.cmake).
*/
#if defined(__XBOX__) && defined(SE_USE_LWMEM)

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>

#include "lwmem/lwmem.h"
#include "system/lwmem_sys.h"

#ifndef SE_LWMEM_HEAP_SIZE
#define SE_LWMEM_HEAP_SIZE (32u * 1024u * 1024u)
#endif

/* Minimum memory reserved for the NV2A GPU buffers (framebuffer/depth/pushbuffer)
   and kernel/drivers when expanded RAM is detected. */
#ifndef SE_LWMEM_GPU_RESERVE_BYTES
#define SE_LWMEM_GPU_RESERVE_BYTES (24u * 1024u * 1024u)
#endif

/* Never go below this when falling back to a smaller region */
#define SE_LWMEM_MIN_HEAP_SIZE (4u * 1024u * 1024u)

/* ------------------------------------------------------------------------ */
/* lwmem OS port: a static critical section so the port never allocates.    */
/* ------------------------------------------------------------------------ */

static CRITICAL_SECTION se_lwmem_cs;

uint8_t lwmem_sys_mutex_create(LWMEM_CFG_OS_MUTEX_HANDLE *m) {
    InitializeCriticalSection(&se_lwmem_cs);
    *m = &se_lwmem_cs;
    return 1;
}

uint8_t lwmem_sys_mutex_isvalid(LWMEM_CFG_OS_MUTEX_HANDLE *m) {
    return *m != NULL;
}

uint8_t lwmem_sys_mutex_wait(LWMEM_CFG_OS_MUTEX_HANDLE *m) {
    EnterCriticalSection((CRITICAL_SECTION *)*m);
    return 1;
}

uint8_t lwmem_sys_mutex_release(LWMEM_CFG_OS_MUTEX_HANDLE *m) {
    LeaveCriticalSection((CRITICAL_SECTION *)*m);
    return 1;
}

/* ------------------------------------------------------------------------ */
/* Region setup                                                             */
/* ------------------------------------------------------------------------ */

static volatile int se_lwmem_ready = 0;
static size_t se_lwmem_allocated_size = 0;
static size_t se_lwmem_total_ram = 0;
static int se_lwmem_is_expanded = 0;

static size_t se_get_target_heap_size(void) {
    MM_STATISTICS stats;
    memset(&stats, 0, sizeof(stats));
    stats.Length = sizeof(stats);

    if (NT_SUCCESS(MmQueryStatistics(&stats))) {
        se_lwmem_total_ram = (size_t)stats.TotalPhysicalPages * 4096u;

        /* If expanded RAM is detected (>64MB, typically 128MB on modded consoles/devkits) */
        if (se_lwmem_total_ram > (64u * 1024u * 1024u)) {
            se_lwmem_is_expanded = 1;
            /* Give runtime priority over GPU: reserve what the GPU and drivers
               need for display/audio buffers, and allocate the rest to lwmem. */
            if (se_lwmem_total_ram > SE_LWMEM_GPU_RESERVE_BYTES) {
                return se_lwmem_total_ram - SE_LWMEM_GPU_RESERVE_BYTES;
            }
        }
    } else {
        se_lwmem_total_ram = 64u * 1024u * 1024u;
    }

    return SE_LWMEM_HEAP_SIZE;
}

static int se_lwmem_init(void) {
    /* The first allocation happens during CRT start-up, long before any
       thread is spawned, so there is no need for a lock here. */
    if (se_lwmem_ready) return 1;

    size_t size = se_get_target_heap_size();
    void *base = NULL;
    while (size >= SE_LWMEM_MIN_HEAP_SIZE) {
        base = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (base) break;
        if (size > (4u * 1024u * 1024u)) {
            size -= (4u * 1024u * 1024u);
        } else {
            break;
        }
    }
    if (!base) return 0;

    lwmem_region_t regions[] = {
        {base, size},
        {NULL, 0},
    };
    if (!lwmem_assignmem(regions)) return 0;

    se_lwmem_allocated_size = size;
    se_lwmem_ready = 1;
    return 1;
}

void se_lwmem_get_info(size_t *heap_size, size_t *total_ram, int *is_expanded) {
    if (heap_size) *heap_size = se_lwmem_allocated_size;
    if (total_ram) *total_ram = se_lwmem_total_ram;
    if (is_expanded) *is_expanded = se_lwmem_is_expanded;
}

/* ------------------------------------------------------------------------ */
/* libc overrides                                                           */
/* ------------------------------------------------------------------------ */

void *malloc(size_t size) {
    if (!se_lwmem_init()) return NULL;
    return lwmem_malloc(size);
}

void *calloc(size_t nmemb, size_t size) {
    if (!se_lwmem_init()) return NULL;
    return lwmem_calloc(nmemb, size);
}

void *realloc(void *ptr, size_t size) {
    if (!se_lwmem_init()) return NULL;
    return lwmem_realloc(ptr, size);
}

void free(void *ptr) {
    if (ptr == NULL || !se_lwmem_ready) return;
    lwmem_free(ptr);
}

/* lwmem only guarantees LWMEM_CFG_ALIGN_NUM alignment (16, see lwmem_opts.h) */
void *aligned_alloc(size_t alignment, size_t size) {
    if (alignment > LWMEM_CFG_ALIGN_NUM) {
        errno = EINVAL;
        return NULL;
    }
    return malloc(size);
}

#endif /* defined(__XBOX__) && defined(SE_USE_LWMEM) */
