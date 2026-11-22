// AP: RAM server for the Animal Crossing AP client (gc_client.py, NintendontRAM). After the game boots, a thread
// brings the Wii's network up, listens on TCP AP_SERVER_PORT and serves one client at a time. Requests are fixed
// 12-byte headers, big-endian: u8 cmd, u8 pad[3], u32 a, u32 b.
//   'P' ping           -> u32 AP_SERVER_MAGIC, u32 AP_SERVER_VERSION
//   'F' find from a    -> u32 address (0x80xxxxxx) of the first 4-aligned "ACAPMBOX" at or after game address a
//                         in MEM1, 0 = none (the client checks the header and asks again past a false match)
//   'R' read a, len b  -> u32 len, then len bytes (len 0 = refused: outside MEM1 or over AP_SERVER_MAX)
//   'W' write a, len b, then b bytes -> u32 len written (0 = refused)
//   'L' log            -> u32 len, then the log ring, oldest first
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
static u8 ap_buf[AP_SERVER_MAX] __attribute__((aligned(32))); // request payloads and replies
static u8 ap_word[32] __attribute__((aligned(32)));             // u32 replies (never ap_buf: it may hold data)
static u32 ap_params[8] __attribute__((aligned(32)));
static u32 ap_out[8] __attribute__((aligned(32)));
static u8 ap_addr[8] __attribute__((aligned(32))); // IOS sockaddr: len 8, family, port, IPv4
static struct ap_bind_params ap_bind __attribute__((aligned(32)));
static struct ap_sendto_params ap_send __attribute__((aligned(32)));
static ioctlv ap_vec[3] __attribute__((aligned(32)));
static int ap_top = -1, ap_kd = -1;

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

static s32 ApIoctl(int fd, int cmd, void *in, int in_len, void *out, int out_len)
{
	return IOS_Ioctl(fd, cmd, in, in_len, out, out_len);
}

static s32 ApClose(s32 s)
{
	ap_params[0] = s;
	return ApIoctl(ap_top, so_close, ap_params, 4, NULL, 0);
}

// Brings the network up from the Wii's saved connection; returns our IP, 0 on failure
static u32 ApNetworkUp(void)
{
	int i;
	char name[32] __attribute__((aligned(32)));
	strcpy(name, "/dev/net/ip/top");
	ap_top = IOS_Open(name, 0);
	strcpy(name, "/dev/net/kd/request");
	ap_kd = IOS_Open(name, 0);
	ApLog("open top %d kd %d\r\n", ap_top, ap_kd);
	if (ap_top < 0 || ap_kd < 0)
		return 0;
	for (i = 0; i < 10; i++)
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
		u32 ip = (u32)ApIoctl(ap_top, SO_GETHOSTID, NULL, 0, NULL, 0);
		if ((s32)ip > 0 || (s32)ip < -1000) // small negative = error code (an IP can look negative too)
			return ip;
		mdelay(250);
	}
	return 0;
}

