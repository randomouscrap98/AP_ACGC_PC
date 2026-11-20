// GC side of pc_ap_state.h: the sidecar texts (ap_state.ini and ap_settings.ini, the same ini text as on PC) live
// in one RAM image of a memory card file, GC_AP_CARD_FILE, next to the town save:
// - pc_ap_state_read / _write only touch the RAM image (the file name picks state or settings).
// - gc_ap_card_save writes the image to the card right after each successful town save (m_card.c, next to the
//   NES save, which does the same); gc_ap_card_load reads it right after the town loads (end of mCD_LoadLand).
// The write goes through a backup name like the NES save, so a failed write leaves the old file.
// The game's own town file is never touched.
#include "gc_ap_card.h"
#include "pc_ap_state.h"
#include "pc_ap_settings.h"
#include "pc_ap_logic.h"
#include "pc_ap_platform.h"
#include "ap_archipelago.h"
#include "m_malloc.h"
#include "dolphin/card.h"
#include "dolphin/os.h"

#include <ini.h>
#include <string.h>
#include <stdio.h>

#define GC_AP_CARD_FILE    "DobutsunomoriP_AP"
#define GC_AP_CARD_TEMP    "DobutsunomoriP_AP~"
#define GC_AP_CARD_TOWN    "DobutsunomoriP_MURA" // our file is only written next to a town save
#define GC_AP_CARD_SIZE    0x2000                 // one 8 KB block
#define GC_AP_CARD_MAGIC   0x41434150             // "ACAP"
#define GC_AP_CARD_VERSION 1                      // layout of gc_ap_card_file (not the ini keys: PC_AP_STATE_VERSION)
#define GC_AP_SETTINGS_MAX 1024                   // ap_settings.ini text (about 300 bytes now)
#define GC_AP_HEADER_SIZE  (CARD_COMMENT_SIZE + 4 * 4 + 64 + 2 * 4)
#define GC_AP_STATE_MAX    (GC_AP_CARD_SIZE - GC_AP_HEADER_SIZE - GC_AP_SETTINGS_MAX)

typedef struct {
  char comment[CARD_COMMENT_SIZE]; // what the IPL's card screen shows: game name (32), file description (32)
  u32 magic;
  u32 version;
  u32 state_len; // bytes of text in state, without the zero
  u32 settings_len;
  char seed[64]; // slot this town belongs to (written on save; compared on load)
  s32 team;
  s32 slot;
  char settings[GC_AP_SETTINGS_MAX]; // zero-terminated ini text
  char state[GC_AP_STATE_MAX];       // zero-terminated ini text
} gc_ap_card_file;

// The image: what the next town save writes, and what pc_ap_state_read hands out
static gc_ap_card_file s_card ATTRIBUTE_ALIGN(32);
// A newer client wrote the file: never write over it (pc_ap_plat_newer_save)
static int s_card_locked;

// The text slot a sidecar file name stands for
static char* gc_ap_card_text(const char* filename, int* max) {
  if(filename != NULL && strcmp(filename, PC_AP_SETTINGS_FILENAME) == 0) {
    *max = GC_AP_SETTINGS_MAX;
    return s_card.settings;
  }
  *max = GC_AP_STATE_MAX;
  return s_card.state;
}

ini_t* pc_ap_state_read(const char* filename) {
  int max;
  char* text;
  if(filename == NULL) {
    return NULL;
  }
  text = gc_ap_card_text(filename, &max);
  if(text[0] == '\0') {
    return NULL; // missing: the defaults
  }
  return ini_load(text, NULL);
}

int pc_ap_state_write(const char* filename, ini_t* ini) {
  int max;
  char* text = gc_ap_card_text(filename, &max);
  int size;
  pc_ap_ini_set_int(ini, NULL, PC_AP_STATE_KEY_VERSION, PC_AP_STATE_VERSION);
  size = ini_save(ini, NULL, 0); // with the zero
  if(size > max) {
    OSReport("[AP] %s: %d bytes, room for %d\n", filename, size, max);
    return 0; // the old text stays
  }
  ini_save(ini, text, size);
  return 1;
}

