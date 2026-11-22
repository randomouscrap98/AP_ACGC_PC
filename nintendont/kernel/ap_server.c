// AP: RAM server for the Animal Crossing AP client (gc_client.py, NintendontRAM). After the game boots, a thread
// brings the Wii's network up, listens on TCP AP_SERVER_PORT and serves one client at a time. Requests are fixed
// 12-byte headers, big-endian: u8 cmd, u8 pad[3], u32 a, u32 b.
//   'P' ping           -> u32 AP_SERVER_MAGIC, u32 AP_SERVER_VERSION
//   'F' find from a    -> u32 address (0x80xxxxxx) of the first 4-aligned "ACAPMBOX" at or after game address a
//                         in MEM1, 0 = none (the client checks the header and asks again past a false match)
//   'R' read a, len b  -> u32 len, then len bytes (len 0 = refused: outside MEM1 or over AP_SERVER_MAX)
//   'W' write a, len b, then b bytes -> u32 len written (0 = refused)
//   'L' log            -> u32 len, then the log ring, oldest first
//   'Q' quit           -> u32 1, then the game exits to the loader at its next pad read (PADReadGC.c), like Home
// The game keeps its mailbox in uncached memory (gc_ap_backend.c), so plain reads and writes here see its data;
// this side only syncs the ARM's own cache. The log also goes to AP_LOG_PATH on the game's drive, but only from
// the main loop (ApLogFlushDue / ApLogFlush): the memory card emulation writes files there too.
#include "global.h"
#include "common.h"
#include "string.h"
#include "syscalls.h"
#include "vsprintf.h"
#include "sock.h"
#include "ff_utf8.h"
#include "ap_server.h"

#define AP_SERVER_MAGIC   0x41434150 // "ACAP"
#define AP_SERVER_VERSION 1
#define AP_SERVER_MAX     4096 // bytes per read / write request
#define AP_MEM1_SIZE      0x01800000
#define AP_LOG_SIZE       4096
#define AP_LOG_PATH       "/acap_kernel.log"
#define AP_LOG_FLUSH_SECONDS 10 // plain lines wait at most this long for the SD card; milestone lines go at once
#define AP_IP_WAIT_SECONDS   60

#define NWC_STARTUP 6     // /dev/net/kd/request: start the network (libogc NWC24iStartupSocket)
#define SO_GETHOSTID 0x10 // /dev/net/ip/top: our IP, 0 until DHCP is done

// IOS socket parameter blocks (as sock.c's bind_params / sendto_params, which stay private there)
struct ap_bind_params {
	u32 socket;
	u32 has_name;
	u8 name[28];
};
struct ap_sendto_params {
	u32 socket;
	u32 flags;
	u32 has_destaddr;
	u8 destaddr[28];
};

static char ap_log[AP_LOG_SIZE];
static u32 ap_log_head;   // total bytes ever written (ring index = head % size)
static u32 ap_log_saved;  // head at the last SD write
static volatile int ap_log_urgent; // a milestone line is waiting for the SD card
static u32 ap_log_timer;  // HW_TIMER at the last SD write

static u32 ap_stack[0x800 / sizeof(u32)] __attribute__((aligned(32)));
// Buffers the network module reads or writes come from sock.c's heap (0x13040000, where sock.c keeps its own):
// the kernel's own memory isn't at the same address for the module (with static buffers socket() saw a wrong
// address family, -5, and the NWC startup result was never written, on hardware). IOS_Open's path is the
// exception: it must be in kernel memory (-6 from the heap). Every call flushes / invalidates (ApIoctl/ApIoctlv).
extern int soHeap;
static u8 *ap_buf;    // 4 + AP_SERVER_MAX: payloads; replies = u32 len + data
static u8 *ap_word;   // 32: small replies (never ap_buf: it may hold data)
static u8 *ap_req;    // 32: a request header
static u8 *ap_rx;     // AP_SERVER_MAX: every receive lands here first (the module needs 32-aligned buffers: a
                      // partial receive continued at buf + got desynced the stream on hardware)
static u32 *ap_params; // 32
static u32 *ap_out;   // 32
static u8 *ap_addr;   // 32: IOS sockaddr: len 8, family, port, IPv4
static struct ap_bind_params *ap_bind;
static struct ap_sendto_params *ap_send;
static ioctlv *ap_vec; // 3
static int ap_top = -1, ap_kd = -1;
static s32 ap_last_io; // result of the last failed receive / send (why a client was dropped)
static const char *ap_last_op = "";

