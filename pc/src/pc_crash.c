#include "pc_platform.h"
#include "pc_crash.h"
#include "pc_gpu.h"
#include "ap_archipelago.h"
#include "ap_log.h"

#include "m_common_data.h"
#include "m_play.h"
#include "m_player_lib.h"
#include "m_malloc.h"
#include "m_kankyo.h"
#include "game.h"

#include <stdlib.h>

#ifdef _WIN32
#include <tlhelp32.h>
#else
#include <signal.h>
#include <ucontext.h>
#include <link.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

// As on Windows (32-bit), so the shared code and its %lu formats are the same on both
typedef unsigned long DWORD;
#endif

// Formatting without malloc or floats for the handler's short strings
#ifdef _WIN32
#define crash_sprintf wsprintfA
#else
#define crash_sprintf sprintf
#endif

#define CRASH_SCENES 16    // scene changes kept
#define CRASH_STACK_BYTES 0x4000
#define CRASH_STACK_HITS 64
#define CRASH_MODULES 128
#define CRASH_LOG_LINES 40

// Filled during normal play, so the handler only prints memory it owns
typedef struct {
  int frame; // pc_crash_note_frame calls (play frames only)
  DWORD tick_ms; // crash_now_ms at the last snapshot
  int scene;
  lbRTC_time_c date;
  int weather;
  int weather_intensity;
  int has_player;
  int player_x, player_y, player_z;
  int ap_state;
} CrashFrame;

typedef struct {
  int frame;
  DWORD tick_ms;
  int from, to;
  size_t heap_free, heap_max_block, heap_alloc;
} CrashScene;

typedef struct {
  char name[64];
  DWORD base, size;
} CrashModule;

static char s_os[128] = "?";
static char s_gl[384] = "?";
static CrashFrame s_frame;
static CrashScene s_scenes[CRASH_SCENES];
static int s_scene_count; // total ever, ring index = count % CRASH_SCENES
static int s_last_scene = -1;
static DWORD s_start_ms;

// Handler state: static so a stack overflow has little left to use
static char s_line[1024];
// Taken at startup (pc_crash_init_info). Windows takes the list again at crash time and uses
// that one if it works (it can't if the crash broke the heap), so DLLs loaded later show up too.
static CrashModule s_modules_init[CRASH_MODULES];
static int s_module_count_init;
static CrashModule* s_modules = s_modules_init;
static int s_module_count;
static struct ApLogEntry s_log[CRASH_LOG_LINES];

// Platform part 1: output, safe reads, time, module list. Used by the shared report code below.
#ifdef _WIN32

static volatile LONG s_in_handler;
static HANDLE s_file = INVALID_HANDLE_VALUE;
static CrashModule s_modules_crash[CRASH_MODULES];

static void crash_write(const char* fmt, ...) {
  va_list args;
  DWORD written;
  va_start(args, fmt);
  int n = wvsprintfA(s_line, fmt, args); // no floats, no malloc; caps at 1024
  va_end(args);
  if(n > 0 && s_file != INVALID_HANDLE_VALUE) {
    WriteFile(s_file, s_line, (DWORD)n, &written, NULL);
  }
}

static void crash_flush(void) {
  if(s_file != INVALID_HANDLE_VALUE) {
    FlushFileBuffers(s_file);
  }
}

// Reads that fail instead of faulting
static int crash_read(const void* addr, void* out, SIZE_T size) {
  SIZE_T got = 0;
  return ReadProcessMemory(GetCurrentProcess(), addr, out, size, &got) && got == size;
}

static DWORD crash_now_ms(void) {
  return GetTickCount();
}

static int crash_collect_modules(CrashModule* out) {
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
  MODULEENTRY32 me;
  int count = 0;

  if(snap == INVALID_HANDLE_VALUE) {
    return 0;
  }
  me.dwSize = sizeof(me);
  if(Module32First(snap, &me)) {
    do {
      CrashModule* m = &out[count++];
      lstrcpynA(m->name, me.szModule, sizeof(m->name));
      m->base = (DWORD)(uintptr_t)me.modBaseAddr;
      m->size = me.modBaseSize;
    } while(count < CRASH_MODULES && Module32Next(snap, &me));
  }
  CloseHandle(snap);
  return count;
}

