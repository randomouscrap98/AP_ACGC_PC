#include "ap_archipelago.h"
#include "ap_slotdata.h"

#include <apclient.hpp>
#include <apuuid.hpp>
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
static ap_config g_ap_config;
static ap_connectstate g_ap_connectstate;
static std::vector<int64_t> g_ap_items;
static std::set<int64_t> g_ap_checks;
static int g_ap_goal = 0;
// Connected to a different room than the first one (server restarted with a
// new seed): the game's save belongs to the old room, so ap_poll stops the client
static bool g_ap_room_changed = false;
#define AP_ROOM_CHANGED_REASON "Seed changed, restart"
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

size_t ap_getitemcount(void) {
  return g_ap_items.size();
}

int64_t ap_getitem(size_t idx) {
  if(idx >= g_ap_items.size()) {
    return -1;
  }
  return g_ap_items.at(idx);
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

int ap_location_checked(int64_t id) {
  if(g_ap_checks.count(id)) {
    return 1;
  }
  return g_ap && g_ap->get_checked_locations().count(id);
}

// apclientpp drops a StatusUpdate made while not connected, so remember it and
// resend on every connect
void ap_send_goal(void) {
  if(!g_ap_goal && g_ap) {
    g_ap->StatusUpdate(APClient::ClientStatus::GOAL);
  }
  g_ap_goal = 1;
}

// Attempt to load config at given path. If it does not exist or
// some other error occurs, returns non-zero
static int load_config(const char * path, ap_config * out) {
  APLOG_DEBUG("Loading AP config file %s", path);
  // Reading files in c++ is weird... I'm used to c. sorry if this is weird?
  std::ifstream f(path, std::ios::binary);
  if(!f) {
    APLOG_WARN("Can't find AP config file at %s", path);
    return -1;
  }
  std::stringstream ss;
  ss << f.rdbuf();
  std::string data = ss.str();

  ini_t* ini = ini_load(data.c_str(), NULL); // null for normal malloc?
  int hostp = ini_find_property(ini, INI_GLOBAL_SECTION, "host", 0);
  int slotp = ini_find_property(ini, INI_GLOBAL_SECTION, "slotname", 0);
  int passwordp = ini_find_property(ini, INI_GLOBAL_SECTION, "password", 0);
  int offlinep = ini_find_property(ini, INI_GLOBAL_SECTION, "offline", 0);
  // offline mode doesn't need a server
  if (offlinep != INI_NOT_FOUND) {
    snprintf(out->offline, sizeof(out->offline), "%s", ini_property_value(ini, INI_GLOBAL_SECTION, offlinep));
    ini_destroy(ini);
    return 0;
  }
  if (hostp == INI_NOT_FOUND || slotp == INI_NOT_FOUND) {
    APLOG_WARN("Malformed AP config file at %s (needs host and slotname, or offline)", path);
    ini_destroy(ini);
    return 1;
  }
  snprintf(out->host, sizeof(out->host), "%s", ini_property_value(ini, INI_GLOBAL_SECTION, hostp));
  snprintf(out->slotname, sizeof(out->slotname), "%s", ini_property_value(ini, INI_GLOBAL_SECTION, slotp));
  // password is optional
  snprintf(out->password, sizeof(out->password), "%s",
           passwordp == INI_NOT_FOUND ? "" : ini_property_value(ini, INI_GLOBAL_SECTION, passwordp));

  ini_destroy(ini);
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
    return -1;
  }
  nlohmann::json data = nlohmann::json::parse(f, nullptr, false);
  if (!data.is_object() || !data.contains("seed") || !data["seed"].is_string() ||
      data["seed"].get<std::string>().empty()) {
    APLOG_WARN("Malformed offline file at %s (needs a json object with a seed)", path);
    return 1;
  }

  ap_roomplayer * rp = &g_ap_connectstate.roomplayer;
  ap_slotdata * sd = ap_getslotdata();
  // Hand-written file: a wrong value type (value() throws) is an error, not a crash
  try {
    rp->team = data.value("team", 0);
    rp->player = data.value("player", 1);
    ap_slotdata_fill(sd, data);
  } catch (const nlohmann::json::exception& e) {
    APLOG_WARN("Malformed offline file at %s: %s", path, e.what());
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
  sd->goal = 0;
  for (int i = 0; i < AP_LOAN_NUM; i++) { sd->loan_checks[i] = 0; }

  g_ap_connectstate.state = AP_CSTATE_OFFLINE;
  APLOG_INFO("OFFLINE: %s/%d/%d", rp->seed, rp->team, rp->player);
  return 0;
}

int ap_start(void) {
                 
  APLOG_INFO("Starting AP system");

  ap_config_init(&g_ap_config);
  ap_connectstate_init(&g_ap_connectstate);

  ap_slotdata * sd = ap_getslotdata();
  ap_slotdata_init(sd);

  int result = load_config(AP_CONFIGNAME, &g_ap_config);
  if (result) { return result; }
  if (g_ap_config.offline[0]) { return ap_start_offline(g_ap_config.offline); }

  std::string uuid = ap_get_uuid("uuid");          // persists a uuid in a file
  std::string pw = g_ap_config.password, name = g_ap_config.slotname;

  g_ap = std::make_unique<APClient>(uuid, AP_GAMENAME, g_ap_config.host, AP_CERTPATH);
  g_ap_connectstate.state = AP_CSTATE_CONNECTING;

  g_ap->set_slot_connected_handler([](const nlohmann::json& slot_data) {
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
    g_ap_connectstate.connect_once = 1;
    g_ap_connectstate.state = AP_CSTATE_CONNECTED;
    // Set every time
    ap_slotdata * sd = ap_getslotdata();
    ap_slotdata_fill(sd, slot_data);
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
    APLOG_ERROR("CONNECTION ERROR (RECONNECTING): %s", g_ap_connectstate.last_connect_error);
  });

  g_ap->set_socket_disconnected_handler([](void) { 
    g_ap_connectstate.state = g_ap_connectstate.connect_once ? AP_CSTATE_RECONNECTING : AP_CSTATE_CONNECTING;
    snprintf(g_ap_connectstate.last_connect_error, 
        sizeof(g_ap_connectstate.last_connect_error), "Disconnected");
    APLOG_ERROR("CONNECTION END (RECONNECTING): %s", g_ap_connectstate.last_connect_error);
  });

  g_ap->set_room_info_handler([name, pw] {
    g_ap_connectstate.state = AP_CSTATE_JOINING;
    APLOG_INFO("CONNECTED - WAITING ON SLOT: %s", g_ap_config.slotname);
    g_ap->ConnectSlot(name, pw, 0b111 /* items_handling */);
  });

  g_ap->set_items_received_handler([](const std::list<APClient::NetworkItem>& items) {
    if(g_ap_room_changed) return; // same poll as the refused slot_connected
    if(items.empty()) return; // This is weird, maybe should log?
    int start = items.front().index;
    // Sending a NEW list
    if (start == 0) {
      g_ap_items.clear();
    } else if(start != (int)g_ap_items.size()) {
      APLOG_WARN("ITEM INDEX OUT OF SYNC! %d, expected %d (resyncing)", start, (int)g_ap_items.size());
      g_ap->Sync();
      return;
    }
    for (auto& i : items) { 
      g_ap_items.push_back(i.item);
    }
  });

  g_ap->set_print_json_handler([](const APClient::PrintJSONArgs & args) {
    if(g_ap_room_changed) return;
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

  return 0;
}

ap_config * ap_getconfig(void) {
  return &g_ap_config;
}

ap_connectstate * ap_getconnectstate(void) {
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

