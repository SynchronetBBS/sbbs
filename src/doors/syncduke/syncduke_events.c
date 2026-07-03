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
	if (strcmp(syncduke_net_role, "master") != 0 && strcmp(syncduke_net_role, "join") != 0)
		return "single";
	/* In a net game ud.coop reflects the master's chosen mode (set from the /c flag on
	 * the master, from the start packet on the joiner): 1 = co-op, 0 = DM spawn,
	 * 2 = DM no-spawn. */
	if (ud.coop == 1)
		return "co-op";
	return (ud.coop == 2) ? "dukematch-nospawn" : "dukematch";
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

/* A completed level and the time it took -- logged on the level-change edge, so
 * `vol`/`lev` name the level just CLEARED (not the one entered) and `secs` is its
 * own elapsed. Matches SyncDOOM's sd_event_level ("cleared <map> in M:SS"). */
static void ev_level(int vol, int lev, int secs)
{
	char j[300], ua[80], map[16];
	snprintf(map, sizeof(map), "E%dL%d", vol + 1, lev + 1);
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

static void ev_frag(int victim)
{
	char        j[400], ka[80], va[80], vn[24], map[16];
	const char *vname = ud.user_name[victim];
	if (vname == NULL || vname[0] == '\0') {
		snprintf(vn, sizeof(vn), "player %d", victim + 1);
		vname = vn;
	}
	ev_map(map, sizeof(map));
	snprintf(j, sizeof(j),
	         "{\"time\":%ld,\"type\":\"frag\",\"node\":%d,\"killer\":\"%s\",\"victim\":\"%s\","
	         "\"map\":\"%s\"}",
	         (long)time(NULL), sbbs_my_node(),
	         ev_esc(syncduke_door_alias(), ka, sizeof(ka)),
	         ev_esc(vname, va, sizeof(va)), map);
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
	} else if (!ev_real_game() && in_game) {
		in_game = 0;
		last_vol = last_lev = -1;
	}

	if (in_game && (vol != last_vol || lev != last_lev)) {   /* finished a level */
		int prev = ev_secs();                       /* elapsed on the level we just cleared */
		ev_level(last_vol, last_lev, prev);         /* log the COMPLETED level + its time */
		last_vol = vol; last_lev = lev;
		ev_level_start = (uint32_t)totalclock;
	}

	if (in_game) {                                  /* frags by us, then our death edge */
		int victim;
		int dead = syncduke_player_dead();
		while (syncduke_next_frag(&victim))
			ev_frag(victim);
		if (dead && !dead_last)
			ev_death();
		dead_last = dead;
	} else {
		dead_last = 0;
	}
}
