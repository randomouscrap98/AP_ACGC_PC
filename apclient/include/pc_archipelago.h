#ifndef PC_ARCHIPELAGO_H
#define PC_ARCHIPELAGO_H

#if defined(_WIN32) && defined(AP_CLIENT_EXPORTS)
#define AP_API __declspec(dllexport)
#else
#define AP_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Begin an AP client handler. The apclientpp library works off events, so you
// setup the connection to begin with and then poll every frame.
AP_API int ap_start();
// Poll every frame
AP_API void ap_poll(void);
// Call on shutdown
AP_API void ap_stop(void);

// Eh, keep it simple I guess
#define AP_CONFIGNAME "ap_config.ini"
#define AP_MAXSTRING       1024

typedef struct {
  char host[AP_MAXSTRING];
  char slotname[AP_MAXSTRING];
  char password[AP_MAXSTRING];
} ap_config;

#ifdef __cplusplus
}
#endif
#endif
