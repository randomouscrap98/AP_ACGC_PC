#ifndef AP_LOG_H
#define AP_LOG_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(AP_CLIENT_EXPORTS)
#define AP_LOG_API __declspec(dllexport)
#elif defined(_WIN32)
#define AP_LOG_API __declspec(dllimport)
#else
#define AP_LOG_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Define these to disable the various levels entirely from code
// define AP_LOG_NO_DEBUG
// define AP_LOG_NO_INFO
// define AP_LOG_NO_WARN
// define AP_LOG_NO_ERROR

#define AP_LOG_MSG_MAX 256

struct ApLogEntry {
    uint64_t seq;     // monotonically increasing, 0-based
    int64_t time_ms;  // unix time, ms
    char msg[AP_LOG_MSG_MAX];
};

#if defined(__GNUC__)
#define AP_LOG_PRINTF_FMT __attribute__((format(printf, 2, 3)))
#else
#define AP_LOG_PRINTF_FMT
#endif

AP_LOG_API void ap_log_write(const char * level, const char* fmt, ...) AP_LOG_PRINTF_FMT;

// Copies up to max_count of the most recent entries.
// into out, newest first. Returns the number written.
AP_LOG_API size_t ap_log_get(size_t max_count, struct ApLogEntry* out);

#ifdef __cplusplus
}
#endif

// Use these macros to log so min levels get compiled out entirely
#ifdef AP_LOG_NO_DEBUG 
#define APLOG_DEBUG(...) ((void)0)
#else
#define APLOG_DEBUG(...) ap_log_write("DBG", __VA_ARGS__)
#endif
#ifdef AP_LOG_NO_INFO
#define APLOG_INFO(...) ((void)0)
#else
#define APLOG_INFO(...) ap_log_write("INF", __VA_ARGS__)
#endif
#ifdef AP_LOG_NO_WARN
#define APLOG_WARN(...) ((void)0)
#else
#define APLOG_WARN(...) ap_log_write("WRN", __VA_ARGS__)
#endif
#ifdef AP_LOG_NO_ERROR
#define APLOG_ERROR(...) ((void)0)
#else
#define APLOG_ERROR(...) ap_log_write("ERR", __VA_ARGS__)
#endif

#endif
