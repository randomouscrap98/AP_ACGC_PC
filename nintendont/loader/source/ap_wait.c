// AP: the fork's start screen. Shows the Wii's address and waits for two things before the game boots:
// - the AP client (gc_client.py) connects and names the per-seed memory card (handshake below)
// - a disc image is chosen (the saved one, if it's still there; A opens the stock picker)
//
// Handshake on TCP port AP_WAIT_PORT, plain text lines so it can be tried with netcat:
//   Wii -> client: "ACAP 1\n"            (protocol version)
//   client -> Wii: "CARD <name>\n"       (<name>: 1-AP_CARD_NAME_MAX chars of A-Z a-z 0-9 _ -)
//   Wii -> client: "OK\n" or "ERR <reason>\n" (then the Wii closes the connection)
// Or "EXIT\n" -> "OK\n", then back to the Homebrew Channel like Start (remote testing).
// Or instead of CARD: "LOG\n" -> "LOG <n>\n" + n bytes: the kernel log of the last game session
// (/acap_kernel.log on the game's drive, nintendont/kernel/ap_server.c), so it can be read without the SD card.
// A later client connection replaces the card name, so reconnecting with another slot before boot works.
//
// Or instead of CARD: "ISO <size>\n" followed by exactly <size> raw bytes of a disc image (/wii_iso in the AP
// client). The loader writes it to /games/ACAP_UPLOAD/game.iso on the boot device and answers "OK ...\n" when the
// whole image is on the card; "ERR <reason>\n" for a bad or zero size, a folder/file it can't create, a write
// error (disk full) or a transfer that ends early or stalls (file errors end with the FatFS FRESULT in brackets).
// A failed upload deletes the partial file (and with it an earlier upload). A successful upload is this boot's game (no size
// check beyond the byte count, no hash); /acap_cfg.bin keeps the image that was saved before it. While receiving,
// the screen shows the progress and the 10 s silence drop for handshakes does not apply: a transfer with no bytes
// for AP_UPLOAD_STALL_SECONDS fails instead.
//
// Discovery on UDP port AP_WAIT_PORT: the client broadcasts "ACAP?\n"; the Wii answers the sender with
//   "ACAP 1 <game> <card>\n"   (<game> 1 = disc image chosen, else 0; <card> the card name so far, "-" if none)
// so the client finds the Wii without the address being typed in.
#include <gccore.h>
#include <network.h>
#include <fcntl.h>
#include <ogc/lwp_watchdog.h>
#include <stdio.h>
#include <string.h>
#include "global.h"
#include "font.h"
#include "FPad.h"
#include "menu.h"
#include "ap_wait.h"
#include "ff_utf8.h"
#include "diskio.h"

extern u32 Shutdown;

#define DARK_GRAY 0x2A2A2AFF // AP: darker than menu.c 0x666666, readable on the green background
#define IOS_O_NONBLOCK 4 // IOS's own flag value, not libogc's O_NONBLOCK (http.c uses the same)

static volatile s32 net_result = 1; // 1 = starting, 0 = up, < 0 = failed (net_init_async callback)
static s32 listen_sock = -1;
static s32 udp_sock = -1; // discovery
static s32 client_sock = -1;
static char client_buf[96];
static int client_len;
static int client_frames; // frames since the client connected (drops silent clients)
static bool exit_wanted;  // a client sent EXIT

// AP: a disc image sent by the client. It goes to /games/ACAP_UPLOAD/game.iso on the boot device and is this
// boot's game only. The paths have no drive prefix, so FatFS uses its default drive: the one /acap_cfg.bin is
// written to (the loader mounts SD before USB, so normally the SD card).
#define AP_UPLOAD_DIR "/games/ACAP_UPLOAD"
#define AP_UPLOAD_ISO AP_UPLOAD_DIR "/game.iso"
#define AP_UPLOAD_CHUNK (256 * 1024) // bytes received before a write: FatFS writes are slow per call
#define AP_UPLOAD_READ 16384         // bytes per net_recv: libogc copies through a 64 KB buffer it allocates
#define AP_UPLOAD_STALL_SECONDS 60   // no bytes for this long: the transfer is dead
#define AP_UPLOAD_READS 64           // net_recv calls per frame (one returns only what IOS has buffered)

