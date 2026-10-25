#include "pc_ap_state.h"
#include "pc_dirs.h"

#include <ini.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PCAP_MAXFILE 16384 // Global Storage: up to ~4 KB (4 players x 200 items)


int pc_ap_ini_get_int(ini_t * ini, const char * section, const char * key, int def) {
  if (ini == NULL) {
    return def;
  }
  int s = section == NULL ? INI_GLOBAL_SECTION : ini_find_section(ini, section, 0);
  if (s == INI_NOT_FOUND) {
    return def;
  }
  int p = ini_find_property(ini, s, key, 0);
  if (p == INI_NOT_FOUND) {
    return def;
  }
  return atoi(ini_property_value(ini, s, p));
}

void pc_ap_ini_set_int(ini_t * ini, const char * section, const char * key, int value) {
  char num[64]; // Absurdly large
  snprintf(num, sizeof(num), "%d", value);
  pc_ap_ini_set_str(ini, section, key, num);
}

const char * pc_ap_ini_get_str(ini_t * ini, const char * section, const char * key) {
  if (ini == NULL) {
    return NULL;
  }
  int s = section == NULL ? INI_GLOBAL_SECTION : ini_find_section(ini, section, 0);
  if (s == INI_NOT_FOUND) {
    return NULL;
  }
  int p = ini_find_property(ini, s, key, 0);
  if (p == INI_NOT_FOUND) {
    return NULL;
  }
  return ini_property_value(ini, s, p);
}

void pc_ap_ini_set_str(ini_t * ini, const char * section, const char * key, const char * value) {
  int s = section == NULL ? INI_GLOBAL_SECTION : ini_find_section(ini, section, 0);
  if (s == INI_NOT_FOUND) {
    s = ini_section_add(ini, section, 0);
  }
  ini_property_add(ini, s, key, 0, value, 0);
}

// The only code that knows sidecar versions. Add a step only for a version that removed a key or changed
// what one means; a new key needs none (missing key = its default = the old behaviour).
int pc_ap_state_upgrade(ini_t * ini) {
  int version = pc_ap_ini_get_int(ini, NULL, PC_AP_STATE_KEY_VERSION, 1);
  if(version > PC_AP_STATE_VERSION) { return 0; }
  // Steps go here, oldest first: if (version < 2) { ...v1 -> v2... }
  return 1;
}

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
