#include "pc_archipelago.h"

#include <apclient.hpp>
#include <apuuid.hpp>
#include <memory>

static std::unique_ptr<APClient> g_ap;

void ap_start(const char* host, const char* slot, const char* password) {
  std::string uuid = ap_get_uuid("uuid");          // persists a uuid in a file
  std::string pw = password, name = slot;

  g_ap = std::make_unique<APClient>(uuid, "Animal Crossing", host, "cacert.pem");

  g_ap->set_socket_error_handler([](const std::string& e) { /* show "can't reach server" */ });
  g_ap->set_room_info_handler([name, pw] {
      g_ap->ConnectSlot(name, pw, 0b111 /* items_handling */);
      });
  g_ap->set_slot_connected_handler([](const nlohmann::json& slot_data) { /* ready */ });
  g_ap->set_slot_refused_handler([](const std::list<std::string>& why) { /* bad slot/pw */ });
  g_ap->set_items_received_handler([](const std::list<APClient::NetworkItem>& items) {
      for (auto& i : items) { /* queue i.item, apply at a safe point */ }
      });
}

void ap_poll() { if (g_ap) g_ap->poll(); }         // call once per frame
void ap_stop() { g_ap.reset(); }

