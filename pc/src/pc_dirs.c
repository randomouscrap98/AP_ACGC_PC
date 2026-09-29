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
static char g_pc_savepath_temp[PC_PATHSIZE];

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

// old get_card_dir but with global storage, value only valid until next call
// of get_card_dir!! be careful!!
const char * pc_card_dir(s32 chan) {
  pc_card_dir_out(chan, g_pc_savepath_temp, sizeof(g_pc_savepath_temp));
  return g_pc_savepath_temp;
}

const char * pc_card_file(s32 chan, const char * fname) {
  pc_card_file_out(chan, g_pc_savepath_temp, sizeof(g_pc_savepath_temp), fname);
  return g_pc_savepath_temp;
}

void pc_card_dir_create(void) {
#ifdef _WIN32
    _mkdir(g_pc_saveroot);
    _mkdir(pc_card_dir(0));
    _mkdir(pc_card_dir(1));
#else
    mkdir(g_pc_saveroot, 0755);
    mkdir(pc_card_dir(0), 0755);
    mkdir(pc_card_dir(1), 0755);
#endif
}

