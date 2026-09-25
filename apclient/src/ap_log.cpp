#include "ap_log.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <mutex>

#define AP_LOG_RING_SIZE 512

static std::mutex g_mtx;
static ApLogEntry g_ring[AP_LOG_RING_SIZE];
static uint64_t g_next_seq = 0; // total entries ever written

void ap_log_write(const char * level, const char* fmt, ...) {
  ApLogEntry e;
  e.time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch())
    .count();
  va_list args;
  va_start(args, fmt);
  vsnprintf(e.msg, sizeof e.msg, fmt, args); // truncates, always terminates
  va_end(args);

  std::lock_guard<std::mutex> lock(g_mtx);
  e.seq = g_next_seq++;
  g_ring[e.seq % AP_LOG_RING_SIZE] = e;
  fprintf(stderr, "[AP][%s] %s\n", level, e.msg);
}

size_t ap_log_get(size_t max_count, ApLogEntry* out) {
  if (!out || max_count == 0) { return 0; }
  if(max_count > AP_LOG_RING_SIZE) {
    max_count = AP_LOG_RING_SIZE;
  }
  std::lock_guard<std::mutex> lock(g_mtx);
  size_t s = 0;
  for (s = 0; s < max_count; s++) {
    int64_t pos = g_next_seq - s - 1;
    if(pos < 0) { break; }
    out[s] = g_ring[pos % AP_LOG_RING_SIZE];
  }
  return s;
  }
