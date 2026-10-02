#include "pc_ap_state.h"
#include "pc_dirs.h"

#include <ini.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PCAP_MAXFILE 8192
#define PCKEY_BELLSAPPLIED "bells_applied"
#define PCKEY_FAVORSDONE "favors_done"
#define PCKEY_LOANLETTERPENDING "loan_letter_pending"


static pc_ap_state g_pc_ap_state = {0};


// Get integer in global space
static int ini_get_int(ini_t * ini, const char * key, int def) {
  int p = ini_find_property(ini, INI_GLOBAL_SECTION, key, 0);
  if (p != INI_NOT_FOUND) {
    return atoi(ini_property_value(ini, INI_GLOBAL_SECTION, p));
  }
  return def;
}

// Set integer in global space
static void ini_set_int(ini_t * ini, const char * key, int value) {
  char num[64]; // Absurdly large
  snprintf(num, sizeof(num), "%d", value);
  ini_property_add(ini, INI_GLOBAL_SECTION, key, 0, num, 0);
}


static void pc_ap_state_init(pc_ap_state * s) {
  // IDK, for now... yeah
  memset(s, 0, sizeof(pc_ap_state));
}

// Save global ap state into given file
int pc_ap_state_save(const char * filename) {
  char buf[PCAP_MAXFILE];

  ini_t* ini = ini_create(NULL);
  ini_set_int(ini, PCKEY_BELLSAPPLIED, g_pc_ap_state.bells_applied);
  ini_set_int(ini, PCKEY_FAVORSDONE, g_pc_ap_state.favors_done);
  ini_set_int(ini, PCKEY_LOANLETTERPENDING, g_pc_ap_state.loan_letter_pending);
  int size = ini_save(ini, buf, sizeof(buf));
  ini_destroy(ini); // is this necessary? probably...

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

// Load global ap state from given file
int pc_ap_state_load(const char * filename) {
  // Zero out the old struct? since we're reading in values one at a time
  // and you may get a partial read anyway?
  pc_ap_state_init(&g_pc_ap_state);
  FILE * f = fopen(filename, "rb");
  if(!f) { return 0; }
  char buf[PCAP_MAXFILE];

  size_t bytes_read = fread(buf, 1, sizeof(buf) - 1, f);
  buf[bytes_read] = '\0';

  ini_t* ini = ini_load(buf, NULL);
  g_pc_ap_state.bells_applied = ini_get_int(ini, PCKEY_BELLSAPPLIED, 0);
  g_pc_ap_state.favors_done = ini_get_int(ini, PCKEY_FAVORSDONE, 0);
  g_pc_ap_state.loan_letter_pending = ini_get_int(ini, PCKEY_LOANLETTERPENDING, 0);

  ini_destroy(ini);
  fclose(f);
  return 1;
}

// Get a pointer to the ap state (through which you can mutate values)
pc_ap_state * pc_ap_state_get(void) {
  return &g_pc_ap_state;
}
