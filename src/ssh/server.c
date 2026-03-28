/*
 * Example SSH server using the high-level DeuceSSH API.
 * Forks per connection, 60-second session timeout.
 * Usage: server [port]
 */

#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "deucessh-algorithms.h"
#include "deucessh-auth.h"
#include "deucessh-conn.h"
#include "deucessh.h"

static int          conn_fd = -1;
static dssh_session sess;

/* ================================================================
 * Terminate callback -- unblock I/O callbacks
 * ================================================================ */
static void
on_terminate(dssh_session s, void *cbdata)
{
	if (conn_fd != -1)
		shutdown(conn_fd, SHUT_RDWR);
}

/* ================================================================
 * I/O callbacks
 * ================================================================ */
static int
tx(uint8_t *buf, size_t bufsz, dssh_session s, void *cbdata)
{
	size_t sent = 0;

	while (sent < bufsz && !dssh_session_is_terminated(s)) {
		ssize_t n = send(conn_fd, &buf[sent], bufsz - sent, 0);

		if (n > 0)
			sent += n;
		else if ((n < 0) && (errno != EAGAIN) && (errno != EINTR))
			return DSSH_ERROR_INIT;
	}
	return 0;
}

static int
rx(uint8_t *buf, size_t bufsz, dssh_session s, void *cbdata)
{
	size_t got = 0;

	while (got < bufsz && !dssh_session_is_terminated(s)) {
		struct pollfd fd = {.fd = conn_fd, .events = POLLIN};
		int           pr = poll(&fd, 1, 5000);

		if (pr < 0)
			return DSSH_ERROR_INIT;
		if (pr == 0)
			continue;
		if (fd.revents & (POLLHUP | POLLERR) && !(fd.revents & POLLIN))
			return DSSH_ERROR_INIT;

		ssize_t n = recv(conn_fd, &buf[got], bufsz - got, 0);

		if (n > 0)
			got += n;
		else if (n == 0)
			return DSSH_ERROR_INIT;
		else if ((errno != EAGAIN) && (errno != EINTR))
			return DSSH_ERROR_INIT;
	}
	return 0;
}

static int
rxline(uint8_t *buf, size_t bufsz, size_t *bytes_received,
    dssh_session s, void *cbdata)
{
	size_t pos = 0;
	bool   lastcr = false;

	while (!dssh_session_is_terminated(s)) {
		int res = rx(&buf[pos], 1, s, cbdata);

		if (res < 0)
			return res;
		if (buf[pos] == '\r')
			lastcr = true;
		if ((buf[pos] == '\n') && lastcr) {
			*bytes_received = pos + 1;
			return 0;
		}
		if (pos + 1 < bufsz)
			pos++;
		else
			return DSSH_ERROR_TOOLONG;
	}
	return DSSH_ERROR_TERMINATED;
}

/* ================================================================
 * Auth callbacks
 * ================================================================ */
static const char *reject_quips[] = {
	"Nope. Try again, champ.\r\n",
	"Denied! The dice gods are not amused.\r\n",
	"Wrong! (Just kidding, we rolled a bad number.)\r\n",
	"Access denied. Have you tried being luckier?\r\n",
	"The RNG has spoken, and it says no.\r\n",
	"75% chance of failure. You hit it. Congrats?\r\n",
	"That's a no from me, dawg.\r\n",
	"Have you considered a career that doesn't involve computers?\r\n",
	"I'm not saying your credentials are bad, but... yeah, they're bad.\r\n",
	"Error 418: I'm a teapot. Also, no.\r\n",
	"Roses are red, violets are blue, auth denied, try something new.\r\n",
	"Permission denied (it's not you, it's me. Actually, it IS you).\r\n",
	"INSERT COIN TO CONTINUE\r\n",
	"Your princess is in another castle.\r\n",
	"Did you try turning it off and on again?\r\n",
	"I asked my magic 8-ball. It said 'outlook not so good'.\r\n",
};
#define N_QUIPS (sizeof(reject_quips) / sizeof(reject_quips[0]))

static void
set_banner(const char *msg)
{
	fprintf(stderr, "  BANNER: %s", msg);
	dssh_auth_set_banner(sess, msg, NULL);
}

static int
auth_none(const uint8_t *username, size_t username_len, void *cbdata)
{
	static bool tried;
	char buf[256];

	if (tried) {
		set_banner("You already tried that. It didn't work then either.\r\n");
		return DSSH_AUTH_FAILURE;
	}
	tried = true;

	if (arc4random_uniform(10) != 0) {
		snprintf(buf, sizeof(buf),
		    "Oh, '%.*s' wants in without even trying? %s",
		    (int)username_len, username,
		    reject_quips[arc4random_uniform(N_QUIPS)]);
		set_banner(buf);
		return DSSH_AUTH_FAILURE;
	}
	snprintf(buf, sizeof(buf),
	    "Oh, '%.*s' wants in without even trying? "
	    "1 in 10 chance and you hit it. Buy a lottery ticket!\r\n",
	    (int)username_len, username);
	set_banner(buf);
	return DSSH_AUTH_SUCCESS;
}

