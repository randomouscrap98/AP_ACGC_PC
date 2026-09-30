#include "ap_archipelago.h"
#include "ap_slotdata.h"

#include <apclient.hpp>
#include <apuuid.hpp>
#include <memory>
#include <fstream>
#include <sstream>
#include <string>
#include "ap_log.h"

#define INI_IMPLEMENTATION
#include "ini.h"


static std::unique_ptr<APClient> g_ap;
static ap_config g_ap_config;
static ap_connectstate g_ap_connectstate;

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
  if (hostp == INI_NOT_FOUND || slotp == INI_NOT_FOUND) {
    APLOG_WARN("Malformed AP config file at %s (needs host and slotname)", path);
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
}

int ap_roomplayer_valid(const ap_roomplayer * rp) {
  return strlen(rp->seed) > 0;
}

int ap_start(void) {
                 
  APLOG_INFO("Starting AP system");

  ap_config_init(&g_ap_config);
  ap_connectstate_init(&g_ap_connectstate);

  ap_slotdata * sd = ap_getslotdata();
  ap_slotdata_init(sd);

  int result = load_config(AP_CONFIGNAME, &g_ap_config);
  if (result) { return result; }

  std::string uuid = ap_get_uuid("uuid");          // persists a uuid in a file
  std::string pw = g_ap_config.password, name = g_ap_config.slotname;

  g_ap = std::make_unique<APClient>(uuid, AP_GAMENAME, g_ap_config.host, AP_CERTPATH);
  g_ap_connectstate.state = AP_CSTATE_CONNECTING;

  g_ap->set_slot_connected_handler([](const nlohmann::json& slot_data) { 
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
    APLOG_INFO("SLOT CONNECTED: %s", g_ap_config.slotname);
  });

  g_ap->set_slot_refused_handler([](const std::list<std::string>& why) { 
    g_ap_connectstate.state = AP_CSTATE_SLOTREFUSED;
    g_ap_connectstate.last_refuse_reason[0] = 0;
    for (auto& e : why) {
      size_t n = strlen(g_ap_connectstate.last_refuse_reason);
      snprintf(g_ap_connectstate.last_refuse_reason + n, 
          sizeof(g_ap_connectstate.last_refuse_reason) - n, "%s%s", n ? ", " : "", e.c_str());
    }
    APLOG_ERROR("SLOT REFUSED: %s", g_ap_connectstate.last_refuse_reason);
  });

  g_ap->set_socket_error_handler([](const std::string& e) { 
    g_ap_connectstate.state = g_ap_connectstate.connect_once ? AP_CSTATE_RECONNECTING : AP_CSTATE_CONNECTING;
    snprintf(g_ap_connectstate.last_connect_error, 
        sizeof(g_ap_connectstate.last_connect_error), "%s", e.c_str());
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
    for (auto& i : items) { 
      /* queue i.item, apply at a safe point */ 
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
}         // call once per frame
void ap_stop(void) { 
  g_ap.reset(); 
  ap_connectstate_init(&g_ap_connectstate);
}

