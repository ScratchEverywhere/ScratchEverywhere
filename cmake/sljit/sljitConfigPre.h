#ifndef SE_SLJITCONFIGPRE_H
#define SE_SLJITCONFIGPRE_H

#if defined(__arm__) && defined(__ARM_PCS) && !defined(__ARM_PCS_VFP) && !defined(__SOFTFP__)
#define __SOFTFP__ 1
#endif

#ifdef SE_SLJIT_CUSTOM_EXEC

#ifdef __cplusplus
extern "C" {
#endif

void *se_sljit_malloc_exec(unsigned long size);
void se_sljit_free_exec(void *ptr);
long se_sljit_exec_offset(void *ptr);
void se_sljit_update_wx(void *from, void *to, int executable);
void se_sljit_cache_flush(void *from, void *to);
const char *se_sljit_exec_failure(void);

#ifdef __cplusplus
}
#endif

#define SLJIT_EXECUTABLE_ALLOCATOR 0
#define SLJIT_UTIL_STACK 0
#define SLJIT_SINGLE_THREADED 1

#define SLJIT_MALLOC_EXEC(size, exec_allocator_data) se_sljit_malloc_exec(size)
#define SLJIT_FREE_EXEC(ptr, exec_allocator_data) se_sljit_free_exec(ptr)

#ifdef SE_SLJIT_DUAL_MAPPING
#define SLJIT_EXEC_OFFSET(ptr) se_sljit_exec_offset(ptr)
#endif

#ifdef SE_SLJIT_WX
#define SLJIT_UPDATE_WX_FLAGS(from, to, executable) se_sljit_update_wx((void *)(from), (void *)(to), executable)
#endif

#ifdef SE_SLJIT_FLUSH
#define SLJIT_CACHE_FLUSH(from, to) se_sljit_cache_flush((void *)(from), (void *)(to))
#endif

#endif /* SE_SLJIT_CUSTOM_EXEC */

#endif
