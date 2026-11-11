#ifndef SE_EXEC_ARENA_H
#define SE_EXEC_ARENA_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define SE_EXEC_ALIGN 64

static const char *se_exec_why = NULL;

static void se_exec_fail(const char *why) { se_exec_why = why; }

const char *se_sljit_exec_failure(void) { return se_exec_why; }

typedef struct {
    size_t offset;
    size_t size;
    int used;
} se_exec_range;

typedef struct {
    uint8_t *base;
    size_t size;
    se_exec_range *ranges;
    size_t count;
    size_t capacity;
} se_exec_arena;

static int se_exec_arena_reserve(se_exec_arena *arena, size_t wanted) {
    if (wanted <= arena->capacity) return 1;
    size_t capacity = arena->capacity != 0 ? arena->capacity * 2 : 16;
    while (capacity < wanted)
        capacity *= 2;
    se_exec_range *ranges = (se_exec_range *)realloc(arena->ranges, capacity * sizeof(se_exec_range));
    if (ranges == NULL) return 0;
    arena->ranges = ranges;
    arena->capacity = capacity;
    return 1;
}

static int se_exec_arena_init(se_exec_arena *arena, void *base, size_t size) {
    arena->base = (uint8_t *)base;
    arena->size = size;
    arena->ranges = NULL;
    arena->count = 0;
    arena->capacity = 0;
    if (!se_exec_arena_reserve(arena, 16)) return 0;
    arena->ranges[0].offset = 0;
    arena->ranges[0].size = size;
    arena->ranges[0].used = 0;
    arena->count = 1;
    return 1;
}

static void *se_exec_arena_alloc(se_exec_arena *arena, size_t size) {
    const size_t needed = (size + SE_EXEC_ALIGN - 1) & ~(size_t)(SE_EXEC_ALIGN - 1);
    if (needed == 0) return NULL;
    for (size_t i = 0; i < arena->count; i++) {
        se_exec_range *range = &arena->ranges[i];
        if (range->used || range->size < needed) continue;
        if (range->size > needed) {
            if (!se_exec_arena_reserve(arena, arena->count + 1)) return NULL;
            range = &arena->ranges[i];
            for (size_t j = arena->count; j > i + 1; j--)
                arena->ranges[j] = arena->ranges[j - 1];
            arena->ranges[i + 1].offset = range->offset + needed;
            arena->ranges[i + 1].size = range->size - needed;
            arena->ranges[i + 1].used = 0;
            arena->count++;
            range->size = needed;
        }
        range->used = 1;
        return arena->base + range->offset;
    }
    return NULL;
}

/* returns 1 if the offset was an allocation of this arena */
static int se_exec_arena_free(se_exec_arena *arena, size_t offset) {
    for (size_t i = 0; i < arena->count; i++) {
        if (arena->ranges[i].offset != offset) continue;
        if (!arena->ranges[i].used) return 0;
        arena->ranges[i].used = 0;
        if (i + 1 < arena->count && !arena->ranges[i + 1].used) {
            arena->ranges[i].size += arena->ranges[i + 1].size;
            for (size_t j = i + 1; j + 1 < arena->count; j++)
                arena->ranges[j] = arena->ranges[j + 1];
            arena->count--;
        }
        if (i > 0 && !arena->ranges[i - 1].used) {
            arena->ranges[i - 1].size += arena->ranges[i].size;
            for (size_t j = i; j + 1 < arena->count; j++)
                arena->ranges[j] = arena->ranges[j + 1];
            arena->count--;
        }
        return 1;
    }
    return 0;
}

static void se_exec_arena_destroy(se_exec_arena *arena) {
    free(arena->ranges);
    arena->ranges = NULL;
    arena->count = 0;
    arena->capacity = 0;
}

static int se_exec_arena_contains(const se_exec_arena *arena, const void *pointer) {
    const uint8_t *p = (const uint8_t *)pointer;
    return p >= arena->base && p < arena->base + arena->size;
}

#endif
