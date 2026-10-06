#ifndef AP_SLOTDATA_H
#define AP_SLOTDATA_H

#include "ap_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AP_LOAN_NUM     5 // starting, medium, basement, large, upper (upgrade order)
#define AP_BELLCREDIT_NUM 3 // small, modest, large
#define AP_NPC_NUM      236 // NPC_NUM in the game (villager npc indices)

#define AP_GOAL_STATUE  (1 << 0)

typedef struct {
  // Base data (not necessarily set)
  char player_name[9];
  char town_name[9];
  int house;
  int gender;
  int face;
  int starting_shirt; // ITM_CLOTH000 + n; 16 (or missing) keeps the game's pick
  int town_fruit;
  int grass_shape;    // mFM_BG_TEX_* index
  int train_station;  // station_type (0-14), -1 if unset
  int town_day;       // day in July, never 4
  int letter_paper;   // stationery for AP letters (paper_type 0-63)
  char letter_sender[33]; // signature (footer) of AP letters, max MAIL_FOOTER_LEN
  char loan_letter_text[193]; // body of the "loan ready for payoff" letter, max MAIL_BODY_LEN, '\n' = line break
  unsigned char villager_blacklist[AP_NPC_NUM]; // 1 = this npc index never moves in
  unsigned char starting_villagers[AP_NPC_NUM]; // 1 = starts in a new town (pool; blacklist wins)
  // QOL
  int skip_intro;
  int no_cockroaches;
  int shops_always_open;
  int no_weeds;
  int normalized_time_travel; // date changes count as one day passing
  // Goal / checks
  int goal;                          // AP_GOAL_* bits; all set goals are required
  int loansanity;                    // nonzero: loan checks + Progressive House gates upgrades
  int loans[AP_LOAN_NUM];            // loan amounts in bells
  int loan_checks[AP_LOAN_NUM];      // checks sent when each loan is paid off
  int favorsanity;                   // number of favor checks
  // Timesanity
  int timesanity;                    // nonzero: frozen clock, months and time slots are items
  int starting_month;                // 0-11
  int starting_time;                 // 0-3: Morning 4-8, Day 9-15, Evening 16-20, Night 21-3
  int start_year;                    // year of the frozen clock (seed generation year, 2001-2100)
  int bell_credits[AP_BELLCREDIT_NUM]; // bells per small/modest/large bell credit
  char world_version[16];            // apworld version the seed was generated with ("" if missing)
  // Whether the struct has valid data
  int valid;
} ap_slotdata;

AP_API ap_slotdata * ap_getslotdata(void);
AP_API void ap_slotdata_init(ap_slotdata *);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <nlohmann/json_fwd.hpp>
void ap_slotdata_fill(ap_slotdata * sd, const nlohmann::json& slot_data);
#endif

#endif