static void ApLogV(int urgent, const char *fmt, va_list args)
{
	char line[0x100];
	int i, n = _vsprintf(line, fmt, args);
	for (i = 0; i < n; i++)
		ap_log[(ap_log_head + i) % AP_LOG_SIZE] = line[i];
	ap_log_head += n;
	if (urgent)
		ap_log_urgent = 1;
}

// A plain line (goes to the SD card within AP_LOG_FLUSH_SECONDS)
void ApLog(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	ApLogV(0, fmt, args);
	va_end(args);
}

// A milestone line: the next main-loop pass writes the log
static void ApLogNow(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	ApLogV(1, fmt, args);
	va_end(args);
}

bool ApLogFlushDue(void)
{
	if (ap_log_head == ap_log_saved)
		return false;
	return ap_log_urgent || TimerDiffSeconds(ap_log_timer) >= AP_LOG_FLUSH_SECONDS;
}

// The whole ring, rewritten (small, and the file then always matches the ring)
void ApLogFlush(void)
{
	FIL f;
	u32 wrote, head = ap_log_head;
	u32 len = head < AP_LOG_SIZE ? head : AP_LOG_SIZE;
	u32 start = head - len;
	ap_log_urgent = 0;
	ap_log_saved = head;
	ap_log_timer = read32(HW_TIMER);
	if (f_open_char(&f, AP_LOG_PATH, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
		return;
	u32 first = AP_LOG_SIZE - start % AP_LOG_SIZE; // bytes up to the ring's end
	if (first > len)
		first = len;
	f_write(&f, &ap_log[start % AP_LOG_SIZE], first, &wrote);
	if (len > first)
		f_write(&f, ap_log, len - first, &wrote);
	f_close(&f);
}

// Flush our buffers before each call (sock.c does it for some). No invalidate afterwards: the network module runs
// on the same ARM and copies received data in through the same cache, so invalidating threw fresh bytes away
// (requests parsed from stale payload text, on hardware)
static void ApFlush(void *p, int len)
{
	if (p != NULL && len > 0)
		sync_after_write((void*)((u32)p & ~31), (((u32)p & 31) + len + 31) & ~31);
}

static s32 ApIoctl(int fd, int cmd, void *in, int in_len, void *out, int out_len)
{
	ApFlush(in, in_len);
	ApFlush(out, out_len);
	return IOS_Ioctl(fd, cmd, in, in_len, out, out_len);
}

// vec[0..in-1] are read by the module, vec[in..in+out-1] written
static s32 ApIoctlv(int fd, int cmd, int in, int out)
{
	int i;
	for (i = 0; i < in + out; i++)
		ApFlush(ap_vec[i].data, ap_vec[i].len);
	ApFlush(ap_vec, sizeof(ioctlv) * 3);
	return IOS_Ioctlv(fd, cmd, in, out, ap_vec);
}

static s32 ApClose(s32 s)
{
	ap_params[0] = s;
	return ApIoctl(ap_top, so_close, ap_params, 4, NULL, 0);
}

static u32 ApHostIp(void)
{
	s32 ip = ApIoctl(ap_top, SO_GETHOSTID, NULL, 0, NULL, 0);
	// An IP can look negative as an s32; small negatives are error codes
	return (ip > 0 || ip < -1000) ? (u32)ip : 0;
}

// Our IP, after the network startup steps the loader's libogc did and undid before boot (net_deinit): the
// network-control startup through sock.c's handle (the device takes one opener), then the socket layer's.
// Without them socket() fails (-5 on hardware) even though the IP stays configured.
static u32 ApNetworkUp(void)
{
	int i;
	char name[32] __attribute__((aligned(32)));
	strcpy(name, "/dev/net/ip/top");
	ap_top = IOS_Open(name, 0);
	ap_kd = SOCKGetNwcFd();
	ApLog("open /dev/net/ip/top: %d, kd handle %d\r\n", ap_top, ap_kd);
	if (ap_top < 0)
		return 0;
	for (i = 0; ap_kd >= 0 && i < 10; i++)
	{
		ap_out[0] = (u32)-1;
		s32 r = ApIoctl(ap_kd, NWC_STARTUP, NULL, 0, ap_out, 0x20);
		ApLog("nwc startup %d result %d\r\n", r, (s32)ap_out[0]);
		if ((s32)ap_out[0] != -29) // -29: busy, try again
			break;
		mdelay(500);
	}
	ApLog("so startup %d\r\n", ApIoctl(ap_top, so_startinterface, NULL, 0, NULL, 0));
	for (i = 0; i < AP_IP_WAIT_SECONDS * 4; i++)
	{
		u32 ip = ApHostIp();
		if (ip != 0)
			return ip;
		mdelay(250);
	}
	ApLog("last gethostid: %d\r\n", ApIoctl(ap_top, SO_GETHOSTID, NULL, 0, NULL, 0));
	return 0;
}

static s32 ApListen(void)
{
	ap_params[0] = 2; // AF_INET
	ap_params[1] = 1; // SOCK_STREAM
	ap_params[2] = 0;
	s32 s = ApIoctl(ap_top, so_socket, ap_params, 12, NULL, 0);
	if (s < 0)
	{
		ApLog("socket: %d\r\n", s);
		return s;
	}
	memset(ap_bind, 0, sizeof(*ap_bind));
	ap_bind->socket = s;
	ap_bind->has_name = 1;
	ap_bind->name[0] = 8;
	ap_bind->name[1] = 2; // AF_INET
	ap_bind->name[2] = AP_SERVER_PORT >> 8;
	ap_bind->name[3] = AP_SERVER_PORT & 0xFF; // address 0 = any
	s32 r = ApIoctl(ap_top, so_bind, ap_bind, sizeof(*ap_bind), NULL, 0);
	if (r < 0)
	{
		ApLog("bind: %d\r\n", r);
		ApClose(s);
		return r;
	}
	ap_params[0] = s;
	ap_params[1] = 1; // backlog
	r = ApIoctl(ap_top, so_listen, ap_params, 8, NULL, 0);
	if (r < 0)
	{
		ApLog("listen: %d\r\n", r);
		ApClose(s);
		return r;
	}
	return s;
}

static s32 ApAccept(s32 s)
{
	ap_params[0] = s;
	memset(ap_addr, 0, 8);
	ap_addr[0] = 8;
	return ApIoctl(ap_top, so_accept, ap_params, 4, ap_addr, 8);
}

// Up to len (<= AP_SERVER_MAX) bytes into ap_rx; <= 0 = closed or error
static s32 ApRecvSome(s32 s, u32 len)
{
	void *buf = ap_rx;
	ap_params[0] = s;
	ap_params[1] = 0; // flags
	memset(ap_vec, 0, sizeof(ioctlv) * 3);
	ap_vec[0].data = ap_params;
	ap_vec[0].len = 8;
	ap_vec[1].data = buf;
	ap_vec[1].len = len;
	return ApIoctlv(ap_top, so_recvfrom, 1, 2);
}

static bool ApRecvAll(s32 s, u8 *buf, u32 len)
{
	while (len > 0)
	{
		s32 got = ApRecvSome(s, len > AP_SERVER_MAX ? AP_SERVER_MAX : len);
		if (got <= 0)
		{
			ap_last_io = got;
			ap_last_op = "recv";
			return false;
		}
		memcpy(buf, ap_rx, got);
		buf += got;
		len -= got;
	}
	return true;
}

// buf: one of our aligned buffers (ap_buf, ap_word). A partial send moves the rest to buf's start, so every
// send starts on the aligned address (see ap_rx)
static bool ApSendAll(s32 s, u8 *buf, u32 len)
{
	while (len > 0)
	{
		memset(ap_send, 0, sizeof(*ap_send));
		ap_send->socket = s;
		memset(ap_vec, 0, sizeof(ioctlv) * 3);
		ap_vec[0].data = (void*)buf;
		ap_vec[0].len = len;
		ap_vec[1].data = ap_send;
		ap_vec[1].len = sizeof(*ap_send);
		s32 sent = ApIoctlv(ap_top, so_sendto, 2, 0);
		if (sent <= 0)
		{
			ap_last_io = sent;
			ap_last_op = "send";
			return false;
		}
		if ((u32)sent >= len)
			break;
		u32 i, left = len - sent;
		for (i = 0; i < left; i++) // to the start: a forward copy is safe (no memmove in this kernel)
			buf[i] = buf[sent + i];
		len = left;
	}
	return true;
}

static void ApPutU32(u8 *p, u32 v)
{
	p[0] = v >> 24;
	p[1] = v >> 16;
	p[2] = v >> 8;
	p[3] = v;
}

static bool ApSendU32(s32 s, u32 v)
{
	ap_word[0] = v >> 24;
	ap_word[1] = v >> 16;
	ap_word[2] = v >> 8;
	ap_word[3] = v;
	return ApSendAll(s, ap_word, 4);
}

// MEM1 offset for a game address (0x80 cached / 0xC0 uncached / physical), or -1 when outside or too long
static s32 ApMem1(u32 addr, u32 len)
{
	u32 off = addr & 0x3FFFFFFF;
	if ((addr >> 30) == 1 || len > AP_SERVER_MAX || off >= AP_MEM1_SIZE || len > AP_MEM1_SIZE - off)
		return -1;
	return (s32)off;
}

static u32 ApFind(u32 from)
{
	u32 off, start = (from & 0x3FFFFFFF & ~3);
	for (off = start & ~0x7FFF; off < AP_MEM1_SIZE; off += 0x8000)
	{
		u32 i;
		if (off != (start & ~0x7FFF))
			udelay(100); // between 32 KB pieces: this thread outranks the main loop now
		sync_before_read((void*)off, 0x8000);
		for (i = off > start ? off : start; i < off + 0x8000; i += 4)
		{
			if (read32(i) == 0x41434150 && i + 4 < AP_MEM1_SIZE && read32(i + 4) == 0x4D424F58) // "ACAP" "MBOX"
				return 0x80000000 | i;
		}
	}
	return 0;
}

// Serves one client until it goes away
static void ApServe(s32 c)
{
	u8 *req = ap_req;
	while (ApRecvAll(c, req, 12))
	{
		u32 a = (req[4] << 24) | (req[5] << 16) | (req[6] << 8) | req[7];
		u32 b = (req[8] << 24) | (req[9] << 16) | (req[10] << 8) | req[11];
		s32 off;
		bool ok = true;
		switch (req[0])
		{
		// Each reply goes out in one send: two small sends in a row cost ~2 s per request on hardware (the
		// second waits for the first's delayed acknowledgement)
		case 'P':
			ApPutU32(ap_word, AP_SERVER_MAGIC);
			ApPutU32(ap_word + 4, AP_SERVER_VERSION);
			ok = ApSendAll(c, ap_word, 8);
			break;
		case 'F':
			ok = ApSendU32(c, ApFind(a));
			break;
		case 'R':
			off = ApMem1(a, b);
			if (off < 0)
			{
				ApLog("refused R %x %d\r\n", a, b);
				ok = ApSendU32(c, 0);
				break;
			}
			sync_before_read((void*)(off & ~31), ((off & 31) + b + 31) & ~31);
			memcpy(ap_buf + 4, (void*)off, b); // MEM1 is at physical address 0 on this side
			ApPutU32(ap_buf, b);
			ok = ApSendAll(c, ap_buf, 4 + b);
			break;
		case 'W':
			off = ApMem1(a, b);
			if (b > AP_SERVER_MAX || !ApRecvAll(c, ap_buf, b))
			{
				ok = false; // can't skip the payload: drop the client
				break;
			}
			if (off >= 0)
			{
				memcpy((void*)off, ap_buf, b);
				sync_after_write((void*)(off & ~31), ((off & 31) + b + 31) & ~31);
			}
			if (off < 0)
				ApLog("refused W %x %d\r\n", a, b);
			ok = ApSendU32(c, off >= 0 ? b : 0);
			break;
		case 'Q':
			write32(0x130031C0, 0x41505158); // "APQX" (mem_map.txt)
			sync_after_write((void*)0x130031C0, 0x20);
			ApLogNow("quit requested\r\n");
			ok = ApSendU32(c, 1);
			break;
		case 'L':
		{
			u32 head = ap_log_head;
			u32 len = head < AP_LOG_SIZE ? head : AP_LOG_SIZE;
			u32 start = head - len, i;
			for (i = 0; i < len; i++)
				ap_buf[4 + i] = ap_log[(start + i) % AP_LOG_SIZE];
			ApPutU32(ap_buf, len);
			ok = ApSendAll(c, ap_buf, 4 + len);
			break;
		}
		default:
			ApLog("unknown command %d\r\n", req[0]);
			ok = false;
			break;
		}
		if (!ok)
			break;
	}
}

static u32 ApServerThread(void *arg)
{
	u32 ip = ApNetworkUp();
	if (ip == 0)
	{
		ApLogNow("network failed (steps above)\r\n");
		return 0;
	}
	ApLogNow("network up, IP %d.%d.%d.%d\r\n", ip >> 24, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);
	// Right after boot the socket layer sometimes refuses (socket() -5 / -28 on hardware, the next boot fine):
	// try again every second for a minute
	s32 s = -1;
	int tries;
	for (tries = 1; tries <= 60; tries++)
	{
		s = ApListen();
		if (s >= 0)
			break;
		mdelay(1000);
		if (tries % 5 == 0)
			ApLog("so startup %d\r\n", ApIoctl(ap_top, so_startinterface, NULL, 0, NULL, 0));
	}
	if (s < 0)
	{
		ApLogNow("listen on port %d failed: %d (60 tries)\r\n", AP_SERVER_PORT, s);
		return 0;
	}
	if (tries > 1)
		ApLog("listen worked on try %d\r\n", tries);
	ApLogNow("listening on port %d\r\n", AP_SERVER_PORT);
	while (1)
	{
		s32 c = ApAccept(s);
		if (c < 0)
		{
			ApLog("accept failed: %d\r\n", c);
			mdelay(1000);
			continue;
		}
		ApLogNow("client %d.%d.%d.%d connected\r\n", ap_addr[4], ap_addr[5], ap_addr[6], ap_addr[7]);
		ap_last_io = 0;
		ap_last_op = "";
		ApServe(c);
		ApClose(c);
		ApLogNow("client gone (%s %d)\r\n", ap_last_op, ap_last_io);
	}
	return 0;
}

static void *ApAlloc(int size)
{
	void *p = heap_alloc_aligned(soHeap, (size + 31) & ~31, 32);
	if (p != NULL)
		memset(p, 0, size);
	return p;
}

void ApServerInit(void)
{
	ap_log_timer = read32(HW_TIMER);
	ApLogNow("Animal Crossing AP kernel server %d\r\n", AP_SERVER_VERSION);
	write32(0x130031C0, 0); // no quit request left over from an earlier session ('Q')
	sync_after_write((void*)0x130031C0, 0x20);
	if (soHeap < 0)
	{
		ApLogNow("no socket heap\r\n");
		return;
	}
	ap_buf = ApAlloc(4 + AP_SERVER_MAX);
	ap_word = ApAlloc(32);
	ap_req = ApAlloc(32);
	ap_rx = ApAlloc(AP_SERVER_MAX);
	ap_params = ApAlloc(32);
	ap_out = ApAlloc(32);
	ap_addr = ApAlloc(32);
	ap_bind = ApAlloc(sizeof(*ap_bind));
	ap_send = ApAlloc(sizeof(*ap_send));
	ap_vec = ApAlloc(sizeof(ioctlv) * 3);
	if (!ap_buf || !ap_word || !ap_req || !ap_rx || !ap_params || !ap_out || !ap_addr || !ap_bind || !ap_send || !ap_vec)
	{
		ApLogNow("no memory in the socket heap\r\n");
		return;
	}
	// Priority 0x70, above the main loop's 0x50 (like sock.c / HID's threads): below it, replies stalled for
	// seconds once the game ran (hardware). It mostly waits in IOS calls; the mailbox search pauses every 32 KB.
	// The stack argument is the stack's top (it grows down), as for the kernel's fixed stacks in kernel.ld:
	// passing ap_stack itself let this thread overwrite the kernel data below it (garbage socket parameters,
	// a freeze, on hardware). Started through the IOS trampoline like the kernel's other threads.
	u32 t = do_thread_create(ApServerThread, (u32*)((u8*)ap_stack + sizeof(ap_stack)), sizeof(ap_stack), 0x70);
	thread_continue(t);
}