void gc_ap_card_newer_save(void) {
  s_card_locked = 1;
}

int gc_ap_card_newer(void) {
  return s_card_locked;
}

// CARDProbeEx + CARDMount + CARDCheck, like famicom.cpp's memcard_mount
static s32 gc_ap_card_mount(s32 chan, void* work) {
  s32 sector_size;
  s32 result;
  do {
    result = CARDProbeEx(chan, NULL, &sector_size);
  } while(result == CARD_RESULT_BUSY);
  if(result != CARD_RESULT_READY) {
    return result;
  }
  if(sector_size != GC_AP_CARD_SIZE) {
    return CARD_RESULT_WRONGDEVICE;
  }
  result = CARDMount(chan, work, NULL);
  if(result == CARD_RESULT_READY || result == CARD_RESULT_BROKEN) {
    result = CARDCheck(chan);
    if(result != CARD_RESULT_READY) {
      CARDUnmount(chan);
    }
  } else if(result == CARD_RESULT_ENCODING) {
    CARDUnmount(chan);
  }
  return result;
}

// Reads the AP file of the town on chan into the image. 1 = a usable file
static int gc_ap_card_read(s32 chan) {
  void* work;
  CARDFileInfo info;
  s32 result;
  work = zelda_malloc_align(CARD_WORKAREA_SIZE, 32);
  if(work == NULL) {
    OSReport("[AP] card load: no memory\n");
    return 0;
  }
  result = gc_ap_card_mount(chan, work);
  if(result == CARD_RESULT_READY) {
    result = CARDOpen(chan, GC_AP_CARD_FILE, &info);
    if(result == CARD_RESULT_READY) {
      result = CARDRead(&info, &s_card, sizeof(s_card), 0);
      CARDClose(&info);
    }
    CARDUnmount(chan);
  }
  zelda_free(work);
  if(result != CARD_RESULT_READY || s_card.magic != GC_AP_CARD_MAGIC) {
    // Missing (AP state zeroed: only errs toward giving Bells again, as on PC) or unreadable
    OSReport("[AP] card load: no AP file (%d)\n", result);
    memset(&s_card, 0, sizeof(s_card));
    return 0;
  }
  if(s_card.version > GC_AP_CARD_VERSION) {
    pc_ap_plat_newer_save(GC_AP_CARD_FILE);
    return 0;
  }
  // Text always ends inside its slot, whatever the card held
  s_card.state[GC_AP_STATE_MAX - 1] = '\0';
  s_card.settings[GC_AP_SETTINGS_MAX - 1] = '\0';
  s_card.seed[sizeof(s_card.seed) - 1] = '\0';
  return 1;
}

void gc_ap_card_load(s32 chan) {
  int found;
  memset(&s_card, 0, sizeof(s_card));
  s_card_locked = 0;
  found = chan >= 0 && gc_ap_card_read(chan); // chan < 0: new town, no AP state yet
  // As PC's mCD_LoadLand: this save's AP settings, then its sidecar (missing = the defaults)
  if(pc_ap_start_allowed()) {
    pc_ap_settings_open();
  }
  pc_ap_load(found && !s_card_locked ? PC_AP_STATE_FILENAME : NULL);
}

void gc_ap_card_slot(char* seed, int seed_size, int* team, int* slot) {
  snprintf(seed, seed_size, "%s", s_card.seed);
  *team = s_card.team;
  *slot = s_card.slot;
}

