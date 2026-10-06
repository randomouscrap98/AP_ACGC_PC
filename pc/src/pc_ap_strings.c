#include "pc_ap_strings.h"
#include "pc_ap_logic.h"
#include "pc_menu_util.h"
#include "m_font.h" // CHAR_SPACE, CHAR_NEW_LINE, mem_clear

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void pc_ap_text_to_game(u8* dst, int dst_len, const char* src) {
  mem_clear(dst, dst_len, CHAR_SPACE);
  for(int i = 0; i < dst_len && src[i] != 0; i++) {
    if(src[i] == '\n') {
      dst[i] = CHAR_NEW_LINE;
    } else {
      char c[2] = { src[i], 0 };
      pc_menu_fixtext(c);
      dst[i] = (u8)c[0];
    }
  }
}

// Index of the first needle (n bytes) at or after from, -1 if missing
static int pc_find_bytes(const u8* buf, int len, int from, const void* needle, int n) {
  for(int i = from; i <= len - n; i++) {
    if(memcmp(buf + i, needle, n) == 0) {
      return i;
    }
  }
  return -1;
}

// Remove n bytes at index at, returns the new length
static int pc_msg_remove(u8* buf, int len, int at, int n) {
  memmove(buf + at, buf + at + n, len - at - n);
  return len - n;
}

// Replace the FIRST instance of needle with replace, shifting all text over and
// returning the new length (not null terminated)
static int pc_replace_msg_text(u8* buf, int len, const char * needle, const char * replace) {
  int nlen = strlen(needle);
  int rlen = strlen(replace);
  int shift = rlen - nlen;
  int idx = pc_find_bytes(buf, len, 0, needle, nlen);
  if(idx >= 0) { // idk, it's weird if you don't find it but ehg?
    int afterneedle = idx + nlen;
    if (len + shift > mMsg_MSG_BUF_MAX) return len; // If it won't fit, do nothing
    memmove(buf + afterneedle + shift, buf + afterneedle, len - afterneedle);
    memcpy(buf + idx, replace, rlen);
    len += shift;
  }
  return len;
}

void pc_comma_number(char * out, size_t maxsize, int number) {
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

// Message control codes are 0x7F + code (m_font.h mFont_CONT_CODE_*), message and choice
// numbers in them are 2 bytes big endian.
#define PC_MSG_NEXT_MESSAGE_2   0x11
#define PC_MSG_NEXT_MESSAGE_3   0x12
#define PC_MSG_SELECT_STRING_3  0x17
#define PC_MSG_SELECT_STRING_4  0x18
#define PC_MSG_CLEAR            0x02

// Continue villager's "These are the only other things you can do" (one message per villager
// look): timesanity drops the "Set clock" choice (choice 2). Its SET_NEXT_MESSAGE_2 goes too, so
// the old choice 3 ("Never mind") becomes choice 2; ac_npc_p_sel2_talk.c_inc treats it that way.
static int pc_ap_msg_drop_set_clock(u8* buf, int len) {
  // Demolish a house (114), Build a new town (115), Set clock (113), then "never mind" (varies)
  const u8 sel4[] = { 0x7F, PC_MSG_SELECT_STRING_4, 0x00, 0x72, 0x00, 0x73, 0x00, 0x71 };
  const u8 next2[] = { 0x7F, PC_MSG_NEXT_MESSAGE_2 };
  int sel = pc_find_bytes(buf, len, 0, sel4, sizeof(sel4));
  if(sel < 0) {
    return len;
  }
  int next = pc_find_bytes(buf, len, sel, next2, sizeof(next2));
  if(next < 0 || next + 6 > len || buf[next + 4] != 0x7F || buf[next + 5] != PC_MSG_NEXT_MESSAGE_3) {
    return len; // not the layout we know, leave it alone
  }
  // Order matters: next is after sel, so remove it first
  len = pc_msg_remove(buf, len, next, 4); // SET_NEXT_MESSAGE_2 + its message
  buf[next + 1] = PC_MSG_NEXT_MESSAGE_2;  // old SET_NEXT_MESSAGE_3
  len = pc_msg_remove(buf, len, sel + 6, 2); // the Set clock choice
  buf[sel + 1] = PC_MSG_SELECT_STRING_3;
  return len;
}

// Rover on the train asks if the clock is right (10950), then thanks you (10951 page 1) before
// asking to sit (10951 page 2). With timesanity he starts on 10951 (ac_npc_guide_move.c_inc):
// drop the thanks page and open with "Excuse me..." instead of "So,".
static int pc_ap_msg_rover_no_clock(u8* buf, int len) {
  const u8 clear[] = { 0x7F, PC_MSG_CLEAR };
  int page2 = pc_find_bytes(buf, len, 0, clear, sizeof(clear));
  if(page2 < 0) {
    return len;
  }
  len = pc_msg_remove(buf, len, 0, page2 + sizeof(clear));
  return pc_replace_msg_text(buf, len, "So,", "Excuse me...");
}

// Generic pc message patch, which will patch ANY message which matches
// one of the patches.
u32 pc_ap_msg_patch(int index, mMsg_Data_c* msg_data, u32 size) {
  char needle[64];
  char replace[64];
  needle[0] = 0;
  replace[0] = 0;
  if(index == 2078) { // 19,800, so loan 0 + 2400
    strcpy(needle, "19,800");
    pc_comma_number(replace, sizeof(replace), pc_ap_loan_amount(0) + 2400);
  } else if(index == 2107) {
    strcpy(needle, "17,400");
    pc_comma_number(replace, sizeof(replace), pc_ap_loan_amount(0));
  } else if(index == 4222) {
    strcpy(needle, "148,000");
    pc_comma_number(replace, sizeof(replace), pc_ap_loan_amount(1));
  } else if(index == 4226) {
    strcpy(needle, "398,000");
    pc_comma_number(replace, sizeof(replace), pc_ap_loan_amount(3));
  } else if(index == 11954) {
    strcpy(needle, "49,800");
    pc_comma_number(replace, sizeof(replace), pc_ap_loan_amount(2));
  } else if(index == 11963) {
    strcpy(needle, "798,000");
    pc_comma_number(replace, sizeof(replace), pc_ap_loan_amount(4));
  }
  if(needle[0]) { // there is a replacement
    size = pc_replace_msg_text(msg_data->text_buf.data, size, needle, replace);
  }

  if(pc_ap_time_frozen()) {
    // Continue villager, "other things" menu: 5138 for villager look 0, +40 per look (6 looks)
    if(index >= 5138 && index <= 5338 && (index - 5138) % 40 == 0) {
      size = pc_ap_msg_drop_set_clock(msg_data->text_buf.data, size);
    } else if(index == 10951) {
      size = pc_ap_msg_rover_no_clock(msg_data->text_buf.data, size);
    }
  }

  return size;
}
