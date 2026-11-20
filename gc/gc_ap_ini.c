// ini.h (apclient/lib/ini) for the GC build: the sidecar text (PC links it from pc_main.c)
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// GC's MSL has no strnicmp / strncasecmp
static int gc_ap_ini_strnicmp(const char* a, const char* b, size_t n) {
  size_t i;
  for(i = 0; i < n; i++) {
    int ca = (unsigned char)a[i];
    int cb = (unsigned char)b[i];
    if(ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
    if(cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
    if(ca != cb) return ca - cb;
    if(ca == 0) return 0;
  }
  return 0;
}

#define INI_STRNICMP(s1, s2, cnt) (gc_ap_ini_strnicmp(s1, s2, cnt))
#define INI_IMPLEMENTATION
#include <ini.h>