static int
auth_password(const uint8_t *username, size_t username_len,
    const uint8_t *password, size_t password_len,
    uint8_t **change_prompt, size_t *change_prompt_len,
    void *cbdata)
{
	char buf[256];

	if (arc4random_uniform(4) != 0) {
		snprintf(buf, sizeof(buf),
		    "'%.*s'/'%.*s'? Pfft. %s"
		    "But I'll give you a chance to change it...\r\n",
		    (int)username_len, username,
		    (int)password_len, password,
		    reject_quips[arc4random_uniform(N_QUIPS)]);
		set_banner(buf);
		static const char prompt[] = "Your password is terrible. Pick a new one.";
		*change_prompt = malloc(sizeof(prompt));
		if (*change_prompt != NULL) {
			memcpy(*change_prompt, prompt, sizeof(prompt));
			*change_prompt_len = sizeof(prompt) - 1;
		}
		return DSSH_AUTH_CHANGE_PASSWORD;
	}
	snprintf(buf, sizeof(buf),
	    "Wow, '%.*s' with password '%.*s'? "
	    "Security at its finest. But you got lucky!\r\n",
	    (int)username_len, username,
	    (int)password_len, password);
	set_banner(buf);
	return DSSH_AUTH_SUCCESS;
}

static int
auth_passwd_change(const uint8_t *username, size_t username_len,
    const uint8_t *old_password, size_t old_password_len,
    const uint8_t *new_password, size_t new_password_len,
    uint8_t **change_prompt, size_t *change_prompt_len,
    void *cbdata)
{
	char buf[256];

	snprintf(buf, sizeof(buf),
	    "Password changed from '%.*s' to '%.*s'? "
	    "Sure, why not. Welcome in!\r\n",
	    (int)old_password_len, old_password,
	    (int)new_password_len, new_password);
	set_banner(buf);
	return DSSH_AUTH_SUCCESS;
}

static int
auth_publickey(const uint8_t *username, size_t username_len,
    const char *algo_name,
    const uint8_t *pubkey_blob, size_t pubkey_blob_len,
    bool has_signature, void *cbdata)
{
	char buf[256];

	if (!has_signature) {
		/* PK_OK query -- just say yes so they send the real attempt */
		return DSSH_AUTH_SUCCESS;
	}
	if (arc4random_uniform(4) != 0) {
		snprintf(buf, sizeof(buf),
		    "Nice key, '%.*s'. Shame about the luck. %s",
		    (int)username_len, username,
		    reject_quips[arc4random_uniform(N_QUIPS)]);
		set_banner(buf);
		return DSSH_AUTH_FAILURE;
	}
	snprintf(buf, sizeof(buf),
	    "Key auth from '%.*s' with %s? "
	    "Fancy. And lucky!\r\n",
	    (int)username_len, username, algo_name);
	set_banner(buf);
	return DSSH_AUTH_SUCCESS;
}

/* ================================================================
 * KBI Adventure Game
 * ================================================================ */
enum room { ROOM_GATE, ROOM_GARDEN, ROOM_TOWER, ROOM_PARAPET };

struct game_state {
	enum room  room;
	bool       has_key;
	bool       riddle_solved;
	bool       talked_raven;
	bool       looked;  /* looked in current room this visit */
	char       username[64];
};

static const char *gate_desc =
    "\r\n"
    "You stand before a towering iron gate, rusted shut.\r\n"
    "A heavy padlock secures the latch. Beyond the bars,\r\n"
    "you can see a warm terminal session, tantalizingly close.\r\n"
    "A path leads north to an overgrown garden, and\r\n"
    "a crumbling tower rises to the west.\r\n\r\n";

static const char *garden_desc =
    "\r\n"
    "An overgrown garden surrounds an ancient stone fountain.\r\n"
    "Strange symbols are carved into the fountain's rim.\r\n"
    "A mossy path leads south back to the gate.\r\n\r\n";

static const char *tower_desc =
    "\r\n"
    "You stand at the base of a crumbling watchtower.\r\n"
    "Stone steps spiral upward into shadow.\r\n"
    "The path leads east back to the gate.\r\n\r\n";

static const char *parapet_desc =
    "\r\n"
    "You emerge onto the windswept parapet atop the tower.\r\n"
    "A large raven perches on the crumbling battlement,\r\n"
    "watching you with unsettling intelligence.\r\n"
    "The stairs lead back down.\r\n\r\n";

static const char *gate_prompt = "Gate> ";
static const char *garden_prompt = "Garden> ";
static const char *tower_prompt = "Tower> ";
static const char *parapet_prompt = "Parapet> ";

static const char *help_text =
    "Commands: look [object], examine [object], go <direction>,\r\n"
    "          n/s/e/w/u/d, take, use, read, talk, answer <text>,\r\n"
    "          inventory, help, quit\r\n";

static bool
cmd_eq(const char *input, const char *cmd)
{
	return strcasecmp(input, cmd) == 0;
}

static bool
cmd_starts(const char *input, const char *prefix)
{
	size_t len = strlen(prefix);
	return strncasecmp(input, prefix, len) == 0 && input[len] == ' ';
}