// The image with this save's header and texts, ready to write
static void gc_ap_card_fill(void) {
  const ap_connectstate* cs = ap_getconnectstate();
  memset(s_card.comment, 0, sizeof(s_card.comment));
  strcpy(s_card.comment, "Animal Crossing AP");
  strcpy(s_card.comment + 32, "Archipelago state");
  s_card.magic = GC_AP_CARD_MAGIC;
  s_card.version = GC_AP_CARD_VERSION;
  s_card.state_len = strlen(s_card.state);
  s_card.settings_len = strlen(s_card.settings);
  snprintf(s_card.seed, sizeof(s_card.seed), "%s", cs->roomplayer.seed);
  s_card.team = cs->roomplayer.team;
  s_card.slot = cs->roomplayer.player;
}

// Writes the image as GC_AP_CARD_FILE: old file renamed to the temp name, new one created and written,
// then the temp one deleted (famicom.cpp's memcard_data_save, minus the icon)
static s32 gc_ap_card_write(s32 chan) {
  void* work = zelda_malloc_align(CARD_WORKAREA_SIZE, 32);
  CARDFileInfo info;
  CARDStat stat;
  s32 result;
  s32 free_bytes, free_files;
  if(work == NULL) {
    return CARD_RESULT_FATAL_ERROR;
  }
  result = gc_ap_card_mount(chan, work);
  if(result != CARD_RESULT_READY) {
    zelda_free(work);
    return result;
  }
  // Only next to a town save
  result = CARDOpen(chan, GC_AP_CARD_TOWN, &info);
  if(result != CARD_RESULT_READY) {
    goto unmount;
  }
  CARDClose(&info);
  // Old file -> temp name (a leftover temp from an earlier failure goes first)
  CARDDelete(chan, GC_AP_CARD_TEMP);
  result = CARDRename(chan, GC_AP_CARD_FILE, GC_AP_CARD_TEMP);
  if(result == CARD_RESULT_NOFILE) {
    // First save: room for one more block and file?
    if(CARDFreeBlocks(chan, &free_bytes, &free_files) != CARD_RESULT_READY || free_bytes < GC_AP_CARD_SIZE ||
       free_files < 1) {
      result = CARD_RESULT_INSSPACE;
      goto unmount;
    }
  } else if(result != CARD_RESULT_READY) {
    goto unmount;
  }
  result = CARDCreate(chan, GC_AP_CARD_FILE, GC_AP_CARD_SIZE, &info);
  if(result != CARD_RESULT_READY) {
    CARDRename(chan, GC_AP_CARD_TEMP, GC_AP_CARD_FILE); // put the old one back
    goto unmount;
  }
  result = CARDWrite(&info, &s_card, sizeof(s_card), 0);
  CARDClose(&info);
  if(result != CARD_RESULT_READY) {
    CARDDelete(chan, GC_AP_CARD_FILE);
    CARDRename(chan, GC_AP_CARD_TEMP, GC_AP_CARD_FILE);
    goto unmount;
  }
  // Comment at the start of the file, no icon or banner
  result = CARDGetStatus(chan, info.fileNo, &stat);
  if(result == CARD_RESULT_READY) {
    stat.commentAddr = 0;
    stat.iconAddr = 0xFFFFFFFF;
    stat.bannerFormat = 0;
    stat.iconFormat = 0;
    stat.iconSpeed = 0;
    result = CARDSetStatus(chan, info.fileNo, &stat);
  }
  CARDDelete(chan, GC_AP_CARD_TEMP);
unmount:
  CARDUnmount(chan);
  zelda_free(work);
  return result;
}

void gc_ap_card_save(s32 chan) {
  s32 result;
  if(s_card_locked || !pc_ap_start_allowed()) {
    return; // newer file, or no slot (nothing AP to save)
  }
  if(!pc_ap_save(PC_AP_STATE_FILENAME)) {
    OSReport("[AP] card save: state didn't fit, the last one stays\n");
  }
  gc_ap_card_fill();
  result = gc_ap_card_write(chan);
  if(result != CARD_RESULT_READY) {
    OSReport("[AP] card save failed (%d)\n", result);
  }
}
