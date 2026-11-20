// Sidecar ini helpers and versions, shared by PC and GC. Reading and writing the file: pc_ap_state_file.c (PC),
// gc/gc_ap_state_card.c (GC: the memory card file).
#include "pc_ap_state.h"

#include <ini.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


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
  return (int)strtol(ini_property_value(ini, s, p), NULL, 10); // not atoi: GC's MSL has none
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
  // v3 -> v4: [favors] done -> banked + sent needs the server's checked list, so
  // pc_ap_favorsanity.c does it (old_done) once connected
  return 1;
}
