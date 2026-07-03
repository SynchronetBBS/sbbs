/*
 * syncduke_net.c -- minimal UDP transport for Duke3D's mmulti seam (LAN co-op).
 *
 * The vendored Build/Duke netcode (game.c getpackets() + the move-FIFO lockstep)
 * is intact; only the transport seam is stubbed.  This fills it for 2-player LAN
 * play: getpacket/sendpacket move raw game packets between two door instances over
 * UDP, and initmultiplayers does a tiny master/slave handshake to set the mmulti
 * globals (numplayers / myconnectindex / connect list).  Chocolate-Duke's ENet
 * mmulti is the protocol oracle, but the ENet lib was removed from the tree, so we
 * carry our own UDP transport (no new dependency -- matches SyncDOOM).
 *
 * Configured by env (the door launcher / a wrapper sets these):
 *   SYNCDUKE_NET=master  SYNCDUKE_NET_PORT=<udp port>     -> player 0, binds + waits
 *   SYNCDUKE_NET=join    SYNCDUKE_NET_PEER=<host>:<port>  -> player 1, connects
 *   (unset)                                               -> single-player (numplayers=1)
 *
 * v1 scope: exactly 2 players, same LAN, low latency (lockstep cadence is fine).
 * The game's packet protocol is unchanged -- we only carry bytes + the sender index.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>        /* GetTickCount */
  #include <process.h>        /* _exit */
  #define SD_BADSOCK INVALID_SOCKET
  #define sd_closesock closesocket
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
  #include <time.h>           /* clock_gettime */
typedef int SOCKET;
  #define SD_BADSOCK (-1)
  #define sd_closesock close
#endif

#include "build.h"        /* MAXPLAYERS + the mmulti global types */
#include "syncduke.h"     /* syncduke_log */

/* --- mmulti globals (defined in dummy_multi.c; we set them up here) --- */
extern short numplayers, myconnectindex;
extern short connecthead, connectpoint2[];

#define MAXPACKETSIZE  2048            /* matches the original mmulti.c (game's packbuf) */
#define SD_NET_MAGIC   0x4b554431u     /* 'DUK1' handshake tag */
#define SD_HELLO       1               /* join -> master: "I want in" */
#define SD_WELCOME     2               /* master -> join: "you are player 1" */
#define SD_HS_TIMEOUT_MS 60000         /* how long to wait for the peer at startup */

typedef struct { uint32_t magic; uint8_t kind; uint8_t idx; } sd_handshake_t;

static SOCKET             g_sock = SD_BADSOCK;
static struct sockaddr_in g_peer[MAXPLAYERS];   /* addr of each player index */
static int                g_net_active;
static unsigned           g_rx, g_tx;           /* game-packet counters (bring-up diagnostics) */

/* Net config from the command line (the lobby's path), filled by syncduke_config.c's
 * pre-main arg pass.  Each falls back to the matching env var (the standalone test
 * harness path) when its buffer is empty. */
char syncduke_net_role[16];     /* -netrole master|join   (env SYNCDUKE_NET)      */
char syncduke_net_port[16];     /* -netport <udp port>    (env SYNCDUKE_NET_PORT) */
char syncduke_net_peer[128];    /* -netpeer <host:port>   (env SYNCDUKE_NET_PEER) */

/* Prefer the command-line value (the lobby sets these); else the env var. */
static const char *sd_net_cfg(const char *cli, const char *envname)
{
	if (cli && cli[0])
		return cli;
	return getenv(envname);
}

static uint32_t sd_net_now_ms(void)
{
#ifdef _WIN32
	return (uint32_t)GetTickCount();
#else
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint32_t)(t.tv_sec * 1000ULL + t.tv_nsec / 1000000ULL);
#endif
}

static void sd_set_nonblock(SOCKET s)
{
#ifdef _WIN32
	u_long nb = 1; ioctlsocket(s, FIONBIO, &nb);
#else
	int    fl = fcntl(s, F_GETFL, 0);
	if (fl != -1)
		fcntl(s, F_SETFL, fl | O_NONBLOCK);
#endif
}

/* Set the mmulti connect state for a 2-player game once the peers know each other. */
static void sd_net_become(int me)   /* me = 0 (master) or 1 (join) */
{
	numplayers     = 2;
	myconnectindex = (short)me;
	connecthead    = 0;
	connectpoint2[0] = 1;
	connectpoint2[1] = -1;
	g_net_active   = 1;
	syncduke_log("net: connected as player %d of 2", me);
}

