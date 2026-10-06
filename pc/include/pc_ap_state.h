// Preserve ap state alongside the save file (so you never touch
// the original save file, but we can add ap values). Only the file and
// ini plumbing: each pc_ap_* module saves/loads its own section.
#ifndef PC_AP_STATE_H
#define PC_AP_STATE_H

// Sidecar file name, stored next to the home town GCI
#define PC_AP_STATE_FILENAME "ap_state.ini"

#ifdef __cplusplus
extern "C" {
#endif

struct ini_t;

// Read the sidecar into a new ini (ini_destroy it). NULL if missing or unreadable
struct ini_t* pc_ap_state_read(const char * filename);
// Write ini to the sidecar (through a temp file). Returns 0 on failure
int pc_ap_state_write(const char * filename, struct ini_t* ini);

// Integer property in a section; def if the section or key is missing
int pc_ap_ini_get_int(struct ini_t* ini, const char * section, const char * key, int def);
// Add an integer property to a section (made if missing)
void pc_ap_ini_set_int(struct ini_t* ini, const char * section, const char * key, int value);

#ifdef __cplusplus
}
#endif

#endif
