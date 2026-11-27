// GC diagnostics (gc_ap_mailbox.h "diag"): what the game is doing, for the client's diagnostics log. Made to catch
// rare freezes in the field (Wii or Dolphin), where nothing else can look inside the game:
// - a state snapshot every frame (scene, pause / input hold, pads, weather, date; player, acre, menus per play frame)
//   and an event line for each change of it, written into the log ring by the game, read by the client;
// - a watchdog: an OS alarm once a second (timer interrupt, no thread) that records where the graph thread is, with its
//   call stack, once a second while game_tick stands still;
// - a crash record: wraps the OS error handlers the game's crash handler installed (JUTException), records the
//   crash, waits 3 s so the client can read it (the game's handler then restarts at once), then hands it on.
// Everything goes through the uncached mailbox (gc_ap_mbox), so the client sees it even when the game is stuck.
#include "gc_ap_diag.h"
#include "gc_ap_mailbox.h"
#include "pc_ap_logic.h"
#include "pc_pause_menu.h"
#include "gc_ap_hooks.h"
#include "dolphin/os.h"
#include "dolphin/os/OSError.h"
#include "dolphin/os/OSCache.h"
#include "libu64/pad.h"
#include "game.h"
#include "main.h"
#include "m_common_data.h"
#include "m_play.h"
#include "m_demo.h"
#include "m_player_lib.h"
#include "m_field_info.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define GC_AP_DIAG_HANG_SECONDS 2  // watchdog: seconds standing still before it records the graph thread
#define GC_AP_DIAG_CRASH_WAIT   3  // seconds the crash record waits for the client before the game restarts
#define GC_AP_DIAG_HOLD_REPEAT  120 // frames between "input still held" lines

static OSTime gc_ap_diag_cost;  // this frame's ticks in diagnostics so far
// The last frame's values the events compare against (all 0xFFFFFFFF at first: everything counts as changed)
typedef struct {
  u32 scene, game, paused, reload, weather, save_weather, date, pads;
  u32 block, submenu, demo, wipe;
  s32 action;
} gc_ap_diag_seen;
static gc_ap_diag_seen gc_ap_diag_last;
static int gc_ap_diag_started;
static u32 gc_ap_diag_hold_frames;
static OSAlarm gc_ap_diag_alarm;
static u32 gc_ap_diag_alarm_tick; // game_tick at the last watchdog run
static OSErrorHandler gc_ap_diag_prev[OS_ERROR_MAX]; // the handlers wrapped (NULL = not wrapped)
u32 gc_ap_diag_trap[2] = { 0x7FE00008, 0x4E800020 }; // self-test crash: tw 31,0,0; blr (also the .data anchor)

void gc_ap_diag_log(const char* fmt, ...) {
  // Built here, then copied as whole words: byte stores to the uncached mirror come out as 32-bit writes in
  // Dolphin (seen 2026-11-27: "00 00 00 67" for 'g'), so the game writes the mailbox in words only
  u32 buf[GC_AP_LOG_LEN / 4];
  gc_ap_diag* d = &gc_ap_mbox.diag;
  u32* line;
  va_list args;
  BOOL enable;
  int i;
  memset(buf, 0, sizeof(buf));
  va_start(args, fmt);
  vsnprintf((char*)&buf[1], GC_AP_LOG_LEN - 4, fmt, args);
  va_end(args);
  enable = OSDisableInterrupts(); // a slot per caller (card callbacks log from other threads)
  buf[0] = gc_ap_mbox.game_tick;
  line = (u32*)&d->log[d->log_head % GC_AP_LOG_MAX];
  for(i = 0; i < GC_AP_LOG_LEN / 4; i++) {
    line[i] = buf[i];
  }
  d->log_head++;
  OSRestoreInterrupts(enable);
}

static int gc_ap_diag_ram(u32 p) {
  return p >= 0x80000000 && p < 0x81800000 && (p & 3) == 0;
}

// pc, lr, sp and the return addresses up the stack's back chain (PPC EABI: frame[0] = caller's frame, the
// caller's frame[1] = the saved return address)
static void gc_ap_diag_where(gc_ap_diag_ctx* out, const OSContext* ctx) {
  u32 sp = ctx->gpr[1];
  int i;
  out->pc = ctx->srr0;
  out->lr = ctx->lr;
  out->sp = sp;
  for(i = 0; i < GC_AP_DIAG_TRACE; i++) {
    u32 ret = 0;
    if(gc_ap_diag_ram(sp)) {
      sp = *(u32*)sp;
      if(gc_ap_diag_ram(sp)) {
        ret = ((u32*)sp)[1];
      }
    }
    out->trace[i] = ret;
  }
}