static const char *
cmd_arg(const char *input, const char *prefix)
{
	return input + strlen(prefix) + 1;
}

static bool
dir_match(const char *input, const char *dir, const char *abbrev)
{
	if (cmd_eq(input, dir) || cmd_eq(input, abbrev))
		return true;
	if (cmd_starts(input, "go"))
		return cmd_eq(cmd_arg(input, "go"), dir)
		    || cmd_eq(cmd_arg(input, "go"), abbrev);
	return false;
}

static bool
looking_at(const char *cmd, const char *obj)
{
	if (cmd_starts(cmd, "look"))
		return cmd_eq(cmd_arg(cmd, "look"), obj);
	if (cmd_starts(cmd, "examine"))
		return cmd_eq(cmd_arg(cmd, "examine"), obj);
	if (cmd_starts(cmd, "x"))
		return cmd_eq(cmd_arg(cmd, "x"), obj);
	return false;
}

static int
auth_kbi(const uint8_t *username, size_t username_len,
    uint32_t num_responses,
    const uint8_t **responses, const size_t *response_lens,
    char **name_out, char **instruction_out,
    uint32_t *num_prompts_out,
    char ***prompts_out, bool **echo_out,
    void *cbdata)
{
	static struct game_state gs;
	char banner[1024];

	/* Reset session timeout on each KBI interaction */
	alarm(60);

	/* First call -- initialize game */
	if (num_responses == 0 && responses == NULL) {
		memset(&gs, 0, sizeof(gs));
		gs.room = ROOM_GATE;
		size_t nlen = username_len < sizeof(gs.username) - 1
		    ? username_len : sizeof(gs.username) - 1;
		memcpy(gs.username, username, nlen);
		gs.username[nlen] = '\0';

		snprintf(banner, sizeof(banner),
		    "\r\n--- The Gate of Authentication ---\r\n"
		    "\r\n"
		    "Welcome, %s. To prove your worth, you must find\r\n"
		    "the key that unlocks the gate.\r\n"
		    "%s",
		    gs.username, gate_desc);
		set_banner(banner);
		gs.looked = true;
		goto prompt;
	}

	/* Shouldn't happen, but handle gracefully */
	if (num_responses < 1 || responses == NULL)
		return DSSH_AUTH_FAILURE;

	/* Get the command -- copy to NUL-terminated string */
	char cmd[256];
	size_t clen = response_lens[0] < sizeof(cmd) - 1
	    ? response_lens[0] : sizeof(cmd) - 1;
	memcpy(cmd, responses[0], clen);
	cmd[clen] = '\0';

	/* Strip trailing whitespace */
	while (clen > 0 && (cmd[clen - 1] == ' ' || cmd[clen - 1] == '\t'))
		cmd[--clen] = '\0';

	/* Empty command */
	if (clen == 0)
		goto prompt;

	/* Universal commands */
	if (cmd_eq(cmd, "quit") || cmd_eq(cmd, "exit")) {
		set_banner("You walk away from the gate. Maybe next time.\r\n");
		return DSSH_AUTH_FAILURE;
	}
	if (cmd_eq(cmd, "help") || cmd_eq(cmd, "?")) {
		set_banner(help_text);
		goto prompt;
	}
	if (cmd_eq(cmd, "inventory") || cmd_eq(cmd, "inv") || cmd_eq(cmd, "i")) {
		if (gs.has_key)
			set_banner("You are carrying: a rusty iron key\r\n");
		else
			set_banner("You are carrying nothing.\r\n");
		goto prompt;
	}

	/* Room-specific commands */
	switch (gs.room) {
	case ROOM_GATE:
		if (cmd_eq(cmd, "look") || cmd_eq(cmd, "l")) {
			set_banner(gate_desc);
			gs.looked = true;
			goto prompt;
		}
		if (looking_at(cmd, "gate") || looking_at(cmd, "door")) {
			set_banner(
			    "\r\nThe gate is wrought from black iron, "
			    "twisted into thorny spirals.\r\n"
			    "It's cold to the touch and far too heavy "
			    "to force open.\r\n\r\n");
			goto prompt;
		}
		if (looking_at(cmd, "padlock") || looking_at(cmd, "lock")) {
			if (gs.has_key)
				set_banner(
				    "\r\nA heavy padlock. The keyhole looks "
				    "like it might accept the key you're "
				    "carrying.\r\n\r\n");
			else
				set_banner(
				    "\r\nA heavy padlock, green with verdigris. "
				    "The keyhole is shaped\r\n"
				    "like an old-fashioned skeleton key. "
				    "You'll need to find one.\r\n\r\n");
			goto prompt;
		}
		if (looking_at(cmd, "bars")) {
			set_banner(
			    "\r\nThrough the bars you can see a softly "
			    "glowing terminal prompt.\r\n"
			    "It blinks patiently. It has all the time "
			    "in the world.\r\n\r\n");
			goto prompt;
		}
		if (looking_at(cmd, "path") || looking_at(cmd, "paths")) {
			set_banner(
			    "\r\nA cobblestone path branches north toward "
			    "an overgrown garden,\r\n"
			    "and west toward a crumbling stone "
			    "tower.\r\n\r\n");
			goto prompt;
		}
		if (looking_at(cmd, "tower")) {
			set_banner(
			    "\r\nThe tower rises to the west, its upper "
			    "stones crumbling.\r\n"
			    "Something dark moves on the parapet.\r\n\r\n");
			goto prompt;
		}
		if (looking_at(cmd, "garden")) {
			set_banner(
			    "\r\nTo the north, vines and wildflowers spill "
			    "over what was once\r\n"
			    "a formal garden. You can just make out a "
			    "stone fountain.\r\n\r\n");
			goto prompt;
		}
		if (dir_match(cmd, "north", "n")) {
			gs.room = ROOM_GARDEN;
			gs.looked = false;
			set_banner(garden_desc);
			gs.looked = true;
			goto prompt;
		}
		if (dir_match(cmd, "west", "w")) {
			gs.room = ROOM_TOWER;
			gs.looked = false;
			set_banner(tower_desc);
			gs.looked = true;
			goto prompt;
		}
		if (dir_match(cmd, "east", "e")
		    || dir_match(cmd, "south", "s")
		    || dir_match(cmd, "up", "u")
		    || dir_match(cmd, "down", "d")) {
			set_banner("There is no path in that direction.\r\n");
			goto prompt;
		}
		if (cmd_eq(cmd, "use key") || cmd_eq(cmd, "unlock gate")
		    || cmd_eq(cmd, "unlock door")
		    || cmd_eq(cmd, "open gate")
		    || cmd_eq(cmd, "open door")) {
			if (!gs.has_key) {
				set_banner(
				    "You rattle the padlock uselessly. "
				    "You need a key.\r\n");
				goto prompt;
			}
			snprintf(banner, sizeof(banner),
			    "\r\nThe rusty key turns in the padlock with a "
			    "satisfying click.\r\n"
			    "The gate swings open with a groan.\r\n"
			    "\r\n"
			    "Welcome, %s. You have proven yourself worthy.\r\n"
			    "\r\n",
			    gs.username);
			set_banner(banner);
			return DSSH_AUTH_SUCCESS;
		}
		set_banner("You can't do that here.\r\n");
		goto prompt;

	case ROOM_GARDEN:
		if (cmd_eq(cmd, "look") || cmd_eq(cmd, "l")
		    || cmd_eq(cmd, "examine")) {
			if (gs.riddle_solved && !gs.has_key)
				set_banner(
				    "\r\n"
				    "The overgrown garden. The fountain's "
				    "symbols glow faintly.\r\n"
				    "A rusty key glints beneath a loose "
				    "stone.\r\n\r\n");
			else if (gs.has_key)
				set_banner(
				    "\r\n"
				    "The overgrown garden. The fountain is "
				    "quiet now.\r\n\r\n");
			else
				set_banner(garden_desc);
			gs.looked = true;
			goto prompt;
		}
		if (looking_at(cmd, "fountain")) {
			set_banner(
			    "\r\nThe fountain is carved from a single block "
			    "of grey stone.\r\n"
			    "It hasn't held water in decades, but the "
			    "basin is filled with\r\n"
			    "dead leaves. Strange symbols are carved "
			    "around the rim.\r\n\r\n");
			goto prompt;
		}
		if (looking_at(cmd, "rim") || looking_at(cmd, "symbols")
		    || looking_at(cmd, "carvings")) {
			set_banner(
			    "\r\nThe symbols look like an ancient script. "
			    "They seem to form words,\r\n"
			    "but you can't quite make them out from "
			    "a glance. Try 'read'.\r\n\r\n");
			goto prompt;
		}
		if (looking_at(cmd, "stones") || looking_at(cmd, "stepping stones")) {
			set_banner(
			    "\r\nMoss-covered stepping stones form a path "
			    "south toward the gate.\r\n"
			    "They're slippery but solid.\r\n\r\n");
			goto prompt;
		}
		if (looking_at(cmd, "stone") || looking_at(cmd, "loose stone")) {
			if (gs.riddle_solved && !gs.has_key)
				set_banner(
				    "\r\nA loose stone at the fountain's base "
				    "has shifted, revealing\r\n"
				    "a rusty iron key underneath.\r\n\r\n");
			else if (gs.riddle_solved)
				set_banner(
				    "\r\nThe loose stone. The key is gone -- "
				    "you have it.\r\n\r\n");
			else
				set_banner(
				    "\r\nThe stones around the fountain's base "
				    "are tightly fitted.\r\n"
				    "Nothing seems loose.\r\n\r\n");
			goto prompt;
		}
		if (looking_at(cmd, "key")) {
			if (gs.riddle_solved && !gs.has_key)
				set_banner(
				    "\r\nA rusty iron key, half-buried under "
				    "the loose stone.\r\n"
				    "It looks like it would fit an old "
				    "padlock.\r\n\r\n");
			else if (gs.has_key)
				set_banner("You already picked it up.\r\n");
			else
				set_banner("You don't see a key here.\r\n");
			goto prompt;
		}
		if (looking_at(cmd, "leaves") || looking_at(cmd, "basin")) {
			set_banner(
			    "\r\nDead leaves fill the dry basin. Nothing "
			    "useful, just the slow\r\n"
			    "decay of a forgotten place.\r\n\r\n");
			goto prompt;
		}
		if (looking_at(cmd, "vines") || looking_at(cmd, "flowers")
		    || looking_at(cmd, "wildflowers")) {
			set_banner(
			    "\r\nWild morning glories twist through the "
			    "remains of a trellis.\r\n"
			    "They're beautiful in a melancholy way.\r\n\r\n");
			goto prompt;
		}
		if (dir_match(cmd, "south", "s")) {
			gs.room = ROOM_GATE;
			gs.looked = false;
			set_banner(gate_desc);
			gs.looked = true;
			goto prompt;
		}
		if (dir_match(cmd, "north", "n")
		    || dir_match(cmd, "east", "e")
		    || dir_match(cmd, "west", "w")
		    || dir_match(cmd, "up", "u")
		    || dir_match(cmd, "down", "d")) {
			set_banner("There is no path in that direction.\r\n");
			goto prompt;
		}
		if (cmd_eq(cmd, "read fountain") || cmd_eq(cmd, "read")
		    || cmd_eq(cmd, "read rim") || cmd_eq(cmd, "read symbols")) {
			set_banner(
			    "\r\nCarved into the fountain's rim:\r\n\r\n"
			    "  \"I have cities, but no houses.\r\n"
			    "   I have mountains, but no trees.\r\n"
			    "   I have water, but no fish.\r\n"
			    "   What am I?\"\r\n\r\n"
			    "Below the riddle, a small depression "
			    "awaits an answer.\r\n");
			goto prompt;
		}
		if (cmd_starts(cmd, "answer")) {
			const char *ans = cmd_arg(cmd, "answer");
			if (strcasecmp(ans, "a map") == 0
			    || strcasecmp(ans, "map") == 0) {
				gs.riddle_solved = true;
				set_banner(
				    "\r\nThe symbols on the fountain glow "
				    "bright blue!\r\n"
				    "A stone at the fountain's base shifts, "
				    "revealing a rusty key.\r\n");
				goto prompt;
			}
			set_banner(
			    "The fountain remains silent. "
			    "That is not the answer.\r\n");
			goto prompt;
		}
		if (cmd_eq(cmd, "take key") || cmd_eq(cmd, "get key")) {
			if (!gs.riddle_solved) {
				set_banner("You don't see a key here.\r\n");
				goto prompt;
			}
			if (gs.has_key) {
				set_banner("You already have the key.\r\n");
				goto prompt;
			}
			gs.has_key = true;
			set_banner(
			    "You pick up the rusty iron key. "
			    "It's heavy and cold.\r\n");
			goto prompt;
		}
		set_banner("You can't do that here.\r\n");
		goto prompt;

	case ROOM_TOWER:
		if (cmd_eq(cmd, "look") || cmd_eq(cmd, "l")) {
			set_banner(tower_desc);
			gs.looked = true;
			goto prompt;
		}
		if (looking_at(cmd, "tower") || looking_at(cmd, "walls")) {
			set_banner(
			    "\r\nThe tower is built from rough-hewn stone, "
			    "crumbling at the edges.\r\n"
			    "It must have been a watchtower once. Ivy "
			    "claims the lower walls.\r\n\r\n");
			goto prompt;
		}
		if (looking_at(cmd, "steps") || looking_at(cmd, "stairs")) {
			set_banner(
			    "\r\nNarrow stone steps spiral upward into "
			    "shadow, worn smooth\r\n"
			    "by centuries of footsteps. They look "
			    "sturdy enough.\r\n\r\n");
			goto prompt;
		}
		if (looking_at(cmd, "ivy")) {
			set_banner(
			    "\r\nThick ivy clings to the stonework. "
			    "Tiny spiders scurry away\r\n"
			    "from your gaze.\r\n\r\n");
			goto prompt;
		}
		if (dir_match(cmd, "east", "e")) {
			gs.room = ROOM_GATE;
			gs.looked = false;
			set_banner(gate_desc);
			gs.looked = true;
			goto prompt;
		}
		if (dir_match(cmd, "up", "u")) {
			gs.room = ROOM_PARAPET;
			gs.looked = false;
			set_banner(parapet_desc);
			gs.looked = true;
			goto prompt;
		}
		if (dir_match(cmd, "north", "n")
		    || dir_match(cmd, "south", "s")
		    || dir_match(cmd, "west", "w")
		    || dir_match(cmd, "down", "d")) {
			set_banner("There is no path in that direction.\r\n");
			goto prompt;
		}
		set_banner("You can't do that here.\r\n");
		goto prompt;

	case ROOM_PARAPET:
		if (cmd_eq(cmd, "look") || cmd_eq(cmd, "l")) {
			set_banner(parapet_desc);
			gs.looked = true;
			goto prompt;
		}
		if (looking_at(cmd, "raven") || looking_at(cmd, "bird")) {
			if (gs.talked_raven)
				set_banner(
				    "\r\nThe raven watches you with one "
				    "beady eye. It's already said\r\n"
				    "its piece and seems smugly satisfied "
				    "about it.\r\n\r\n");
			else
				set_banner(
				    "\r\nA large raven, jet black with an "
				    "iridescent sheen. It cocks\r\n"
				    "its head as if waiting for you to "
				    "speak. Try 'talk'.\r\n\r\n");
			goto prompt;
		}
		if (looking_at(cmd, "battlement") || looking_at(cmd, "wall")
		    || looking_at(cmd, "parapet")) {
			set_banner(
			    "\r\nThe battlement is crumbling badly. "
			    "Several merlons have fallen\r\n"
			    "away entirely, leaving gaps through which "
			    "the wind howls.\r\n\r\n");
			goto prompt;
		}
		if (looking_at(cmd, "view") || looking_at(cmd, "horizon")
		    || looking_at(cmd, "landscape")) {
			set_banner(
			    "\r\nFrom up here you can see the garden to "
			    "the north-east, the locked\r\n"
			    "gate below to the east, and beyond it... "
			    "a blinking cursor.\r\n"
			    "An endless expanse of terminal awaits.\r\n\r\n");
			goto prompt;
		}
		if (looking_at(cmd, "stairs") || looking_at(cmd, "steps")) {
			set_banner(
			    "\r\nThe spiral stairs disappear into shadow "
			    "below.\r\n\r\n");
			goto prompt;
		}
		if (dir_match(cmd, "down", "d")) {
			gs.room = ROOM_TOWER;
			gs.looked = false;
			set_banner(tower_desc);
			gs.looked = true;
			goto prompt;
		}
		if (dir_match(cmd, "north", "n")
		    || dir_match(cmd, "south", "s")
		    || dir_match(cmd, "east", "e")
		    || dir_match(cmd, "west", "w")
		    || dir_match(cmd, "up", "u")) {
			set_banner("There is no path in that direction.\r\n");
			goto prompt;
		}
		if (cmd_eq(cmd, "talk raven") || cmd_eq(cmd, "talk")
		    || cmd_eq(cmd, "ask raven")) {
			gs.talked_raven = true;
			set_banner(
			    "\r\nThe raven cocks its head and croaks:\r\n\r\n"
			    "  \"Cartographers know the answer, traveler.\r\n"
			    "   But you won't find it in any atlas.\"\r\n\r\n"
			    "The raven ruffles its feathers smugly.\r\n");
			goto prompt;
		}
		set_banner("You can't do that here.\r\n");
		goto prompt;
	}

prompt:
	/* Set up the prompt for this room */
	*name_out = strdup("The Gate of Authentication");
	*instruction_out = strdup("");
	*num_prompts_out = 1;
	*prompts_out = malloc(sizeof(char *));
	*echo_out = malloc(sizeof(bool));
	if (*prompts_out == NULL || *echo_out == NULL)
		return DSSH_AUTH_FAILURE;

	const char *prompt;
	switch (gs.room) {
	case ROOM_GATE:    prompt = gate_prompt;    break;
	case ROOM_GARDEN:  prompt = garden_prompt;  break;
	case ROOM_TOWER:   prompt = tower_prompt;   break;
	case ROOM_PARAPET: prompt = parapet_prompt; break;
	default:           prompt = "> ";           break;
	}
	(*prompts_out)[0] = strdup(prompt);
	(*echo_out)[0] = true;
	return DSSH_AUTH_KBI_PROMPT;
}

