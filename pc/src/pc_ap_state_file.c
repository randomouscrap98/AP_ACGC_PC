// PC side of pc_ap_state.h: the sidecar is a file next to the save
#include "pc_ap_state.h"
#include "pc_dirs.h"

#include <ini.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PCAP_MAXFILE 16384 // Global Storage: up to ~4 KB (4 players x 200 items)

int pc_ap_state_write(const char * filename, ini_t * ini) {
  char buf[PCAP_MAXFILE];
  pc_ap_ini_set_int(ini, NULL, PC_AP_STATE_KEY_VERSION, PC_AP_STATE_VERSION);
  int size = ini_save(ini, buf, sizeof(buf));

  // Oops, too big!
  if(size > sizeof(buf)) { return 0; }

  // Write a temp file first so a crash mid-write can't leave a truncated file
  char tmp[PC_PATHSIZE + 8];
  snprintf(tmp, sizeof(tmp), "%s.tmp", filename);

  FILE * f = fopen(tmp, "wb");
  if(!f) { return 0; }
  size_t written = fwrite(buf, 1, size - 1, f);
  if(fclose(f) != 0 || written != (size_t)(size - 1)) {
    remove(tmp);
    return 0;
  }

  // Windows rename() fails if dest exists, so remove first
  remove(filename);
  if(rename(tmp, filename) != 0) { return 0; }

  return 1;
}

ini_t * pc_ap_state_read(const char * filename) {
  if(!filename) { return NULL; }
  FILE * f = fopen(filename, "rb");
  if(!f) { return NULL; }
  char buf[PCAP_MAXFILE];

  size_t bytes_read = fread(buf, 1, sizeof(buf) - 1, f);
  buf[bytes_read] = '\0';
  fclose(f);

  return ini_load(buf, NULL);
}
