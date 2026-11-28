// GC backend of ap_archipelago.h / ap_slotdata.h: the same functions the PC game code calls in ap_client.dll,
// answered from the RAM mailbox (gc_ap_mailbox.h) that the PC client (apworld gc_client.py) fills.
#include "gc_ap_mailbox.h"
#include "gc_ap_card.h"
#include "gc_ap_diag.h"
#include "ap_archipelago.h"
#include "ap_slotdata.h"
#include "ap_log.h"
#include "pc_ap_slotdata_parse.h"
#include "dolphin/os.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

gc_ap_mailbox gc_ap_mbox_mem; // only through gc_ap_mbox (uncached), see gc_ap_mailbox.h

// Cached copies of what game code reads often: slot_data (taken once, like the DLL takes one slot) and the
// received items (append-only; a shorter list = the client started over)
static ap_slotdata gc_ap_sd;
// The slot_data JSON text, copied out of the uncached mailbox once: parsing it straight from uncached memory would
// be slow. gc_ap_sd_json_len = the length last tried, so bad text isn't parsed again every frame (0 = not tried)
static char gc_ap_sd_json[GC_AP_SD_JSON_MAX];
static u32 gc_ap_sd_json_len;
#ifdef GC_AP_SD_TIMING
// Off by default, for the Dolphin timing run (ignore/pi-slotdata_stage2/gc_sd_timing.py): the copy out of the
// uncached mailbox and the parse of the cached copy, in microseconds, read from RAM by the harness
volatile u32 gc_ap_sd_copy_us;
volatile u32 gc_ap_sd_parse_us;
#endif
static gc_ap_item gc_ap_items[GC_AP_ITEM_MAX];
static u32 gc_ap_items_have;

static ap_connectstate gc_ap_cs;
static ap_config gc_ap_config; // filled from the mailbox (ap_getconfig)
static u32 gc_ap_last_client_tick;
static u32 gc_ap_client_quiet; // frames since client_tick last changed

// Checked locations as a hash set (open addressing, 0 = empty: no AP location id is 0), filled from the
// append-only checked list as it grows; rebuilt when the client resets it (new connection)
#define GC_AP_CHECKED_SET 4096 // power of 2, > 1.5x GC_AP_CHECKED_MAX
static u32 gc_ap_checked_set[GC_AP_CHECKED_SET];
static u32 gc_ap_checked_in_set; // checked entries already in the set

static u32 gc_ap_hash(u32 id) {
  return (id * 2654435761u) & (GC_AP_CHECKED_SET - 1);
}

// Ids the game code sent (ap_send_location), like the DLL's own checks set: a one-shot send (tree / beach pickup)
// counts as checked at once, so a second pickup before the server confirms the first takes the next id. Cleared when
// a new client process starts (gc_ap_mailbox_frame): a send the old client never got to the server is then sent
// again by the code that repeats its sends (museum, favors, loans).
static u32 gc_ap_sent_set[GC_AP_CHECKED_SET];
static u32 gc_ap_sent_in_set;

// 1 = id is in set
static int gc_ap_set_has(const u32* set, u32 id) {
  u32 h = gc_ap_hash(id);
  while(set[h] != 0) {
    if(set[h] == id) {
      return 1;
    }
    h = (h + 1) & (GC_AP_CHECKED_SET - 1);
  }
  return 0;
}

// Adds id to set; 1 = it was new
static int gc_ap_set_add(u32* set, u32 id) {
  u32 h = gc_ap_hash(id);
  while(set[h] != 0) {
    if(set[h] == id) {
      return 0;
    }
    h = (h + 1) & (GC_AP_CHECKED_SET - 1);
  }
  set[h] = id;
  return 1;
}

static void gc_ap_checked_add(u32 id) {
  gc_ap_set_add(gc_ap_checked_set, id);
}

static u32 gc_ap_checked_total(void) {
  u32 n = gc_ap_mbox.checked_count;
  return n > GC_AP_CHECKED_MAX ? GC_AP_CHECKED_MAX : n;
}

// The set up to date with the checked list
static void gc_ap_checked_sync(void) {
  u32 n = gc_ap_checked_total();
  if(n < gc_ap_checked_in_set) {
    memset(gc_ap_checked_set, 0, sizeof(gc_ap_checked_set));
    gc_ap_checked_in_set = 0;
  }
  while(gc_ap_checked_in_set < n) {
    gc_ap_checked_add(gc_ap_mbox.checked[gc_ap_checked_in_set++]);
  }
}

