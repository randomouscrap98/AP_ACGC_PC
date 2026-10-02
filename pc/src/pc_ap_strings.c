#include "pc_ap_strings.h"
#include "ap_slotdata.h"
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
