#include "pc_dirs.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <dirent.h>
#endif

static char g_pc_saveroot[PC_PATHSIZE] = "saves_pre_ap";

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

void pc_card_dir_create(void) {
  char dir_a[PC_PATHSIZE];
  char dir_b[PC_PATHSIZE];
  pc_card_dir_out(0, dir_a, sizeof(dir_a));
  pc_card_dir_out(1, dir_b, sizeof(dir_b));
#ifdef _WIN32
    _mkdir(g_pc_saveroot);
    _mkdir(dir_a);
    _mkdir(dir_b);
#else
    mkdir(g_pc_saveroot, 0755);
    mkdir(dir_a, 0755);
    mkdir(dir_b, 0755);
#endif
}