/* Blocking-with-timeout handshake at game init (so numplayers is set before the
 * engine reads it).  Returns 1 on success (2 players), 0 to stay single-player. */
static int sd_net_handshake(const char *role)
{
	const char *       peer = sd_net_cfg(syncduke_net_peer, "SYNCDUKE_NET_PEER");
	const char *       port = sd_net_cfg(syncduke_net_port, "SYNCDUKE_NET_PORT");
	struct sockaddr_in me;
	uint32_t           deadline = sd_net_now_ms() + SD_HS_TIMEOUT_MS;

#ifdef _WIN32
	{ WSADATA w; WSAStartup(MAKEWORD(2, 2), &w); }
#endif
	g_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (g_sock == SD_BADSOCK) { syncduke_log("net: socket() failed"); return 0; }

	memset(&me, 0, sizeof me);
	me.sin_family = AF_INET;
	me.sin_addr.s_addr = htonl(INADDR_ANY);

	if (strcmp(role, "master") == 0) {
		sd_handshake_t hs;
		me.sin_port = htons((uint16_t)(port ? atoi(port) : 0));
		if (bind(g_sock, (struct sockaddr *)&me, sizeof me) != 0) {
			syncduke_log("net: bind(%s) failed", port ? port : "0"); sd_closesock(g_sock); return 0;
		}
		syncduke_log("net: master listening on udp/%s, waiting for player 2...", port ? port : "?");
		sd_set_nonblock(g_sock);
		/* poll (with timeout) until a join's HELLO arrives */
		for (;;) {
			socklen_t sl = sizeof g_peer[1];
			int       n;
			if ((int32_t)(sd_net_now_ms() - deadline) >= 0) { syncduke_log("net: no peer, single-player"); sd_closesock(g_sock); return 0; }
			n = recvfrom(g_sock, (char *)&hs, sizeof hs, 0, (struct sockaddr *)&g_peer[1], &sl);
			if (n == (int)sizeof hs && hs.magic == htonl(SD_NET_MAGIC) && hs.kind == SD_HELLO) {
				sd_handshake_t wel = { htonl(SD_NET_MAGIC), SD_WELCOME, 1 };
				sendto(g_sock, (const char *)&wel, sizeof wel, 0, (struct sockaddr *)&g_peer[1], sl);
				break;
			}
#ifndef _WIN32
			usleep(50000);
#endif
		}
		sd_net_become(0);
		return 1;
	}

	/* join (player 1): resolve the master's host:port, say HELLO, await WELCOME */
	if (!peer) { syncduke_log("net: SYNCDUKE_NET_PEER unset"); sd_closesock(g_sock); return 0; }
	{
		char            host[128]; char *colon; int pport;
		struct addrinfo hints, *ai = NULL;
		strncpy(host, peer, sizeof host - 1); host[sizeof host - 1] = 0;
		colon = strrchr(host, ':');
		if (!colon) { syncduke_log("net: SYNCDUKE_NET_PEER needs host:port"); sd_closesock(g_sock); return 0; }
		*colon = 0; pport = atoi(colon + 1);
		memset(&hints, 0, sizeof hints); hints.ai_family = AF_INET; hints.ai_socktype = SOCK_DGRAM;
		if (getaddrinfo(host, NULL, &hints, &ai) != 0 || !ai) { syncduke_log("net: resolve '%s' failed", host); sd_closesock(g_sock); return 0; }
		memcpy(&g_peer[0], ai->ai_addr, sizeof(struct sockaddr_in));
		g_peer[0].sin_port = htons((uint16_t)pport);
		freeaddrinfo(ai);
	}
	bind(g_sock, (struct sockaddr *)&me, sizeof me);   /* ephemeral local port */
	for (;;) {
		sd_handshake_t hi = { htonl(SD_NET_MAGIC), SD_HELLO, 0 }, rep;
		socklen_t      sl = sizeof g_peer[0];
		int            n;
		if ((int32_t)(sd_net_now_ms() - deadline) >= 0) { syncduke_log("net: master not found, single-player"); sd_closesock(g_sock); return 0; }
		sendto(g_sock, (const char *)&hi, sizeof hi, 0, (struct sockaddr *)&g_peer[0], sizeof g_peer[0]);
		sd_set_nonblock(g_sock);
		n = recvfrom(g_sock, (char *)&rep, sizeof rep, 0, (struct sockaddr *)&g_peer[0], &sl);
		if (n == (int)sizeof rep && rep.magic == htonl(SD_NET_MAGIC) && rep.kind == SD_WELCOME) {
			sd_net_become(1);
			return 1;
		}
#ifndef _WIN32
		usleep(100000);   /* 100ms between HELLOs */
#endif
	}
}

