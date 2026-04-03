#pragma once
// PSP type compatibility layer — replaces Windows-specific types used in the original 4J source

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

// Windows integer types → stdint equivalents
typedef uint8_t   BYTE;
typedef uint16_t  WORD;
typedef uint32_t  DWORD;
typedef int64_t   __int64;
typedef uint64_t  __uint64;
typedef int64_t   LONGLONG;
typedef uint64_t  ULONGLONG;

// Windows string types (we avoid wstring on PSP — use narrow strings)
// wstring → std::string in adapted code
#include <string>
typedef std::string  psp_string;

// NULL / boolean compat
typedef uint8_t       byte;
typedef unsigned int  UINT;

// PSP-specific helpers
#define PI 3.14159265358979323846f

// Replacement for QueryPerformanceCounter — use PSP tick counter
#include <pspkernel.h>
#include <psptypes.h>

static inline int64_t psp_get_ticks()
{
    return (int64_t)sceKernelGetSystemTimeWide();
}
