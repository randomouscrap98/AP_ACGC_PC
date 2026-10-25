#include "ap_slotdata.h"
#include "ap_log.h"
#include "ap_slotdata_upgrades.h"

#include <nlohmann/json.hpp>
#include <climits>
#include <cstdio>
#include <cstring>
#include <string>

static ap_slotdata g_slotdata;

const ap_slotdata * ap_getslotdata(void) {
  return &g_slotdata;
}

ap_slotdata * ap_slotdata_writable(void) {
  return &g_slotdata;
}

void ap_slotdata_init(ap_slotdata * sd) {
  memset(sd, 0, sizeof(ap_slotdata));
}

// One step of slot_data_upgrades.json (its _readme), applied like slot_data_upgrade.py apply
static void apply_step(nlohmann::json& slot_data, const nlohmann::json& step, int v) {
  for (const auto& op : step.items()) {
    const std::string& name = op.key();
    if (name != "note" && name != "renamed" && name != "remapped" && name != "added" && name != "removed") {
      APLOG_WARN("slot_data upgrade step %d: unknown entry %s", v, name.c_str());
    }
  }
  if (step.contains("renamed")) {
    for (const auto& kv : step["renamed"].items()) {
      if (slot_data.contains(kv.key())) {
        nlohmann::json value = slot_data[kv.key()];
        slot_data.erase(kv.key());
        slot_data[kv.value().get<std::string>()] = value;
      }
    }
  }
  if (step.contains("remapped")) {
    for (const auto& kv : step["remapped"].items()) {
      if (!slot_data.contains(kv.key())) { continue; }
      for (const auto& pair : kv.value()) {
        if (slot_data[kv.key()] == pair.at(0)) {
          slot_data[kv.key()] = pair.at(1);
          break;
        }
      }
    }
  }
  if (step.contains("added")) {
    for (const auto& kv : step["added"].items()) {
      if (!slot_data.contains(kv.key())) { slot_data[kv.key()] = kv.value(); }
    }
  }
  if (step.contains("removed")) {
    for (const auto& key : step["removed"]) { slot_data.erase(key.get<std::string>()); }
  }
}

int ap_slotdata_apply_upgrades(nlohmann::json& slot_data, const nlohmann::json& steps, int current) {
  int version = slot_data.value("slot_data_version", 1);
  if (version > current) {
    return 0;
  }
  for (int v = version + 1; v <= current; v++) {
    auto it = steps.find(std::to_string(v));
    if (it != steps.end()) { apply_step(slot_data, *it, v); }
  }
  slot_data["slot_data_version"] = current;
  return 1;
}

// The only code that knows slot_data versions: old seeds are translated up to the current format here, so the
// parser and the game only ever see the current one. The steps are the apworld's slot_data_upgrades.json, embedded
// at build time (ap_slotdata_upgrades.h, apclient/embed_upgrades.cmake), so the tracker and the client agree.
int ap_slotdata_upgrade(nlohmann::json& slot_data) {
  static const nlohmann::json steps = nlohmann::json::parse(AP_SLOTDATA_UPGRADES_JSON);
  return ap_slotdata_apply_upgrades(slot_data, steps, AP_SLOTDATA_VERSION);
}

// An int key in [min, max]; missing = def, anything else (wrong type, out of range) = def + a warning.
// json true/false count as 1/0 (hand-written offline json)
static int read_int(const nlohmann::json& slot_data, const char * key, int def, int min, int max) {
  auto it = slot_data.find(key);
  if (it == slot_data.end()) { return def; }
  long long v = min - 1LL;
  if (it->is_boolean()) { v = it->get<bool>() ? 1 : 0; }
  if (it->is_number_integer()) { v = it->get<long long>(); }
  if (v >= min && v <= max) { return (int)v; }
  APLOG_WARN("%s: %s is not in %d..%d, using %d", key, it->dump().c_str(), min, max, def);
  return def;
}