static struct dssh_auth_server_cbs auth_cbs = {
	.methods_str = "publickey,password,keyboard-interactive",
	.none_cb = auth_none,
	.password_cb = auth_password,
	.passwd_change_cb = auth_passwd_change,
	.publickey_cb = auth_publickey,
	.keyboard_interactive_cb = auth_kbi,
};

/* ================================================================
 * Session channel request callback
 * ================================================================ */
static int
channel_request_cb(const char *type, size_t type_len,
    bool want_reply, const uint8_t *data, size_t data_len,
    void *cbdata)
{
	fprintf(stderr, "  REQ: %.*s (want_reply=%d, data_len=%zu)\n",
	    (int)type_len, type, want_reply, data_len);
	if ((type_len == 7) && (memcmp(type, "pty-req", 7) == 0)) {
		struct dssh_pty_req pty;

		if (dssh_parse_pty_req_data(data, data_len, &pty) == 0) {
			fprintf(stderr, "  PTY: %.*s %ux%u\n",
			    (int)strlen(pty.term), pty.term,
			    pty.cols, pty.rows);
		}
		return 0;
	}
	if ((type_len == 3) && (memcmp(type, "env", 3) == 0)) {
		const uint8_t *name, *value;
		size_t         nlen, vlen;

		if (dssh_parse_env_data(data, data_len,
		    &name, &nlen, &value, &vlen) == 0)
			fprintf(stderr, "  ENV: %.*s=%.*s\n",
			    (int)nlen, name, (int)vlen, value);
		return 0;
	}
	if ((type_len == 5) && (memcmp(type, "shell", 5) == 0))
		return 0;
	if ((type_len == 4) && (memcmp(type, "exec", 4) == 0))
		return 0;
	if ((type_len == 9) && (memcmp(type, "subsystem", 9) == 0))
		return 0;

        /* Reject unknown requests */
	return -1;
}

