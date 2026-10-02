#ifndef AP_ARCHIPELAGO_H
#define AP_ARCHIPELAGO_H

#include "ap_common.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Begin an AP client handler. The apclientpp library works off events, so you
// setup the connection to begin with and then poll every frame.
AP_API int ap_start(void);
// Poll every frame
AP_API void ap_poll(void);
// Call on shutdown
AP_API void ap_stop(void);

// Eh, keep it simple I guess
#define AP_CONFIGNAME   "ap_config.ini"
#define AP_CERTPATH     "cacert.pem"
#define AP_GAMENAME     "Animal Crossing"
#define AP_MAXSTRING    1024

typedef struct {
  char host[AP_MAXSTRING];
  char slotname[AP_MAXSTRING];
  char password[AP_MAXSTRING];
} ap_config;

// WARN: sends the ACTUAL config being used! Careful with modifications!!
// Shouldn't ever send null
AP_API ap_config * ap_getconfig(void);

#define AP_CSTATE_UNKNOWN       0
#define AP_CSTATE_CONNECTING    1
#define AP_CSTATE_JOINING       2
#define AP_CSTATE_CONNECTED     3
#define AP_CSTATE_RECONNECTING  4
#define AP_CSTATE_SLOTREFUSED   -1

typedef struct {
  char seed[AP_MAXSTRING];
  int team;
  int player;
} ap_roomplayer;

AP_API int ap_roomplayer_valid(const ap_roomplayer * rp);

typedef struct {
  char last_refuse_reason[AP_MAXSTRING];
  char last_connect_error[AP_MAXSTRING];
  ap_roomplayer roomplayer; // only set once!!
  int state;
  int connect_once; // ever connected once
} ap_connectstate;

// WARN: sends the ACTUAL connect state being tracked! Careful with modifications!
// Shouldn't ever send null
AP_API ap_connectstate * ap_getconnectstate(void);

// Get the ap item at ap index idx
AP_API int64_t ap_getitem(size_t idx);
// Get total amount of items in list right now
AP_API size_t ap_getitemcount(void);

#ifdef __cplusplus
}
#endif
#endif