static int read_bool(const nlohmann::json& slot_data, const char * key, int def) {
  return read_int(slot_data, key, def, 0, 1);
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

// Per-thing item indices (bug_passes, fossil_series): out[i] in [0, num), -1 for a missing key,
// a short list or a bad entry (= nothing needed)
static void read_pass_array(const nlohmann::json& slot_data, const char * key, int * out, int n, int num) {
  for (int i = 0; i < n; i++) { out[i] = -1; }
  auto it = slot_data.find(key);
  if (it == slot_data.end() || !it->is_array()) { return; }
  for (int i = 0; i < n && i < (int)it->size(); i++) {
    const nlohmann::json& v = (*it)[i];
    if (v.is_number_integer() && v.get<long long>() >= 0 && v.get<long long>() < num) {
      out[i] = v.get<int>();
    } else {
      APLOG_WARN("%s[%d]: ignoring %s", key, i, v.dump().c_str());
    }
  }
}

// fish_passes: per pass a list of [fish index, area] entries (area "" = every area) -> fish_area_passes bits.
// Missing key: nothing gated. Bad entries (and passes past AP_FISH_PASS_NUM) are skipped.
static void read_fish_passes(const nlohmann::json& slot_data, ap_slotdata * sd) {
  static const char * areas[AP_FISH_AREA_NUM] = { "river", "waterfall", "river_mouth", "lake", "pond", "sea" };
  memset(sd->fish_area_passes, 0, sizeof(sd->fish_area_passes));
  auto it = slot_data.find("fish_passes");
  if (it == slot_data.end() || !it->is_array()) { return; }
  for (int p = 0; p < AP_FISH_PASS_NUM && p < (int)it->size(); p++) {
    const nlohmann::json& entries = (*it)[p];
    if (!entries.is_array()) { APLOG_WARN("fish_passes[%d]: ignoring %s", p, entries.dump().c_str()); continue; }
    for (auto& e : entries) {
      int fish = -1, area = -2; // -1: every area
      if (e.is_array() && e.size() == 2 && e[0].is_number_integer() && e[1].is_string()) {
        if (e[0].get<long long>() >= 0 && e[0].get<long long>() < AP_CRITTER_NUM) { fish = e[0].get<int>(); }
        std::string name = e[1].get<std::string>();
        if (name.empty()) { area = -1; }
        for (int a = 0; a < AP_FISH_AREA_NUM; a++) {
          if (name == areas[a]) { area = a; }
        }
      }
      if (fish < 0 || area == -2) { APLOG_WARN("fish_passes[%d]: ignoring %s", p, e.dump().c_str()); continue; }
      for (int a = 0; a < AP_FISH_AREA_NUM; a++) {
        if (area == -1 || area == a) { sd->fish_area_passes[fish][a] |= 1 << p; }
      }
    }
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

// Catalogsanity pools: a list of lists of game item ids. Bad entries are skipped
// (apworld/client mismatch), anything past AP_CATALOG_MAX is dropped
static void read_catalog_pools(const nlohmann::json& slot_data, ap_slotdata * sd) {
  sd->catalog_count = 0;
  auto it = slot_data.find("catalog_pools");
  if (it == slot_data.end() || !it->is_array()) { return; }
  int pool = 0;
  for (auto& items : *it) {
    if (items.is_array()) {
      for (auto& v : items) {
        if (!v.is_number_integer() || v.get<int>() <= 0 || v.get<int>() > 0xFFFF) {
          APLOG_WARN("catalog_pools: ignoring bad entry %s", v.dump().c_str());
        } else if (sd->catalog_count >= AP_CATALOG_MAX) {
          APLOG_WARN("catalog_pools: more than %d items, ignoring %s", AP_CATALOG_MAX, v.dump().c_str());
        } else {
          sd->catalog_items[sd->catalog_count] = (unsigned short)v.get<int>();
          sd->catalog_pool[sd->catalog_count] = (unsigned char)(pool < 255 ? pool : 255);
          sd->catalog_count++;
        }
      }
    }
    pool++;
  }
}

// Furniture Set goal pieces: game item ids. Bad entries are skipped, past AP_FURNITURE_SET_MAX dropped
static void read_furniture_set(const nlohmann::json& slot_data, ap_slotdata * sd) {
  sd->furniture_set_count = 0;
  auto it = slot_data.find("furniture_set_items");
  if (it == slot_data.end() || !it->is_array()) { return; }
  for (auto& v : *it) {
    if (!v.is_number_integer() || v.get<int>() <= 0 || v.get<int>() > 0xFFFF ||
        sd->furniture_set_count >= AP_FURNITURE_SET_MAX) {
      APLOG_WARN("furniture_set_items: ignoring %s", v.dump().c_str());
    } else {
      sd->furniture_set_items[sd->furniture_set_count++] = (unsigned short)v.get<int>();
    }
  }
}

// Catalog Sources option set (names) -> AP_CATALOG_SOURCE_* bits; missing = none
static int read_catalog_sources(const nlohmann::json& slot_data) {
  // Same order as the bits (apworld options.CATALOG_SOURCES)
  static const char * names[] = { "Favors", "Trades", "Lost and Found", "Dump", "Letters", "Trees", "Presents",
                                  "Grab Bags", "Wisp" };
  auto it = slot_data.find("catalog_sources");
  if (it == slot_data.end() || !it->is_array()) { return 0; }
  int bits = 0;
  for (auto& v : *it) {
    for (int i = 0; i < (int)(sizeof(names) / sizeof(names[0])); i++) {
      if (v.is_string() && v.get<std::string>() == names[i]) { bits |= 1 << i; }
    }
  }
  return bits;
}

static int read_goal(const nlohmann::json& slot_data) {
  auto it = slot_data.find("goal");
  if (it == slot_data.end() || !it->is_array()) { return AP_GOAL_STATUE; }
  int goal = 0;
  for (auto& g : *it) {
    if (g.is_string() && g.get<std::string>() == "Statue") { goal |= AP_GOAL_STATUE; }
    if (g.is_string() && g.get<std::string>() == "Museum") { goal |= AP_GOAL_MUSEUM; }
    if (g.is_string() && g.get<std::string>() == "Froggy Chair") { goal |= AP_GOAL_FROGGY; }
    if (g.is_string() && g.get<std::string>() == "Catalog") { goal |= AP_GOAL_CATALOG; }
    if (g.is_string() && g.get<std::string>() == "Furniture Set") { goal |= AP_GOAL_FURNITURE_SET; }
    if (g.is_string() && g.get<std::string>() == "Chess Set") { goal |= AP_GOAL_CHESS_SET; }
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
  sd->skip_intro = read_int(slot_data, "skip_intro", 2, 0, 2);
  sd->gender = read_int(slot_data, "gender", 0, 0, 1);
  sd->face = read_int(slot_data, "face", 0, 0, 7);
  sd->starting_shirt = read_int(slot_data, "starting_shirt", 16, 0, 16);
  sd->house = read_int(slot_data, "house", 0, 0, 3);
  sd->town_fruit = read_int(slot_data, "town_fruit", 0, 0, 4);
  sd->grass_shape = read_int(slot_data, "grass_shape", 0, 0, 2);
  sd->train_station = read_int(slot_data, "train_station", -1, -1, 14);
  sd->town_day = read_int(slot_data, "town_day", 0, 0, 31);
  sd->letter_paper = read_int(slot_data, "letter_paper", 0, 0, 63);
  snprintf(sd->letter_sender, sizeof(sd->letter_sender), "%s",
      slot_data.value("letter_sender", "Archipelago").c_str());
  snprintf(sd->loan_letter_text, sizeof(sd->loan_letter_text), "%s",
      slot_data.value("loan_letter_text", "Your loan is ready for\npayoff at the post office!").c_str());
  read_npc_list(slot_data, "villager_blacklist", sd->villager_blacklist);
  read_npc_list(slot_data, "starting_villagers", sd->starting_villagers);
  sd->shops_always_open = read_bool(slot_data, "shops_always_open", 0);
  sd->global_storage = read_bool(slot_data, "global_storage", 0);
  sd->no_weeds = read_bool(slot_data, "no_weeds", 1);
  sd->more_favors = read_bool(slot_data, "more_favors", 1);
  sd->villagers_dont_leave = read_bool(slot_data, "villagers_dont_leave", 0);
  sd->villagers_never_sleep = read_bool(slot_data, "villagers_never_sleep", 0);
  sd->turnips_never_spoil = read_bool(slot_data, "turnips_never_spoil", 1);
  sd->no_falling_stalks = read_bool(slot_data, "no_falling_stalks", 0);
  sd->skip_hra_talk = read_bool(slot_data, "skip_hra_talk", 1);
  sd->shop_never_sells_out = read_bool(slot_data, "shop_never_sells_out", 0);
  sd->coconuts_wash_up = read_bool(slot_data, "coconuts_wash_up", 1);
  sd->normalized_time_travel = read_bool(slot_data, "normalized_time_travel", 1);

  // Missing keys: offline-relevant ones default to the apworld defaults, AP-only ones to off
  static const int default_loans[AP_LOAN_NUM] = { 15000, 45000, 30000, 60000, 80000 };
  // the apworld always sends the real split; missing = no loan checks
  static const int default_loan_checks[AP_LOAN_NUM] = { 0, 0, 0, 0, 0 };
  sd->goal = read_goal(slot_data);
  sd->loansanity = read_bool(slot_data, "loansanity", 1);
  read_int_array(slot_data, "loans", sd->loans, default_loans, AP_LOAN_NUM);
  read_int_array(slot_data, "loan_checks", sd->loan_checks, default_loan_checks, AP_LOAN_NUM);
  // Favor checks in total: Favor Pools x Favors per Pool, 0 with Favorsanity off
  int favor_pools = read_bool(slot_data, "favorsanity", 0) ? read_int(slot_data, "favor_pools", 0, 0, 100) : 0;
  sd->favors_per_pool = read_int(slot_data, "favors_per_pool", 0, 0, 100); // 0: one pool, all open
  sd->favorsanity = favor_pools * sd->favors_per_pool;
  sd->tools_in_pool = read_bool(slot_data, "tools_in_pool", 0);
  sd->store_upgrades = read_int(slot_data, "nook_store_upgrades", AP_STORE_VANILLA, AP_STORE_VANILLA, AP_STORE_CATALOG);
  static const int no_thresholds[AP_STORE_LEVELS] = { 0, 0, 0 };
  read_int_array(slot_data, "nook_store_thresholds", sd->store_thresholds, no_thresholds, AP_STORE_LEVELS);
  sd->museumsanity = read_bool(slot_data, "museumsanity", 0);
  sd->bug_checks = read_int(slot_data, "bug_checks", 0, 0, AP_MUSEUM_FIND | AP_MUSEUM_DONATE);
  sd->fish_checks = read_int(slot_data, "fish_checks", 0, 0, AP_MUSEUM_FIND | AP_MUSEUM_DONATE);
  sd->fossil_checks = read_int(slot_data, "fossil_checks", 0, 0, AP_MUSEUM_FIND | AP_MUSEUM_DONATE);
  sd->painting_checks = read_int(slot_data, "painting_checks", 0, 0, AP_MUSEUM_FIND | AP_MUSEUM_DONATE);
  sd->museum_goal_count = read_int(slot_data, "museum_goal_count", 0, 0, INT_MAX); // too high: goal never done
  sd->critter_odds = read_int(slot_data, "critter_odds", AP_CRITTER_ODDS_DYNAMIC, AP_CRITTER_ODDS_VANILLA, AP_CRITTER_ODDS_DYNAMIC);
  // Missing = all (older seeds have no bug pass items)
  sd->bug_availability = read_int(slot_data, "bug_availability", AP_BUG_AVAIL_ALL, AP_BUG_AVAIL_ALL, AP_BUG_AVAIL_PROGRESSIVE);
  // Missing = all (older seeds have no fish pass items)
  sd->fish_availability = read_int(slot_data, "fish_availability", AP_FISH_AVAIL_ALL, AP_FISH_AVAIL_ALL, AP_FISH_AVAIL_PROGRESSIVE);
  // Missing = all (0.3 seeds have no series items, progressive would never spawn dinosaurs)
  sd->fossil_availability = read_int(slot_data, "fossil_availability", AP_FOSSIL_AVAIL_ALL, AP_FOSSIL_AVAIL_ALL,
      AP_FOSSIL_AVAIL_PROGRESSIVE);
  sd->fossil_odds = read_int(slot_data, "fossil_odds", AP_FOSSIL_ODDS_DYNAMIC, AP_FOSSIL_ODDS_VANILLA, AP_FOSSIL_ODDS_DYNAMIC);
  // WARN: copy of the apworld's FOSSIL_SEASONS (fossil_availability.py), the default for offline play
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
  read_pass_array(slot_data, "bug_passes", sd->bug_passes, AP_CRITTER_NUM, AP_BUG_PASS_NUM);
  read_fish_passes(slot_data, sd);
  read_pass_array(slot_data, "fossil_series", sd->fossil_series, AP_FOSSIL_NUM, AP_FOSSIL_SERIES_NUM);
  sd->timesanity = read_bool(slot_data, "timesanity", 0);
  sd->starting_month = read_int(slot_data, "starting_month", 0, 0, 11);
  sd->starting_time = read_int(slot_data, "starting_time", 0, 0, 3);
  // Same range as the game (mTM_MIN_YEAR / mTM_MAX_YEAR on PC)
  sd->start_year = read_int(slot_data, "start_year", 2001, 2001, 2100);
  sd->catalogsanity = read_bool(slot_data, "catalogsanity", 0);
  read_catalog_pools(slot_data, sd);
  sd->new_item_chance = read_int(slot_data, "new_item_chance", 0, 0, 100);
  sd->catalog_sources = read_catalog_sources(slot_data);
  sd->catalog_goal_count = read_int(slot_data, "catalog_goal_count", 0, 0, AP_CATALOG_MAX);
  read_furniture_set(slot_data, sd);
  sd->beach_per_month = read_bool(slot_data, "shellsanity", 0) ?
      read_int(slot_data, "beach_checks_per_month", 0, 0, AP_BEACH_PER_MONTH_MAX) : 0;
  sd->shellsanity_item_chance = read_int(slot_data, "shellsanity_item_chance", 20, 0, 100);
  sd->tree_per_fruit = read_bool(slot_data, "treesanity", 0) ?
      read_int(slot_data, "tree_checks_per_fruit", 0, 0, AP_TREE_PER_FRUIT_MAX) : 0;

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