// Timer interrupt, once a second: no locals to speak of, no printing (it runs on whatever stack it interrupted).
// Sets itself again (the DOL has no OSSetPeriodicAlarm).
static void gc_ap_diag_watchdog(OSAlarm* alarm, OSContext* context) {
  gc_ap_diag* d = &gc_ap_mbox.diag;
  u32 tick = gc_ap_mbox.game_tick;
  OSSetAlarm(&gc_ap_diag_alarm, OSSecondsToTicks((OSTime)1), gc_ap_diag_watchdog);
  d->wd_ticks++;
  if(tick != gc_ap_diag_alarm_tick) {
    gc_ap_diag_alarm_tick = tick;
    d->stall = 0;
    return;
  }
  d->stall++;
  if(d->stall >= GC_AP_DIAG_HANG_SECONDS) {
    // The graph thread's registers: saved into its context when this interrupt (or a thread switch) stopped it
    gc_ap_diag_where(&d->hang, &graphThread.context);
    d->hang_seq++;
  }
}

static void gc_ap_diag_error(OSError error, OSContext* context, u32 dsisr, u32 dar) {
  gc_ap_diag* d = &gc_ap_mbox.diag;
  OSTime start;
  d->crash_error = error;
  d->crash_dsisr = dsisr;
  d->crash_dar = dar;
  d->crash_thread = (u32)OSGetCurrentThread();
  gc_ap_diag_where(&d->crash, context);
  d->crash_seq++;
  start = OSGetTime();
  while(OSGetTime() - start < OSSecondsToTicks((OSTime)GC_AP_DIAG_CRASH_WAIT)) {
  }
  ((void (*)(OSError, OSContext*, u32, u32))gc_ap_diag_prev[error])(error, context, dsisr, dar);
}

static void gc_ap_diag_start(void) {
  gc_ap_diag* d = &gc_ap_mbox.diag;
  static const OSError errors[] = { OS_ERROR_DSI, OS_ERROR_ISI, OS_ERROR_PROGRAM, OS_ERROR_ALIGNMENT,
                                    OS_ERROR_PROTECTION };
  int i;
  // Only the ones the game handles: an unhandled error keeps the OS's own way (print and halt)
  for(i = 0; i < (int)(sizeof(errors) / sizeof(errors[0])); i++) {
    OSErrorHandler prev = OSSetErrorHandler(errors[i], (OSErrorHandler)gc_ap_diag_error);
    if(prev == NULL) {
      OSSetErrorHandler(errors[i], NULL);
    }
    gc_ap_diag_prev[errors[i]] = prev;
  }
  d->anchor_text = (u32)gc_ap_diag_frame;
  d->anchor_data = (u32)gc_ap_diag_trap;
  d->anchor_bss = (u32)&gc_ap_mbox_mem;
  gc_ap_diag_alarm_tick = gc_ap_mbox.game_tick;
  OSCreateAlarm(&gc_ap_diag_alarm);
  OSSetAlarm(&gc_ap_diag_alarm, OSSecondsToTicks((OSTime)1), gc_ap_diag_watchdog);
  gc_ap_diag_log("diagnostics on (mailbox v%d)", GC_AP_MAILBOX_VERSION);
}

// Self-test, written by a harness: 1 = crash (a trap instruction: a program exception on Dolphin and the Wii
// alike), 2 = hang until it changes
static void gc_ap_diag_test(gc_ap_diag* d) {
  volatile u32* test = &d->test;
  if(*test == 1) {
    *test = 0;
    gc_ap_diag_log("self-test: crash");
    DCStoreRange(gc_ap_diag_trap, sizeof(gc_ap_diag_trap));
    ICInvalidateRange(gc_ap_diag_trap, sizeof(gc_ap_diag_trap));
    ((void (*)(void))gc_ap_diag_trap)();
  } else if(*test == 2) {
    gc_ap_diag_log("self-test: hang");
    while(*test == 2) {
    }
    gc_ap_diag_log("self-test: hang over");
  }
}

static u32 gc_ap_diag_pack_pad(const pad_t* p) {
  return ((u32)p->now.button << 16) | ((u32)(u8)p->now.stick_x << 8) | (u8)p->now.stick_y;
}

void gc_ap_diag_pads(const void* pads, int count) {
  OSTime start = OSGetTime();
  const pad_t* pad = (const pad_t*)pads;
  gc_ap_diag* d = &gc_ap_mbox.diag;
  int i;
  if(count > 4) {
    count = 4;
  }
  for(i = 0; i < 4; i++) {
    d->pad[i] = i < count ? gc_ap_diag_pack_pad(&pad[i]) : 0;
  }
  d->pads = (u32)count;
  gc_ap_diag_cost += OSGetTime() - start;
}