#else

static volatile sig_atomic_t s_in_handler;
static int s_fd = -1;
// Own signal stack, so a stack overflow can still be reported
static char s_altstack[0x10000];

// vsnprintf: no malloc for these formats (no floats)
static void crash_write(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int n = vsnprintf(s_line, sizeof(s_line), fmt, args);
  va_end(args);
  if(n > (int)sizeof(s_line) - 1) {
    n = sizeof(s_line) - 1;
  }
  if(n > 0 && s_fd >= 0) {
    ssize_t written = write(s_fd, s_line, (size_t)n);
    (void)written;
  }
}

// write() already hands the data to the kernel
static void crash_flush(void) {}

// Reads that fail instead of faulting
static int crash_read(const void* addr, void* out, size_t size) {
  struct iovec local = { out, size };
  struct iovec remote = { (void*)addr, size };
  return process_vm_readv(getpid(), &local, 1, &remote, 1, 0) == (ssize_t)size;
}

static DWORD crash_now_ms(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (DWORD)t.tv_sec * 1000 + (DWORD)(t.tv_nsec / 1000000);
}

typedef struct {
  CrashModule* out;
  int count;
} CrashModuleList;

static int crash_add_module(struct dl_phdr_info* info, size_t size, void* data) {
  CrashModuleList* list = (CrashModuleList*)data;
  DWORD lo = 0xFFFFFFFF, hi = 0;
  (void)size;
  if(list->count >= CRASH_MODULES) {
    return 1;
  }
  for(int i = 0; i < info->dlpi_phnum; i++) {
    const ElfW(Phdr)* ph = &info->dlpi_phdr[i];
    if(ph->p_type == PT_LOAD) {
      DWORD start = (DWORD)(info->dlpi_addr + ph->p_vaddr);
      DWORD end = start + (DWORD)ph->p_memsz;
      if(start < lo) lo = start;
      if(end > hi) hi = end;
    }
  }
  if(hi == 0) {
    return 0;
  }
  // The exe has an empty name
  const char* name = info->dlpi_name[0] != '\0' ? info->dlpi_name : "AnimalCrossing";
  const char* slash = strrchr(name, '/');
  CrashModule* m = &list->out[list->count++];
  snprintf(m->name, sizeof(m->name), "%s", slash != NULL ? slash + 1 : name);
  m->base = lo;
  m->size = hi - lo;
  return 0;
}

static int crash_collect_modules(CrashModule* out) {
  CrashModuleList list = { out, 0 };
  dl_iterate_phdr(crash_add_module, &list);
  return list.count;
}

#endif

// Shared report code

static const CrashModule* crash_module_of(DWORD addr) {
  for(int i = 0; i < s_module_count; i++) {
    if(addr - s_modules[i].base < s_modules[i].size) {
      return &s_modules[i];
    }
  }
  return NULL;
}

// "module+0xoffset", or the bare address
static void crash_format_addr(char* out, DWORD addr) {
  const CrashModule* m = crash_module_of(addr);
  if(m != NULL) {
    crash_sprintf(out, "%s+0x%lx", m->name, addr - m->base);
  } else {
    crash_sprintf(out, "0x%08lx", addr);
  }
}

// Likely a return address: the bytes before it are a call (E8 rel32, or FF /2 in its
// 2/3/6/7-byte forms). Cuts most of the stale data out of the stack scan.
static int crash_after_call(DWORD addr) {
  unsigned char b[7];
  if(crash_module_of(addr) == NULL || !crash_read((const void*)(uintptr_t)(addr - 7), b, sizeof(b))) {
    return 0;
  }
  if(b[2] == 0xE8) {
    return 1;
  }
  static const int back[] = { 2, 3, 6, 7 };
  for(int i = 0; i < 4; i++) {
    int at = 7 - back[i];
    if(b[at] == 0xFF && ((b[at + 1] >> 3) & 7) == 2) {
      return 1;
    }
  }
  return 0;
}

