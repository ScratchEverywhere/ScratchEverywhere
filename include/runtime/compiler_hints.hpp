#pragma once

#if defined(__GNUC__) || defined(__clang__)
#define SE_EXPECT(cond, expected) (__builtin_expect(!!(cond), (expected)))
#define SE_FORCEINLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define SE_EXPECT(cond, expected) (cond)
#define SE_FORCEINLINE __forceinline
#else
#define SE_EXPECT(cond, expected) (cond)
#define SE_FORCEINLINE inline
#endif

#define SE_LIKELY_IF(cond) if (SE_EXPECT(cond, 1))
#define SE_UNLIKELY_IF(cond) if (SE_EXPECT(cond, 0))
