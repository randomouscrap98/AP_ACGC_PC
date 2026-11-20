#ifndef _STDLIB_H_
#define _STDLIB_H_

#ifdef TARGET_PC
#include_next <stdlib.h>
#else

#include <stddef.h>
#include <wchar.h>

#define RAND_MAX 32767

#ifdef AP_GC_UNIT
// AP units on GC: the DOL's malloc (libc64 malloc_arena) and strtol
void* malloc(size_t size);
void free(void* ptr);
long strtol(const char *str, char **end, int base);
#endif
void srand(unsigned int seed);
int rand(void);
void abort(void);
void exit(int status);
size_t wcstombs(char *dest, const wchar_t *src, size_t max);

#ifdef __MWERKS__
#define abs(x) __abs(x)
#else
static inline int abs(int x)
{
    int mask = x >> 31;
    return (x + mask) ^ mask;
}
#endif

#endif /* TARGET_PC */
#endif
