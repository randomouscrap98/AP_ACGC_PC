// GC backend of ap_archipelago.h: the same functions the PC game code calls in ap_client.dll, answered from the
// RAM mailbox (gc_ap_mailbox.h) that the PC client fills. Phase 2 probe: only what the probe module uses.
#include "gc_ap_mailbox.h"
#include "ap_archipelago.h"

#include <string.h>

gc_ap_mailbox gc_ap_mbox;

static ap_connectstate gc_ap_cs;
static u32 gc_ap_last_client_tick;
static u32 gc_ap_client_quiet; // frames since client_tick last changed

void gc_ap_mailbox_frame(void) {
  gc_ap_mailbox* m = &gc_ap_mbox;
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
    m->slotdata_size = sizeof(ap_slotdata);
    m->tail = GC_AP_TAIL;
    m->magic[1] = GC_AP_MAGIC1;
    m->magic[0] = GC_AP_MAGIC0;
  }
  m->game_tick++;
  if(m->client_tick != gc_ap_last_client_tick) {
    gc_ap_last_client_tick = m->client_tick;
    gc_ap_client_quiet = 0;
  } else if(gc_ap_client_quiet < GC_AP_CLIENT_TIMEOUT_FRAMES) {
    gc_ap_client_quiet++;
  }
}

static int gc_ap_client_alive(void) {
  return gc_ap_mbox.client_tick != 0 && gc_ap_client_quiet < GC_AP_CLIENT_TIMEOUT_FRAMES;
}

const ap_connectstate* ap_getconnectstate(void) {
  gc_ap_cs.state = gc_ap_client_alive() ? gc_ap_mbox.client_state : AP_CSTATE_CONNECTING;
  return &gc_ap_cs;
}

static u32 gc_ap_item_total(void) {
  u32 n = gc_ap_mbox.item_count;
  return n > GC_AP_ITEM_MAX ? GC_AP_ITEM_MAX : n;
}

size_t ap_getitemcount(void) {
  return gc_ap_item_total();
}

int64_t ap_getitem(size_t idx) {
  return idx < gc_ap_item_total() ? (int64_t)gc_ap_mbox.items[idx].id : -1;
}

int ap_getitem_sender(size_t idx) {
  return idx < gc_ap_item_total() ? gc_ap_mbox.items[idx].sender : -1;
}

unsigned ap_getitem_flags(size_t idx) {
  return idx < gc_ap_item_total() ? gc_ap_mbox.items[idx].flags : 0;
}

int ap_item_count(int64_t id) {
  u32 n = gc_ap_item_total();
  u32 i;
  int count = 0;
  for(i = 0; i < n; i++) {
    if(gc_ap_mbox.items[i].id == (u32)id) {
      count++;
    }
  }
  return count;
}

int ap_location_checked(int64_t id) {
  u32 n = gc_ap_mbox.checked_count;
  u32 i;
  if(n > GC_AP_CHECKED_MAX) {
    n = GC_AP_CHECKED_MAX;
  }
  for(i = 0; i < n; i++) {
    if(gc_ap_mbox.checked[i] == (u32)id) {
      return 1;
    }
  }
  return 0;
}

// The checked list only grows, so its length works as the generation
unsigned ap_checked_generation(void) {
  return gc_ap_mbox.checked_count;
}

// Like the DLL, repeats are dropped: already checked, or still waiting in the ring. A full ring drops the id;
// game code sends its checks again every tick until they show up as checked.
void ap_send_location(int64_t id) {
  gc_ap_mailbox* m = &gc_ap_mbox;
  u32 i;
  if(ap_location_checked(id)) {
    return;
  }
  for(i = m->send_tail; i != m->send_head; i++) {
    if(m->send[i % GC_AP_SEND_MAX] == (u32)id) {
      return;
    }
  }
  if(m->send_head - m->send_tail >= GC_AP_SEND_MAX) {
    return;
  }
  m->send[m->send_head % GC_AP_SEND_MAX] = (u32)id;
  m->send_head++;
}

void ap_send_goal(void) {
  gc_ap_mbox.goal = 1;
}
