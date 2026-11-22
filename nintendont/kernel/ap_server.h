// AP: RAM server for the Animal Crossing AP client, plus a small log (ap_server.c)
#ifndef AP_SERVER_H
#define AP_SERVER_H
#include "global.h"

#define AP_SERVER_PORT 38291

// Starts the server thread (network up, listen); call once after SOCKInit
void ApServerInit(void);
// A line for the log ring (\r\n at the end, like dbgprintf)
void ApLog(const char *fmt, ...);
// Main loop only: the log has news worth writing to the SD card now
bool ApLogFlushDue(void);
void ApLogFlush(void);

#endif