/* ---- the mmulti API the Build engine / game.c calls ---- */

void initmultiplayers(uint8_t damultioption, uint8_t dacomrateoption, uint8_t dapriority)
{
	const char *role = sd_net_cfg(syncduke_net_role, "SYNCDUKE_NET");
	(void)damultioption; (void)dacomrateoption; (void)dapriority;

	/* default single-player unless a net role is configured */
	numplayers = 1; myconnectindex = 0;
	connecthead = 0; connectpoint2[0] = -1;
	if (role && (strcmp(role, "master") == 0 || strcmp(role, "join") == 0)) {
		if (!sd_net_handshake(role)) {
			/* The lobby launched us for a MULTIPLAYER game (master/join) but the peer
			 * never connected.  Exit cleanly (freeing the BBS node) instead of silently
			 * dropping the host into a single-player match -- a lone game is never what
			 * a Create/Join asked for.  A true single-player launch has no net role and
			 * never reaches here, so it still plays solo.  _exit(0) matches the door's
			 * hangup path (syncduke_door.c): skip the engine's atexit cleanup, which
			 * could block on the socket. */
			syncduke_log("net: multiplayer requested (role=%s) but no peer connected -- exiting (no solo fallback)", role);
			fprintf(stderr, "syncduke: multiplayer peer did not connect -- exiting\n");
			fflush(stderr);
			_exit(0);
		}
	}
}

void uninitmultiplayers(void)
{
	if (g_sock != SD_BADSOCK) { sd_closesock(g_sock); g_sock = SD_BADSOCK; }
	g_net_active = 0;
}

/* Receive one game packet (non-blocking).  *other = sender's player index; returns
 * the length, or 0 if nothing is queued.  Late handshake datagrams are dropped. */
short getpacket(short *other, uint8_t *bufptr)
{
	struct sockaddr_in from;
	socklen_t          sl = sizeof from;
	int                n;

	if (!g_net_active || g_sock == SD_BADSOCK)
		return 0;
	for (;;) {
		n = recvfrom(g_sock, (char *)bufptr, MAXPACKETSIZE, 0, (struct sockaddr *)&from, &sl);
		if (n <= 0)
			return 0;
		if (n == (int)sizeof(sd_handshake_t)) {
			sd_handshake_t *hs = (sd_handshake_t *)bufptr;
			if (hs->magic == htonl(SD_NET_MAGIC))
				continue;            /* stray handshake datagram -- skip, read the next */
		}
		/* identify the sender by source address (2-player: peer 0 or peer 1) */
		*other = (from.sin_addr.s_addr == g_peer[0].sin_addr.s_addr
		          && from.sin_port == g_peer[0].sin_port) ? 0 : 1;
		if (++g_rx <= 16 || (g_rx & 0x3F) == 0)
			syncduke_log("net: rx #%u type=%d len=%d from=p%d", g_rx, bufptr[0], n, *other);
		return (short)n;
	}
}

void sendpacket(int32_t other, uint8_t *bufptr, int32_t messleng)
{
	if (!g_net_active || g_sock == SD_BADSOCK || messleng <= 0)
		return;
	if (++g_tx <= 16 || (g_tx & 0x3F) == 0)
		syncduke_log("net: tx #%u type=%d to=p%d len=%d", g_tx, bufptr[0], (int)other, (int)messleng);
	if (other < 0) {                 /* broadcast: to every other player */
		int i;
		for (i = connecthead; i >= 0; i = connectpoint2[i])
			if (i != myconnectindex)
				sendto(g_sock, (const char *)bufptr, messleng, 0,
				       (struct sockaddr *)&g_peer[i], sizeof g_peer[i]);
		return;
	}
	if (other == myconnectindex || other >= MAXPLAYERS)
		return;
	sendto(g_sock, (const char *)bufptr, messleng, 0,
	       (struct sockaddr *)&g_peer[other], sizeof g_peer[other]);
}

/* logon/logoff carry player info in the full netcode; for 2-player LAN the connect
 * list is already established by the handshake, so these are no-ops for now. */
void sendlogon(void)  { }
void sendlogoff(void) { }
void setpackettimeout(int32_t a, int32_t b) { (void)a; (void)b; }
void flushpackets(void) { }
int  getoutputcirclesize(void) { return 0; }