static void crash_write_stack(DWORD eip, DWORD esp) {
  DWORD end = esp + CRASH_STACK_BYTES; // the scan also stops at the stack's top (first failed read)
  char where[96];
  int hits = 0;

  crash_write("\r\n[stack scan] return-address-like values from esp up (newest first)\r\n");
  crash_format_addr(where, eip);
  crash_write("  eip      %s\r\n", where);
  for(DWORD p = esp & ~3u; p + 4 <= end && hits < CRASH_STACK_HITS; p += 4) {
    DWORD v = 0;
    if(!crash_read((const void*)(uintptr_t)p, &v, sizeof(v))) {
      break;
    }
    if(crash_after_call(v)) {
      crash_format_addr(where, v);
      crash_write("  esp+%04lx %s\r\n", p - esp, where);
      hits++;
    }
  }
  crash_flush();
}

static void crash_write_state(void) {
  CrashFrame f = s_frame;
  GAME* game = NULL;
  unsigned char points[2];
  DWORD now = crash_now_ms();

  crash_write("\r\n[game]\r\nuptime %lu s\r\n", (now - s_start_ms) / 1000);
  if(crash_read(&gamePT, &game, sizeof(game)) && game != NULL &&
     crash_read(&game->doing_point, points, sizeof(points))) {
    crash_write("doing_point %d, doing_point_specific 0x%02x\r\n", points[0], points[1]);
  }
  if(f.frame == 0) {
    crash_write("no play frame yet\r\n");
  } else {
    crash_write("last play frame %d, %lu ms before the crash\r\n", f.frame, now - f.tick_ms);
    crash_write("scene %d\r\n", f.scene);
    crash_write("date %d-%02d-%02d %02d:%02d:%02d\r\n", f.date.year, f.date.month, f.date.day, f.date.hour,
                f.date.min, f.date.sec);
    crash_write("weather %d intensity %d\r\n", f.weather, f.weather_intensity);
    if(f.has_player) {
      crash_write("player %d %d %d\r\n", f.player_x, f.player_y, f.player_z);
    } else {
      crash_write("player none\r\n");
    }
    crash_write("ap state %d\r\n", f.ap_state);
  }

  crash_write("\r\n[scene changes] oldest first: frame, ms since start, from -> to, heap free/max block/allocated\r\n");
  int first = s_scene_count > CRASH_SCENES ? s_scene_count - CRASH_SCENES : 0;
  for(int i = first; i < s_scene_count; i++) {
    const CrashScene* s = &s_scenes[i % CRASH_SCENES];
    crash_write("  %d %lu %d -> %d  %lu/%lu/%lu\r\n", s->frame, s->tick_ms - s_start_ms, s->from, s->to,
                (DWORD)s->heap_free, (DWORD)s->heap_max_block, (DWORD)s->heap_alloc);
  }
  crash_flush();
}

static void crash_write_modules(void) {
  crash_write("\r\n[modules]\r\n");
  for(int i = 0; i < s_module_count; i++) {
    crash_write("  %08lx %08lx %s\r\n", s_modules[i].base, s_modules[i].size, s_modules[i].name);
  }
  crash_flush();
}

static void crash_write_log(void) {
  size_t n = ap_log_try_get(CRASH_LOG_LINES, s_log);
  crash_write("\r\n[ap log] newest first%s\r\n", n == 0 ? " (empty or locked)" : "");
  for(size_t i = 0; i < n; i++) {
    crash_write("  %s\r\n", s_log[i].msg);
  }
  crash_flush();
}

static void crash_write_header(void) {
  crash_write("Animal Crossing AP crash report\r\nversion %s\r\nos %s\r\ngl %s\r\n\r\n", PC_AP_VERSION, s_os,
              s_gl);
}

// Platform part 2: the handler and setup
#ifdef _WIN32

