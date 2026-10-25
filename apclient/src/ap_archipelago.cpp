#include "ap_archipelago.h"
#include "ap_info.h"
#include "ap_messages_internal.h"
#include "ap_slotdata.h"

#include <apclient.hpp>
#include <apuuid.hpp>
#include <cstdarg>
#include <deque>
#include <memory>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "ap_log.h"

#define INI_IMPLEMENTATION
#include "ini.h"


static std::unique_ptr<APClient> g_ap;

// This game's item / location names for the text client's "!hint" completion (ap_game_names_set)
static void ap_names_from_package(const nlohmann::json& game) {
  const char * keys[2] = { "item_name_to_id", "location_name_to_id" }; // AP_NAMES_ITEMS, AP_NAMES_LOCATIONS
  for (int which = 0; which < 2; which++) {
    std::vector<std::string> names;
    auto table = game.find(keys[which]);
    if (table != game.end() && table->is_object()) {
      for (const auto& kv : table->items()) names.push_back(kv.key());
    }
    ap_game_names_set(which, std::move(names));
  }
  APLOG_INFO("Data package: %d item and %d location names for the text client",
      (int)ap_game_name_count(AP_NAMES_ITEMS), (int)ap_game_name_count(AP_NAMES_LOCATIONS));
}

// apclientpp's default data package cache, also taking this game's names from it: when the cached package is
// current, apclientpp only loads it (no request, no data_package_changed callback); a new one is saved here first
class ap_names_store : public DefaultDataPackageStore {
public:
  bool load(const std::string& game, const std::string& checksum, nlohmann::json& data) override {
    bool ok = DefaultDataPackageStore::load(game, checksum, data);
    if (ok && game == AP_GAMENAME) ap_names_from_package(data);
    return ok;
  }
  bool save(const std::string& game, const nlohmann::json& data) override {
    if (game == AP_GAMENAME) ap_names_from_package(data);
    return DefaultDataPackageStore::save(game, data);
  }
};
static ap_names_store g_ap_names_store;
static ap_config g_ap_config;
static ap_connectstate g_ap_connectstate;
static std::vector<int64_t> g_ap_items;
static std::vector<int> g_ap_item_senders; // slot that sent g_ap_items[i]
static std::vector<int64_t> g_ap_item_locations; // ... from which of its locations
static std::vector<unsigned> g_ap_item_flags;    // ... AP_ITEM_*
static std::vector<ap_hint> g_ap_hints;          // hints involving this slot (data storage _read_hints_<team>_<slot>)
static uint64_t g_ap_hint_version = 0;
static std::set<int64_t> g_ap_checks;
static unsigned g_ap_checked_generation = 0; // +1 when the server's checked list changes
static int g_ap_goal = 0;
// Connected to a different room than the first one (server restarted with a
// new seed): the game's save belongs to the old room, so ap_poll stops the client
static bool g_ap_room_changed = false;
#define AP_ROOM_CHANGED_REASON "Seed changed, restart"
// Reconnect attempts repeat the error every few seconds: log only the first one until the slot connects
// again (the overlay shows the current one)
static bool g_ap_outage_logged = false;
static std::deque<std::string> g_ap_toast;
static const size_t AP_TOAST_MAX = 16;

// Text from outside (server, asio errors) shown in the overlay: control bytes
// would act as AP_CTRL_* codes there, so they become spaces. Trailing spaces
// dropped (Windows error text may end in "\r\n").
static std::string ap_plain(const std::string& in) {
  std::string out = in;
  for (char& c : out) {
    if ((unsigned char)c < 0x20) c = ' ';
  }
  while (!out.empty() && out.back() == ' ') out.pop_back();
  return out;
}

