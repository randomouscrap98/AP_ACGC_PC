// The AP client side of beach hints: the three calls the pc_ap_logic.c facade makes. The AP client picks the
// location (it has the server's missing locations and hint list) and tells the game the hint's names, so the game
// only asks and shows. PC: ap_client.dll (pc/src/pc_ap_beach_hint_client.c); GC: the RAM mailbox, the picking is in
// apworld gc_client.py (gc/gc_ap_beach_hint_client.c).
// Built where AP_BEACH_HINT is defined (both game builds); the C unit tests of pc_ap_logic.c build without it, since
// neither backend has stubs there.
#ifndef PC_AP_BEACH_HINT_CLIENT_H
#define PC_AP_BEACH_HINT_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

// Longest name the client returns per field (the letter cuts them by font width anyway)
#define PC_AP_BEACH_HINT_NAME_LEN 64

// One hint's text, as the letter needs it (pc_ap_beach_hint_body)
typedef struct {
  char player[PC_AP_BEACH_HINT_NAME_LEN];   // the receiving player
  char item[PC_AP_BEACH_HINT_NAME_LEN];     // the item's name in that player's game
  char location[PC_AP_BEACH_HINT_NAME_LEN]; // the location's name in our game
} pc_ap_beach_hint_answer;

// How many of this slot's locations are missing and not hinted with us as the finder; 0 when none or not connected.
// The facade's spawn check asks this on every town shell spawn.
int pc_ap_beach_hint_eligible_count(void);

// Ask for a hint on one of them, picked uniformly at random (PC: here; GC: by gc_client.py). Returns what
// pc_ap_beach_hint_poll wants back, -1 = nothing was asked (nothing eligible, or not connected).
int pc_ap_beach_hint_request(void);

// Poll a request: 1 = answered, out filled (all names "" = there was no eligible location left). 0 = still waiting.
// PC: the answer is the server's hint for the request in the hint list (it arrives after a reconnect too).
// GC: the client's mailbox answer; a request from an earlier game session, or one the client never answered (it
// restarted, say), reads as answered with no names, so nothing waits forever.
int pc_ap_beach_hint_poll(int request, pc_ap_beach_hint_answer* out);

#ifdef __cplusplus
}
#endif

#endif
