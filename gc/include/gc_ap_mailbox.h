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
// - toasts: overlay lines the client formats (item received / sent), a ring the other way round: the client
//   writes toasts[toast_head % toast_max], then toast_head++; the game shows up to toast_head, writes toast_tail.
// - names: slot names by slot number (0 = the server), zero-terminated, cut to GC_AP_NAME_LEN - 1 bytes. The
//   client writes them once per connection, before slotdata_ready.
// - slotdata: the upgraded slot_data as compact JSON text (ASCII, \uXXXX escapes; the game parses it with
//   pc/src/pc_ap_slotdata_parse.c). The client writes the text, then slotdata_len, then slotdata_ready.
// A new connection (other slot, or the server restarted) resets item_count / checked_count to 0 first.
// - gset: the global settings (Settings > Gameplay / Overlay), which have no file on a console: the client keeps
//   them (gc_client.py). Client -> game: it writes gset[], then gset_client_gen (any new value); the game takes
//   them. Game -> client: on Apply the game writes gset[], then gset_game_gen++; the client stores them. A client
//   that finds gset_client_gen other than what it last wrote (the game restarted) writes its values again.
// - hint: beach hints (pc_ap_beach_hint_client.h). The client has the server's lists, so it picks the location: the
//   game writes hint_req++ for every AP Hint picked up; the client writes the three names of the hint it scouted
//   (or nothing when it found no eligible location), then hint_req_done = that hint_req. The client keeps
//   hint_eligible = how many of this slot's locations are missing and not hinted with us as the finder (the game's
//   spawn check). hint_req_done 0 = nothing answered yet; hint_req 0 = no request since the game started.
// - diag: what the game is doing, for the client's diagnostics log (gc_ap_diag.c, gc_client.py DiagLog): a state
//   snapshot every frame, a watchdog record while the game stands still, a crash record, and an event log ring
//   (the game writes log[log_head % log_max], then log_head++). Game -> client, except test.
// - death: DeathLink. Game -> client: the beach hint request's pattern: the game writes death_cause (the cause text,
//   zero-terminated) and then death_req++; the client sends one death per death_req it hasn't seen (two deaths
//   between two reads: sending the later one is fine). A counter that went back = the game restarted: no send.
//   Client -> game: a ring of gc_ap_death (source name + cause): the client writes death[death_head % death_max],
//   then death_head++; the game takes up to death_head and writes death_tail (a full ring drops the oldest and the
//   game skips ahead). Deaths are dropped, not kept, while the client has no game or no server connection.

#include "types.h"
#include "dolphin/os.h" // OSCachedToUncached

#define GC_AP_MAGIC0 0x41434150 // "ACAP"
#define GC_AP_MAGIC1 0x4D424F58 // "MBOX"
#define GC_AP_TAIL   0x454E4421 // "END!"
// Bump when the header layout or a section's entry format changes (client refuses other versions)
#define GC_AP_MAILBOX_VERSION 7

#define GC_AP_ITEM_MAX    2560 // received items (apworld: at most 2220 locations, so items, per slot)
#define GC_AP_CHECKED_MAX 2560 // checked locations
#define GC_AP_SEND_MAX    64   // send ring
#define GC_AP_TOAST_MAX   8    // toast ring
#define GC_AP_TOAST_LEN   96   // bytes per toast, with the zero (AP_CTRL_* colour codes allowed)
#define GC_AP_NAME_MAX    64   // slots with a name (higher slots show no name)
#define GC_AP_NAME_LEN    17   // AP slot names are at most 16 characters
#define GC_AP_SD_JSON_MAX 16384 // slot_data JSON text (9.8 KB raw is the largest real seed, 5.8 KB compact)
#define GC_AP_STATUS_LEN  96   // client status line, with the zero
#define GC_AP_HINT_NAME_LEN 64 // beach hint player / item / location name, with the zero
#define GC_AP_DIAG_TRACE  16   // return addresses in a watchdog / crash record
#define GC_AP_LOG_MAX     64   // diagnostics event log ring
#define GC_AP_LOG_LEN     80   // bytes per log entry: u32 game_tick + text with the zero
#define GC_AP_DEATH_MAX   4    // received deaths, a ring (pc_ap_traps.c keeps 3 traps waiting)
#define GC_AP_DEATH_NAME_LEN  32 // death source slot name, with the zero (pc_ap_traps.c reads 32)
#define GC_AP_DEATH_CAUSE_LEN 64 // death cause text, with the zero (pc_ap_traps.c reads 64)

