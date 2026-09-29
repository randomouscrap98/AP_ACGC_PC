#include "pc_ap_logic.h"
#include "ap_archipelago.h"

int pc_ap_start_allowed(void) {
  return ap_roomplayer_valid(&ap_getconnectstate()->roomplayer);
}