extern FATFS *devices[2]; // global.c: the mounted file systems, [DEV_SD] and [DEV_USB]

static bool upload_active;   // receiving: ApUploadFrame runs instead of ApPollClient
static bool upload_ended;    // a transfer just ended: ApWaitScreen makes upload_ok its game
static bool upload_ok;       // ... which succeeded
static bool upload_used;     // this boot's game came from an upload (ApGameUploaded)
static FIL upload_file;      // game.iso being written
static u8 *upload_buf;       // AP_UPLOAD_CHUNK bytes, 32-aligned (memalign; freed when the transfer ends)
static int upload_len;       // bytes in upload_buf not written yet
static u32 upload_size;      // what the client announced
static u32 upload_got;       // bytes received so far (written + upload_len)
static int upload_frames;    // frames since bytes last arrived (stall timeout)
static u64 upload_start;     // gettime() at the ISO line, for the speed line
static u64 upload_write_ticks; // time spent in f_write / f_close (the card side of the speed)
static char upload_note[96]; // the screen's result line ("" = no upload this boot)
static char kept_path[sizeof(((NIN_CFG *)0)->GamePath)]; // the game before the first upload: /acap_cfg.bin keeps it
static u32 kept_usb;                                      // ... and its NIN_CFG_USB bit

static s32 ApNetDone(s32 result, void *usrdata)
{
	net_result = result < 0 ? result : 0;
	return 0;
}

static void ApListen(void)
{
	struct sockaddr_in addr;
	listen_sock = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if (listen_sock < 0)
		return;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(AP_WAIT_PORT);
	addr.sin_addr.s_addr = INADDR_ANY;
	if (net_bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0 || net_listen(listen_sock, 1) < 0)
	{
		net_close(listen_sock);
		listen_sock = -1;
		return;
	}
	net_fcntl(listen_sock, F_SETFL, IOS_O_NONBLOCK);
}

