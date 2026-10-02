// Preserve ap state alongside the save file (so you never touch
// the original save file, but we can add ap values)
#ifndef PC_AP_STATE_H
#define PC_AP_STATE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int bells_applied;
  int favors_done;
} pc_ap_state;

// Save global ap state into given file
int pc_ap_state_save(const char * filename);
// Load global ap state from given file
int pc_ap_state_load(const char * filename);

// Get a pointer to the ap state (through which you can mutate values)
pc_ap_state * pc_ap_state_get();

#ifdef __cplusplus
}
#endif

#endif