// New items from the mailbox into gc_ap_items
static void gc_ap_items_sync(void) {
  gc_ap_mailbox* m = &gc_ap_mbox;
  u32 n = m->item_count;
  if(n > GC_AP_ITEM_MAX) {
    n = GC_AP_ITEM_MAX;
  }
  if(n < gc_ap_items_have) {
    gc_ap_items_have = 0;
  }
  while(gc_ap_items_have < n) {
    gc_ap_items[gc_ap_items_have] = m->items[gc_ap_items_have];
    gc_ap_items_have++;
  }
}

void gc_ap_mailbox_frame(void) {
  static int flushed;
  gc_ap_mailbox* m = &gc_ap_mbox;
  if(!flushed) {
    // Before any uncached use: .bss clearing may have left dirty cache lines here, and their later write-back
    // would overwrite what the client wrote
    DCFlushRange(&gc_ap_mbox_mem, sizeof(gc_ap_mbox_mem));
    flushed = 1;
  }
  if(m->magic[0] != GC_AP_MAGIC0) {
    // .bss is zero: fill in the header once. The magic goes last, so a client never sees a half-written header
    m->version = GC_AP_MAILBOX_VERSION;
    m->size = sizeof(gc_ap_mailbox);
    m->items_off = (u32)((u8*)m->items - (u8*)m);
    m->items_max = GC_AP_ITEM_MAX;
    m->checked_off = (u32)((u8*)m->checked - (u8*)m);
    m->checked_max = GC_AP_CHECKED_MAX;
    m->send_off = (u32)((u8*)m->send - (u8*)m);
    m->send_max = GC_AP_SEND_MAX;
    m->slotdata_off = (u32)((u8*)&m->slotdata - (u8*)m);
    m->slotdata_size = GC_AP_SD_JSON_MAX;
    m->toasts_off = (u32)((u8*)m->toasts - (u8*)m);
    m->toast_max = GC_AP_TOAST_MAX;
    m->toast_len = GC_AP_TOAST_LEN;
    m->names_off = (u32)((u8*)m->names - (u8*)m);
    m->name_max = GC_AP_NAME_MAX;
    m->name_len = GC_AP_NAME_LEN;
    m->gset_off = (u32)((u8*)m->gset - (u8*)m);
    m->gset_max = GC_AP_GSET_NUM;
    m->hint_off = (u32)((u8*)m->hint - (u8*)m);
    m->hint_name_len = GC_AP_HINT_NAME_LEN;
    m->diag_off = (u32)((u8*)&m->diag - (u8*)m);
    m->diag_size = (u32)((u8*)m->diag.log - (u8*)&m->diag);
    m->log_off = (u32)((u8*)m->diag.log - (u8*)m);
    m->log_max = GC_AP_LOG_MAX;
    m->log_len = GC_AP_LOG_LEN;
    m->tail = GC_AP_TAIL;
    m->magic[1] = GC_AP_MAGIC1;
    m->magic[0] = GC_AP_MAGIC0;
  }
  m->game_tick++;
  if(m->client_tick != gc_ap_last_client_tick) {
    // A new client counts client_tick from 1 again: what the old one read from the ring may never have reached
    // the server, so the sent set starts over
    if(m->client_tick < gc_ap_last_client_tick) {
      memset(gc_ap_sent_set, 0, sizeof(gc_ap_sent_set));
      gc_ap_sent_in_set = 0;
    }
    gc_ap_last_client_tick = m->client_tick;
    gc_ap_client_quiet = 0;
  } else if(gc_ap_client_quiet < GC_AP_CLIENT_TIMEOUT_FRAMES) {
    gc_ap_client_quiet++;
  }
  gc_ap_checked_sync();
  gc_ap_items_sync();
  if(!gc_ap_sd.valid && m->slotdata_ready) {
    // The client writes the text, then slotdata_len, then slotdata_ready: read the length after the ready flag
    u32 n = m->slotdata_len;
    if(n > 0 && n <= GC_AP_SD_JSON_MAX && n != gc_ap_sd_json_len) {
      gc_ap_sd_json_len = n;
#ifdef GC_AP_SD_TIMING
      OSTime t_copy = OSGetTime();
#endif
      memcpy(gc_ap_sd_json, m->slotdata, n);
#ifdef GC_AP_SD_TIMING
      OSTime t_parsed = OSGetTime();
#endif
      if(!pc_ap_slotdata_parse(gc_ap_sd_json, (int)n, &gc_ap_sd)) {
        // Not JSON / too many tokens: gc_ap_sd stays invalid, so Start stays blocked
        APLOG_ERROR("slot_data: can't parse %u bytes, Start stays blocked", (unsigned)n);
      }
#ifdef GC_AP_SD_TIMING
      gc_ap_sd_copy_us = (u32)OSTicksToMicroseconds(t_parsed - t_copy);
      gc_ap_sd_parse_us = (u32)OSTicksToMicroseconds(OSGetTime() - t_parsed);
#endif
    }
  } else if(!m->slotdata_ready) {
    gc_ap_sd_json_len = 0; // a new connection writes new text: try it once
  }
  gc_ap_gsettings_frame();
}

