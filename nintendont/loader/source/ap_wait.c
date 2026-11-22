// AP: the fork's start screen. Shows the Wii's address and waits for two things before the game boots:
// - the AP client (gc_client.py) connects and names the per-seed memory card (handshake below)
// - a disc image is chosen (the saved one, if it's still there; A opens the stock picker)
//
// Handshake on TCP port AP_WAIT_PORT, plain text lines so it can be tried with netcat:
//   Wii -> client: "ACAP 1\n"            (protocol version)
//   client -> Wii: "CARD <name>\n"       (<name>: 1-AP_CARD_NAME_MAX chars of A-Z a-z 0-9 _ -)
//   Wii -> client: "OK\n" or "ERR <reason>\n" (then the Wii closes the connection)
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

// One step per frame: accept a client, read its line, answer. Sets the card name when the handshake succeeds.
static void ApPollClient(void)
{
	if (client_sock < 0)
	{
		s32 s = net_accept(listen_sock, NULL, NULL);
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
	if (strncmp(client_buf, "CARD ", 5) == 0 && ApValidName(client_buf + 5))
	{
		strcpy(NIN_CFG_AP_CARD(ncfg), client_buf + 5);
		ApSend("OK\n");
	}
	else
		ApSend("ERR expected CARD <name>\n");
	ApDropClient();
}

void ApWaitScreen(bool (*game_exists)(void))
{
	NIN_CFG_AP_CARD(ncfg)[0] = 0; // a card name only ever comes from this boot's client
	net_result = 1;
	if (net_init_async(ApNetDone, NULL) < 0)
		net_result = -1;

	bool game_ok = game_exists();
	char ip[16] = "";
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
			break;

		if (FPAD_OK(0))
		{
			// The stock picker (SD/USB, then the game list); it saves nothing itself
			SelectDevAndGame();
			game_ok = game_exists();
		}
		else if (FPAD_Start(0))
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
	net_deinit();
}
