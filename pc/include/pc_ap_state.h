// Preserve ap state alongside the save file (so you never touch
// the original save file, but we can add ap values). Only the file and
// ini plumbing: each pc_ap_* module saves/loads its own section.
#ifndef PC_AP_STATE_H
#define PC_AP_STATE_H

// Sidecar file name, stored next to the home town GCI
#define PC_AP_STATE_FILENAME "ap_state.ini"
// Format of ap_state.ini and ap_settings.ini (one number for both), in their global section.
// Bump rules: ignore/docs/reference.md "Versions"
#define PC_AP_STATE_VERSION 2
#define PC_AP_STATE_KEY_VERSION "version"

#ifdef __cplusplus
extern "C" {
#endif

struct ini_t;

// Read the sidecar into a new ini (ini_destroy it). NULL if missing or unreadable
struct ini_t* pc_ap_state_read(const char * filename);
// Old sidecar -> the current keys, right after reading. 0 = written by a newer client: don't load or
// overwrite it. NULL (no file) is fine
int pc_ap_state_upgrade(struct ini_t* ini);
// Write ini to the sidecar (through a temp file), with the current version. Returns 0 on failure
int pc_ap_state_write(const char * filename, struct ini_t* ini);

// Integer property in a section (NULL = the global section, for g_ap's own fields);
// def if ini is NULL or the section or key is missing
int pc_ap_ini_get_int(struct ini_t* ini, const char * section, const char * key, int def);
// Add an integer property to a section (made if missing; NULL = the global section)
void pc_ap_ini_set_int(struct ini_t* ini, const char * section, const char * key, int value);

#ifdef __cplusplus
}
#endif

#endif