// Global settings in gset[], by index: append-only (gc_client.py GSET_NAMES lists the same names in this order)
enum {
  GC_AP_GSET_NO_COCKROACHES,
  GC_AP_GSET_NO_FISHING_TRASH,
  GC_AP_GSET_EASIER_CRITTER_CATCH,
  GC_AP_GSET_HIDDEN_BUG_HINTS,
  GC_AP_GSET_HIDDEN_ITEM_HINTS,
  GC_AP_GSET_SHORT_SHOP_TALK,
  GC_AP_GSET_AP_STATUS_ALWAYS,
  GC_AP_GSET_AP_TRACKER_ALWAYS,
  GC_AP_GSET_AP_TOAST_SECONDS,
  GC_AP_GSET_AP_TOAST_MAX,
  GC_AP_GSET_DISABLE_RESETTI,
  GC_AP_GSET_INSTANT_TEXT,
  GC_AP_GSET_BITE_SOUND,
  GC_AP_GSET_LESS_MAIL,
  GC_AP_GSET_MAP_DIVINATION,
  GC_AP_GSET_PLANTING_MARKS,
  GC_AP_GSET_SCREEN_MARGIN,
  GC_AP_GSET_NUM
};

// Frames without a client_tick change before the game counts the client as gone. Longer than the client's 5 s
// request timeout (gc_wii.NintendontRAM), so a Wi-Fi hiccup the client gets over by reconnecting never shows
#define GC_AP_CLIENT_TIMEOUT_FRAMES (10 * 60)

typedef struct {
  u32 id;     // AP item id (ours all fit in 32 bits)
  s16 sender; // slot that sent it
  u8 flags;   // AP_ITEM_* (ap_archipelago.h)
  u8 pad;
} gc_ap_item;

// Where a thread was: pc, lr, stack pointer and the return addresses up its stack (0 = end of the chain)
typedef struct {
  u32 pc, lr, sp;
  u32 trace[GC_AP_DIAG_TRACE];
} gc_ap_diag_ctx;

typedef struct {
  u32 tick; // game_tick when it was written
  char text[GC_AP_LOG_LEN - 4];
} gc_ap_diag_line;

// A death received (DeathLink): who died and why, both zero-terminated
typedef struct {
  char source[GC_AP_DEATH_NAME_LEN];
  char cause[GC_AP_DEATH_CAUSE_LEN];
} gc_ap_death;

// Diagnostics (v6). All u32, so the client reads it as one struct (gc_client.py DIAG_FIELDS, same order).
typedef struct {
  // Every game frame (graph_main)
  u32 scene;        // Save scene_no
  u32 game;         // gamePT->exec: which game state (look it up in the map file)
  u32 paused;       // pause menu open
  u32 hold;         // buttons down when the pause menu closed, still hidden from the game (gc_ap_pause_held)
  u32 reload;       // Date & Time / Save reload step (PC_AP_RELOAD_*)
  u32 pads;         // controllers padmgr reads
  u32 pad[4];       // button << 16 | (u8)stick_x << 8 | (u8)stick_y, before the pause menu's hold
  u32 weather;      // Common weather << 16 | intensity
  u32 save_weather; // the save's weather byte
  u32 date;         // year << 16 | month << 8 | day
  u32 time;         // hour << 16 | minute << 8 | second
  u32 heap;         // free bytes in the game state's arena
  u32 cost_us;      // microseconds the diagnostics took last frame
  u32 cost_max_us;  // most they took in one frame since the game started (wall clock: includes other threads)
  u32 cost_sum_us;  // all of it since the game started (the client logs the average per frame)
  // Where the REL went (it moves): ignore/claude/tools/gc/diag_addr.py maps the log's addresses with these
  u32 anchor_text;  // gc_ap_diag_frame
  u32 anchor_data;  // gc_ap_diag_trap
  u32 anchor_bss;   // gc_ap_mbox_mem
  // Every play frame (pc_ap_tick, not while paused)
  u32 play_tick;
  s32 pos[3];       // player position, whole units
  s32 action;       // player now_main_index (mPlayer_INDEX_*), -1 = no player
  u32 block;        // acre: bx << 8 | bz
  u32 submenu;      // submenu process_status (0 = none open)
  u32 demo;         // mDemo_CheckDemo: talk, door, event
  u32 wipe;         // fb_wipe_mode: scene transition
  // Watchdog (timer interrupt, once a second)
  u32 wd_ticks;     // watchdog runs
  u32 stall;        // seconds game_tick has stood still (0 = moving)
  u32 hang_seq;     // +1 per hang record (from 2 s standing still on, once a second)
  gc_ap_diag_ctx hang; // where the graph thread (the game) is
  // Crash (OS error handler, before the game's own handler restarts it, which waits 3 s for the client)
  u32 crash_seq;
  u32 crash_error, crash_dsisr, crash_dar;
  u32 crash_thread; // the OSThread that crashed (main.h graphThread = the game)
  gc_ap_diag_ctx crash;
  // Client -> game
  u32 test;         // self-test (harnesses): 1 = crash now, 2 = hang while it stays 2
  u32 log_head;
  gc_ap_diag_line log[GC_AP_LOG_MAX];
} gc_ap_diag;

