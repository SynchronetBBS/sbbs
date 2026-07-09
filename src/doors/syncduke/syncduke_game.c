/*
 * syncduke_game.c -- small queries into Duke game state for the door shim.
 *
 * Isolated here because it pulls in the full game header (duke3d.h); the rest of
 * our input layer (syncduke_input.c) stays engine-independent and unit-testable. The
 * caller (_handle_events) passes the result into syncduke_input_pump so the WASD/Space
 * action layer applies only while actually playing -- in a menu or while typing a
 * save-game name, letters and Space must arrive literally.
 */

#include <stdio.h>    /* snprintf (node status) */
#include <string.h>   /* strncpy (HUD capture) */
#include "duke3d.h"
#include "syncduke.h"
#include "dirwrap.h"  /* xpdev: getfname()/getfext() -- user-map display name */

int syncduke_in_gameplay(void)
{
	uint8_t gm = ps[myconnectindex].gm;
	/* MODE_TYPE = the player is typing a chat message ('T'alk). Like a menu, the
	 * action layer must be OFF so letters/digits/Space reach the message buffer
	 * literally -- otherwise a typed 'a' becomes a strafe scancode and shows as
	 * punctuation (the on-screen "OUT OF SYNC" from chat was a separate vendored
	 * bug -- typemode sent the wrong buffer -- fixed in game.c). */
	return (gm & MODE_GAME) && !(gm & (MODE_MENU | MODE_TYPE));
}

/* True while the player is dead (health <= 0).  The door treats this like "not in
 * gameplay" so the spacebar reverts from Fire to its plain Open key (sc_Space) -- which
 * is what Duke's "PRESS SPACE TO RESTART LEVEL" prompt actually waits for (gamefunc_Open,
 * default key Space).  Without this the spacebar sends Fire and the restart never fires. */
int syncduke_player_dead(void)
{
	uint8_t gm = ps[myconnectindex].gm;
	return (gm & MODE_GAME) && sprite[ps[myconnectindex].i].extra <= 0;
}

/* Display name for a level, shared by the node status and the events feed.
 *
 * A stock (or add-on) level is named by the engine's own level_names[] table, which
 * gamedef.c fills from the CON's definelevelname directives ("RAW MEAT") -- uppercased
 * by the parser, and on the PLATFORM_UNIX path keeping everything up to the newline, so
 * a CRLF CON leaves a trailing '\r' to trim.  An entry is empty for any slot the CON
 * never defined; fall back to "E<vol>L<lev>" there.
 *
 * A -map user map is named by its file's basename with ".map" stripped ("Roch") -- its
 * level slot is always 7, and its level_names[] entry is whatever the CON defined for
 * that slot (or nothing), so neither the table nor "E1L8" would mean anything. */
void syncduke_map_name(int vol, int lev, char *buf, size_t sz)
{
	const char *name;
	size_t      n;

	if (boardfilename[0] != '\0' && lev == 7 && vol == 0) {
		const char *b = getfname(boardfilename);
		const char *e = getfext(b);
		n = strlen(b);
		if (e != NULL
		    && (e[1] == 'm' || e[1] == 'M')
		    && (e[2] == 'a' || e[2] == 'A')
		    && (e[3] == 'p' || e[3] == 'P') && e[4] == '\0')
			n = (size_t)(e - b);
		snprintf(buf, sz, "%.*s", (int)n, b);
		return;
	}

	name = (vol >= 0 && lev >= 0 && (vol * 11) + lev < 44)
	       ? level_names[(vol * 11) + lev] : "";
	for (n = strlen(name); n > 0 && (unsigned char)name[n - 1] <= ' '; n--)
		;
	if (n > 0)
		snprintf(buf, sz, "%.*s", (int)n, name);
	else
		snprintf(buf, sz, "E%dL%d", vol + 1, lev + 1);
}

/* Who's-online free-text status for the node (syncduke_node.c).  "playing SyncDuke",
 * plus the current level's name while a REAL game is in progress -- excluded during the
 * menu/title and attract-mode demos (MODE_DEMO / ud.recstat==2), which would otherwise
 * advertise the demo's level as the player's. */
void syncduke_game_status(char *buf, size_t bufsz)
{
	uint8_t gm = ps[myconnectindex].gm;
	char    map[64];

	if ((gm & MODE_GAME) && !(gm & MODE_DEMO) && ud.recstat != 2) {
		syncduke_map_name(ud.volume_number, ud.level_number, map, sizeof(map));
		snprintf(buf, bufsz, "playing SyncDuke (Nukem 3D): %s", map);
	}
	else
		snprintf(buf, bufsz, "playing SyncDuke (Nukem 3D)");
}

/* --- text-tier legible HUD overlay (see syncduke.h) ---------------------------
 * operatefta() (game.c) captures the active quote/chat strings here -- instead of
 * rasterising the (unreadable in a block tier) game font -- whenever syncduke_text_hud
 * is set; syncduke_io.c's text-tier present() redraws them as real terminal characters.
 * Kept here (not the engine-independent input layer) since the flag/buffer are the
 * engine<->door seam.  Reset each frame by the door (syncduke_hud_begin at present's
 * end) so a frame that draws no quote -- or a menu, where operatefta doesn't run --
 * leaves the overlay empty. */
int                        syncduke_text_hud;

static syncduke_hud_line_t sd_hud[SYNCDUKE_HUD_MAX];
static int                 sd_hud_n;

void syncduke_hud_begin(void) { sd_hud_n = 0; }

void syncduke_hud_add(const char *text, int y)
{
	syncduke_hud_line_t *L;

	if (text == NULL || text[0] == '\0' || sd_hud_n >= SYNCDUKE_HUD_MAX)
		return;
	L = &sd_hud[sd_hud_n++];
	strncpy(L->text, text, SYNCDUKE_HUD_LEN - 1);
	L->text[SYNCDUKE_HUD_LEN - 1] = '\0';
	L->y = y;
}

int syncduke_hud_lines(const syncduke_hud_line_t **out)
{
	if (out != NULL)
		*out = sd_hud;
	return sd_hud_n;
}

/* --- dukematch frag detection (see syncduke_events.c) --------------------------
 * The deterministic lockstep sim runs every player on both door instances, so the
 * global frags[killer][victim] matrix is identical on each.  We report ONLY our own
 * row (frags[myconnectindex][*]); the peer reports its own -- so each frag is logged
 * exactly once, no central writer, no dedup (mirrors SyncDOOM's sd_next_frag).  A
 * static shadow lets a multi-frag tic drain one victim per call.  Self-frags stay the
 * existing 'death' event (they don't increment the cross-player matrix -- player.c's
 * fraggedself path), so they're intentionally not reported here. */
int syncduke_next_frag(int *victim)
{
	static short seen[MAXPLAYERS];
	static int   seen_valid;
	int          me = myconnectindex, j;

	/* Only in a real dukematch (multiplayer, not co-op). */
	if (!(ud.multimode > 1 && ud.coop != 1)) {
		seen_valid = 0;
		return 0;
	}
	if (!seen_valid) {                 /* first call this match: adopt current counts */
		for (j = 0; j < MAXPLAYERS; j++)
			seen[j] = frags[me][j];
		seen_valid = 1;
		return 0;
	}
	for (j = 0; j < MAXPLAYERS; j++) {
		if (j == me)
			continue;
		if (frags[me][j] > seen[j]) {
			seen[j]++;
			if (victim != NULL)
				*victim = j;
			return 1;
		}
	}
	return 0;
}
