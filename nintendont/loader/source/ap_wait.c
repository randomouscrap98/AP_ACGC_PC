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
// Discovery on UDP port AP_WAIT_PORT: the client broadcasts "ACAP?\n"; the Wii answers the sender with
//   "ACAP 1 <game> <card>\n"   (<game> 1 = disc image chosen, else 0; <card> the card name so far, "-" if none)
// so the client finds the Wii without the address being typed in.
#include <gccore.h>
#include <network.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include "global.h"
#include "font.h"
#include "FPad.h"
#include "menu.h"
#include "ap_wait.h"
#include "ff_utf8.h"

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
		if (listen_sock >= 0)
			ApPollClient();
		if (udp_sock >= 0)
			ApUdpPoll(game_ok);

		bool card_ok = NIN_CFG_AP_CARD(ncfg)[0] != 0;
		if (game_ok && card_ok)
		{
			if (++ready_frames > 30)
				break;
		}
		else
			ready_frames = 0;

		if (FPAD_OK(0))
		{
			// The stock picker (SD/USB, then the game list); it saves nothing itself
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
			PrintFormat(DEFAULT_SIZE, BLACK, x, y, "Game: %s:%s", (ncfg->Config & NIN_CFG_USB) ? "usb" : "sd", ncfg->GamePath);
		else
			PrintFormat(DEFAULT_SIZE, MAROON, x, y, "No game chosen: press A to pick the patched disc image");
		y += 20 * 2;
		PrintFormat(DEFAULT_SIZE, BLACK, x, y, "The game starts when both are ready.");
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