static int gc_ap_client_alive(void) {
  return gc_ap_mbox.client_tick != 0 && gc_ap_client_quiet < GC_AP_CLIENT_TIMEOUT_FRAMES;
}

// No ap_config.ini on GC: the overlay's "<slot>@<host>" comes from the client
const ap_config* ap_getconfig(void) {
  gc_ap_mailbox* m = &gc_ap_mbox;
  if(gc_ap_client_alive() && m->host[0] != '\0') {
    snprintf(gc_ap_config.host, sizeof(gc_ap_config.host), "%.*s", (int)sizeof(m->host) - 1, m->host);
    snprintf(gc_ap_config.slotname, sizeof(gc_ap_config.slotname), "%.*s", GC_AP_NAME_LEN - 1, m->slotname);
  } else {
    snprintf(gc_ap_config.host, sizeof(gc_ap_config.host), "%s", "AP client");
    gc_ap_config.slotname[0] = '\0';
  }
  return &gc_ap_config;
}

int ap_roomplayer_valid(const ap_roomplayer* rp) {
  return rp->seed[0] != '\0';
}

const ap_connectstate* ap_getconnectstate(void) {
  gc_ap_mailbox* m = &gc_ap_mbox;
  int alive = gc_ap_client_alive();
  gc_ap_cs.state = alive ? m->client_state : AP_CSTATE_CONNECTING;
  if(alive && m->status[0] != '\0') {
    snprintf(gc_ap_cs.last_refuse_reason, sizeof(gc_ap_cs.last_refuse_reason), "%.*s", GC_AP_STATUS_LEN - 1,
             m->status);
  }
  // Like the DLL, the slot is only taken once: another slot later is refused
  if(alive && m->slotdata_ready && m->seed[0] != '\0') {
    if(!ap_roomplayer_valid(&gc_ap_cs.roomplayer)) {
      snprintf(gc_ap_cs.roomplayer.seed, sizeof(gc_ap_cs.roomplayer.seed), "%.*s", (int)sizeof(m->seed), m->seed);
      gc_ap_cs.roomplayer.team = m->team;
      gc_ap_cs.roomplayer.player = m->slot;
      gc_ap_cs.connect_once = 1;
    } else if(strncmp(gc_ap_cs.roomplayer.seed, m->seed, sizeof(m->seed)) != 0 ||
              gc_ap_cs.roomplayer.team != m->team || gc_ap_cs.roomplayer.player != m->slot) {
      gc_ap_cs.state = AP_CSTATE_SLOTREFUSED;
      snprintf(gc_ap_cs.last_refuse_reason, sizeof(gc_ap_cs.last_refuse_reason),
               "Another slot: restart the game to change slots");
    }
  }
  // The title's card check (gc_ap_state_card.c): another slot's town on the card
  if(gc_ap_cs.state != AP_CSTATE_SLOTREFUSED && gc_ap_card_title_refusal() != NULL) {
    gc_ap_cs.state = AP_CSTATE_SLOTREFUSED;
    snprintf(gc_ap_cs.last_refuse_reason, sizeof(gc_ap_cs.last_refuse_reason), "%s", gc_ap_card_title_refusal());
  }
  return &gc_ap_cs;
}

const ap_slotdata* ap_getslotdata(void) {
  return &gc_ap_sd;
}

static u32 gc_ap_item_total(void) {
  return gc_ap_items_have;
}

size_t ap_getitemcount(void) {
  return gc_ap_item_total();
}

int64_t ap_getitem(size_t idx) {
  return idx < gc_ap_item_total() ? (int64_t)gc_ap_items[idx].id : -1;
}

int ap_getitem_sender(size_t idx) {
  return idx < gc_ap_item_total() ? gc_ap_items[idx].sender : -1;
}

int64_t ap_getitem_location(size_t idx) {
  return -1; // not in the mailbox (nothing on GC asks)
}

