#include "ap_slotdata.h"
#include "ap_log.h"
#include "ap_slotdata_upgrades.h"
#include "pc_ap_slotdata_parse.h"

#include <nlohmann/json.hpp>
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

// WARN: keep offline_example.json (next to this dir's CMakeLists.txt) in sync with the keys that matter offline
// (new town, intro, QoL, loan amounts)
void ap_slotdata_fill(ap_slotdata * sd, const nlohmann::json& slot_data) {
  // The parser itself is plain C (pc/src/pc_ap_slotdata_parse.c, compiled into this DLL too): it reads the
  // compact JSON text, which is exactly what the GC client writes into the game's mailbox.
  std::string text = slot_data.dump();
  if (!pc_ap_slotdata_parse(text.data(), (int)text.size(), sd)) {
    ap_slotdata_init(sd); // can't happen for a dumped object (only past PC_AP_SD_MAX_TOKENS): no half-filled struct
  }
}
