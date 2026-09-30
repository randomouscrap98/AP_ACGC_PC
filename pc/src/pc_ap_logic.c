#include "pc_ap_logic.h"
#include "ap_archipelago.h"
#include "pc_menu_util.h"
#include "m_font.h" // CHAR_SPACE, mem_clear

#include <stdio.h>
#include <string.h>

int pc_ap_start_allowed(void) {
  return ap_roomplayer_valid(&ap_getconnectstate()->roomplayer);
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
