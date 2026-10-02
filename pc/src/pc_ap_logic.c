#include "pc_ap_logic.h"
#include "pc_ap_state.h"
#include "ap_archipelago.h"
#include "ap_slotdata.h"
#include "pc_menu_util.h"
#include "m_font.h" // CHAR_SPACE, mem_clear
#include "m_common_data.h"
#include "m_house.h"

#include <stdio.h>
#include <string.h>

// WARN: keep in sync with apworld items.py!
#define PC_AP_ITEM_PROGRESSIVE_HOUSE  0x10000

int pc_ap_start_allowed(void) {
  return ap_roomplayer_valid(&ap_getconnectstate()->roomplayer);
}

int pc_ap_loan_amount(int size) {
  if(size < 0 || size >= AP_LOAN_NUM) {
    return 0;
  }
  ap_slotdata * sd = ap_getslotdata();
  return sd->loans[size];
}

void pc_ap_start_loan(void) {
  Now_Private->inventory.loan = pc_ap_loan_amount(0);
  pc_ap_state_get()->loan_started = 1;
}

int pc_ap_pay_allowed(void) {
  return pc_ap_state_get()->loan_started;
}

int pc_ap_houses_received() {
  int count = ap_getitemcount();
  int houses = 0;
  for(int i = 0; i < count; i++) {
    int64_t item = ap_getitem(i);
    if(item == PC_AP_ITEM_PROGRESSIVE_HOUSE) {
      houses++;
    }
  }
  return houses;
}

int pc_ap_stage_from(int size, int has_basement) {
  switch(size) {
    case mHm_HOMESIZE_SMALL:  return 0;
    case mHm_HOMESIZE_MEDIUM: return has_basement ? 2 : 1;
    case mHm_HOMESIZE_LARGE:  return 3;
    default:                  return 4; // UPPER, STATUE
  }
}

int pc_ap_loans_paid_from(int stage, u32 loan, int renew) {
  // Upgrade k is only offered once loan k-1 is paid, so the first `stage`
  // loans are always paid. renew = just built, Nook hasn't set the new loan
  // yet (it's 0 then), so the current one doesn't count as paid.
  if(loan == 0 && !renew) {
    return stage + 1;
  }
  return stage;
}

// The current player's house, or NULL (title screen, visiting another town)
static mHm_hs_c* pc_ap_my_home(void) {
  if(Now_Private == NULL || Common_Get(player_no) >= mPr_FOREIGNER) {
    return NULL;
  }
  return Save_GetPointer(homes[mHS_get_arrange_idx(Common_Get(player_no))]);
}

int pc_ap_house_stage(void) {
  mHm_hs_c* home = pc_ap_my_home();
  if(home == NULL) {
    return 0;
  }
  return pc_ap_stage_from(home->size_info.size, home->flags.has_basement);
}

int pc_ap_loans_paid(void) {
  mHm_hs_c* home = pc_ap_my_home();
  if(home == NULL) {
    return 0;
  }
  return pc_ap_loans_paid_from(pc_ap_house_stage(), Now_Private->inventory.loan,
      home->size_info.renew);
}

int pc_ap_house_offer_allowed(void) {
  return pc_ap_houses_received() > pc_ap_house_stage();
}

void pc_ap_name_to_game(u8* dst, int dst_len, const char* src) {
  char tmp[16];
  int len;

  snprintf(tmp, sizeof(tmp), "%s", src);
  pc_menu_fixtext(tmp);
  len = (int)strlen(tmp);
  if (len > dst_len) { len = dst_len; }
  mem_clear(dst, dst_len, CHAR_SPACE);
  memcpy(dst, tmp, len);
}

static int pc_find_bytes(const u8* buf, int len, const char* needle) {
  int n = (int)strlen(needle);
  for(int i = 0; i <= len - n; i++) {
    if(memcmp(buf + i, needle, n) == 0) {
      return i;
    }
  }
  return -1;
}

// Replace the FIRST instance of needle with replace, shifting all text over and
// returning the new length (not null terminated)
static int pc_replace_msg_text(u8* buf, int len, const char * needle, const char * replace) {
  int nlen = strlen(needle);
  int rlen = strlen(replace);
  int shift = rlen - nlen;
  int idx = pc_find_bytes(buf, len, needle);
  if(idx >= 0) { // idk, it's weird if you don't find it but ehg?
    int afterneedle = idx + nlen;
    if (len + shift > mMsg_MSG_BUF_MAX) return len; // If it won't fit, do nothing
    memmove(buf + afterneedle + shift, buf + afterneedle, len - afterneedle);
    memcpy(buf + idx, replace, rlen);
    len += shift;
  }
  return len;
}

static void pc_comma_number(char * out, size_t maxsize, int number) {
  // This is silly but I am a human and lol
  int baseline = number % 1000;
  int thousands = (number / 1000) % 1000;
  int millions = (number / 1000000) % 1000;
  int billions = number / 1000000000;
  if(billions) {
    snprintf(out, maxsize, "%d,%03d,%03d,%03d", billions, abs(millions), abs(thousands), abs(baseline));
  } else if(millions) {
    snprintf(out, maxsize, "%d,%03d,%03d", millions, abs(thousands), abs(baseline));
  } else if(thousands) {
    snprintf(out, maxsize, "%d,%03d", thousands, abs(baseline));
  } else {
    snprintf(out, maxsize, "%d", baseline);
  }
}

// Generic pc message patch, which will patch ANY message which matches
// one of the patches.
u32 pc_ap_msg_patch(int index, mMsg_Data_c* msg_data, u32 size) {
  ap_slotdata * sd = ap_getslotdata();
  char needle[64];
  char replace[64];
  needle[0] = 0;
  replace[0] = 0;
  if(index == 2078) { // 19,800, so loan 0 + 2400
    strcpy(needle, "19,800");
    pc_comma_number(replace, sizeof(replace), sd->loans[0] + 2400);
  } else if(index == 2107) {
    strcpy(needle, "17,400");
    pc_comma_number(replace, sizeof(replace), sd->loans[0]);
  } else if(index == 4222) {
    strcpy(needle, "148,000");
    pc_comma_number(replace, sizeof(replace), sd->loans[1]);
  } else if(index == 4226) {
    strcpy(needle, "398,000");
    pc_comma_number(replace, sizeof(replace), sd->loans[3]);
  } else if(index == 11954) {
    strcpy(needle, "49,800");
    pc_comma_number(replace, sizeof(replace), sd->loans[2]);
  } else if(index == 11963) {
    strcpy(needle, "798,000");
    pc_comma_number(replace, sizeof(replace), sd->loans[4]);
  }
  if(needle[0]) { // there is a replacement
    size = pc_replace_msg_text(msg_data->text_buf.data, size, needle, replace);
  }

  return size;
}
