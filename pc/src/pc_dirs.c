#include "pc_dirs.h"

#include <stdio.h>
#include <string.h>

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