static struct dssh_server_session_cbs session_cbs = {
	.request_cb = channel_request_cb,
};

/* ================================================================
 * Session timeout handler
 * ================================================================ */
static void
timeout_handler(int sig)
{
	(void)sig;
	if (sess != NULL)
		dssh_session_terminate(sess);
}

/* ================================================================
 * Debug and global request callbacks
 * ================================================================ */
static void
debug_cb(bool always_display, const uint8_t *message, size_t message_len,
    void *cbdata)
{
	fprintf(stderr, "  DEBUG(%s): %.*s\n",
	    always_display ? "display" : "nodisplay",
	    (int)message_len, message);
}

static void
unimplemented_cb(uint32_t rejected_seq, void *cbdata)
{
	fprintf(stderr, "  UNIMPLEMENTED: seq %u\n", rejected_seq);
}

static int
global_request_cb(const uint8_t *name, size_t name_len,
    bool want_reply, const uint8_t *data, size_t data_len,
    void *cbdata)
{
	fprintf(stderr, "  GLOBAL_REQ: %.*s (want_reply=%d, data_len=%zu)\n",
	    (int)name_len, name, want_reply, data_len);
	return -1; /* reject all */
}

/* ================================================================
 * Handle one connection (runs in forked child)
 * ================================================================ */
