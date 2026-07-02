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

/* Who's-online free-text status for the node (syncduke_node.c).  "playing SyncDuke",
 * plus the current episode/level as (E#L#) while a REAL game is in progress -- excluded
 * during the menu/title and attract-mode demos (MODE_DEMO / ud.recstat==2), which would
 * otherwise advertise the demo's level as the player's.  Duke stores volume/level 0-based;
 * the game itself displays them +1 as "e%dl%d" (game.c), so we match that. */
void syncduke_game_status(char *buf, size_t bufsz)
{
	uint8_t gm = ps[myconnectindex].gm;

	if ((gm & MODE_GAME) && !(gm & MODE_DEMO) && ud.recstat != 2)
		snprintf(buf, bufsz, "playing SyncDuke (E%dL%d)",
		         ud.volume_number + 1, ud.level_number + 1);
	else
		snprintf(buf, bufsz, "playing SyncDuke");
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
