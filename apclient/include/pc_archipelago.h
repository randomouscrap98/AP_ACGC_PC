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
AP_API void ap_start(const char* host, const char* slot, const char* password);
// Poll every frame
AP_API void ap_poll(void);
// Call on shutdown
AP_API void ap_stop(void);

#ifdef __cplusplus
}
#endif
#endif