static int
handle_connection(void)
{
	/* 60-second session timeout */
	signal(SIGALRM, timeout_handler);
	alarm(60);

        /* Initialize session */
	sess = dssh_session_init(false, 0);
	if (sess == NULL) {
		fprintf(stderr, "session_init failed\n");
		return 1;
	}
	dssh_session_set_terminate_cb(sess, on_terminate, NULL);
	dssh_session_set_debug_cb(sess, debug_cb, NULL);
	dssh_session_set_unimplemented_cb(sess, unimplemented_cb, NULL);
	dssh_session_set_global_request_cb(sess, global_request_cb, NULL);
        /* Handshake */
	fprintf(stderr, "Handshake...\n");
	int res = dssh_transport_handshake(sess);
	if (res < 0) {
		fprintf(stderr, "handshake failed: %d\n", res);
		return 1;
	}
	fprintf(stderr, "Remote: %s\n", dssh_transport_get_remote_version(sess));
	fprintf(stderr, "KEX: %s, Host key: %s, Enc: %s, MAC: %s\n",
	    dssh_transport_get_kex_name(sess),
	    dssh_transport_get_hostkey_name(sess),
	    dssh_transport_get_enc_name(sess),
	    dssh_transport_get_mac_name(sess));

        /* Auth */
	fprintf(stderr, "Handling auth...\n");
	set_banner("Welcome to the DeuceSSH test server, have fun!\r\n");

	uint8_t auth_user[256];
	size_t  auth_user_len = sizeof(auth_user);

	res = dssh_auth_server(sess, &auth_cbs, auth_user, &auth_user_len);
	if (res < 0) {
		fprintf(stderr, "auth failed: %d\n", res);
		return 1;
	}
	fprintf(stderr, "Client authenticated.\n");

        /* Start demux thread */
	res = dssh_session_start(sess);
	if (res < 0) {
		fprintf(stderr, "session_start failed: %d\n", res);
		return 1;
	}

        /* Accept incoming channel */
	struct dssh_incoming_open *inc;

	res = dssh_session_accept(sess, &inc, -1);
	if (res < 0) {
		fprintf(stderr, "accept failed: %d\n", res);
		return 1;
	}

        /* Accept as session channel */
	const char *req_type, *req_data;

	fprintf(stderr, "Handling channel...\n");

	dssh_channel ch = dssh_session_accept_channel(sess, inc,
	        &session_cbs, &req_type, &req_data);

	if (ch == NULL) {
		fprintf(stderr, "session_accept_channel failed\n");
		return 1;
	}
	fprintf(stderr, "  Request: %s %s\n", req_type, req_data);

	if (strcmp(req_type, "exec") == 0) {
                /* Execute command -- respond and close */
		if (strcmp(req_data, "ping") == 0) {
			dssh_session_write(sess, ch,
			    (const uint8_t *)"pong\n", 5);
			dssh_session_write_ext(sess, ch,
			    (const uint8_t *)"gnop\n", 5);
		}
		else {
			dssh_session_write(sess, ch,
			    (const uint8_t *)"unknown command\n", 16);
		}
	}
	else {
                /* Shell mode -- simple line-based command parser */
		uint8_t buf[4096];
		size_t  line_len = 0;

		dssh_session_write(sess, ch,
		    (const uint8_t *)"> ", 2);

		for (;;) {
			int ev = dssh_session_poll(sess, ch,
			        DSSH_POLL_READ, -1);

			if (ev <= 0)
				break;

			ssize_t n = dssh_session_read(sess, ch,
			        &buf[line_len],
			        sizeof(buf) - line_len - 1);

			if (n <= 0)
				break;

			/* Echo input back, translating \r to \r\n */
			for (ssize_t ei = 0; ei < n; ei++) {
				if (buf[line_len + ei] == '\r')
					dssh_session_write(sess, ch,
					    (const uint8_t *)"\r\n", 2);
				else
					dssh_session_write(sess, ch,
					    &buf[line_len + ei], 1);
			}

			line_len += n;

			/* Look for newline */
			if (line_len == 0
			    || (buf[line_len - 1] != '\n'
			        && buf[line_len - 1] != '\r'))
				continue;

			/* Strip trailing CR/LF */
			while (line_len > 0
			    && (buf[line_len - 1] == '\n'
			        || buf[line_len - 1] == '\r'))
				line_len--;
			buf[line_len] = '\0';

			if (strcmp((char *)buf, "ping") == 0) {
				dssh_session_write(sess, ch,
				    (const uint8_t *)"pong\r\n", 6);
			}
			else if (strcmp((char *)buf, "quit") == 0
			    || strcmp((char *)buf, "exit") == 0) {
				dssh_session_write(sess, ch,
				    (const uint8_t *)"bye\r\n", 5);
				break;
			}
			else if (strcmp((char *)buf, "diediedie") == 0) {
				dssh_session_write(sess, ch,
				    (const uint8_t *)"dying...\r\n", 10);
				kill(getppid(), SIGTERM);
				break;
			}
			else if (line_len > 0) {
				static const char help[] =
				    "commands: ping, quit, exit\r\n";
				dssh_session_write(sess, ch,
				    (const uint8_t *)help, sizeof(help) - 1);
			}

			line_len = 0;
			dssh_session_write(sess, ch,
			    (const uint8_t *)"> ", 2);
		}
	}

        /* Clean shutdown */
	alarm(0);
	fprintf(stderr, "Channel closed.\n");
	dssh_session_close(sess, ch, 0);
	dssh_session_cleanup(sess);
	close(conn_fd);
	return 0;
}

