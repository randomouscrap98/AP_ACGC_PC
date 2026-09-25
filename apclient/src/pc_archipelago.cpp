#include "pc_archipelago.h"

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

int ap_start() {
                 
  APLOG_INFO("Starting AP system");

  ap_config config;
  int result = load_config(AP_CONFIGNAME, &config);
  if (result) { return result; }

  std::string uuid = ap_get_uuid("uuid");          // persists a uuid in a file
  std::string pw = config.password, name = config.slotname;

  g_ap = std::make_unique<APClient>(uuid, "Animal Crossing", config.host, "cacert.pem");

  g_ap->set_socket_error_handler([](const std::string& e) { /* show "can't reach server" */ });
  g_ap->set_room_info_handler([name, pw] {
      g_ap->ConnectSlot(name, pw, 0b111 /* items_handling */);
      });
  g_ap->set_slot_connected_handler([](const nlohmann::json& slot_data) { /* ready */ });
  g_ap->set_slot_refused_handler([](const std::list<std::string>& why) { /* bad slot/pw */ });
  g_ap->set_items_received_handler([](const std::list<APClient::NetworkItem>& items) {
      for (auto& i : items) { /* queue i.item, apply at a safe point */ }
      });

  return 0;
}

void ap_poll() { if (g_ap) g_ap->poll(); }         // call once per frame
void ap_stop() { g_ap.reset(); }