typedef struct {
  // Header: written once by the game at startup
  u32 magic[2];
  u32 version;
  u32 size; // sizeof(gc_ap_mailbox)
  u32 items_off, items_max;
  u32 checked_off, checked_max;
  u32 send_off, send_max;
  u32 slotdata_off, slotdata_size; // slot_data JSON text, uncached: < slotdata_size bytes
  u32 slotdata_len; // JSON text length the client wrote (client -> game, before slotdata_ready)
  u32 toasts_off, toast_max, toast_len;
  u32 names_off, name_max, name_len;

  // Game -> client
  u32 game_tick; // +1 per game frame: the game is running
  u32 send_head;
  u32 goal; // 1 = goal done
  u32 toast_tail;

  // Client -> game
  u32 client_tick; // changes while the client runs (heartbeat)
  s32 client_state; // AP_CSTATE_*
  u32 send_tail;
  u32 item_count;
  u32 checked_count;
  u32 slotdata_ready; // 1 = slotdata and names hold this slot's
  u32 toast_head;
  s32 team;
  s32 slot;

  // Global settings (header place + both generations; gset[] below)
  u32 gset_off, gset_max;
  u32 gset_game_gen;   // game -> client
  u32 gset_client_gen; // client -> game

  // Beach hints (v5; header place, see hint[] below)
  u32 hint_off, hint_name_len;
  u32 hint_req;      // game -> client: +1 per AP Hint picked up (1, 2, ...: 0 stays "no request")
  u32 hint_req_done; // client -> game: the hint_req hint[] answers (0 = none answered yet)
  u32 hint_eligible; // client -> game: missing own locations not hinted with us (0 = none / not known)

  // Diagnostics (v6; header place, see diag below)
  u32 diag_off, diag_size; // the gc_ap_diag struct; diag_size = bytes before log[] (the fixed fields, log_head last)
  u32 log_off, log_max, log_len; // diag.log

  // Deaths (v7; header place, see death[] and death_cause below)
  u32 death_off, death_max, death_name_len, death_cause_len; // the gc_ap_death ring
  u32 death_head; // client -> game: entries written (the game reads death[death_tail % death_max])
  u32 death_tail; // game -> client: entries taken
  u32 death_req;     // game -> client: +1 per death to send (1, 2, ...: 0 stays "no death")
  u32 death_req_off, death_req_len; // game -> client: death_cause, the cause of the latest death_req

  char seed[64];
  char status[GC_AP_STATUS_LEN]; // why the client can't go on (slot_data from a newer apworld...), "" = fine
  char host[64];     // server the client talks to, for the overlay ("" before it knows)
  char slotname[GC_AP_NAME_LEN];

  // Sections
  gc_ap_item items[GC_AP_ITEM_MAX];
  u32 checked[GC_AP_CHECKED_MAX];
  u32 send[GC_AP_SEND_MAX];
  char toasts[GC_AP_TOAST_MAX][GC_AP_TOAST_LEN];
  char names[GC_AP_NAME_MAX][GC_AP_NAME_LEN];
  s32 gset[GC_AP_GSET_NUM];
  char slotdata[GC_AP_SD_JSON_MAX]; // compact JSON text of the upgraded slot_data (pc_ap_slotdata_parse)
  char hint[3][GC_AP_HINT_NAME_LEN]; // beach hint: receiving player, item, location; all "" = no eligible one
  gc_ap_diag diag;
  gc_ap_death death[GC_AP_DEATH_MAX];       // client -> game: deaths to apply, oldest at death_tail
  char death_cause[GC_AP_DEATH_CAUSE_LEN]; // game -> client: the cause of the latest death_req
  u32 tail;
} gc_ap_mailbox;

// The mailbox's memory. The game only ever uses it through gc_ap_mbox, the CPU's uncached mirror of it: on a Wii
// the client reaches game RAM through the Nintendont kernel (nintendont/kernel/ap_server.c), which reads and writes
// RAM directly, so cached copies on the game side would go stale both ways. gc_ap_mailbox_frame flushes the cache
// lines once before the first use. Hot data (slot_data, received items) is copied to cached memory in
// gc_ap_backend.c, so game code doesn't pay for uncached reads.
extern gc_ap_mailbox gc_ap_mbox_mem;
#define gc_ap_mbox (*(gc_ap_mailbox*)OSCachedToUncached(&gc_ap_mbox_mem))

// Once per game frame: header on the first call, game_tick, client timeout
void gc_ap_mailbox_frame(void);
// Once per game frame, after it: global settings from the client (gc_ap_port.c)
void gc_ap_gsettings_frame(void);

#endif
