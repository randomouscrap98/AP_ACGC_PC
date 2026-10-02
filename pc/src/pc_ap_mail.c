#include "pc_ap_mail.h"
#include "pc_ap_logic.h"
#include "pc_menu_util.h"
#include "m_common_data.h"
#include "m_mail.h"
#include "m_font.h"
#include "m_private.h"

#include <string.h>

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

// Same fields as the game's own letters (mMl_get_mail_to_player_com, m_mail.c),
// with our text instead of a ROM handbill
int pc_ap_send_letter(const char* body, mActor_name_t present) {
  mHm_hs_c* home = pc_ap_my_home();
  if(home == NULL) {
    return 0;
  }
  int slot = mMl_chk_mail_free_space(home->mailbox, HOME_MAILBOX_SIZE);
  if(slot == -1) {
    return 0;
  }

  Mail_c mail;
  u8 sender[PLAYER_NAME_LEN];
  mMl_clear_mail(&mail);

  // "Dear <name>,": the name goes in at header_back_start
  pc_ap_text_to_game(mail.content.header, MAIL_HEADER_LEN, "Dear ,");
  mail.content.header_back_start = 5;
  pc_ap_text_to_game(mail.content.body, MAIL_BODY_LEN, body);
  pc_ap_text_to_game(mail.content.footer, MAIL_FOOTER_LEN, PC_AP_MAIL_FOOTER);
  mail.content.font = mMl_FONT_RECV;
  mail.content.mail_type = mMl_TYPE_MAIL;
  mail.content.paper_type = PC_AP_MAIL_PAPER;

  mPr_CopyPersonalID(&mail.header.recipient.personalID, &Now_Private->player_ID);
  mail.header.recipient.type = mMl_NAME_TYPE_PLAYER;
  pc_ap_text_to_game(sender, PLAYER_NAME_LEN, PC_AP_MAIL_SENDER);
  mPr_ClearPersonalID(&mail.header.sender.personalID);
  mPr_CopyPlayerName(mail.header.sender.personalID.player_name, sender);
  mail.header.sender.type = mMl_NAME_TYPE_MUSEUM; // non-villager sender, like the bank's letters
  mail.present = present;

  mMl_copy_mail(&home->mailbox[slot], &mail);
  return 1;
}