static const char* crash_code_name(DWORD code) {
  switch(code) {
    case EXCEPTION_ACCESS_VIOLATION: return "access violation";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "array bounds exceeded";
    case EXCEPTION_DATATYPE_MISALIGNMENT: return "misaligned data";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "float divide by zero";
    case EXCEPTION_INT_DIVIDE_BY_ZERO: return "integer divide by zero";
    case EXCEPTION_ILLEGAL_INSTRUCTION: return "illegal instruction";
    case EXCEPTION_IN_PAGE_ERROR: return "in-page error";
    case EXCEPTION_PRIV_INSTRUCTION: return "privileged instruction";
    case EXCEPTION_STACK_OVERFLOW: return "stack overflow";
    case 0xC0000409: return "stack buffer overrun / fail-fast";
    case 0xC0000374: return "heap corruption";
    default: return "unknown";
  }
}

static void crash_write_exception(EXCEPTION_POINTERS* ep) {
  EXCEPTION_RECORD* er = ep->ExceptionRecord;
  CONTEXT* c = ep->ContextRecord;
  char where[96];

  crash_format_addr(where, (DWORD)(uintptr_t)er->ExceptionAddress);
  crash_write("[exception]\r\ncode 0x%08lx (%s) at %s\r\n", er->ExceptionCode,
              crash_code_name(er->ExceptionCode), where);
  if(er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2) {
    static const char* kinds[] = { "reading", "writing", "?", "?", "?", "?", "?", "?", "executing" };
    ULONG_PTR kind = er->ExceptionInformation[0];
    crash_write("%s 0x%08lx\r\n", kind <= 8 ? kinds[kind] : "?", (DWORD)er->ExceptionInformation[1]);
  }
  crash_write("thread %lu\r\n", GetCurrentThreadId());
  crash_write("\r\n[registers]\r\neip %08lx esp %08lx ebp %08lx eflags %08lx\r\n", c->Eip, c->Esp, c->Ebp,
              c->EFlags);
  crash_write("eax %08lx ebx %08lx ecx %08lx edx %08lx esi %08lx edi %08lx\r\n", c->Eax, c->Ebx, c->Ecx,
              c->Edx, c->Esi, c->Edi);
  crash_flush();
}