void gc_ap_diag_frame(void) {
  OSTime start = OSGetTime();
  gc_ap_diag* d = &gc_ap_mbox.diag;
  gc_ap_diag_seen* last = &gc_ap_diag_last;
  const lbRTC_time_c* rtc = &Common_Get(time.rtc_time);
  u32 scene, game, paused, hold, reload, weather, save_weather, date, time;

  if(!gc_ap_diag_started) {
    gc_ap_diag_started = 1;
    memset(last, 0xFF, sizeof(*last));
    gc_ap_diag_start();
    start = OSGetTime(); // the one-time setup isn't a frame's cost
  }
  // Last frame's cost (pads and play ran after this function)
  d->cost_us = (u32)OSTicksToMicroseconds(gc_ap_diag_cost);
  if(d->cost_us > d->cost_max_us) {
    d->cost_max_us = d->cost_us;
  }
  d->cost_sum_us += d->cost_us;
  gc_ap_diag_cost = 0;

  scene = (u32)Save_Get(scene_no);
  game = gamePT != NULL ? (u32)gamePT->exec : 0;
  paused = (u32)g_pc_paused;
  hold = (u32)gc_ap_pause_held();
  reload = (u32)pc_ap_reload_step();
  weather = ((u32)(u16)Common_Get(weather) << 16) | (u16)Common_Get(weather_intensity);
  save_weather = Save_Get(weather);
  date = ((u32)rtc->year << 16) | ((u32)rtc->month << 8) | rtc->day;
  time = ((u32)rtc->hour << 16) | ((u32)rtc->min << 8) | rtc->sec;
  d->scene = scene;
  d->game = game;
  d->paused = paused;
  d->hold = hold;
  d->reload = reload;
  d->weather = weather;
  d->save_weather = save_weather;
  d->date = date;
  d->time = time;
  d->heap = gamePT != NULL ? (u32)game_getFreeBytes(gamePT) : 0;

  if(scene != last->scene) {
    gc_ap_diag_log("scene %u", scene);
  }
  if(game != last->game) {
    gc_ap_diag_log("game state %08x", game);
  }
  if(paused != last->paused) {
    gc_ap_diag_log(paused ? "pause menu open" : "pause menu closed");
  }
  if(hold) {
    // Buttons down when the pause menu closed, hidden from the game until let go (a resting trigger stays here)
    if(gc_ap_diag_hold_frames % GC_AP_DIAG_HOLD_REPEAT == 0) {
      gc_ap_diag_log("buttons %04x hidden since the pause menu (%u frames): pads %08x %08x %08x %08x", hold,
                     gc_ap_diag_hold_frames, d->pad[0], d->pad[1], d->pad[2], d->pad[3]);
    }
    gc_ap_diag_hold_frames++;
  } else if(gc_ap_diag_hold_frames != 0) {
    gc_ap_diag_log("hidden buttons let go after %u frames", gc_ap_diag_hold_frames);
    gc_ap_diag_hold_frames = 0;
  }
  if(reload != last->reload) {
    gc_ap_diag_log("reload step %u", reload);
  }
  if(weather != last->weather || save_weather != last->save_weather) {
    gc_ap_diag_log("weather %u/%u (save %02x)", weather >> 16, weather & 0xFFFF, save_weather);
  }
  if(date != last->date) {
    gc_ap_diag_log("date %04u-%02u-%02u %02u:%02u", date >> 16, (date >> 8) & 0xFF, date & 0xFF, time >> 16,
                   (time >> 8) & 0xFF);
  }
  if(d->pads != last->pads) {
    gc_ap_diag_log("controllers read: %u", d->pads);
  }
  last->scene = scene;
  last->game = game;
  last->paused = paused;
  last->reload = reload;
  last->weather = weather;
  last->save_weather = save_weather;
  last->date = date;
  last->pads = d->pads;

  gc_ap_diag_cost += OSGetTime() - start;
  gc_ap_diag_test(d); // not counted: a hang test would be the max forever
}

void gc_ap_diag_play(struct game_play_s* play_) {
  OSTime start = OSGetTime();
  GAME_PLAY* play = (GAME_PLAY*)play_;
  gc_ap_diag* d = &gc_ap_mbox.diag;
  gc_ap_diag_seen* last = &gc_ap_diag_last;
  PLAYER_ACTOR* player = GET_PLAYER_ACTOR(play);
  s32 action = -1;
  u32 block = 0xFFFF;

  d->play_tick++;
  if(player != NULL) {
    ACTOR* actor = (ACTOR*)player;
    int bx, bz;
    action = player->now_main_index;
    d->pos[0] = (s32)actor->world.position.x;
    d->pos[1] = (s32)actor->world.position.y;
    d->pos[2] = (s32)actor->world.position.z;
    if(mFI_Wpos2BlockNum(&bx, &bz, actor->world.position)) {
      block = ((u32)(u8)bx << 8) | (u8)bz;
    }
  }
  d->action = action;
  d->block = block;
  d->submenu = (u32)play->submenu.process_status;
  d->demo = (u32)mDemo_CheckDemo();
  d->wipe = play->fb_wipe_mode;

  if(block != last->block) {
    gc_ap_diag_log("acre %u,%u at %d,%d", block >> 8, block & 0xFF, d->pos[0], d->pos[2]);
  }
  if(action != last->action) {
    gc_ap_diag_log("player action %d", action);
  }
  if(d->submenu != last->submenu) {
    gc_ap_diag_log("submenu %u", d->submenu);
  }
  if(d->demo != last->demo) {
    gc_ap_diag_log("demo %u", d->demo);
  }
  if(d->wipe != last->wipe) {
    gc_ap_diag_log("wipe %u", d->wipe);
  }
  last->block = block;
  last->action = action;
  last->submenu = d->submenu;
  last->demo = d->demo;
  last->wipe = d->wipe;
  gc_ap_diag_cost += OSGetTime() - start;
}
