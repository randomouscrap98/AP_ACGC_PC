#ifndef AP_COMMON_H
#define AP_COMMON_H

#if defined(_WIN32) && defined(AP_CLIENT_EXPORTS)
#define AP_API __declspec(dllexport)
#else
#define AP_API
#endif

#endif