// One PrintJSON as text client segments, coloured like AP's text client (NetUtils JSONtoTextParser)
static void ap_msg_from_print_json(const APClient::PrintJSONArgs& args) {
  std::vector<ap_msg_seg> segs;
  // This slot's own chat and commands (the server echoes both as chat with the sender's slot): name magenta,
  // text orange, so they stand out
  if (args.type == "Chat" && args.slot && args.team && args.message && *args.slot == g_ap->get_player_number() &&
      *args.team == g_ap->get_team_number()) {
    ap_msg_add({ { g_ap->get_player_alias(*args.slot) + ":", ap_msg_color("magenta") },
                 { " " + *args.message, ap_msg_color("orange") } }, AP_MSG_CHAT);
    return;
  }
  for (const auto& node : args.data) {
    std::string text = node.text;
    int color = ap_msg_color(node.color);
    long long id = strtoll(node.text.c_str(), NULL, 10);
    if (node.type == "player_id") {
      text = g_ap->get_player_alias((int)id);
      color = ap_msg_player_color(g_ap->slot_concerns_self((int)id));
    } else if (node.type == "player_name") {
      color = ap_msg_player_color(false);
    } else if (node.type == "item_id" || node.type == "item_name") {
      if (node.type == "item_id") { text = g_ap->get_item_name(id, g_ap->get_player_game(node.player)); }
      color = ap_msg_item_color(node.flags); // APClient::FLAG_* = AP_ITEM_*
    } else if (node.type == "location_id" || node.type == "location_name") {
      if (node.type == "location_id") { text = g_ap->get_location_name(id, g_ap->get_player_game(node.player)); }
      color = ap_msg_color("green");
    } else if (node.type == "entrance_name") {
      color = ap_msg_color("blue");
    } else if (node.type == "hint_status") {
      color = ap_msg_hint_color(node.hintStatus); // APClient::HINT_* = AP_HINT_*
    }
    segs.push_back({ text, color });
  }
  // What it's about, for the text client's filters
  int me = g_ap->get_player_number();
  int kind = AP_MSG_SYSTEM;
  if (args.type == "Chat" || args.type == "ServerChat") {
    kind = AP_MSG_CHAT;
  } else if (args.type == "ItemSend" || args.type == "ItemCheat" || args.type == "Hint") {
    bool mine = (args.receiving && *args.receiving == me) || (args.item && args.item->player == me);
    kind = mine ? AP_MSG_SELF : AP_MSG_OTHERS;
  } else if (args.type == "Release" || args.type == "Collect") {
    kind = args.slot && *args.slot == me ? AP_MSG_SELF : AP_MSG_OTHERS;
  } else if (args.type == "Goal" || args.type == "CommandResult" || args.type == "AdminCommandResult") {
    kind = AP_MSG_SELF;
  }
  ap_msg_add(segs, kind);
}

int ap_say(const char * text) {
  if (!g_ap || text == NULL || text[0] == 0 || !g_ap->Say(text)) {
    return 0;
  }
  return 1;
}

// ---- text client information (ap_info.h) ----

int ap_item_name(int64_t item, int slot, char * buf, size_t len) {
  if (!g_ap || len == 0) return 0;
  std::string name = g_ap->get_item_name(item, g_ap->get_player_game(slot));
  snprintf(buf, len, "%s", name.c_str());
  return name != "Unknown";
}

int ap_location_name(int64_t location, int slot, char * buf, size_t len) {
  if (!g_ap || len == 0) return 0;
  std::string name = g_ap->get_location_name(location, g_ap->get_player_game(slot));
  snprintf(buf, len, "%s", name.c_str());
  return name != "Unknown";
}

int ap_my_slot(void) {
  return g_ap && g_ap_connectstate.state == AP_CSTATE_CONNECTED ? g_ap->get_player_number() : 0;
}

int ap_slot_is_me(int slot) {
  return ap_my_slot() != 0 && g_ap->slot_concerns_self(slot);
}

static std::string ap_hints_key(void) {
  return "_read_hints_" + std::to_string(g_ap->get_team_number()) + "_" + std::to_string(g_ap->get_player_number());
}

