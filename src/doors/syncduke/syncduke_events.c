/*
 * syncduke_events.c -- door-written events.jsonl activity log (start/level/death)
 * for the lobby's recent-activity feed. One compact JSON line per event, appended
 * (atomic, no lock) to the -eventlog path; "" disables logging. Includes duke3d.h
 * for the engine state the events describe. Mirrors src/doors/syncdoom/syncdoom.c's
 * sd_event_* model. Detector runs once per frame from _nextpage (syncduke_plat.c).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#include "duke3d.h"
#include "syncduke.h"
#include "sbbs_node.h"  /* termgfx: sbbs_my_node() -- current node # from SBBSNNUM */
#include "git_hash.h"   /* GIT_HASH, GIT_DATE */

/* Minimal JSON string escaper (aliases / tier / build). */
static const char *ev_esc(const char *s, char *out, size_t sz)
{
	size_t o = 0;
	if (s == NULL)
		s = "";
	for (; *s && o + 7 < sz; s++) {
		unsigned char c = (unsigned char)*s;
		if (c == '"' || c == '\\') { out[o++] = '\\'; out[o++] = (char)c; }
		else if (c < 0x20)         { o += (size_t)snprintf(out + o, sz - o, "\\u%04x", c); }
		else                        { out[o++] = (char)c; }
	}
	out[o] = '\0';
	return out;
}

static void ev_emit(const char *json)
{
	const char *path = syncduke_eventlog_path();
	FILE *      f;
	char        line[700];
	int         n;
	if (path[0] == '\0')
		return;
	n = snprintf(line, sizeof(line), "%s\n", json);
	if (n < 1 || (f = fopen(path, "ab")) == NULL)
		return;
	fwrite(line, 1, (size_t)n, f);   /* single append -> atomic line */
	fclose(f);
}

static int ev_real_game(void)
{
	uint8_t gm = ps[myconnectindex].gm;
	return (gm & MODE_GAME) && !(gm & MODE_DEMO) && ud.recstat != 2;
}

static const char *ev_mode(void)
{
	extern char syncduke_net_role[16];
	return (strcmp(syncduke_net_role, "master") == 0 || strcmp(syncduke_net_role, "join") == 0)
	       ? "co-op" : "single";
}

static void ev_map(char *buf, size_t sz)
{
	snprintf(buf, sz, "E%dL%d", ud.volume_number + 1, ud.level_number + 1);
}

/* usernum from the -home path (".../user/<num>/duke/"), like syncdoom. */
static int ev_usernum(void)
{
	const char *u = strstr(syncduke_home(), "user/");
	return u ? atoi(u + 5) : 0;
}

static void ev_start(void)
{
	char j[700], ua[80], ti[24], hh[24], hd[48];
	int  cols = (syncduke_term_px_w() > 0 ? syncduke_term_px_w() : 640) / 8;
	int  rows = (syncduke_term_px_h() > 0 ? syncduke_term_px_h() : 384) / 16;
	snprintf(j, sizeof(j),
	         "{\"time\":%ld,\"type\":\"start\",\"node\":%d,\"usernum\":%d,\"user\":\"%s\","
	         "\"term\":{\"cols\":%d,\"rows\":%d},\"tier\":\"%s\","
	         "\"build\":{\"hash\":\"%s\",\"date\":\"%s\"},\"mode\":\"%s\",\"skill\":%d}",
	         (long)time(NULL), sbbs_my_node(), ev_usernum(),
	         ev_esc(syncduke_door_alias(), ua, sizeof(ua)), cols, rows,
	         ev_esc(syncduke_active_tier_name(), ti, sizeof(ti)),
	         ev_esc(GIT_HASH, hh, sizeof(hh)), ev_esc(GIT_DATE, hd, sizeof(hd)),
	         ev_mode(), ud.player_skill + 1);
	ev_emit(j);
}

static uint32_t ev_level_start;    /* totalclock at the current level's entry */

static int ev_secs(void)
{
	return (int)((uint32_t)totalclock - ev_level_start) / TICRATE;
}

static void ev_level(int secs)     /* secs = the PREVIOUS level's elapsed */
{
	char j[300], ua[80], map[16];
	ev_map(map, sizeof(map));
	snprintf(j, sizeof(j),
	         "{\"time\":%ld,\"type\":\"level\",\"node\":%d,\"user\":\"%s\","
	         "\"map\":\"%s\",\"secs\":%d,\"skill\":%d}",
	         (long)time(NULL), sbbs_my_node(),
	         ev_esc(syncduke_door_alias(), ua, sizeof(ua)), map, secs, ud.player_skill + 1);
	ev_emit(j);
}

static void ev_death(void)
{
	char j[300], ua[80], map[16];
	ev_map(map, sizeof(map));
	snprintf(j, sizeof(j),
	         "{\"time\":%ld,\"type\":\"death\",\"node\":%d,\"user\":\"%s\","
	         "\"map\":\"%s\",\"secs\":%d}",
	         (long)time(NULL), sbbs_my_node(),
	         ev_esc(syncduke_door_alias(), ua, sizeof(ua)), map, ev_secs());
	ev_emit(j);
}

void syncduke_events_tick(void)
{
	static int in_game, dead_last;
	static int last_vol = -1, last_lev = -1;
	int        vol = ud.volume_number, lev = ud.level_number;

	if (syncduke_eventlog_path()[0] == '\0')
		return;

	if (ev_real_game() && !in_game) {
		in_game = 1;
		ev_start();
		ev_level_start = (uint32_t)totalclock;      /* first level of the session */
		last_vol = vol; last_lev = lev;
		ev_level(0);                                /* entered first level */
	} else if (!ev_real_game() && in_game) {
		in_game = 0;
		last_vol = last_lev = -1;
	}

	if (in_game && (vol != last_vol || lev != last_lev)) {   /* level change */
		int prev = ev_secs();                       /* elapsed on the level we just left */
		last_vol = vol; last_lev = lev;
		ev_level_start = (uint32_t)totalclock;
		ev_level(prev);
	}

	if (in_game) {                                  /* death rising edge */
		int dead = syncduke_player_dead();
		if (dead && !dead_last)
			ev_death();
		dead_last = dead;
	} else {
		dead_last = 0;
	}
}
