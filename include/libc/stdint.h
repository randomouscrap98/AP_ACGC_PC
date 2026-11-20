#ifndef _STDINT_H_
#define _STDINT_H_

#ifdef TARGET_PC
#include_next <stdint.h>
#else

typedef unsigned int uintptr_t;
// Fixed-width types for shared AP headers (apclient/include) on GC
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed long int32_t;
typedef unsigned long uint32_t;
typedef signed long long int64_t;
typedef unsigned long long uint64_t;

#endif /* TARGET_PC */
#endif
