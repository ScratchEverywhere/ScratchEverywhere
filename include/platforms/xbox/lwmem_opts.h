/**
 * \file            lwmem_opts.h
 * \brief           lwmem configuration for the original Xbox (nxdk)
 *
 * Picked up by lwmem through the platform include directory
 * (include/platforms/xbox). See source/platforms/xbox/lwmem_override.c for
 * the region setup, mutex port and the libc overrides built on top of it.
 */
#ifndef LWMEM_OPTS_HDR_H
#define LWMEM_OPTS_HDR_H

#if defined(__XBOX__)

/* Several threads allocate concurrently; mutex port lives in lwmem_override.c */
#define LWMEM_CFG_OS 1

/* Keep 16 byte alignment so SSE loads on malloc'd memory stay safe */
#define LWMEM_CFG_ALIGN_NUM 16

/* Full feature set (realloc, calloc, free_s, ...) */
#define LWMEM_CFG_FULL 1

#define LWMEM_CFG_CLEAN_MEMORY 0
#define LWMEM_CFG_ENABLE_STATS 0

#endif /* defined(__XBOX__) */

#endif /* LWMEM_OPTS_HDR_H */
