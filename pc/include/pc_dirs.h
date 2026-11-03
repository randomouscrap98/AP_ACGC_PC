#ifndef PC_DIRS_H
#define PC_DIRS_H

#include <stdlib.h>
#include "pc_types.h"

#define PC_PATHSIZE   1024

#ifdef __cplusplus
extern "C" {
#endif

// Set root based on archipelago data
void pc_card_dir_set_root_ap(const char * seed, int team, int player);
// From now on the save root is always dir (pc_card_dir_set_root_ap ignores the seed); render test
void pc_card_dir_force_root(const char * dir);
void pc_card_dir_out(s32 chan, char * out, size_t size);
void pc_card_file_out(s32 chan, char * out, size_t size, const char * fname);
// A file in the save folder itself (next to card_a/)
void pc_save_root_file_out(char * out, size_t size, const char * fname);
void pc_card_dir_create(void);
void pc_mkdir_p(const char * path);

// Save backups kept next to a file: <file>.bak1 (newest) .. .bak<PC_SAVE_MAX_BACKUPS>
#define PC_SAVE_MAX_BACKUPS 3
// .bak<max> deleted, the others move up one, file -> .bak1 (if it exists)
void pc_save_rotate_backups(const char* path);
// Puts finished temp files in place: rotates the backups of path (and side_path), then renames tmp -> path
// (and side_tmp -> side_path), so <path>.bakN and <side_path>.bakN always belong together. side_path NULL =
// one file. If the main rename fails, both are restored from .bak1 and the temps removed: returns 0. If only
// the side rename fails, side_path gets its .bak1 back (one save older) and it still returns 1.
int pc_save_commit(const char* tmp, const char* path, const char* side_tmp, const char* side_path);

int pc_card_filename_safe(const char* name);
void pc_path_sanitize(char* path);

#ifdef __cplusplus
}
#endif

#endif