/* ================================================================
 * Main
 * ================================================================ */
static void
sigchld_handler(int sig)
{
	(void)sig;
	while (waitpid(-1, NULL, WNOHANG) > 0)
		;
}

int
main(int argc, char **argv)
{
	int port = 2222;

	if (argc > 1)
		port = atoi(argv[1]);

	/* Set version */
	dssh_transport_set_version("Mozilla/5.0", "(Windows NT 12.0; Win32; x32) AppleWebKit/540.70 (KHTML, like Gecko) Chrome/153.0.6626.357 Safari/540.70");

        /* Register algorithms */
	if (dssh_transport_set_callbacks(tx, rx, rxline, NULL) != 0)
		return 1;
	if (dssh_register_mlkem768x25519_sha256() != 0)
		return 1;
	if (dssh_register_sntrup761x25519_sha512() != 0)
		return 1;
	if (dssh_register_curve25519_sha256() != 0)
		return 1;
	if (dssh_register_dh_gex_sha256() != 0)
		return 1;
	if (dssh_register_ssh_ed25519() != 0)
		return 1;
	if (dssh_register_rsa_sha2_256() != 0)
		return 1;
	if (dssh_register_aes256_ctr() != 0)
		return 1;
	if (dssh_register_hmac_sha2_256() != 0)
		return 1;
	if (dssh_register_none_comp() != 0)
		return 1;

        /* Generate host keys once (shared across all forks) */
	int res = dssh_ed25519_generate_key();

	if (res < 0) {
		fprintf(stderr, "ed25519 key gen failed: %d\n", res);
		return 1;
	}
	res = dssh_rsa_sha2_256_generate_key(4096);
	if (res < 0) {
		fprintf(stderr, "rsa key gen failed: %d\n", res);
		return 1;
	}

        /* Listen */
	int listen_fd = socket(PF_INET, SOCK_STREAM, 0);

	if (listen_fd == -1) {
		perror("socket");
		return 1;
	}

	int opt = 1;

	setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	struct sockaddr_in sa = {
		.sin_family = AF_INET,
		.sin_port = htons(port),
		.sin_addr = {htonl(INADDR_ANY)},
	};

	if (bind(listen_fd, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
		perror("bind");
		return 1;
	}
	if (listen(listen_fd, 5) == -1) {
		perror("listen");
		return 1;
	}

	signal(SIGCHLD, sigchld_handler);
	fprintf(stderr, "Listening on port %d...\n", port);

        /* Accept loop */
	for (;;) {
		conn_fd = accept(listen_fd, NULL, NULL);
		if (conn_fd == -1) {
			if (errno == EINTR)
				continue;
			perror("accept");
			break;
		}
		fprintf(stderr, "Connection accepted.\n");

		pid_t pid = fork();

		if (pid < 0) {
			perror("fork");
			close(conn_fd);
			continue;
		}
		if (pid == 0) {
			/* Child -- handle connection */
			close(listen_fd);
			_exit(handle_connection());
		}
		/* Parent -- close child's fd, loop */
		close(conn_fd);
	}

	close(listen_fd);
	return 0;
}