// The server's hint list (data storage value: a list of Hint objects); anything malformed is skipped
static void ap_hints_from_json(const nlohmann::json& list) {
  g_ap_hints.clear();
  if (list.is_array()) {
    for (const auto& h : list) {
      if (!h.is_object()) continue;
      ap_hint hint = {};
      hint.receiving = h.value("receiving_player", 0);
      hint.finding = h.value("finding_player", 0);
      hint.item = h.value("item", (int64_t)0);
      hint.location = h.value("location", (int64_t)0);
      hint.flags = h.value("item_flags", 0u);
      hint.found = h.value("found", false);
      hint.status = h.value("status", hint.found ? AP_HINT_FOUND : AP_HINT_UNSPECIFIED);
      snprintf(hint.entrance, sizeof(hint.entrance), "%s", h.value("entrance", std::string()).c_str());
      g_ap_hints.push_back(hint);
    }
  }
  g_ap_hint_version++;
}

int ap_hint_get(size_t i, ap_hint * out) {
  if (i >= g_ap_hints.size() || out == NULL) return 0;
  *out = g_ap_hints[i];
  return 1;
}

uint64_t ap_hint_version(void) {
  return g_ap_hint_version;
}

int ap_hint_set_status(int finding, int64_t location, int status) {
  if (!g_ap || g_ap_connectstate.state != AP_CSTATE_CONNECTED) return 0;
  return g_ap->UpdateHint(finding, location, (APClient::HintStatus)status);
}

size_t ap_location_ids(int checked, int64_t * out, size_t max) {
  if (!g_ap) return 0;
  std::set<int64_t> ids = checked ? g_ap->get_checked_locations() : g_ap->get_missing_locations();
  size_t i = 0;
  for (int64_t id : ids) {
    if (i < max && out) out[i] = id;
    i++;
  }
  return i;
}

size_t ap_getitemcount(void) {
  return g_ap_items.size();
}

int64_t ap_getitem(size_t idx) {
  if(idx >= g_ap_items.size()) {
    return -1;
  }
  return g_ap_items.at(idx);
}

int ap_getitem_sender(size_t idx) {
  if(idx >= g_ap_item_senders.size()) {
    return -1;
  }
  return g_ap_item_senders.at(idx);
}

int64_t ap_getitem_location(size_t idx) {
  return idx < g_ap_item_locations.size() ? g_ap_item_locations[idx] : 0;
}

unsigned ap_getitem_flags(size_t idx) {
  return idx < g_ap_item_flags.size() ? g_ap_item_flags[idx] : 0;
}

int ap_player_name(int slot, char * buf, size_t len) {
  if(!g_ap || len == 0) {
    return 0;
  }
  snprintf(buf, len, "%s", ap_plain(g_ap->get_player_alias(slot)).c_str());
  return 1;
}

int ap_item_count(int64_t id) {
  int found = 0;
  for(int64_t item : g_ap_items) {
    if(item == id) {
      found++;
    }
  }
  return found;
}

int ap_pop_toast(char * buf, size_t len) {
  if(g_ap_toast.size() == 0) return 0;
  snprintf(buf, len, "%s", g_ap_toast.front().c_str());
  g_ap_toast.pop_front();
  return 1;
}

void ap_send_location(int64_t id) {
  auto result = g_ap_checks.insert(id);
  if(result.second && g_ap) {
    g_ap->LocationChecks({ id });
  }
}

int64_t ap_highest_checked(int64_t first, int64_t last) {
  int64_t best = first - 1;
  for(int64_t id : g_ap_checks) {
    if(id >= first && id <= last && id > best) best = id;
  }
  if(g_ap) {
    for(int64_t id : g_ap->get_checked_locations()) {
      if(id >= first && id <= last && id > best) best = id;
    }
  }
  return best;
}

int ap_unchecked_count(int64_t first, int64_t last) {
  std::set<int64_t> server;
  if(g_ap) {
    server = g_ap->get_checked_locations();
  }
  int count = 0;
  for(int64_t id = first; id <= last; id++) {
    count += !g_ap_checks.count(id) && !server.count(id);
  }
  return count;
}

int64_t ap_first_unchecked(int64_t first, int64_t last) {
  std::set<int64_t> server;
  if(g_ap) {
    server = g_ap->get_checked_locations();
  }
  for(int64_t id = first; id <= last; id++) {
    if(!g_ap_checks.count(id) && !server.count(id)) {
      return id;
    }
  }
  return -1;
}

