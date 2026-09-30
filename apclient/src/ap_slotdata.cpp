#include "ap_slotdata.h"

#include <apclient.hpp>

static ap_slotdata g_slotdata;

ap_slotdata * ap_getslotdata(void) {
  return &g_slotdata;
}

void ap_slotdata_init(ap_slotdata * sd) {
  memset(sd, 0, sizeof(ap_slotdata));
}

void ap_slotdata_fill(ap_slotdata * sd, const nlohmann::json& slot_data) { 
  snprintf(sd->town_name, sizeof(sd->town_name), "%s",
      slot_data.value("town_name", "pelago").c_str());
  snprintf(sd->player_name, sizeof(sd->player_name), "%s",
      slot_data.value("player_name", "archi").c_str());
  sd->skip_intro = slot_data.value("skip_intro", 0) % 3;
  sd->gender = slot_data.value("gender", 0) % 2;
  sd->face = slot_data.value("face", 0) % 8;
  sd->house = slot_data.value("house", 0) % 4;
  sd->town_fruit = slot_data.value("town_fruit", 0) % 5;
  sd->grass_shape = slot_data.value("grass_shape", 0) % 3;
  sd->train_station = slot_data.value("train_station", -1);
  sd->town_day = slot_data.value("town_day", 0) % 32;
  sd->no_cockroaches = slot_data.value("no_cockroaches", 0) % 2;
  sd->shops_always_open = slot_data.value("shops_always_open", 0) % 2;
  sd->no_weeds = slot_data.value("no_weeds", 0) % 2;
  sd->valid = 1;
}
