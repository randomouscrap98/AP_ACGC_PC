#include "pc_ap_beach_hint.h"
#include "pc_ap_state.h"

#include "m_font.h"
#include "pc_menu_util.h"

#include <stdio.h>
#include <string.h>

#define PC_AP_BEACH_HINT_SECTION "beach_hint"
#define PC_AP_BEACH_HINT_KEY_DELIVERED "delivered"
#define PC_AP_BEACH_HINT_KEY_PENDING "pending"

void pc_ap_beach_hint_init(pc_ap_beach_hint* h, const ap_slotdata* sd) {
  memset(h, 0, sizeof(*h));
  h->count = sd->valid ? sd->beach_hint_count : 0;
  h->letter = sd->valid ? sd->beach_hint_letter : 0;
  h->pending = -1;
}

void pc_ap_beach_hint_load(pc_ap_beach_hint* h, struct ini_t* ini) {
  h->delivered = pc_ap_ini_get_int(ini, PC_AP_BEACH_HINT_SECTION, PC_AP_BEACH_HINT_KEY_DELIVERED, 0);
  h->pending = pc_ap_ini_get_int(ini, PC_AP_BEACH_HINT_SECTION, PC_AP_BEACH_HINT_KEY_PENDING, -1);
  h->wait = PC_AP_BEACH_HINT_WAIT_FRAMES; // an earlier session's request never blocks a pickup
}

void pc_ap_beach_hint_save(const pc_ap_beach_hint* h, struct ini_t* ini) {
  pc_ap_ini_set_int(ini, PC_AP_BEACH_HINT_SECTION, PC_AP_BEACH_HINT_KEY_DELIVERED, h->delivered);
  pc_ap_ini_set_int(ini, PC_AP_BEACH_HINT_SECTION, PC_AP_BEACH_HINT_KEY_PENDING, h->pending);
}

int pc_ap_beach_hint_spawn(const pc_ap_beach_hint* h, int roll) {
  return h->count - h->delivered > 0 && roll < PC_AP_BEACH_HINT_PERCENT;
}

void pc_ap_beach_hint_name(char* out, int len) {
  int n = snprintf(out, len + 1, "AP Hint");
  for(; n < len; n++) {
    out[n] = ' ';
  }
  out[len] = 0;
}

int pc_ap_beach_hint_waiting(const pc_ap_beach_hint* h) {
  return h->pending >= 0 && h->wait < PC_AP_BEACH_HINT_WAIT_FRAMES;
}

// 1 if id is in ids[0..n)
static int pc_ap_beach_hint_in(const int64_t* ids, int n, int64_t id) {
  for(int i = 0; i < n; i++) {
    if(ids[i] == id) {
      return 1;
    }
  }
  return 0;
}

int pc_ap_beach_hint_eligible(const int64_t* missing, int n, const int64_t* hinted, int hinted_n,
                              int64_t* out, int max) {
  int found = 0;
  for(int i = 0; i < n; i++) {
    if(pc_ap_beach_hint_in(hinted, hinted_n, missing[i])) {
      continue;
    }
    if(out != NULL && found < max) {
      out[found] = missing[i];
    }
    found++;
  }
  return found;
}

int64_t pc_ap_beach_hint_pick(const int64_t* ids, int n, int roll) {
  if(n <= 0 || ids == NULL) {
    return -1;
  }
  if(roll < 0) {
    roll = 0;
  }
  return ids[roll % n];
}

int pc_ap_beach_hint_text_width(const char* ascii) {
  char text[256];
  snprintf(text, sizeof(text), "%s", ascii == NULL ? "" : ascii);
  pc_menu_fixtext(text);
  return mFont_GetStringWidth((u8*)text, (int)strlen(text), TRUE);
}

void pc_ap_beach_hint_fit(const char* name, int max_px, pc_ap_beach_hint_width_fn width, char* out, int size) {
  char cut[256];
  int full, len;
  if(size <= 0) {
    return;
  }
  if(name == NULL) {
    name = "";
  }
  full = (int)strlen(name);
  len = full < size - 1 ? full : size - 1;
  memcpy(out, name, (size_t)len);
  out[len] = 0;
  if(len == full && width(out) <= max_px) {
    return;
  }
  while(len > 0 && out[len - 1] == ' ') {
    out[--len] = 0;
  }
  // Longest prefix whose "..." still fits
  while(len > 0) {
    snprintf(cut, sizeof(cut), "%.*s...", len, out);
    if(width(cut) <= max_px) {
      snprintf(out, (size_t)size, "%s", cut);
      return;
    }
    len--;
    while(len > 0 && out[len - 1] == ' ') {
      len--;
    }
  }
  snprintf(out, (size_t)size, "...");
}

void pc_ap_beach_hint_body(const char* player, const char* item, const char* location, char* out, int size) {
  char who_fit[64];
  char item_fit[64];
  char loc_fit[64];
  // The name is cut to leave room for "'s", which always stays
  int who_px = PC_AP_BEACH_HINT_LINE_PX - pc_ap_beach_hint_text_width("'s");
  pc_ap_beach_hint_fit(player, who_px, pc_ap_beach_hint_text_width, who_fit, sizeof(who_fit) - 2);
  strcat(who_fit, "'s");
  pc_ap_beach_hint_fit(item, PC_AP_BEACH_HINT_LINE_PX, pc_ap_beach_hint_text_width, item_fit, sizeof(item_fit));
  pc_ap_beach_hint_fit(location, PC_AP_BEACH_HINT_LINE_PX, pc_ap_beach_hint_text_width, loc_fit, sizeof(loc_fit));
  snprintf(out, (size_t)size, "%s\n%s\ncan be found at:\n%s", who_fit, item_fit, loc_fit);
}