int ap_location_checked(int64_t id) {
  if(g_ap_checks.count(id)) {
    return 1;
  }
  return g_ap && g_ap->get_checked_locations().count(id);
}

unsigned ap_checked_generation(void) {
  return g_ap_checked_generation;
}

void ap_locations_checked(const int64_t * ids, size_t n, uint8_t * out) {
  // get_checked_locations returns a copy of the set: take it once
  std::set<int64_t> server;
  if(g_ap) {
    server = g_ap->get_checked_locations();
  }
  for(size_t i = 0; i < n; i++) {
    out[i] = g_ap_checks.count(ids[i]) || server.count(ids[i]);
  }
}

// apclientpp drops a StatusUpdate made while not connected, so remember it and
// resend on every connect
void ap_send_goal(void) {
  if(!g_ap_goal && g_ap) {
    g_ap->StatusUpdate(APClient::ClientStatus::GOAL);
  }
  g_ap_goal = 1;
}

// Bad or unfilled config: no client, the overlay shows "EDIT ap_config.ini: <reason>"
static void ap_bad_config(const char * fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vsnprintf(g_ap_connectstate.last_refuse_reason, sizeof(g_ap_connectstate.last_refuse_reason), fmt, args);
  va_end(args);
  g_ap_connectstate.state = AP_CSTATE_BADCONFIG;
  APLOG_WARN("Bad AP config: %s", g_ap_connectstate.last_refuse_reason);
}

// Written when there's no config, so a first run just says what to fill in
static const char AP_CONFIG_TEMPLATE[] =
  "; Archipelago connection: fill in host and slotname, then restart the game.\n"
  "; Example: host = archipelago.gg:38281 (use ws:// for a local server: host = ws://localhost:38281)\n"
  "host = \n"
  "slotname = \n"
  "password = \n"
  "; Offline play instead (no server): remove the ; below and edit the json\n"
  ";offline = offline_example.json\n";

// ini_find_property, but ignoring spaces after the name: ini.h keeps them ("host = x" is "host ")
static int ap_ini_find(ini_t * ini, const char * name) {
  size_t len = strlen(name);
  int count = ini_property_count(ini, INI_GLOBAL_SECTION);
  for (int i = 0; i < count; i++) {
    const char * other = ini_property_name(ini, INI_GLOBAL_SECTION, i);
    size_t other_len = strlen(other);
    while (other_len > 0 && other[other_len - 1] <= ' ') { other_len--; }
    if (other_len == len && INI_STRNICMP(name, other, len) == 0) { return i; }
  }
  return INI_NOT_FOUND;
}

// Load the config at path; a missing one is created from AP_CONFIG_TEMPLATE.
// Non-zero (state set by ap_bad_config) when there's nothing to connect to
static int load_config(const char * path, ap_config * out) {
  APLOG_DEBUG("Loading AP config file %s", path);
  // Reading files in c++ is weird... I'm used to c. sorry if this is weird?
  std::ifstream f(path, std::ios::binary);
  if(!f) {
    std::ofstream w(path, std::ios::binary);
    w << AP_CONFIG_TEMPLATE;
    if (!w) {
      ap_bad_config("can't create it next to the game");
      return 1;
    }
    APLOG_INFO("Created AP config file %s", path);
    ap_bad_config("fill in host and slotname");
    return 1;
  }
  std::stringstream ss;
  ss << f.rdbuf();
  std::string data = ss.str();

  ini_t* ini = ini_load(data.c_str(), NULL); // null for normal malloc?
  int hostp = ap_ini_find(ini, "host");
  int slotp = ap_ini_find(ini, "slotname");
  int passwordp = ap_ini_find(ini, "password");
  int offlinep = ap_ini_find(ini, "offline");
  // offline mode doesn't need a server
  if (offlinep != INI_NOT_FOUND) {
    snprintf(out->offline, sizeof(out->offline), "%s", ini_property_value(ini, INI_GLOBAL_SECTION, offlinep));
    ini_destroy(ini);
    if (!out->offline[0]) {
      ap_bad_config("offline: empty");
      return 1;
    }
    return 0;
  }
  if (hostp == INI_NOT_FOUND || slotp == INI_NOT_FOUND) {
    ini_destroy(ini);
    ap_bad_config("needs host and slotname, or offline");
    return 1;
  }
  snprintf(out->host, sizeof(out->host), "%s", ini_property_value(ini, INI_GLOBAL_SECTION, hostp));
  snprintf(out->slotname, sizeof(out->slotname), "%s", ini_property_value(ini, INI_GLOBAL_SECTION, slotp));
  // password is optional
  snprintf(out->password, sizeof(out->password), "%s",
           passwordp == INI_NOT_FOUND ? "" : ini_property_value(ini, INI_GLOBAL_SECTION, passwordp));
  ini_destroy(ini);

  if (!out->host[0] || !out->slotname[0]) {
    ap_bad_config("fill in host and slotname");
    return 1;
  }
  return 0;
}