static s32 ApListen(void)
{
	ap_params[0] = 2; // AF_INET
	ap_params[1] = 1; // SOCK_STREAM
	ap_params[2] = 0;
	s32 s = ApIoctl(ap_top, so_socket, ap_params, 12, NULL, 0);
	if (s < 0)
		return s;
	memset(&ap_bind, 0, sizeof(ap_bind));
	ap_bind.socket = s;
	ap_bind.has_name = 1;
	ap_bind.name[0] = 8;
	ap_bind.name[1] = 2; // AF_INET
	ap_bind.name[2] = AP_SERVER_PORT >> 8;
	ap_bind.name[3] = AP_SERVER_PORT & 0xFF; // address 0 = any
	s32 r = ApIoctl(ap_top, so_bind, &ap_bind, sizeof(ap_bind), NULL, 0);
	if (r < 0)
	{
		ApClose(s);
		return r;
	}
	ap_params[0] = s;
	ap_params[1] = 1; // backlog
	r = ApIoctl(ap_top, so_listen, ap_params, 8, NULL, 0);
	if (r < 0)
	{
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

// Up to len bytes; <= 0 = closed or error
static s32 ApRecvSome(s32 s, void *buf, u32 len)
{
	ap_params[0] = s;
	ap_params[1] = 0; // flags
	memset(ap_vec, 0, sizeof(ap_vec));
	ap_vec[0].data = ap_params;
	ap_vec[0].len = 8;
	ap_vec[1].data = buf;
	ap_vec[1].len = len;
	return IOS_Ioctlv(ap_top, so_recvfrom, 1, 2, ap_vec);
}

static bool ApRecvAll(s32 s, u8 *buf, u32 len)
{
	while (len > 0)
	{
		s32 got = ApRecvSome(s, buf, len);
		if (got <= 0)
			return false;
		buf += got;
		len -= got;
	}
	return true;
}

// buf: one of our aligned buffers (ap_buf, ap_word)
static bool ApSendAll(s32 s, const u8 *buf, u32 len)
{
	while (len > 0)
	{
		memset(&ap_send, 0, sizeof(ap_send));
		ap_send.socket = s;
		memset(ap_vec, 0, sizeof(ap_vec));
		ap_vec[0].data = (void*)buf;
		ap_vec[0].len = len;
		ap_vec[1].data = &ap_send;
		ap_vec[1].len = sizeof(ap_send);
		s32 sent = IOS_Ioctlv(ap_top, so_sendto, 2, 0, ap_vec);
		if (sent <= 0)
			return false;
		buf += sent;
		len -= sent;
	}
	return true;
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
	u8 req[12];
	while (ApRecvAll(c, req, 12))
	{
		u32 a = (req[4] << 24) | (req[5] << 16) | (req[6] << 8) | req[7];
		u32 b = (req[8] << 24) | (req[9] << 16) | (req[10] << 8) | req[11];
		s32 off;
		bool ok = true;
		switch (req[0])
		{
		case 'P':
			ok = ApSendU32(c, AP_SERVER_MAGIC) && ApSendU32(c, AP_SERVER_VERSION);
			break;
		case 'F':
			ok = ApSendU32(c, ApFind(a));
			break;
		case 'R':
			off = ApMem1(a, b);
			if (off < 0)
			{
				ok = ApSendU32(c, 0);
				break;
			}
			sync_before_read((void*)(off & ~31), ((off & 31) + b + 31) & ~31);
			memcpy(ap_buf, (void*)off, b); // MEM1 is at physical address 0 on this side
			ok = ApSendU32(c, b) && ApSendAll(c, ap_buf, b);
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
			ok = ApSendU32(c, off >= 0 ? b : 0);
			break;
		case 'L':
		{
			u32 head = ap_log_head;
			u32 len = head < AP_LOG_SIZE ? head : AP_LOG_SIZE;
			u32 start = head - len, i;
			for (i = 0; i < len; i++)
				ap_buf[i] = ap_log[(start + i) % AP_LOG_SIZE];
			ok = ApSendU32(c, len) && ApSendAll(c, ap_buf, len);
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
		ApLogNow("network failed: no IP after %d s\r\n", AP_IP_WAIT_SECONDS);
		return 0;
	}
	ApLogNow("network up, IP %d.%d.%d.%d\r\n", ip >> 24, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);
	s32 s = ApListen();
	if (s < 0)
	{
		ApLogNow("listen on port %d failed: %d\r\n", AP_SERVER_PORT, s);
		return 0;
	}
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
		ApServe(c);
		ApClose(c);
		ApLogNow("client gone\r\n");
	}
	return 0;
}

void ApServerInit(void)
{
	ap_log_timer = read32(HW_TIMER);
	ApLogNow("Animal Crossing AP kernel server %d\r\n", AP_SERVER_VERSION);
	// Below the main loop's priority (0x50): the main loop sleeps every pass, so this still runs, but a long
	// mailbox search never holds up the game's timers or pads
	u32 t = thread_create(ApServerThread, NULL, ap_stack, sizeof(ap_stack) / sizeof(u32), 0x48, 1);
	thread_continue(t);
}
