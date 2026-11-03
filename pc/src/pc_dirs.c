#include "pc_dirs.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <dirent.h>
#endif

static char g_pc_saveroot[PC_PATHSIZE] = "saves";
static int g_pc_saveroot_forced; // pc_card_dir_force_root: set_root_ap keeps g_pc_saveroot


void pc_card_dir_force_root(const char * dir) {
  snprintf(g_pc_saveroot, sizeof(g_pc_saveroot), "%s", dir);
  g_pc_saveroot_forced = 1;
}

void pc_card_dir_set_root_ap(const char * seed, int team, int player) {
  if (g_pc_saveroot_forced) {
    pc_card_dir_create();
    return;
  }
  snprintf(g_pc_saveroot, sizeof(g_pc_saveroot), "saves/ap_%s_%d_%d",
      seed, team, player);
  pc_path_sanitize(g_pc_saveroot);
  pc_card_dir_create();
}


// A file in the save folder itself (next to card_a/)
void pc_save_root_file_out(char * out, size_t size, const char * fname) {
  snprintf(out, size, "%s/%s", g_pc_saveroot, fname);
}

/* Per-channel directory: chan 0 = card_a, chan 1 = card_b */
void pc_card_dir_out(s32 chan, char * out, size_t size) {
  switch (chan) {
    case 1:
      snprintf(out, size, "%s/card_b", g_pc_saveroot);
      break;
    default:
      snprintf(out, size, "%s/card_a", g_pc_saveroot);
      break;
  }
}

void pc_card_file_out(s32 chan, char * out, size_t size, const char * fname) {
  switch (chan) {
    case 1:
      snprintf(out, size, "%s/card_b/%s", g_pc_saveroot, fname);
      break;
    default:
      snprintf(out, size, "%s/card_a/%s", g_pc_saveroot, fname);
      break;
  }
}

// mkdir every component of path; existing dirs are fine
void pc_mkdir_p(const char * path) {
  char buf[PC_PATHSIZE];
  snprintf(buf, sizeof(buf), "%s", path);
  if (!buf[0]) return;
  for (char * p = buf + 1; *p; p++) {
    if (*p == '/' || *p == '\\') {
      char c = *p;
      *p = '\0';
#ifdef _WIN32
      _mkdir(buf);
#else
      mkdir(buf, 0755);
#endif
      *p = c;
    }
  }
#ifdef _WIN32
  _mkdir(buf);
#else
  mkdir(buf, 0755);
#endif
}

void pc_card_dir_create(void) {
  char dir[PC_PATHSIZE];
  pc_card_dir_out(0, dir, sizeof(dir));
  pc_mkdir_p(dir);
  pc_card_dir_out(1, dir, sizeof(dir));
  pc_mkdir_p(dir);
}

int pc_card_filename_safe(const char* name) {
  static const char* reserved[] = { "CON", "PRN", "AUX", "NUL", NULL };
  size_t len, base;
  int i;

  if (!name || !name[0]) return 0;
  len = strlen(name);
  if (len > 32) return 0;
  if (strstr(name, "..")) return 0;
  for (i = 0; name[i]; i++) {
    unsigned char c = (unsigned char)name[i];
    if (c < 32 || strchr("/\\<>:\"|?*", c)) return 0;
  }
  if (name[len - 1] == '.' || name[len - 1] == ' ') return 0;

  // device names, with or without extension
  base = strcspn(name, ".");
  for (i = 0; reserved[i]; i++) {
    if (base == 3 && strncasecmp(name, reserved[i], 3) == 0) return 0;
  }
  if (base == 4 && (strncasecmp(name, "COM", 3) == 0 || strncasecmp(name, "LPT", 3) == 0) &&
      name[3] >= '1' && name[3] <= '9') return 0;
  return 1;
}

// very very basic sanitization, over-zealous
void pc_path_sanitize(char* path) {
  for (; *path; path++) {
    char c = *path;
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
          c == '_' || c == '-' || c == '/')) {
      *path = '_';
    }
  }
}

void pc_save_rotate_backups(const char* path) {
  char older[PC_PATHSIZE + 16], newer[PC_PATHSIZE + 16];
  struct stat st;
  int i;

  snprintf(older, sizeof(older), "%s.bak%d", path, PC_SAVE_MAX_BACKUPS);
  remove(older);
  // Windows rename() fails if dest exists, so remove first
  for (i = PC_SAVE_MAX_BACKUPS; i > 1; i--) {
    snprintf(older, sizeof(older), "%s.bak%d", path, i - 1);
    snprintf(newer, sizeof(newer), "%s.bak%d", path, i);
    remove(newer);
    rename(older, newer);
  }
  if (stat(path, &st) == 0) {
    snprintf(newer, sizeof(newer), "%s.bak1", path);
    remove(newer);
    rename(path, newer);
  }
}

// .bak1 back in place of path (undoes the last step of a rotation)
static void pc_save_restore_bak1(const char* path) {
  char bak1[PC_PATHSIZE + 16];
  snprintf(bak1, sizeof(bak1), "%s.bak1", path);
  rename(bak1, path);
}

int pc_save_commit(const char* tmp, const char* path, const char* side_tmp, const char* side_path) {
  pc_save_rotate_backups(path);
  if (side_path != NULL) {
    pc_save_rotate_backups(side_path);
  }
  if (rename(tmp, path) != 0) {
    pc_save_restore_bak1(path);
    remove(tmp);
    if (side_path != NULL) {
      pc_save_restore_bak1(side_path);
      remove(side_tmp);
    }
    return 0;
  }
  if (side_path != NULL && rename(side_tmp, side_path) != 0) {
    pc_save_restore_bak1(side_path);
    remove(side_tmp);
  }
  return 1;
}
