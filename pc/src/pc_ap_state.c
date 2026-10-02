#include "pc_ap_state.h"

#include <ini.h>
#include <stdio.h>

pc_ap_state g_pc_ap_state = {0};

// Save global ap state into given file
int pc_ap_state_save(const char * filename) {
  FILE * f = fopen(filename, "wb");
  if(!f) { return 0; }

  fclose(f);
  return 1;
}

// Load global ap state from given file
int pc_ap_state_load(const char * filename) {
  FILE * f = fopen(filename, "rb");
  if(!f) { return 0; }

  fclose(f);
  return 1;
}

// Get a pointer to the ap state (through which you can mutate values)
pc_ap_state * pc_ap_state_get() {
  return &g_pc_ap_state;
}
