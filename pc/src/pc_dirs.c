#include "pc_dirs.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <dirent.h>
#endif

static char g_pc_saveroot[PC_PATHSIZE] = "saves";

int pc_card_dir_set_root(const char * dir) {
  if(strlen(dir) >= PC_PATHSIZE) {
    return 1;
  }
  strcpy(g_pc_saveroot, dir);
  pc_card_dir_create();
  return 0;
}


void pc_card_dir_set_root_ap(const char * seed, int team, int player) {
  snprintf(g_pc_saveroot, sizeof(g_pc_saveroot), "saves/ap_%s_%d_%d",
      seed, team, player);
  pc_path_sanitize(g_pc_saveroot);
  pc_card_dir_create();
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