static void ap_roomplayer_init(ap_roomplayer * rp) {
  rp->seed[0] = 0;
  rp->player = 0;
  rp->team = 0;
}

// This is a big project, don't pollute the namespace unless you need the functions
static void ap_connectstate_init(ap_connectstate * state) {
  state->state = AP_CSTATE_UNKNOWN;
  state->last_refuse_reason[0] = 0;
  state->last_connect_error[0] = 0;
  state->connect_once = 0;
  ap_roomplayer_init(&state->roomplayer);
}

static void ap_config_init(ap_config * config) {
  config->host[0] = 0;
  config->slotname[0] = 0;
  config->password[0] = 0;
  config->offline[0] = 0;
}

int ap_roomplayer_valid(const ap_roomplayer * rp) {
  return strlen(rp->seed) > 0;
}

// Offline mode: slot_data and the session id (which save folder) from a local json, no
// server. Everything that needs AP items is forced off. Non-zero on error.
static int ap_start_offline(const char * path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    APLOG_WARN("Can't find offline file at %s", path);
    ap_bad_config("offline: file not found");
    return 1;
  }
  nlohmann::json data = nlohmann::json::parse(f, nullptr, false);
  if (!data.is_object() || !data.contains("seed") || !data["seed"].is_string() ||
      data["seed"].get<std::string>().empty()) {
    APLOG_WARN("Malformed offline file at %s (needs a json object with a seed)", path);
    ap_bad_config("offline: bad json or no seed");
    return 1;
  }

  ap_roomplayer * rp = &g_ap_connectstate.roomplayer;
  ap_slotdata * sd = ap_slotdata_writable();
  // Hand-written file: a wrong value type (value() throws) is an error, not a crash
  try {
    rp->team = data.value("team", 0);
    rp->player = data.value("player", 1);
    if (!ap_slotdata_upgrade(data)) {
      APLOG_WARN("Offline file at %s is for a newer client (slot_data_version %d, client reads up to %d)", path,
          data.value("slot_data_version", 1), AP_SLOTDATA_VERSION);
      ap_bad_config("offline: needs a newer client");
      ap_roomplayer_init(rp);
      return 1;
    }
    ap_slotdata_fill(sd, data);
  } catch (const nlohmann::json::exception& e) {
    APLOG_WARN("Malformed offline file at %s: %s", path, e.what());
    ap_bad_config("offline: value of the wrong type");
    ap_slotdata_init(sd);
    ap_roomplayer_init(rp);
    return 1;
  }
  snprintf(rp->seed, sizeof(rp->seed), "%s", data["seed"].get<std::string>().c_str());
  // No items or checks offline
  sd->loansanity = 0;
  sd->timesanity = 0;
  sd->favorsanity = 0;
  sd->museumsanity = 0;
  sd->catalogsanity = 0;
  sd->catalog_count = 0;
  sd->beach_per_month = 0;
  sd->tree_per_fruit = 0;
  sd->tools_in_pool = 0;
  sd->store_upgrades = AP_STORE_VANILLA;
  sd->goal = 0;
  sd->furniture_set_count = 0;
  for (int i = 0; i < AP_LOAN_NUM; i++) { sd->loan_checks[i] = 0; }

  g_ap_connectstate.state = AP_CSTATE_OFFLINE;
  APLOG_INFO("OFFLINE: %s/%d/%d", rp->seed, rp->team, rp->player);
  return 0;
}

