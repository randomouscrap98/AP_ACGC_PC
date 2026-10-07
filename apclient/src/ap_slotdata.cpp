#include "ap_slotdata.h"
#include "ap_log.h"

#include <apclient.hpp>

static ap_slotdata g_slotdata;

ap_slotdata * ap_getslotdata(void) {
  return &g_slotdata;
}

void ap_slotdata_init(ap_slotdata * sd) {
  memset(sd, 0, sizeof(ap_slotdata));
}

// Fills out[0..n) from a json int array; missing key or bad entries keep the defaults
static void read_int_array(const nlohmann::json& slot_data, const char * key,
    int * out, const int * defaults, int n) {
  for (int i = 0; i < n; i++) { out[i] = defaults[i]; }
  auto it = slot_data.find(key);
  if (it == slot_data.end() || !it->is_array()) { return; }
  for (int i = 0; i < n && i < (int)it->size(); i++) {
    if ((*it)[i].is_number_integer()) { out[i] = (*it)[i].get<int>(); }
  }
}

// npc indices from the apworld; bad entries are skipped (apworld/client mismatch)
static void read_npc_list(const nlohmann::json& slot_data, const char * key, unsigned char * out) {
  memset(out, 0, AP_NPC_NUM);
  auto it = slot_data.find(key);
  if (it == slot_data.end() || !it->is_array()) { return; }
  for (auto& v : *it) {
    if (v.is_number_integer() && v.get<int>() >= 0 && v.get<int>() < AP_NPC_NUM) {
      out[v.get<int>()] = 1;
    } else {
      APLOG_WARN("%s: ignoring bad entry %s", key, v.dump().c_str());
    }
  }
}

static int read_goal(const nlohmann::json& slot_data) {
  auto it = slot_data.find("goal");
  if (it == slot_data.end() || !it->is_array()) { return AP_GOAL_STATUE; }
  int goal = 0;
  for (auto& g : *it) {
    if (g.is_string() && g.get<std::string>() == "Statue") { goal |= AP_GOAL_STATUE; }
    if (g.is_string() && g.get<std::string>() == "Museum") { goal |= AP_GOAL_MUSEUM; }
  }
  return goal;
}

// WARN: keep offline_example.json (next to this dir's CMakeLists.txt) in sync with the
// keys that matter offline (new town, intro, QoL, loan amounts)
void ap_slotdata_fill(ap_slotdata * sd, const nlohmann::json& slot_data) { 
  snprintf(sd->town_name, sizeof(sd->town_name), "%s",
      slot_data.value("town_name", "pelago").c_str());
  snprintf(sd->player_name, sizeof(sd->player_name), "%s",
      slot_data.value("player_name", "archi").c_str());
  sd->skip_intro = slot_data.value("skip_intro", 0) % 3;
  sd->gender = slot_data.value("gender", 0) % 2;
  sd->face = slot_data.value("face", 0) % 8;
  sd->starting_shirt = slot_data.value("starting_shirt", 16);
  sd->house = slot_data.value("house", 0) % 4;
  sd->town_fruit = slot_data.value("town_fruit", 0) % 5;
  sd->grass_shape = slot_data.value("grass_shape", 0) % 3;
  sd->train_station = slot_data.value("train_station", -1);
  sd->town_day = slot_data.value("town_day", 0) % 32;
  sd->letter_paper = slot_data.value("letter_paper", 0) % 64;
  snprintf(sd->letter_sender, sizeof(sd->letter_sender), "%s",
      slot_data.value("letter_sender", "Archipelago").c_str());
  snprintf(sd->loan_letter_text, sizeof(sd->loan_letter_text), "%s",
      slot_data.value("loan_letter_text", "Your loan is ready for\npayoff at the post office!").c_str());
  read_npc_list(slot_data, "villager_blacklist", sd->villager_blacklist);
  read_npc_list(slot_data, "starting_villagers", sd->starting_villagers);
  sd->no_cockroaches = slot_data.value("no_cockroaches", 0) % 2;
  sd->shops_always_open = slot_data.value("shops_always_open", 0) % 2;
  sd->no_weeds = slot_data.value("no_weeds", 0) % 2;
  sd->normalized_time_travel = slot_data.value("normalized_time_travel", 1) % 2;

  // Defaults match the apworld's option defaults
  static const int default_loans[AP_LOAN_NUM] = { 17400, 98000, 49800, 198000, 298000 };
  static const int default_loan_checks[AP_LOAN_NUM] = { 1, 2, 2, 4, 6 };
  sd->goal = read_goal(slot_data);
  sd->loansanity = slot_data.value("loansanity", 1) % 2;
  read_int_array(slot_data, "loans", sd->loans, default_loans, AP_LOAN_NUM);
  read_int_array(slot_data, "loan_checks", sd->loan_checks, default_loan_checks, AP_LOAN_NUM);
  sd->favorsanity = slot_data.value("favorsanity", 0);
  sd->museumsanity = slot_data.value("museumsanity", 0) % 2;
  sd->bug_checks = slot_data.value("bug_checks", 0) % 4;
  sd->fish_checks = slot_data.value("fish_checks", 0) % 4;
  sd->fossil_checks = slot_data.value("fossil_checks", 0) % 4;
  sd->painting_checks = slot_data.value("painting_checks", 0) % 4;
  sd->museum_goal_count = slot_data.value("museum_goal_count", 0);
  sd->critter_spawns = slot_data.value("critter_spawns", 0) % 3;
  sd->fossil_spawns = slot_data.value("fossil_spawns", 2) % 3;
  // WARN: copy of the apworld's FOSSIL_SEASONS (museum.py), the default for offline play
  static const int default_fossil_seasons[AP_FOSSIL_NUM] = {
    2, 2, 2, 2, 2, 2, // Tricera, T-rex: autumn
    1, 1, 1, 1, 1, 1, // Apato, Stego: summer
    0, 0, 0,          // Ptera: spring
    3, 3, 3, 3, 3,    // Plesio, Mammoth: winter
    3, 3,             // Amber, Dinosaur Track: winter
    0, 0, 0,          // Ammonite, Dinosaur Egg, Trilobite: spring
  };
  read_int_array(slot_data, "fossil_seasons", sd->fossil_seasons, default_fossil_seasons, AP_FOSSIL_NUM);
  for (int i = 0; i < AP_FOSSIL_NUM; i++) { sd->fossil_seasons[i] &= 3; }
  sd->timesanity = slot_data.value("timesanity", 0) % 2;
  sd->starting_month = slot_data.value("starting_month", 0) % 12;
  sd->starting_time = slot_data.value("starting_time", 0) % 4;
  // Same range as the game (mTM_MIN_YEAR / mTM_MAX_YEAR on PC)
  sd->start_year = slot_data.value("start_year", 2001);
  if (sd->start_year < 2001 || sd->start_year > 2100) { sd->start_year = 2001; }

  // Keyed by item name in the apworld (Small/Modest/Large Bell Credit)
  static const char * credit_names[AP_BELLCREDIT_NUM] = { "Small Bell Credit", "Modest Bell Credit", "Large Bell Credit" };
  auto credits = slot_data.find("bell_credits");
  for (int i = 0; i < AP_BELLCREDIT_NUM; i++) {
    sd->bell_credits[i] = 0;
    if (credits != slot_data.end() && credits->is_object()) {
      auto b = credits->find(credit_names[i]);
      if (b != credits->end() && b->is_number_integer()) { sd->bell_credits[i] = b->get<int>(); }
    }
  }
  snprintf(sd->world_version, sizeof(sd->world_version), "%s",
      slot_data.value("world_version", "").c_str());
  sd->valid = 1;
}
