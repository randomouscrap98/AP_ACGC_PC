#ifndef GC_AP_MAILBOX_H
#define GC_AP_MAILBOX_H

// RAM mailbox between the GC game and the PC client (apworld gc_client.py), which reads and writes it through
// Dolphin's memory (or any other RAM bridge). Everything is big-endian, as the GC stores it.
// The client finds it by scanning MEM1 for magic[0..1], then checks version, size and tail, and takes every
// section's place and capacity from the header, so only the header layout is fixed per version.
// Lists are append-only, so neither side ever sees a half-rewritten list:
// - items: what the server sent this slot, in AP index order. The client writes entries, then item_count.
// - checked: the server's checked locations (unsorted). The client writes entries, then checked_count.
// - send: location ids the game wants sent, a ring. The game writes send[send_head % send_max], then send_head++;
//   the client sends up to send_head and writes send_tail.
// A new connection (other slot, or the server restarted) resets item_count / checked_count to 0 first.

#include "types.h"
#include "ap_slotdata.h"

#define GC_AP_MAGIC0 0x41434150 // "ACAP"
#define GC_AP_MAGIC1 0x4D424F58 // "MBOX"
#define GC_AP_TAIL   0x454E4421 // "END!"
// Bump when the header layout or a section's entry format changes (client refuses other versions)
#define GC_AP_MAILBOX_VERSION 1

#define GC_AP_ITEM_MAX    2560 // received items (apworld: at most 2220 locations, so items, per slot)
#define GC_AP_CHECKED_MAX 2560 // checked locations
#define GC_AP_SEND_MAX    64   // send ring

// Seconds without a client_tick change before the game counts the client as gone
#define GC_AP_CLIENT_TIMEOUT_FRAMES (5 * 60)

typedef struct {
  u32 id;     // AP item id (ours all fit in 32 bits)
  s16 sender; // slot that sent it
  u8 flags;   // AP_ITEM_* (ap_archipelago.h)
  u8 pad;
} gc_ap_item;

typedef struct {
  // Header: written once by the game at startup
  u32 magic[2];
  u32 version;
  u32 size; // sizeof(gc_ap_mailbox)
  u32 items_off, items_max;
  u32 checked_off, checked_max;
  u32 send_off, send_max;
  u32 slotdata_off, slotdata_size; // phase 3: the client writes a packed ap_slotdata here

  // Game -> client
  u32 game_tick; // +1 per game frame: the game is running
  u32 send_head;
  u32 goal; // 1 = goal done
  u32 probe_trigger; // probe only: the client sets 1, the game acts as if Favor 1 were done and clears it

  // Client -> game
  u32 client_tick; // changes while the client runs (heartbeat)
  s32 client_state; // AP_CSTATE_*
  u32 send_tail;
  u32 item_count;
  u32 checked_count;
  u32 slotdata_ready; // 1 = slotdata holds this slot's slot_data
  s32 team;
  s32 slot;
  char seed[64];

  // Sections
  gc_ap_item items[GC_AP_ITEM_MAX];
  u32 checked[GC_AP_CHECKED_MAX];
  u32 send[GC_AP_SEND_MAX];
  ap_slotdata slotdata;
  u32 tail;
} gc_ap_mailbox;

extern gc_ap_mailbox gc_ap_mbox;

// Once per game frame: header on the first call, game_tick, client timeout
void gc_ap_mailbox_frame(void);

#endif