void ap_start(void) {
  APLOG_INFO("Starting AP system");

  ap_config_init(&g_ap_config);
  ap_connectstate_init(&g_ap_connectstate);

  ap_slotdata * sd = ap_slotdata_writable();
  ap_slotdata_init(sd);

  if (load_config(AP_CONFIGNAME, &g_ap_config)) { return; }
  if (g_ap_config.offline[0]) {
    ap_start_offline(g_ap_config.offline);
    return;
  }

  std::string uuid = ap_get_uuid("uuid");          // persists a uuid in a file
  std::string pw = g_ap_config.password, name = g_ap_config.slotname;

  g_ap = std::make_unique<APClient>(uuid, AP_GAMENAME, g_ap_config.host, AP_CERTPATH, &g_ap_names_store);
  g_ap_connectstate.state = AP_CSTATE_CONNECTING;

  g_ap->set_slot_connected_handler([](const nlohmann::json& slot_data) {
    g_ap_outage_logged = false;
    ap_roomplayer * rp = &g_ap_connectstate.roomplayer;
    if(ap_roomplayer_valid(rp) && (g_ap->get_seed() != rp->seed ||
        g_ap->get_team_number() != rp->team || g_ap->get_player_number() != rp->player)) {
      g_ap_room_changed = true;
      g_ap_connectstate.state = AP_CSTATE_SLOTREFUSED;
      snprintf(g_ap_connectstate.last_refuse_reason,
          sizeof(g_ap_connectstate.last_refuse_reason), AP_ROOM_CHANGED_REASON);
      APLOG_ERROR("ROOM CHANGED: was %s/%d/%d, now %s/%d/%d", rp->seed, rp->team, rp->player,
          g_ap->get_seed().c_str(), g_ap->get_team_number(), g_ap->get_player_number());
      return;
    }
    // Set every time. A wrong value type throws (json value()); apclientpp would only printf it
    // and we'd sit on CONNECTED with Start blocked, so refuse with a reason instead.
    ap_slotdata * sd = ap_slotdata_writable();
    try {
      nlohmann::json upgraded = slot_data;
      if (!ap_slotdata_upgrade(upgraded)) {
        int version = upgraded.value("slot_data_version", 1);
        ap_slotdata_init(sd);
        g_ap_connectstate.state = AP_CSTATE_SLOTREFUSED;
        snprintf(g_ap_connectstate.last_refuse_reason, sizeof(g_ap_connectstate.last_refuse_reason),
            "Seed needs a newer client (slot_data v%d, client v%d)", version, AP_SLOTDATA_VERSION);
        APLOG_ERROR("SEED TOO NEW: slot_data v%d, client reads up to v%d", version, AP_SLOTDATA_VERSION);
        return;
      }
      ap_slotdata_fill(sd, upgraded);
    } catch (const nlohmann::json::exception& e) {
      ap_slotdata_init(sd);
      g_ap_connectstate.state = AP_CSTATE_SLOTREFUSED;
      snprintf(g_ap_connectstate.last_refuse_reason, sizeof(g_ap_connectstate.last_refuse_reason),
          "Bad slot_data (client/apworld mismatch?)");
      APLOG_ERROR("BAD SLOT_DATA: %s", e.what());
      return;
    }
    g_ap_connectstate.connect_once = 1;
    g_ap_connectstate.state = AP_CSTATE_CONNECTED;
    // The text client's hints: current list now, then every change (retrieved / set_reply handlers)
    g_ap->SetNotify({ ap_hints_key() });
    g_ap->Get({ ap_hints_key() });
    if(!ap_roomplayer_valid(&g_ap_connectstate.roomplayer)) {
      g_ap_connectstate.roomplayer.player = g_ap->get_player_number();
      g_ap_connectstate.roomplayer.team = g_ap->get_team_number();
      snprintf(g_ap_connectstate.roomplayer.seed, 
          sizeof(g_ap_connectstate.roomplayer.seed), "%s", 
          g_ap->get_seed().c_str());
      APLOG_DEBUG("SET ROOMINFO: %s/%d/%d", g_ap_connectstate.roomplayer.seed,
          g_ap_connectstate.roomplayer.team, g_ap_connectstate.roomplayer.player);
    }
    // Resend any location checks?
    if(!g_ap_checks.empty()) {
      g_ap->LocationChecks(std::list<int64_t>(g_ap_checks.begin(), g_ap_checks.end()));
    }
    if(g_ap_goal) {
      g_ap->StatusUpdate(APClient::ClientStatus::GOAL);
    }
    APLOG_INFO("SLOT CONNECTED: %s", g_ap_config.slotname);
  });

  g_ap->set_slot_refused_handler([](const std::list<std::string>& why) { 
    g_ap_connectstate.state = AP_CSTATE_SLOTREFUSED;
    g_ap_connectstate.last_refuse_reason[0] = 0;
    for (auto& e : why) {
      size_t n = strlen(g_ap_connectstate.last_refuse_reason);
      snprintf(g_ap_connectstate.last_refuse_reason + n, 
          sizeof(g_ap_connectstate.last_refuse_reason) - n, "%s%s", n ? ", " : "", ap_plain(e).c_str());
    }
    APLOG_ERROR("SLOT REFUSED: %s", g_ap_connectstate.last_refuse_reason);
  });

  g_ap->set_socket_error_handler([](const std::string& e) { 
    g_ap_connectstate.state = g_ap_connectstate.connect_once ? AP_CSTATE_RECONNECTING : AP_CSTATE_CONNECTING;
    snprintf(g_ap_connectstate.last_connect_error, 
        sizeof(g_ap_connectstate.last_connect_error), "%s", ap_plain(e).c_str());
    if(!g_ap_outage_logged) {
      g_ap_outage_logged = true;
      APLOG_WARN("CONNECTION ERROR (RECONNECTING): %s", g_ap_connectstate.last_connect_error);
    }
  });

  g_ap->set_socket_disconnected_handler([](void) { 
    g_ap_connectstate.state = g_ap_connectstate.connect_once ? AP_CSTATE_RECONNECTING : AP_CSTATE_CONNECTING;
    snprintf(g_ap_connectstate.last_connect_error, 
        sizeof(g_ap_connectstate.last_connect_error), "Disconnected");
    if(!g_ap_outage_logged) {
      g_ap_outage_logged = true;
      APLOG_WARN("CONNECTION END (RECONNECTING): %s", g_ap_connectstate.last_connect_error);
    }
  });

  g_ap->set_room_info_handler([name, pw] {
    // Seed changed (server restarted with another seed, same slot name): refuse before ConnectSlot,
    // else apclientpp flushes checks queued while disconnected into the new room before
    // slot_connected runs. get_seed() is already RoomInfo's (apclientpp sets it before this callback).
    // Team/player can only be compared once connected (slot_connected handler).
    ap_roomplayer * rp = &g_ap_connectstate.roomplayer;
    if(ap_roomplayer_valid(rp) && g_ap->get_seed() != rp->seed) {
      g_ap_room_changed = true;
      g_ap_connectstate.state = AP_CSTATE_SLOTREFUSED;
      snprintf(g_ap_connectstate.last_refuse_reason,
          sizeof(g_ap_connectstate.last_refuse_reason), AP_ROOM_CHANGED_REASON);
      APLOG_ERROR("ROOM CHANGED: was seed %s, now %s", rp->seed, g_ap->get_seed().c_str());
      return;
    }
    g_ap_connectstate.state = AP_CSTATE_JOINING;
    APLOG_INFO("CONNECTED - WAITING ON SLOT: %s", g_ap_config.slotname);
    g_ap->ConnectSlot(name, pw, 0b111 /* items_handling */);
  });

  // Also runs for the full list on every connect
  g_ap->set_location_checked_handler([](const std::list<int64_t>&) {
    g_ap_checked_generation++;
  });

  g_ap->set_items_received_handler([](const std::list<APClient::NetworkItem>& items) {
    if(g_ap_room_changed) return; // same poll as the refused slot_connected
    if(items.empty()) return; // This is weird, maybe should log?
    int start = items.front().index;
    // Sending a NEW list
    if (start == 0) {
      g_ap_items.clear();
      g_ap_item_senders.clear();
      g_ap_item_locations.clear();
      g_ap_item_flags.clear();
    } else if(start != (int)g_ap_items.size()) {
      APLOG_WARN("ITEM INDEX OUT OF SYNC! %d, expected %d (resyncing)", start, (int)g_ap_items.size());
      g_ap->Sync();
      return;
    }
    for (auto& i : items) { 
      g_ap_items.push_back(i.item);
      g_ap_item_senders.push_back(i.player);
      g_ap_item_locations.push_back(i.location);
      g_ap_item_flags.push_back(i.flags);
    }
  });

  g_ap->set_retrieved_handler([](const std::map<std::string, nlohmann::json>& keys) {
    auto it = keys.find(ap_hints_key());
    if (it != keys.end()) ap_hints_from_json(it->second);
  });
  g_ap->set_set_reply_handler([](const std::string& key, const nlohmann::json& value, const nlohmann::json&) {
    if (key == ap_hints_key()) ap_hints_from_json(value);
  });

  g_ap->set_print_json_handler([](const APClient::PrintJSONArgs & args) {
    if(g_ap_room_changed) return;
    ap_msg_from_print_json(args); // the text client gets everything
    int me = g_ap->get_player_number(); // just look it up again, whatever
    // Quickly filter messages we don't care about (for toast)
    // For this version, we only show item/hint messages for us (to reduce spam, it's a toast)
    if (args.type == "ItemSend" || args.type == "Hint") {
      bool to_me = args.receiving && *args.receiving == me;
      bool from_me = args.item && args.item->player == me;
      if (!to_me && !from_me) {
        return;
      }
    } else if(args.type != "Goal") { // show ALL people's goals
      return;
    }

    // Build our own short lines (no locations, the screen is small) instead of the server's text
    auto player = [](int slot) { return AP_CTRL_GREEN + ap_plain(g_ap->get_player_alias(slot)) + AP_CTRL_WHITE; };
    std::string out;
    if (args.type == "Goal") {
      if (!args.slot) return;
      out = player(*args.slot) + " completed their goal!";
    } else {
      if (!args.item || !args.receiving) return;
      int finder = args.item->player, receiver = *args.receiving;
      std::string item = AP_CTRL_RED + ap_plain(g_ap->get_item_name(args.item->item, g_ap->get_player_game(receiver))) + AP_CTRL_WHITE;
      if (args.type == "Hint") {
        out = "Hint: " + player(receiver) + "'s " + item + " is in " + player(finder) + "'s world";
      } else if (finder == receiver) {
        out = player(finder) + " found their " + item;
      } else {
        out = player(finder) + " sent " + item + " to " + player(receiver);
      }
    }

    g_ap_toast.push_back(out);
    if (g_ap_toast.size() > AP_TOAST_MAX) {
      g_ap_toast.pop_front(); // drop the oldest
    }
  });
}

const ap_config * ap_getconfig(void) {
  return &g_ap_config;
}

const ap_connectstate * ap_getconnectstate(void) {
  return &g_ap_connectstate;
}

void ap_poll(void) {
  if (g_ap) g_ap->poll();
  // Stop for good outside the handlers (destroying the client inside one isn't safe).
  // Set the state again after: closing the socket may run the disconnect handler.
  if (g_ap && g_ap_room_changed) {
    g_ap.reset();
    g_ap_connectstate.state = AP_CSTATE_SLOTREFUSED;
    snprintf(g_ap_connectstate.last_refuse_reason,
        sizeof(g_ap_connectstate.last_refuse_reason), AP_ROOM_CHANGED_REASON);
  }
}         // call once per frame
void ap_stop(void) { 
  g_ap.reset(); 
  ap_connectstate_init(&g_ap_connectstate);
}

