/*
 * syncduke_game.c -- small queries into Duke game state for the door shim.
 *
 * Isolated here because it pulls in the full game header (duke3d.h); the rest of
 * our input layer (syncduke_input.c) stays engine-independent and unit-testable. The
 * caller (_handle_events) passes the result into syncduke_input_pump so the WASD/Space
 * action layer applies only while actually playing -- in a menu or while typing a
 * save-game name, letters and Space must arrive literally.
 */

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
