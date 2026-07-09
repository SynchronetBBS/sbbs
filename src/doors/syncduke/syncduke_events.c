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
	syncduke_map_name(ud.volume_number, ud.level_number, buf, sz);
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
static char     ev_cur_map[64];    /* display name of the level in play, captured at entry */

/* Capture the identity of the level now being played. The MODE_EOL edge must log THIS
 * name, not ud.volume/level_number at edge time: every EOL setter (exit switch, nuke
 * button -- sector.c, player.c) advances or wraps ud.level_number in the same tic it
 * sets MODE_EOL, so reading it at the edge names the NEXT level (and a cleared user
 * map, slot 7, wrapped to "E1L1"). */
static void ev_capture_level(void)
{
	syncduke_map_name(ud.volume_number, ud.level_number, ev_cur_map, sizeof(ev_cur_map));
	ev_level_start = (uint32_t)totalclock;
}

static int ev_secs(void)
{
	return (int)((uint32_t)totalclock - ev_level_start) / TICRATE;
}

/* The engine's OWN per-level play time, in seconds -- the value the bonus screen shows
 * as "Your Time". It counts game tics (player_par++ once per 26 Hz game tic; the bonus
 * screen divides by 26, see game.c dobonus) and lives in the player struct, so it is
 * written into savegames and RESUMES on load -- unlike ev_secs()'s door wall clock,
 * which restarts the timer at the door's level entry and so over-reports after a
 * save/load. Read it at the MODE_EOL edge, before the next level's resetplayerstats()
 * zeroes it. */
static int ev_level_secs(void)
{
	return ps[myconnectindex].player_par / 26;
}

/* A completed level and the time it took -- logged on the MODE_EOL edge with the
 * name captured at level entry (see ev_capture_level). Matches SyncDOOM's
 * sd_event_level ("cleared <map> in M:SS"). */
static void ev_level(const char *map, int secs)
{
	char j[300], ua[80], me[80];
	snprintf(j, sizeof(j),
	         "{\"time\":%ld,\"type\":\"level\",\"node\":%d,\"user\":\"%s\","
	         "\"map\":\"%s\",\"secs\":%d,\"skill\":%d}",
	         (long)time(NULL), sbbs_my_node(),
	         ev_esc(syncduke_door_alias(), ua, sizeof(ua)),
	         ev_esc(map, me, sizeof(me)), secs, ud.player_skill + 1);
	ev_emit(j);
}

static void ev_death(void)
{
	char j[300], ua[80], map[64], me[80];
	ev_map(map, sizeof(map));
	snprintf(j, sizeof(j),
	         "{\"time\":%ld,\"type\":\"death\",\"node\":%d,\"user\":\"%s\","
	         "\"map\":\"%s\",\"secs\":%d}",
	         (long)time(NULL), sbbs_my_node(),
	         ev_esc(syncduke_door_alias(), ua, sizeof(ua)),
	         ev_esc(map, me, sizeof(me)), ev_secs());
	ev_emit(j);
}

static void ev_frag(int victim)
{
	char        j[400], ka[80], va[80], vn[24], map[64], me[80];
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
	         ev_esc(vname, va, sizeof(va)),
	         ev_esc(map, me, sizeof(me)));
	ev_emit(j);
}

void syncduke_events_tick(void)
{
	static int in_game, dead_last, was_eol, started;
	static int last_vol = -1, last_lev = -1;
	int        vol = ud.volume_number, lev = ud.level_number;
	int        eol = (ps[myconnectindex].gm & MODE_EOL) != 0;   /* level cleared -> bonus screen */

	if (syncduke_eventlog_path()[0] == '\0')
		return;

	/* Level cleared: the reliable signal is the MODE_EOL edge (end-of-level). During
	 * MODE_EOL ev_real_game() goes false, dropping in_game BEFORE the next level loads.
	 * Log the name captured at level entry (ev_cur_map) -- the EOL setters have already
	 * advanced ud.level_number by now -- with the engine's own Your-Time elapsed. */
	if (eol && !was_eol && in_game && ev_cur_map[0] != '\0')
		ev_level(ev_cur_map, ev_level_secs());   /* engine's Your-Time, survives save/load */
	was_eol = eol;

	if (ev_real_game() && !in_game) {
		in_game = 1;
		if (!started) {
			/* Once per door session: deaths and level changes bounce in_game (gm loses
			 * MODE_GAME until the next enterlevel), and each bounce used to re-log
			 * "start", flooding the event log. */
			started = 1;
			ev_start();
		}
		ev_capture_level();                         /* entering a level (or respawning) */
		last_vol = vol; last_lev = lev;
	} else if (!ev_real_game() && in_game) {
		in_game = 0;
		last_vol = last_lev = -1;
	}

	/* Level changed while still in-game: NOT a clear -- real clears pass through the
	 * MODE_EOL edge above. This fires when the player ABANDONS the level from the
	 * in-game menu (new game / load / warp), so just re-capture the level identity.
	 * (This used to log ev_level: menu-quitting a user map posted "cleared E1L8 in
	 * 0:00" to the lobby feed.) */
	if (in_game && (vol != last_vol || lev != last_lev)) {
		ev_capture_level();
		last_vol = vol; last_lev = lev;
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