static void ApUdpOpen(void)
{
	struct sockaddr_in addr;
	udp_sock = net_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (udp_sock < 0)
		return;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(AP_WAIT_PORT);
	addr.sin_addr.s_addr = INADDR_ANY;
	if (net_bind(udp_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		net_close(udp_sock);
		udp_sock = -1;
		return;
	}
	net_fcntl(udp_sock, F_SETFL, IOS_O_NONBLOCK);
}

// Answers every waiting "ACAP?" packet (a few per frame at most)
static void ApUdpPoll(bool game_ok)
{
	int i;
	for (i = 0; i < 4; i++)
	{
		char buf[16], reply[80];
		struct sockaddr_in from;
		socklen_t from_len = sizeof(from);
		memset(&from, 0, sizeof(from));
		s32 got = net_recvfrom(udp_sock, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&from, &from_len);
		if (got <= 0)
			return;
		buf[got] = 0;
		if (strncmp(buf, "ACAP?", 5) != 0)
			continue;
		const char *card = NIN_CFG_AP_CARD(ncfg)[0] ? NIN_CFG_AP_CARD(ncfg) : "-";
		snprintf(reply, sizeof(reply), "ACAP 1 %d %s\n", game_ok ? 1 : 0, card);
		from.sin_len = 8;
		from.sin_family = AF_INET;
		net_sendto(udp_sock, reply, strlen(reply), 0, (struct sockaddr *)&from, 8);
	}
}

static void ApSend(const char *s)
{
	net_send(client_sock, s, strlen(s), 0);
}

static void ApDropClient(void)
{
	if (client_sock >= 0)
		net_close(client_sock);
	client_sock = -1;
}

// After the last reply: closing right away resets the connection before the reply gets out (seen on hardware),
// so wait until the client closes its side, ~1 s at most, then close
static void ApFinishClient(void)
{
	int i;
	for (i = 0; i < 60 && client_sock >= 0; i++)
	{
		char c;
		if (net_recv(client_sock, &c, 1, 0) == 0)
			break;
		VIDEO_WaitVSync();
	}
	ApDropClient();
}

// --- Disc image upload (/wii_iso) ----------------------------------------------------------------------------

// ASCII to the 16-bit WCHAR paths of the FatFS calls that have no f_*_char wrapper in ff_utf8.h
static void ApWidePath(const char *ascii, WCHAR *out, int size)
{
	int i;
	for (i = 0; i < size - 1 && ascii[i] != 0; i++)
		out[i] = (WCHAR)ascii[i];
	out[i] = 0;
}

// A byte count as the screen's "MB x10" (no floating point in the loader)
static u32 ApMB10(u32 bytes)
{
	return (u32)(((u64)bytes * 10) >> 20);
}

// Creates a folder of the upload path (already there = fine)
static bool ApMakeDir(const char *path)
{
	FRESULT r = f_mkdir_char(path);
	return r == FR_OK || r == FR_EXIST;
}

// Writes what has been received into game.iso; FR_DENIED for a short write (disk full)
static FRESULT ApUploadWrite(void)
{
	UINT wrote = 0;
	u64 t = gettime();
	FRESULT r = f_write(&upload_file, upload_buf, upload_len, &wrote);
	upload_write_ticks += diff_ticks(t, gettime());
	if (r == FR_OK && (int)wrote != upload_len)
		r = FR_DENIED;
	return r;
}

// Gives up on the transfer: throw the partial file away and tell the client why
static void ApUploadFail(const char *reason)
{
	WCHAR iso[64];

	f_close(&upload_file); // flushes, but the content is discarded below
	ApWidePath(AP_UPLOAD_ISO, iso, 64);
	f_unlink(iso);
	if (upload_buf != NULL)
		free(upload_buf);
	upload_buf = NULL;
	upload_active = false;
	upload_ended = true;
	upload_ok = false;
	snprintf(upload_note, sizeof(upload_note), "Upload failed: %s", reason);
	ApSend("ERR ");
	ApSend(reason);
	ApSend("\n");
	ApFinishClient();
}

// The whole image is in: make it the game of this boot
static void ApUploadDone(void)
{
	char reason[48];
	FRESULT r;
	u64 ms;
	u32 mb10 = ApMB10(upload_size); // the size x10, for the note below
	u32 sec10, rate10;

	// Written straight to game.iso, no .tmp + rename: f_rename failed on hardware (2026-11-24, code unknown) and
	// keeping the previous upload during a transfer isn't worth it
	r = upload_len > 0 ? ApUploadWrite() : FR_OK;
	upload_len = 0;
	if (r == FR_OK)
	{
		u64 t = gettime();
		r = f_close(&upload_file); // flushes the disk cache (diskio.c CTRL_SYNC)
		upload_write_ticks += diff_ticks(t, gettime());
	}
	if (r != FR_OK)
	{
		snprintf(reason, sizeof(reason), "write error / disk full (%d)", r);
		ApUploadFail(reason);
		return;
	}
	if (upload_buf != NULL)
		free(upload_buf);
	upload_buf = NULL;
	// This boot boots that image; ApKeepSavedGame puts the game from before it (the saved or A-picked one, not an
	// earlier upload) back into what main.c writes to /acap_cfg.bin
	if (!ApGameUploaded())
	{
		memcpy(kept_path, ncfg->GamePath, sizeof(kept_path));
		kept_usb = ncfg->Config & NIN_CFG_USB;
	}
	snprintf(ncfg->GamePath, sizeof(ncfg->GamePath), "%s", AP_UPLOAD_ISO);
	if (devices[DEV_SD] != NULL)
		ncfg->Config &= ~NIN_CFG_USB; // the upload is on the SD card (the default drive)
	else
		ncfg->Config |= NIN_CFG_USB;
	upload_used = true;
	upload_active = false;
	upload_ended = true;
	upload_ok = true;
	// The screen's speed line: Wi-Fi throughput on a real Wii is not known, so the user can report it
	ms = ticks_to_millisecs(diff_ticks(upload_start, gettime()));
	sec10 = (u32)(ms / 100);
	rate10 = ms > 0 ? (u32)(((u64)upload_size * 10000) / (ms * 1048576)) : 0;
	u32 write10 = (u32)(ticks_to_millisecs(upload_write_ticks) / 100);
	snprintf(upload_note, sizeof(upload_note), "Uploaded %u.%u MB in %u.%u s (%u.%u MB/s, card writes %u.%u s)",
	         mb10 / 10, mb10 % 10, sec10 / 10, sec10 % 10, rate10 / 10, rate10 % 10, write10 / 10, write10 % 10);
	char ok[48];
	snprintf(ok, sizeof(ok), "OK card writes %u ms\n", (u32)ticks_to_millisecs(upload_write_ticks));
	ApSend(ok);
	ApFinishClient();
}

// "ISO <size>" (the line, newline removed): start receiving the image. rest / rest_len are the bytes that came
// right after the line and already belong to it. Errors are sent to the client here; false = nothing was started.
static bool ApUploadStart(const char *line, const char *rest, int rest_len)
{
	const char *p = line + 3;
	u32 size = 0;

	if (*p != ' ')
		goto bad;
	for (p++; *p != 0; p++)
	{
		if (*p < '0' || *p > '9' || size > (0xFFFFFFFFu - 9) / 10)
			goto bad;
		size = size * 10 + (u32)(*p - '0');
	}
	if (size == 0)
		goto bad;
	upload_buf = memalign(32, AP_UPLOAD_CHUNK);
	if (upload_buf == NULL)
	{
		ApSend("ERR out of memory\n");
		return false;
	}
	if (!ApMakeDir("/games") || !ApMakeDir(AP_UPLOAD_DIR))
	{
		free(upload_buf);
		upload_buf = NULL;
		ApSend("ERR cannot create " AP_UPLOAD_DIR "\n");
		return false;
	}
	FRESULT r = f_open_char(&upload_file, AP_UPLOAD_ISO, FA_WRITE | FA_CREATE_ALWAYS);
	if (r != FR_OK)
	{
		char err[64];
		free(upload_buf);
		upload_buf = NULL;
		snprintf(err, sizeof(err), "ERR cannot create " AP_UPLOAD_ISO " (%d)\n", r);
		ApSend(err);
		return false;
	}
	upload_size = size;
	upload_got = 0;
	upload_len = 0;
	upload_frames = 0;
	upload_start = gettime();
	upload_write_ticks = 0;
	upload_note[0] = 0;
	upload_active = true;
	if ((u32)rest_len > size)
		rest_len = (int)size;
	if (rest_len > 0)
	{
		memcpy(upload_buf, rest, rest_len);
		upload_len = rest_len;
		upload_got = rest_len;
	}
	return true;
bad:
	ApSend("ERR bad size\n");
	return false;
}

// One frame of receiving: collect what the network has, write it out when a chunk is full, finish at the end
static void ApUploadFrame(void)
{
	int tries;

	if (++upload_frames > 60 * AP_UPLOAD_STALL_SECONDS)
	{
		ApUploadFail("transfer stalled");
		return;
	}
	for (tries = 0; tries < AP_UPLOAD_READS && upload_len < AP_UPLOAD_CHUNK && upload_got < upload_size; tries++)
	{
		int want = AP_UPLOAD_CHUNK - upload_len;
		if (want > AP_UPLOAD_READ)
			want = AP_UPLOAD_READ;
		if ((u32)want > upload_size - upload_got)
			want = (int)(upload_size - upload_got);
		s32 got = net_recv(client_sock, upload_buf + upload_len, want, 0);
		if (got < 0)
			break; // nothing (more) yet
		if (got == 0)
		{
			ApUploadFail("short transfer");
			return;
		}
		upload_len += got;
		upload_got += got;
		upload_frames = 0;
	}
	if (upload_len == AP_UPLOAD_CHUNK || upload_got == upload_size)
	{
		FRESULT r = ApUploadWrite();
		if (r != FR_OK)
		{
			char reason[48];
			snprintf(reason, sizeof(reason), "write error / disk full (%d)", r);
			ApUploadFail(reason);
			return;
		}
		upload_len = 0;
	}
	if (upload_got == upload_size)
		ApUploadDone();
}

bool ApGameUploaded(void)
{
	return upload_used && strcmp(ncfg->GamePath, AP_UPLOAD_ISO) == 0;
}

void ApKeepSavedGame(NIN_CFG *cfg)
{
	if (!ApGameUploaded())
		return;
	memcpy(cfg->GamePath, kept_path, sizeof(cfg->GamePath));
	cfg->Config = (cfg->Config & ~NIN_CFG_USB) | kept_usb;
}

static bool ApValidName(const char *s)
{
	int i, n = strlen(s);
	if (n < 1 || n > AP_CARD_NAME_MAX)
		return false;
	for (i = 0; i < n; i++)
	{
		char c = s[i];
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
			return false;
	}
	return true;
}

// The last session's kernel log (at most 4 KB, like the kernel's ring); n = 0 when there is none
static void ApSendLog(void)
{
	static char log[4096];
	char path[64], head[24];
	FIL f;
	UINT got = 0;
	snprintf(path, sizeof(path), "%s:/acap_kernel.log", GetRootDevice());
	if (f_open_char(&f, path, FA_READ | FA_OPEN_EXISTING) == FR_OK)
	{
		f_read(&f, log, sizeof(log), &got);
		f_close(&f);
	}
	snprintf(head, sizeof(head), "LOG %u\n", (unsigned)got);
	ApSend(head);
	// The socket is non-blocking: keep at it until all is out (or a couple of seconds pass)
	UINT done = 0;
	int tries = 0;
	while (done < got && tries++ < 120)
	{
		s32 n = net_send(client_sock, log + done, got - done, 0);
		if (n > 0)
			done += n;
		else
			VIDEO_WaitVSync();
	}
}

// One step per frame: accept a client, read its line, answer. Sets the card name when the handshake succeeds.
static void ApPollClient(void)
{
	if (client_sock < 0)
	{
		struct sockaddr_in peer; // libogc refuses NULL here (-EINVAL), so a buffer is needed even unread
		socklen_t peer_len = sizeof(peer);
		s32 s = net_accept(listen_sock, (struct sockaddr *)&peer, &peer_len);
		if (s < 0)
			return; // nobody waiting (non-blocking)
		client_sock = s;
		net_fcntl(client_sock, F_SETFL, IOS_O_NONBLOCK);
		client_len = 0;
		client_frames = 0;
		ApSend("ACAP 1\n");
		return;
	}
	if (++client_frames > 60 * 10)
	{
		ApDropClient(); // 10 s without a full line
		return;
	}
	s32 got = net_recv(client_sock, client_buf + client_len, sizeof(client_buf) - 1 - client_len, 0);
	if (got == 0)
	{
		ApDropClient(); // closed
		return;
	}
	if (got < 0)
		return; // nothing yet
	client_len += got;
	client_buf[client_len] = 0;
	char *nl = strchr(client_buf, '\n');
	if (nl == NULL)
	{
		if (client_len >= (int)sizeof(client_buf) - 1)
		{
			ApSend("ERR line too long\n");
			ApDropClient();
		}
		return;
	}
	*nl = 0;
	if (nl > client_buf && nl[-1] == '\r')
		nl[-1] = 0;
	if (strcmp(client_buf, "EXIT") == 0)
	{
		ApSend("OK\n");
		exit_wanted = true;
		ApFinishClient();
		return;
	}
	if (strcmp(client_buf, "LOG") == 0)
	{
		ApSendLog();
		ApFinishClient();
		return;
	}
	if (strncmp(client_buf, "ISO", 3) == 0)
	{
		// The image follows as raw bytes; some of it may be in the buffer right after the line
		const char *rest = nl + 1;
		int rest_len = client_len - (int)(rest - client_buf);
		if (!ApUploadStart(client_buf, rest, rest_len))
			ApFinishClient(); // the error is already sent
		return;
	}
	if (strncmp(client_buf, "CARD ", 5) == 0 && ApValidName(client_buf + 5))
	{
		strcpy(NIN_CFG_AP_CARD(ncfg), client_buf + 5);
		ApSend("OK\n");
	}
	else
		ApSend("ERR expected CARD <name>\n");
	ApFinishClient();
}

void ApWaitScreen(bool (*game_exists)(void))
{
	NIN_CFG_AP_CARD(ncfg)[0] = 0; // a card name only ever comes from this boot's client
	net_result = 1;
	if (net_init_async(ApNetDone, NULL) < 0)
		net_result = -1;

	bool game_ok = game_exists();
	char ip[16] = "";
	int ready_frames = 0; // frames both have been ready: booting waits ~0.5 s so the client's "OK" gets out
	while (true)
	{
		VIDEO_WaitVSync();
		FPAD_Update();
		if (Shutdown)
			LoaderShutdown();

		if (net_result == 0 && listen_sock < 0 && ip[0] == 0)
		{
			struct in_addr a;
			a.s_addr = net_gethostip();
			strncpy(ip, inet_ntoa(a), sizeof(ip) - 1);
			ApListen();
			ApUdpOpen();
		}
		if (upload_active)
			ApUploadFrame();
		else if (listen_sock >= 0)
			ApPollClient();
		if (upload_ended)
		{
			upload_ended = false;
			game_ok = upload_ok || game_exists(); // a failed upload leaves the saved image (if it's still there)
			if (upload_ok)
				ready_frames = -60 * 4; // keep the boot back a few seconds, so the speed line can be read
		}
		if (udp_sock >= 0)
			ApUdpPoll(game_ok);

		bool card_ok = NIN_CFG_AP_CARD(ncfg)[0] != 0;
		if (game_ok && card_ok && !upload_active) // never boot into a transfer, the saved game isn't the upload yet
		{
			if (++ready_frames > 30)
				break;
		}
		else
			ready_frames = 0;

		if (FPAD_OK(0) && !upload_active)
		{
			// The stock picker (SD/USB, then the game list); it saves nothing itself. A picked game replaces an
			// uploaded one (ApGameUploaded compares the path)
			SelectDevAndGame();
			game_ok = game_exists();
		}
		else if (FPAD_Start(0) || exit_wanted)
			ShowMessageScreenAndExit("Returning to loader...", 0);

		PrintInfo();
		PrintButtonActions("Exit", "Pick game", NULL, NULL);
		const int x = MENU_POS_X + 20;
		int y = MENU_POS_Y + 20 * 5;
		if (net_result == 1)
			PrintFormat(DEFAULT_SIZE, BLACK, x, y, "Starting network...");
		else if (net_result < 0)
			PrintFormat(DEFAULT_SIZE, MAROON, x, y, "Network failed (%d): check the Wii's internet settings", net_result);
		else if (listen_sock < 0)
			PrintFormat(DEFAULT_SIZE, MAROON, x, y, "Could not listen on port %d", AP_WAIT_PORT);
		else
		{
			PrintFormat(DEFAULT_SIZE, BLACK, x, y, "Wii address (the AP client should find it by itself):");
			PrintFormat(40, BLACK, x, y + 24, "%s", ip);
		}
		y += 20 * 5;
		PrintFormat(DEFAULT_SIZE, card_ok ? BLACK : DARK_GRAY, x, y,
			    card_ok ? "AP client: connected (card %s)" : "AP client: waiting...", NIN_CFG_AP_CARD(ncfg));
		y += 20 * 2;
		if (game_ok)
			PrintFormat(DEFAULT_SIZE, BLACK, x, y, "Game: %s:%s%s", (ncfg->Config & NIN_CFG_USB) ? "usb" : "sd",
			            ncfg->GamePath, ApGameUploaded() ? " (uploaded this boot)" : "");
		else
			PrintFormat(DEFAULT_SIZE, MAROON, x, y, "No game chosen: press A to pick the patched disc image");
		y += 20 * 2;
		if (upload_active)
		{
			u32 got10 = ApMB10(upload_got), size10 = ApMB10(upload_size);
			PrintFormat(DEFAULT_SIZE, BLACK, x, y, "Receiving %u.%u / %u.%u MB",
			            got10 / 10, got10 % 10, size10 / 10, size10 % 10);
		}
		else
			PrintFormat(DEFAULT_SIZE, BLACK, x, y, "The game starts when both are ready.");
		if (upload_note[0])
			PrintFormat(DEFAULT_SIZE, BLACK, x, y + 20, "%s", upload_note);
		GRRLIB_Render();
		ClearScreen();
	}

	ApDropClient();
	if (listen_sock >= 0)
		net_close(listen_sock);
	listen_sock = -1;
	if (udp_sock >= 0)
		net_close(udp_sock);
	udp_sock = -1;
	// No net_deinit: the loader already runs on the IOS the game gets (IOS58 is reloaded before this screen), and
	// shutting the network down here left the kernel's AP server unable to make sockets (socket() -5 on
	// hardware). Our sockets are closed above; the network stays up for nintendont/kernel/ap_server.c.
}