static LONG WINAPI pc_crash_handler(EXCEPTION_POINTERS* ep) {
  char name[64];
  char msg[256];
  SYSTEMTIME t;

  // A crash inside the report: give up quietly
  if(InterlockedExchange(&s_in_handler, 1) != 0) {
    return EXCEPTION_EXECUTE_HANDLER;
  }
  GetLocalTime(&t);
  wsprintfA(name, "crash_%04d%02d%02d_%02d%02d%02d.txt", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute,
            t.wSecond);
  s_file = CreateFileA(name, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

  // Most useful first, each section flushed, so a failure late still leaves the rest
  crash_write_header();
  s_module_count = s_module_count_init;
  int n = crash_collect_modules(s_modules_crash);
  if(n > 0) {
    s_modules = s_modules_crash;
    s_module_count = n;
  }
  crash_write_exception(ep);
  crash_write_stack(ep->ContextRecord->Eip, ep->ContextRecord->Esp);
  crash_write_state();
  crash_write_modules();
  crash_write_log();
  if(s_file != INVALID_HANDLE_VALUE) {
    CloseHandle(s_file);
  }

  if(s_file != INVALID_HANDLE_VALUE) {
    wsprintfA(msg,
              "The game crashed.\n\nDetails were saved to %s in the game folder. "
              "Please send that file with your bug report.",
              name);
  } else {
    wsprintfA(msg, "The game crashed, and the crash report couldn't be saved (error %lu).", GetLastError());
  }
  MessageBoxA(NULL, msg, "Animal Crossing - Crash", MB_OK | MB_ICONERROR | MB_TOPMOST);
  return EXCEPTION_EXECUTE_HANDLER;
}

void pc_crash_init(void) {
  s_start_ms = crash_now_ms();
  SetUnhandledExceptionFilter(pc_crash_handler);
}

static void crash_init_os(void) {
  typedef const char*(CDECL * WineVersionFn)(void);
  typedef LONG(WINAPI * RtlGetVersionFn)(OSVERSIONINFOW*);
  HMODULE ntdll = GetModuleHandleA("ntdll.dll");
  WineVersionFn wine = ntdll ? (WineVersionFn)(void*)GetProcAddress(ntdll, "wine_get_version") : NULL;
  RtlGetVersionFn rtl = ntdll ? (RtlGetVersionFn)(void*)GetProcAddress(ntdll, "RtlGetVersion") : NULL;
  OSVERSIONINFOW v;

  ZeroMemory(&v, sizeof(v));
  v.dwOSVersionInfoSize = sizeof(v);
  // RtlGetVersion: GetVersionEx lies to unmanifested exes
  if(rtl != NULL && rtl(&v) == 0) {
    snprintf(s_os, sizeof(s_os), "Windows %lu.%lu build %lu", v.dwMajorVersion, v.dwMinorVersion,
             v.dwBuildNumber);
  }
  if(wine != NULL) {
    size_t len = strlen(s_os);
    snprintf(s_os + len, sizeof(s_os) - len, " (Wine %s)", wine());
  }
}

#else

static const char* crash_signal_name(int sig) {
  switch(sig) {
    case SIGSEGV: return "segmentation fault";
    case SIGBUS: return "bus error";
    case SIGILL: return "illegal instruction";
    case SIGFPE: return "arithmetic exception";
    case SIGABRT: return "abort";
    default: return "unknown";
  }
}

static void crash_write_signal(int sig, const siginfo_t* si, const ucontext_t* uc) {
  const greg_t* g = uc->uc_mcontext.gregs;
  char where[96];

  crash_format_addr(where, (DWORD)g[REG_EIP]);
  crash_write("[signal]\r\nsignal %d (%s) code %d at %s\r\n", sig, crash_signal_name(sig), si->si_code, where);
  if(sig == SIGSEGV || sig == SIGBUS) {
    crash_write("address 0x%08lx\r\n", (DWORD)(uintptr_t)si->si_addr);
  }
  crash_write("thread %lu\r\n", (DWORD)gettid());
  crash_write("\r\n[registers]\r\neip %08lx esp %08lx ebp %08lx eflags %08lx\r\n", (DWORD)g[REG_EIP],
              (DWORD)g[REG_ESP], (DWORD)g[REG_EBP], (DWORD)g[REG_EFL]);
  crash_write("eax %08lx ebx %08lx ecx %08lx edx %08lx esi %08lx edi %08lx\r\n", (DWORD)g[REG_EAX],
              (DWORD)g[REG_EBX], (DWORD)g[REG_ECX], (DWORD)g[REG_EDX], (DWORD)g[REG_ESI], (DWORD)g[REG_EDI]);
  crash_flush();
}

static void pc_crash_handler(int sig, siginfo_t* si, void* ctx) {
  const ucontext_t* uc = (const ucontext_t*)ctx;
  char name[64];
  char msg[256];
  time_t now = time(NULL);
  struct tm t;

  // A crash inside the report: give up quietly
  if(s_in_handler) {
    signal(sig, SIG_DFL);
    raise(sig);
    return;
  }
  s_in_handler = 1;
  localtime_r(&now, &t);
  snprintf(name, sizeof(name), "crash_%04d%02d%02d_%02d%02d%02d.txt", t.tm_year + 1900, t.tm_mon + 1,
           t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
  s_fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0644);

  // Most useful first, so a failure late still leaves the rest
  crash_write_header();
  s_module_count = s_module_count_init;
  crash_write_signal(sig, si, uc);
  crash_write_stack((DWORD)uc->uc_mcontext.gregs[REG_EIP], (DWORD)uc->uc_mcontext.gregs[REG_ESP]);
  crash_write_state();
  crash_write_modules();
  crash_write_log();
  if(s_fd >= 0) {
    close(s_fd);
    snprintf(msg, sizeof(msg),
             "The game crashed.\n\nDetails were saved to %s in the game folder. "
             "Please send that file with your bug report.",
             name);
  } else {
    snprintf(msg, sizeof(msg), "The game crashed, and the crash report couldn't be saved (%s).",
             strerror(*__errno_location())); // osContPad.h leaves errno undefined
  }
  fprintf(stderr, "%s\n", msg);
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Animal Crossing - Crash", msg, NULL);

  // Default action (core dump, exit status) once the handler returns
  signal(sig, SIG_DFL);
  raise(sig);
}

void pc_crash_init(void) {
  static const int sigs[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT };
  stack_t ss;
  struct sigaction sa;

  s_start_ms = crash_now_ms();
  ss.ss_sp = s_altstack;
  ss.ss_size = sizeof(s_altstack);
  ss.ss_flags = 0;
  sigaltstack(&ss, NULL);

  memset(&sa, 0, sizeof(sa));
  sa.sa_sigaction = pc_crash_handler;
  sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
  sigemptyset(&sa.sa_mask);
  for(size_t i = 0; i < sizeof(sigs) / sizeof(sigs[0]); i++) {
    sigaction(sigs[i], &sa, NULL);
  }
}

// "Linux 6.x.y (Debian GNU/Linux 12 (bookworm))"
static void crash_init_os(void) {
  struct utsname u;
  char line[256];
  FILE* f;

  if(uname(&u) == 0) {
    snprintf(s_os, sizeof(s_os), "%s %s", u.sysname, u.release);
  }
  f = fopen("/etc/os-release", "r");
  if(f == NULL) {
    return;
  }
  while(fgets(line, sizeof(line), f) != NULL) {
    if(strncmp(line, "PRETTY_NAME=", 12) == 0) {
      char* v = line + 12;
      size_t n;
      if(*v == '"') v++;
      n = strcspn(v, "\"\n");
      v[n] = '\0';
      size_t len = strlen(s_os);
      snprintf(s_os + len, sizeof(s_os) - len, " (%s)", v);
      break;
    }
  }
  fclose(f);
}

#endif

void pc_crash_init_info(void) {
  crash_init_os();
  pc_gpu_device_info(s_gl, sizeof(s_gl));
  s_module_count_init = crash_collect_modules(s_modules_init);
#ifdef _WIN32
  // Again, in case SDL or the GL driver installed their own
  SetUnhandledExceptionFilter(pc_crash_handler);
#endif
}

void pc_crash_note_frame(GAME_PLAY* play) {
  CrashFrame f;
  PLAYER_ACTOR* player = GET_PLAYER_ACTOR(play);
  int scene = Save_Get(scene_no);

  f.frame = s_frame.frame + 1;
  f.tick_ms = crash_now_ms();
  f.scene = scene;
  f.date = Common_Get(time.rtc_time);
  f.weather = mEnv_SAVE_GET_WEATHER_TYPE(Save_Get(weather));
  f.weather_intensity = mEnv_SAVE_GET_WEATHER_INTENSITY(Save_Get(weather));
  f.has_player = player != NULL;
  if(player != NULL) {
    f.player_x = (int)player->actor_class.world.position.x;
    f.player_y = (int)player->actor_class.world.position.y;
    f.player_z = (int)player->actor_class.world.position.z;
  } else {
    f.player_x = f.player_y = f.player_z = 0;
  }
  f.ap_state = ap_getconnectstate()->state;
  s_frame = f;

  if(scene != s_last_scene) {
    CrashScene* s = &s_scenes[s_scene_count % CRASH_SCENES];
    s->frame = f.frame;
    s->tick_ms = f.tick_ms;
    s->from = s_last_scene;
    s->to = scene;
    zelda_GetFreeArena(&s->heap_max_block, &s->heap_free, &s->heap_alloc);
    s_scene_count++;
    s_last_scene = scene;
  }
}

void pc_crash_test(void) {
  volatile int* p = NULL;
  *p = 1;
}

void pc_fatal_error(const char* title, const char* msg) {
  fprintf(stderr, "%s\n", msg);
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, msg, g_pc_window);
  exit(1);
}
