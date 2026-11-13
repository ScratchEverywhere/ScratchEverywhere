#include <stddef.h>
#include <stdint.h>

// Lower 10 MiB SDRAM arena (0x10200000..0x10C00000), safely above the firmware's
// low vectors/panic slot (0x10000000..0x10010000) and loader stack (~0x101D09C0),
// and below the BOOT.BIN payload region (0x10C00000..0x11000000).
#define VFLASH_ARENA_BASE 0x10200000u
#define VFLASH_ARENA_END  0x10C00000u
#define ALIGN8(x) (((x) + 7u) & ~7u)

typedef struct ArenaBlock {
    size_t size; // payload size; bit 0 = 1 if allocated, 0 if free
    struct ArenaBlock *prev_phys;
    struct ArenaBlock *next_free;
    struct ArenaBlock *prev_free;
} ArenaBlock;

static uint8_t *s_bump = (uint8_t *)VFLASH_ARENA_BASE;
static ArenaBlock *s_last_phys = NULL;
static ArenaBlock *s_free_head = NULL;
static size_t s_used = 0;
static size_t s_peak = 0;
static size_t s_alloc_count = 0;
static size_t s_oom_count = 0;

static void free_list_remove(ArenaBlock *blk) {
    if (blk->prev_free) {
        blk->prev_free->next_free = blk->next_free;
    } else {
        s_free_head = blk->next_free;
    }
    if (blk->next_free) {
        blk->next_free->prev_free = blk->prev_free;
    }
    blk->next_free = NULL;
    blk->prev_free = NULL;
}

static void free_list_insert(ArenaBlock *blk) {
    blk->next_free = s_free_head;
    blk->prev_free = NULL;
    if (s_free_head) {
        s_free_head->prev_free = blk;
    }
    s_free_head = blk;
}

static ArenaBlock *next_phys_block(ArenaBlock *blk) {
    size_t sz = blk->size & ~1u;
    uint8_t *next_addr = (uint8_t *)(blk + 1) + sz;
    if (next_addr >= s_bump) {
        return NULL;
    }
    return (ArenaBlock *)next_addr;
}

void *__wrap_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    size_t aligned = ALIGN8(size);

    // First-fit search in free list
    for (ArenaBlock *curr = s_free_head; curr != NULL; curr = curr->next_free) {
        size_t csize = curr->size & ~1u;
        if (csize >= aligned) {
            free_list_remove(curr);
            if (csize >= aligned + sizeof(ArenaBlock) + 16u) {
                size_t rem = csize - aligned - sizeof(ArenaBlock);
                ArenaBlock *split = (ArenaBlock *)((uint8_t *)(curr + 1) + aligned);
                split->size = rem;
                split->prev_phys = curr;
                split->next_free = NULL;
                split->prev_free = NULL;

                ArenaBlock *after = (ArenaBlock *)((uint8_t *)(split + 1) + rem);
                if ((uint8_t *)after < s_bump) {
                    after->prev_phys = split;
                } else {
                    s_last_phys = split;
                }

                curr->size = aligned;
                free_list_insert(split);
                csize = aligned;
            }
            curr->size = csize | 1u;
            s_used += csize;
            if (s_used > s_peak) {
                s_peak = s_used;
            }
            s_alloc_count++;
            return (void *)(curr + 1);
        }
    }

    // Bump allocate from arena
    size_t total_needed = sizeof(ArenaBlock) + aligned;
    if ((uintptr_t)s_bump + total_needed > VFLASH_ARENA_END ||
        total_needed < aligned) {
        s_oom_count++;
        return NULL;
    }

    ArenaBlock *blk = (ArenaBlock *)s_bump;
    s_bump += total_needed;
    blk->size = aligned | 1u;
    blk->prev_phys = s_last_phys;
    blk->next_free = NULL;
    blk->prev_free = NULL;
    s_last_phys = blk;

    s_used += aligned;
    if (s_used > s_peak) {
        s_peak = s_used;
    }
    s_alloc_count++;
    return (void *)(blk + 1);
}

void __wrap_free(void *ptr) {
    if (!ptr) {
        return;
    }
    ArenaBlock *blk = ((ArenaBlock *)ptr) - 1;
    if (!(blk->size & 1u)) {
        return;
    }

    size_t sz = blk->size & ~1u;
    blk->size = sz;
    if (s_used >= sz) {
        s_used -= sz;
    }
    if (s_alloc_count > 0) {
        s_alloc_count--;
    }

    // Coalesce with next physical block if free
    ArenaBlock *next = next_phys_block(blk);
    if (next && !(next->size & 1u)) {
        free_list_remove(next);
        sz += sizeof(ArenaBlock) + (next->size & ~1u);
        blk->size = sz;
        ArenaBlock *after = next_phys_block(blk);
        if (after) {
            after->prev_phys = blk;
        } else {
            s_last_phys = blk;
        }
    }

    // Coalesce with previous physical block if free
    ArenaBlock *prev = blk->prev_phys;
    if (prev && !(prev->size & 1u)) {
        free_list_remove(prev);
        size_t psz = (prev->size & ~1u) + sizeof(ArenaBlock) + sz;
        prev->size = psz;
        ArenaBlock *after = next_phys_block(prev);
        if (after) {
            after->prev_phys = prev;
        } else {
            s_last_phys = prev;
        }
        blk = prev;
    }

    free_list_insert(blk);
}

void *__wrap_calloc(size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) {
        return NULL;
    }
    if (nmemb > ((size_t)-1) / size) {
        s_oom_count++;
        return NULL;
    }
    size_t total = nmemb * size;
    void *ptr = __wrap_malloc(total);
    if (ptr) {
        uint8_t *p = (uint8_t *)ptr;
        for (size_t i = 0; i < total; ++i) {
            p[i] = 0;
        }
    }
    return ptr;
}

void *__wrap_realloc(void *ptr, size_t size) {
    if (!ptr) {
        return __wrap_malloc(size);
    }
    if (size == 0) {
        __wrap_free(ptr);
        return NULL;
    }
    ArenaBlock *blk = ((ArenaBlock *)ptr) - 1;
    size_t old_size = blk->size & ~1u;
    if (size <= old_size) {
        return ptr;
    }
    void *new_ptr = __wrap_malloc(size);
    if (!new_ptr) {
        return NULL;
    }
    uint8_t *dst = (uint8_t *)new_ptr;
    const uint8_t *src = (const uint8_t *)ptr;
    for (size_t i = 0; i < old_size; ++i) {
        dst[i] = src[i];
    }
    __wrap_free(ptr);
    return new_ptr;
}

size_t vflash_arena_used(void) {
    return s_used;
}

size_t vflash_arena_peak(void) {
    return s_peak;
}

size_t vflash_arena_total(void) {
    return (size_t)(VFLASH_ARENA_END - VFLASH_ARENA_BASE);
}

size_t vflash_arena_alloc_count(void) {
    return s_alloc_count;
}

size_t vflash_arena_oom_count(void) {
    return s_oom_count;
}

void *__wrap__malloc_r(void *reent, size_t size) {
    (void)reent;
    return __wrap_malloc(size);
}

void __wrap__free_r(void *reent, void *ptr) {
    (void)reent;
    __wrap_free(ptr);
}

void *__wrap__calloc_r(void *reent, size_t nmemb, size_t size) {
    (void)reent;
    return __wrap_calloc(nmemb, size);
}

void *__wrap__realloc_r(void *reent, void *ptr, size_t size) {
    (void)reent;
    return __wrap_realloc(ptr, size);
}