unsigned ap_getitem_flags(size_t idx) {
  return idx < gc_ap_item_total() ? gc_ap_items[idx].flags : 0;
}

int ap_player_name(int slot, char* buf, size_t len) {
  const char* name;
  if(slot < 0 || slot >= GC_AP_NAME_MAX || len == 0 || !gc_ap_mbox.slotdata_ready) {
    return 0;
  }
  name = gc_ap_mbox.names[slot];
  if(name[0] == '\0') {
    return 0;
  }
  snprintf(buf, len, "%.*s", GC_AP_NAME_LEN - 1, name);
  return 1;
}

int ap_item_count(int64_t id) {
  u32 n = gc_ap_item_total();
  u32 i;
  int count = 0;
  for(i = 0; i < n; i++) {
    if(gc_ap_items[i].id == (u32)id) {
      count++;
    }
  }
  return count;
}

int ap_location_checked(int64_t id) {
  // The set is synced once per frame (gc_ap_mailbox_frame): here it'd be an uncached read per call
  return gc_ap_set_has(gc_ap_checked_set, (u32)id) || gc_ap_set_has(gc_ap_sent_set, (u32)id);
}

void ap_locations_checked(const int64_t* ids, size_t n, uint8_t* out) {
  size_t i;
  for(i = 0; i < n; i++) {
    out[i] = (uint8_t)ap_location_checked(ids[i]);
  }
}

int64_t ap_highest_checked(int64_t first, int64_t last) {
  int64_t id;
  for(id = last; id >= first; id--) {
    if(ap_location_checked(id)) {
      return id;
    }
  }
  return -1;
}

int ap_unchecked_count(int64_t first, int64_t last) {
  int64_t id;
  int count = 0;
  for(id = first; id <= last; id++) {
    count += !ap_location_checked(id);
  }
  return count;
}

int64_t ap_first_unchecked(int64_t first, int64_t last) {
  int64_t id;
  for(id = first; id <= last; id++) {
    if(!ap_location_checked(id)) {
      return id;
    }
  }
  return -1;
}

// The checked list only grows (a reset comes with a new connection), so its length works as the generation
unsigned ap_checked_generation(void) {
  return gc_ap_mbox.checked_count;
}

// Like the DLL, repeats are dropped (already checked or sent) and a queued id counts as checked from now on. A full
// ring drops the id without marking it, so code that repeats its sends tries again later (64 slots: shouldn't happen).
void ap_send_location(int64_t id) {
  gc_ap_mailbox* m = &gc_ap_mbox;
  if(ap_location_checked(id)) {
    return;
  }
  if(m->send_head - m->send_tail >= GC_AP_SEND_MAX) {
    return;
  }
  // Leaves one slot of the table empty, so lookups always end
  if(gc_ap_sent_in_set >= GC_AP_CHECKED_SET - 1) {
    return;
  }
  gc_ap_sent_in_set += gc_ap_set_add(gc_ap_sent_set, (u32)id);
  m->send[m->send_head % GC_AP_SEND_MAX] = (u32)id;
  m->send_head++;
}

void ap_send_goal(void) {
  gc_ap_mbox.goal = 1;
}

int ap_goal_sent(void) {
  return gc_ap_mbox.goal != 0;
}

int ap_pop_toast(char* buf, size_t len) {
  gc_ap_mailbox* m = &gc_ap_mbox;
  if(m->toast_tail == m->toast_head || len == 0) {
    return 0;
  }
  if(m->toast_head - m->toast_tail > GC_AP_TOAST_MAX) {
    m->toast_tail = m->toast_head - GC_AP_TOAST_MAX; // the client ran ahead: skip what it wrote over
  }
  snprintf(buf, len, "%.*s", GC_AP_TOAST_LEN - 1, m->toasts[m->toast_tail % GC_AP_TOAST_MAX]);
  m->toast_tail++;
  return 1;
}

// DeathLink: stage 4 (GC client, via the mailbox). Until then a send is dropped and nothing is ever received.
void ap_send_death(const char* cause) {
  (void)cause;
}

int ap_pop_death(char* source, size_t slen, char* cause, size_t clen) {
  (void)source;
  (void)slen;
  (void)cause;
  (void)clen;
  return 0;
}

// ap_log.h: the DLL's log file; on GC the OS console (Dolphin's log) and the client's diagnostics log
void ap_log_write(const char* level, const char* fmt, ...) {
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  OSReport("[AP %s] %s\n", level, buf);
  gc_ap_diag_log("%s %s", level, buf);
}
