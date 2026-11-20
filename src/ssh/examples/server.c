/*
 * Example SSH server using the high-level DeuceSSH API.
 * Forks per connection, 60-second session timeout.
 * Usage: server [port]
 */

#include <sys/socket.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "deucessh-algorithms.h"
#include "deucessh-auth.h"
#include "deucessh-conn.h"
#include "deucessh.h"
#include "server-io.h"
#include "server-output.h"
#include "server-shell.h"
#include "server-port.h"

static int          conn_fd = -1;
static dssh_session sess;
static struct example_socket_io connection_io = { .descriptor = -1 };
static atomic_bool diagnostic_failed = ATOMIC_VAR_INIT(false);

static bool
log_failure(void)
{
	atomic_store(&diagnostic_failed, true);
	if (sess != NULL)
		dssh_session_terminate(sess);
	return false;
}

static bool log_message(const char *, ...) DSSH_EXAMPLE_PRINTF_LIKE(1, 2);

static bool
log_message(const char *format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	int written = vfprintf(stderr, format, arguments);
	va_end(arguments);
	if (written < 0 || fflush(stderr) != 0)
		return log_failure();
	return true;
}

static bool
log_bytes(const char *prefix, const uint8_t *bytes, size_t length)
{
	if (fputs(prefix, stderr) == EOF || example_write_text(stderr, bytes, length) < 0
	    || fputc('\n', stderr) == EOF || fflush(stderr) != 0)
		return log_failure();
	return true;
}


/* ================================================================
 * Terminate callback -- unblock I/O callbacks
 * ================================================================ */
static void
on_terminate(dssh_session s, void *cbdata)
{
	(void)s;
	(void)cbdata;
	if (conn_fd >= 0 && shutdown(conn_fd, SHUT_RDWR) != 0 && errno != ENOTCONN)
		atomic_store(&diagnostic_failed, true);
}

/* ================================================================
 * I/O callbacks
 * ================================================================ */
static int
tx(uint8_t *buf, size_t bufsz, dssh_session s, void *cbdata)
{
	(void)cbdata;
	return example_socket_transfer(&connection_io, buf, bufsz, s, true);
}

static int
rx(uint8_t *buf, size_t bufsz, dssh_session s, void *cbdata)
{
	(void)cbdata;
	return example_socket_transfer(&connection_io, buf, bufsz, s, false);
}

static int
rxline(uint8_t *buf, size_t bufsz, size_t *bytes_received, dssh_session s, void *cbdata)
{
	(void)cbdata;
	return example_socket_line(&connection_io, buf, bufsz, bytes_received, s);
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

static int
publish_banner(const char *msg)
{
	int ret = dssh_auth_set_banner(sess, msg, NULL);
	if (ret < 0)
		return ret;
	const uint8_t *bytes = (const uint8_t *)msg;
	return log_bytes("  BANNER: ", bytes, strlen(msg)) ? 0 : DSSH_ERROR_INIT;
}

#define BANNER(message) do { \
	if (publish_banner(message) < 0) \
		return DSSH_AUTH_DISCONNECT; \
} while (0)
#define FORMAT(...) do { \
	if (example_format(__VA_ARGS__) < 0) \
		return DSSH_AUTH_DISCONNECT; \
} while (0)

static int
auth_none(const uint8_t *username, size_t username_len, void *cbdata)
{
	(void)cbdata;
	static bool tried;
	char user[64], buf[256];
	if (example_display_text(user, sizeof(user), username, username_len) < 0)
		return DSSH_AUTH_DISCONNECT;
	if (tried) {
		BANNER("You already tried that. It didn't work then either.\r\n");
		return DSSH_AUTH_FAILURE;
	}
	tried = true;
	if (arc4random_uniform(10) != 0) {
		FORMAT(buf, sizeof(buf), "Oh, '%s' wants in without even trying? %s",
		    user, reject_quips[arc4random_uniform(N_QUIPS)]);
		BANNER(buf);
		return DSSH_AUTH_FAILURE;
	}
	FORMAT(buf, sizeof(buf),
	    "Oh, '%s' wants in without even trying? "
	    "1 in 10 chance and you hit it. Buy a lottery ticket!\r\n", user);
	BANNER(buf);
	return DSSH_AUTH_SUCCESS;
}

static void
static_prompt_release(void *owner)
{
	(void)owner;
}

static int
auth_password(const uint8_t *username, size_t username_len, const uint8_t *password, size_t password_len,
    struct dssh_bytes_result *change_prompt, void *cbdata)
{
	(void)password;
	(void)password_len;
	(void)cbdata;
	char user[64], buf[256];
	if (example_display_text(user, sizeof(user), username, username_len) < 0)
		return DSSH_AUTH_DISCONNECT;
	if (arc4random_uniform(4) != 0) {
		FORMAT(buf, sizeof(buf),
		    "'%s'? Pfft. %sBut I'll give you a chance to change it...\r\n",
		    user, reject_quips[arc4random_uniform(N_QUIPS)]);
		BANNER(buf);
		static const char prompt[] = "Your password is terrible. Pick a new one.";
		const uint8_t *bytes = (const uint8_t *)prompt;
		change_prompt->bytes.data = bytes;
		change_prompt->bytes.len = sizeof(prompt) - 1;
		change_prompt->lease.release = static_prompt_release;
		return DSSH_AUTH_CHANGE_PASSWORD;
	}
	FORMAT(buf, sizeof(buf), "Wow, '%s'? You got lucky!\r\n", user);
	BANNER(buf);
	return DSSH_AUTH_SUCCESS;
}

static int
auth_passwd_change(const uint8_t *username, size_t username_len, const uint8_t *old_password,
    size_t old_password_len, const uint8_t *new_password, size_t new_password_len,
    struct dssh_bytes_result *change_prompt, void *cbdata)
{
	(void)username;
	(void)username_len;
	(void)old_password;
	(void)old_password_len;
	(void)new_password;
	(void)new_password_len;
	(void)change_prompt;
	(void)cbdata;
	BANNER("Password changed. Welcome in!\r\n");
	return DSSH_AUTH_SUCCESS;
}

static int
auth_publickey(const uint8_t *username, size_t username_len, const char *algo_name, const uint8_t *pubkey_blob,
    size_t pubkey_blob_len, bool has_signature, void *cbdata)
{
	(void)pubkey_blob;
	(void)pubkey_blob_len;
	(void)cbdata;
	char user[64], buf[256];
	if (!has_signature)
		return DSSH_AUTH_SUCCESS;
	if (example_display_text(user, sizeof(user), username, username_len) < 0 || algo_name == NULL)
		return DSSH_AUTH_DISCONNECT;
	if (arc4random_uniform(4) != 0) {
		FORMAT(buf, sizeof(buf), "Nice key, '%s'. Shame about the luck. %s",
		    user, reject_quips[arc4random_uniform(N_QUIPS)]);
		BANNER(buf);
		return DSSH_AUTH_FAILURE;
	}
	FORMAT(buf, sizeof(buf), "Key auth from '%s' with %s? Fancy. And lucky!\r\n", user, algo_name);
	BANNER(buf);
	return DSSH_AUTH_SUCCESS;
}

/* ================================================================
 * KBI Adventure Game
 * ================================================================ */
enum room {
	ROOM_GATE,
	ROOM_GARDEN,
	ROOM_TOWER,
	ROOM_PARAPET
};

struct game_state {
	enum room room;
	bool      has_key;
	bool      riddle_solved;
	bool      talked_raven;
	bool      looked; /* looked in current room this visit */
	char      username[64];
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

static const char *gate_prompt    = "Gate> ";
static const char *garden_prompt  = "Garden> ";
static const char *tower_prompt   = "Tower> ";
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
		return cmd_eq(cmd_arg(input, "go"), dir) || cmd_eq(cmd_arg(input, "go"), abbrev);
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

/* These examples serve one connection per process. The request hook marks
 * a new attempt; a zero-response continuation does not restart the game. */
static bool kbi_initial_request;

static int
auth_request(const uint8_t *username, size_t username_len, const uint8_t *method,
    size_t method_len, bool new_identity, void *cbdata)
{
	(void)username;
	(void)username_len;
	(void)new_identity;
	(void)cbdata;
	kbi_initial_request = method_len == sizeof("keyboard-interactive") - 1
	    && memcmp(method, "keyboard-interactive", method_len) == 0;
	return 0;
}

static int
auth_kbi(const uint8_t *username, size_t username_len, uint32_t num_responses, const uint8_t **responses,
    const size_t *response_lens, struct dssh_auth_challenge *out, void *cbdata)
{
	static struct game_state gs;
	char                     banner[1024];

	/* Reset the application deadline on each KBI interaction. */
	if (example_socket_reset(&connection_io) != 0)
		return DSSH_AUTH_DISCONNECT;

	/* First call -- initialize game */
	if (kbi_initial_request) {
		kbi_initial_request = false;
		memset(&gs, 0, sizeof(gs));
		gs.room     = ROOM_GATE;
		if (example_display_text(gs.username, sizeof(gs.username), username, username_len) < 0)
			return DSSH_AUTH_DISCONNECT;

		FORMAT(banner, sizeof(banner),
		    "\r\n--- The Gate of Authentication ---\r\n"
		    "\r\n"
		    "Welcome, %s. To prove your worth, you must find\r\n"
		    "the key that unlocks the gate.\r\n"
		    "%s",
		    gs.username, gate_desc);
		BANNER(banner);
		gs.looked = true;
		goto prompt;
	}

	/* Shouldn't happen, but handle gracefully */
	if (num_responses < 1 || responses == NULL)
		return DSSH_AUTH_FAILURE;

	/* Get the command -- copy to NUL-terminated string */
	char   cmd[256];
	size_t clen = response_lens[0] < sizeof(cmd) - 1 ? response_lens[0] : sizeof(cmd) - 1;
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
		BANNER("You walk away from the gate. Maybe next time.\r\n");
		return DSSH_AUTH_FAILURE;
	}
	if (cmd_eq(cmd, "help") || cmd_eq(cmd, "?")) {
		BANNER(help_text);
		goto prompt;
	}
	if (cmd_eq(cmd, "inventory") || cmd_eq(cmd, "inv") || cmd_eq(cmd, "i")) {
		if (gs.has_key)
			BANNER("You are carrying: a rusty iron key\r\n");
		else
			BANNER("You are carrying nothing.\r\n");
		goto prompt;
	}

	/* Room-specific commands */
	switch (gs.room) {
		case ROOM_GATE:
			if (cmd_eq(cmd, "look") || cmd_eq(cmd, "l")) {
				BANNER(gate_desc);
				gs.looked = true;
				goto prompt;
			}
			if (looking_at(cmd, "gate") || looking_at(cmd, "door")) {
				BANNER(
				    "\r\nThe gate is wrought from black iron, "
				    "twisted into thorny spirals.\r\n"
				    "It's cold to the touch and far too heavy "
				    "to force open.\r\n\r\n");
				goto prompt;
			}
			if (looking_at(cmd, "padlock") || looking_at(cmd, "lock")) {
				if (gs.has_key)
					BANNER(
					    "\r\nA heavy padlock. The keyhole looks "
					    "like it might accept the key you're "
					    "carrying.\r\n\r\n");
				else
					BANNER(
					    "\r\nA heavy padlock, green with verdigris. "
					    "The keyhole is shaped\r\n"
					    "like an old-fashioned skeleton key. "
					    "You'll need to find one.\r\n\r\n");
				goto prompt;
			}
			if (looking_at(cmd, "bars")) {
				BANNER(
				    "\r\nThrough the bars you can see a softly "
				    "glowing terminal prompt.\r\n"
				    "It blinks patiently. It has all the time "
				    "in the world.\r\n\r\n");
				goto prompt;
			}
			if (looking_at(cmd, "path") || looking_at(cmd, "paths")) {
				BANNER(
				    "\r\nA cobblestone path branches north toward "
				    "an overgrown garden,\r\n"
				    "and west toward a crumbling stone "
				    "tower.\r\n\r\n");
				goto prompt;
			}
			if (looking_at(cmd, "tower")) {
				BANNER(
				    "\r\nThe tower rises to the west, its upper "
				    "stones crumbling.\r\n"
				    "Something dark moves on the parapet.\r\n\r\n");
				goto prompt;
			}
			if (looking_at(cmd, "garden")) {
				BANNER(
				    "\r\nTo the north, vines and wildflowers spill "
				    "over what was once\r\n"
				    "a formal garden. You can just make out a "
				    "stone fountain.\r\n\r\n");
				goto prompt;
			}
			if (dir_match(cmd, "north", "n")) {
				gs.room   = ROOM_GARDEN;
				gs.looked = false;
				BANNER(garden_desc);
				gs.looked = true;
				goto prompt;
			}
			if (dir_match(cmd, "west", "w")) {
				gs.room   = ROOM_TOWER;
				gs.looked = false;
				BANNER(tower_desc);
				gs.looked = true;
				goto prompt;
			}
			if (dir_match(cmd, "east", "e") || dir_match(cmd, "south", "s")
			    || dir_match(cmd, "up", "u") || dir_match(cmd, "down", "d")) {
				BANNER("There is no path in that direction.\r\n");
				goto prompt;
			}
			if (cmd_eq(cmd, "use key") || cmd_eq(cmd, "unlock gate") || cmd_eq(cmd, "unlock door")
			    || cmd_eq(cmd, "open gate") || cmd_eq(cmd, "open door")) {
				if (!gs.has_key) {
					BANNER(
					    "You rattle the padlock uselessly. "
					    "You need a key.\r\n");
					goto prompt;
				}
				FORMAT(banner, sizeof(banner),
				    "\r\nThe rusty key turns in the padlock with a "
				    "satisfying click.\r\n"
				    "The gate swings open with a groan.\r\n"
				    "\r\n"
				    "Welcome, %s. You have proven yourself worthy.\r\n"
				    "\r\n",
				    gs.username);
				BANNER(banner);
				return DSSH_AUTH_SUCCESS;
			}
			BANNER("You can't do that here.\r\n");
			goto prompt;

		case ROOM_GARDEN:
			if (cmd_eq(cmd, "look") || cmd_eq(cmd, "l") || cmd_eq(cmd, "examine")) {
				if (gs.riddle_solved && !gs.has_key)
					BANNER(
					    "\r\n"
					    "The overgrown garden. The fountain's "
					    "symbols glow faintly.\r\n"
					    "A rusty key glints beneath a loose "
					    "stone.\r\n\r\n");
				else if (gs.has_key)
					BANNER(
					    "\r\n"
					    "The overgrown garden. The fountain is "
					    "quiet now.\r\n\r\n");
				else
					BANNER(garden_desc);
				gs.looked = true;
				goto prompt;
			}
			if (looking_at(cmd, "fountain")) {
				BANNER(
				    "\r\nThe fountain is carved from a single block "
				    "of grey stone.\r\n"
				    "It hasn't held water in decades, but the "
				    "basin is filled with\r\n"
				    "dead leaves. Strange symbols are carved "
				    "around the rim.\r\n\r\n");
				goto prompt;
			}
			if (looking_at(cmd, "rim") || looking_at(cmd, "symbols") || looking_at(cmd, "carvings")) {
				BANNER(
				    "\r\nThe symbols look like an ancient script. "
				    "They seem to form words,\r\n"
				    "but you can't quite make them out from "
				    "a glance. Try 'read'.\r\n\r\n");
				goto prompt;
			}
			if (looking_at(cmd, "stones") || looking_at(cmd, "stepping stones")) {
				BANNER(
				    "\r\nMoss-covered stepping stones form a path "
				    "south toward the gate.\r\n"
				    "They're slippery but solid.\r\n\r\n");
				goto prompt;
			}
			if (looking_at(cmd, "stone") || looking_at(cmd, "loose stone")) {
				if (gs.riddle_solved && !gs.has_key)
					BANNER(
					    "\r\nA loose stone at the fountain's base "
					    "has shifted, revealing\r\n"
					    "a rusty iron key underneath.\r\n\r\n");
				else if (gs.riddle_solved)
					BANNER(
					    "\r\nThe loose stone. The key is gone -- "
					    "you have it.\r\n\r\n");
				else
					BANNER(
					    "\r\nThe stones around the fountain's base "
					    "are tightly fitted.\r\n"
					    "Nothing seems loose.\r\n\r\n");
				goto prompt;
			}
			if (looking_at(cmd, "key")) {
				if (gs.riddle_solved && !gs.has_key)
					BANNER(
					    "\r\nA rusty iron key, half-buried under "
					    "the loose stone.\r\n"
					    "It looks like it would fit an old "
					    "padlock.\r\n\r\n");
				else if (gs.has_key)
					BANNER("You already picked it up.\r\n");
				else
					BANNER("You don't see a key here.\r\n");
				goto prompt;
			}
			if (looking_at(cmd, "leaves") || looking_at(cmd, "basin")) {
				BANNER(
				    "\r\nDead leaves fill the dry basin. Nothing "
				    "useful, just the slow\r\n"
				    "decay of a forgotten place.\r\n\r\n");
				goto prompt;
			}
			if (looking_at(cmd, "vines") || looking_at(cmd, "flowers")
			    || looking_at(cmd, "wildflowers")) {
				BANNER(
				    "\r\nWild morning glories twist through the "
				    "remains of a trellis.\r\n"
				    "They're beautiful in a melancholy way.\r\n\r\n");
				goto prompt;
			}
			if (dir_match(cmd, "south", "s")) {
				gs.room   = ROOM_GATE;
				gs.looked = false;
				BANNER(gate_desc);
				gs.looked = true;
				goto prompt;
			}
			if (dir_match(cmd, "north", "n") || dir_match(cmd, "east", "e")
			    || dir_match(cmd, "west", "w") || dir_match(cmd, "up", "u")
			    || dir_match(cmd, "down", "d")) {
				BANNER("There is no path in that direction.\r\n");
				goto prompt;
			}
			if (cmd_eq(cmd, "read fountain") || cmd_eq(cmd, "read") || cmd_eq(cmd, "read rim")
			    || cmd_eq(cmd, "read symbols")) {
				BANNER(
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
				if (strcasecmp(ans, "a map") == 0 || strcasecmp(ans, "map") == 0) {
					gs.riddle_solved = true;
					BANNER(
					    "\r\nThe symbols on the fountain glow "
					    "bright blue!\r\n"
					    "A stone at the fountain's base shifts, "
					    "revealing a rusty key.\r\n");
					goto prompt;
				}
				BANNER(
				    "The fountain remains silent. "
				    "That is not the answer.\r\n");
				goto prompt;
			}
			if (cmd_eq(cmd, "take key") || cmd_eq(cmd, "get key")) {
				if (!gs.riddle_solved) {
					BANNER("You don't see a key here.\r\n");
					goto prompt;
				}
				if (gs.has_key) {
					BANNER("You already have the key.\r\n");
					goto prompt;
				}
				gs.has_key = true;
				BANNER(
				    "You pick up the rusty iron key. "
				    "It's heavy and cold.\r\n");
				goto prompt;
			}
			BANNER("You can't do that here.\r\n");
			goto prompt;

		case ROOM_TOWER:
			if (cmd_eq(cmd, "look") || cmd_eq(cmd, "l")) {
				BANNER(tower_desc);
				gs.looked = true;
				goto prompt;
			}
			if (looking_at(cmd, "tower") || looking_at(cmd, "walls")) {
				BANNER(
				    "\r\nThe tower is built from rough-hewn stone, "
				    "crumbling at the edges.\r\n"
				    "It must have been a watchtower once. Ivy "
				    "claims the lower walls.\r\n\r\n");
				goto prompt;
			}
			if (looking_at(cmd, "steps") || looking_at(cmd, "stairs")) {
				BANNER(
				    "\r\nNarrow stone steps spiral upward into "
				    "shadow, worn smooth\r\n"
				    "by centuries of footsteps. They look "
				    "sturdy enough.\r\n\r\n");
				goto prompt;
			}
			if (looking_at(cmd, "ivy")) {
				BANNER(
				    "\r\nThick ivy clings to the stonework. "
				    "Tiny spiders scurry away\r\n"
				    "from your gaze.\r\n\r\n");
				goto prompt;
			}
			if (dir_match(cmd, "east", "e")) {
				gs.room   = ROOM_GATE;
				gs.looked = false;
				BANNER(gate_desc);
				gs.looked = true;
				goto prompt;
			}
			if (dir_match(cmd, "up", "u")) {
				gs.room   = ROOM_PARAPET;
				gs.looked = false;
				BANNER(parapet_desc);
				gs.looked = true;
				goto prompt;
			}
			if (dir_match(cmd, "north", "n") || dir_match(cmd, "south", "s")
			    || dir_match(cmd, "west", "w") || dir_match(cmd, "down", "d")) {
				BANNER("There is no path in that direction.\r\n");
				goto prompt;
			}
			BANNER("You can't do that here.\r\n");
			goto prompt;

		case ROOM_PARAPET:
			if (cmd_eq(cmd, "look") || cmd_eq(cmd, "l")) {
				BANNER(parapet_desc);
				gs.looked = true;
				goto prompt;
			}
			if (looking_at(cmd, "raven") || looking_at(cmd, "bird")) {
				if (gs.talked_raven)
					BANNER(
					    "\r\nThe raven watches you with one "
					    "beady eye. It's already said\r\n"
					    "its piece and seems smugly satisfied "
					    "about it.\r\n\r\n");
				else
					BANNER(
					    "\r\nA large raven, jet black with an "
					    "iridescent sheen. It cocks\r\n"
					    "its head as if waiting for you to "
					    "speak. Try 'talk'.\r\n\r\n");
				goto prompt;
			}
			if (looking_at(cmd, "battlement") || looking_at(cmd, "wall")
			    || looking_at(cmd, "parapet")) {
				BANNER(
				    "\r\nThe battlement is crumbling badly. "
				    "Several merlons have fallen\r\n"
				    "away entirely, leaving gaps through which "
				    "the wind howls.\r\n\r\n");
				goto prompt;
			}
			if (looking_at(cmd, "view") || looking_at(cmd, "horizon")
			    || looking_at(cmd, "landscape")) {
				BANNER(
				    "\r\nFrom up here you can see the garden to "
				    "the north-east, the locked\r\n"
				    "gate below to the east, and beyond it... "
				    "a blinking cursor.\r\n"
				    "An endless expanse of terminal awaits.\r\n\r\n");
				goto prompt;
			}
			if (looking_at(cmd, "stairs") || looking_at(cmd, "steps")) {
				BANNER(
				    "\r\nThe spiral stairs disappear into shadow "
				    "below.\r\n\r\n");
				goto prompt;
			}
			if (dir_match(cmd, "down", "d")) {
				gs.room   = ROOM_TOWER;
				gs.looked = false;
				BANNER(tower_desc);
				gs.looked = true;
				goto prompt;
			}
			if (dir_match(cmd, "north", "n") || dir_match(cmd, "south", "s")
			    || dir_match(cmd, "east", "e") || dir_match(cmd, "west", "w")
			    || dir_match(cmd, "up", "u")) {
				BANNER("There is no path in that direction.\r\n");
				goto prompt;
			}
			if (cmd_eq(cmd, "talk raven") || cmd_eq(cmd, "talk") || cmd_eq(cmd, "ask raven")) {
				gs.talked_raven = true;
				BANNER(
				    "\r\nThe raven cocks its head and croaks:\r\n\r\n"
				    "  \"Cartographers know the answer, traveler.\r\n"
				    "   But you won't find it in any atlas.\"\r\n\r\n"
				    "The raven ruffles its feathers smugly.\r\n");
				goto prompt;
			}
			BANNER("You can't do that here.\r\n");
			goto prompt;
	}

prompt:
	{
		static const uint8_t name[] = "The Gate of Authentication";
		static struct dssh_auth_prompt item;

		const char *prompt;
		switch (gs.room) {
			case ROOM_GATE:
				prompt = gate_prompt;
				break;
			case ROOM_GARDEN:
				prompt = garden_prompt;
				break;
			case ROOM_TOWER:
				prompt = tower_prompt;
				break;
			case ROOM_PARAPET:
				prompt = parapet_prompt;
				break;
			default:
				prompt = "> ";
				break;
		}
		const uint8_t *text = (const uint8_t *)prompt;
		item.text.data = text;
		item.text.len = strlen(prompt);
		item.echo = true;
		out->name.data = name;
		out->name.len = sizeof(name) - 1;
		out->prompts = &item;
		out->count = 1;
		out->lease.release = static_prompt_release;
		return DSSH_AUTH_KBI_PROMPT;
	}
}

static struct dssh_auth_server_cbs auth_cbs = {
    .request_cb              = auth_request,
    .methods_str             = "publickey,password,keyboard-interactive",
    .none_cb                 = auth_none,
    .password_cb             = auth_password,
    .passwd_change_cb        = auth_passwd_change,
    .publickey_cb            = auth_publickey,
    .keyboard_interactive_cb = auth_kbi,
};

#undef BANNER
#undef FORMAT

/* ================================================================
 * Channel accept callbacks (new dssh_chan_* API)
 * ================================================================ */
static int
accept_pty_cb(dssh_channel ch, const struct dssh_chan_params *p, void *cbdata)
{
	(void)ch;
	(void)cbdata;
	if (p == NULL)
		return DSSH_ERROR_INVALID;
	const uint8_t *term = (const uint8_t *)p->term;
	size_t length = p->term == NULL ? 0 : strlen(p->term);
	if (!log_bytes("  PTY: ", term, length)
	    || !log_message("  SIZE: %" PRIu32 "x%" PRIu32 "\n", p->cols, p->rows))
		return DSSH_ERROR_INIT;
	return 0;
}

static int
accept_env_cb(dssh_channel ch, const struct dssh_chan_params *p, void *cbdata)
{
	(void)ch;
	(void)cbdata;
	if (p == NULL || (p->env_count > 0 && p->env == NULL))
		return DSSH_ERROR_INVALID;
	if (p->env_count == 0)
		return 0;
	const char *name = p->env[p->env_count - 1].name;
	if (name == NULL)
		return DSSH_ERROR_INVALID;
	const uint8_t *bytes = (const uint8_t *)name;
	return log_bytes("  ENV_NAME: ", bytes, strlen(name)) ? 0 : DSSH_ERROR_INIT;
}

static int
accept_shell_cb(dssh_channel ch, const struct dssh_chan_params *p, struct dssh_chan_accept_result *result,
    void *cbdata)
{
	(void)ch;
	(void)p;
	(void)result;
	(void)cbdata;
	return 0;
}

static int
accept_exec_cb(dssh_channel ch, const struct dssh_chan_params *p, struct dssh_chan_accept_result *result,
    void *cbdata)
{
	(void)ch;
	(void)result;
	(void)cbdata;
	(void)p;
	return log_message("  EXEC request\n") ? 0 : DSSH_ERROR_INIT;
}

static struct dssh_chan_accept_cbs accept_cbs = {
    .pty_req = accept_pty_cb,
    .env     = accept_env_cb,
    .shell   = accept_shell_cb,
    .exec    = accept_exec_cb,
};

/* ================================================================
 * Debug and global request callbacks
 * ================================================================ */
static void
debug_cb(bool always_display, const uint8_t *message, size_t message_len, void *cbdata)
{
	(void)cbdata;
	if (!log_bytes(always_display ? "  DEBUG(display): " : "  DEBUG(nodisplay): ", message, message_len))
		return;
}

static void
unimplemented_cb(uint32_t rejected_seq, void *cbdata)
{
	(void)cbdata;
	if (!log_message("  UNIMPLEMENTED: seq %" PRIu32 "\n", rejected_seq))
		return;
}

static int
global_request_cb(const uint8_t *name, size_t name_len, bool want_reply, const uint8_t *data, size_t data_len,
    void *cbdata)
{
	(void)data;
	(void)cbdata;
	if (!log_message("  GLOBAL_REQ: want_reply=%d, data_len=%zu\n", want_reply, data_len)
	    || !log_bytes("  REQUEST_NAME: ", name, name_len))
		return -1;
	return -1; /* reject all */
}

/* ================================================================
 * Handle one connection (runs in forked child)
 * ================================================================ */
static int
handle_connection(void)
{
	int status = 1;
	dssh_channel ch = NULL;
	atomic_store(&diagnostic_failed, false);
	connection_io = (struct example_socket_io){ .descriptor = conn_fd };
	/* Readiness alone does not make a blocking transfer bounded. */
	int flags = fcntl(conn_fd, F_GETFL);
	if (flags < 0 || fcntl(conn_fd, F_SETFL, flags | O_NONBLOCK) != 0) {
		(void)log_message("socket configuration failed\n");
		goto connection_done;
	}
	/* The deadline is immutable after authentication/connection startup. */
	if (example_socket_reset(&connection_io) != 0) {
		(void)log_message("clock initialization failed\n");
		goto connection_done;
	}

	sess = dssh_session_init(false, 0);
	if (sess == NULL) {
		(void)log_message("session_init failed\n");
		goto connection_done;
	}
	if (dssh_session_set_terminate_cb(sess, on_terminate, NULL) != 0
	    || dssh_session_set_debug_cb(sess, debug_cb, NULL) != 0
	    || dssh_session_set_unimplemented_cb(sess, unimplemented_cb, NULL) != 0
	    || dssh_session_set_global_request_cb(sess, global_request_cb, NULL) != 0) {
		(void)log_message("callback setup failed\n");
		goto connection_done;
	}
	if (!log_message("Handshake...\n"))
		goto connection_done;
	int res = dssh_transport_handshake(sess);
	if (res != 0) {
		(void)log_message("handshake failed: %d\n", res);
		goto connection_done;
	}
	const char *remote = dssh_transport_get_remote_version(sess);
	if (remote == NULL || !log_bytes("Remote: ", (const uint8_t *)remote, strlen(remote)))
		goto connection_done;
	const char *kex = dssh_transport_get_kex_name(sess);
	if (kex == NULL)
		goto connection_done;
	const char *host = dssh_transport_get_hostkey_name(sess);
	if (host == NULL || !log_message("KEX: %s, Host key: %s\n", kex, host))
		goto connection_done;
	const enum dssh_direction directions[] = {DSSH_CLIENT_TO_SERVER, DSSH_SERVER_TO_CLIENT};
	const char *labels[] = {"Client to server", "Server to client"};
	for (size_t i = 0; i < sizeof(directions) / sizeof(directions[0]); i++) {
		const char *enc = dssh_transport_get_enc_name(sess, directions[i]);
		if (enc == NULL)
			goto connection_done;
		const char *mac = dssh_transport_get_mac_name(sess, directions[i]);
		if (mac == NULL)
			goto connection_done;
		const char *comp = dssh_transport_get_comp_name(sess, directions[i]);
		if (comp == NULL || !log_message("%s: Enc: %s, MAC: %s, Compression: %s\n",
		    labels[i], enc, mac, comp))
			goto connection_done;
	}

	if (!log_message("Handling auth...\n")
	    || publish_banner("Welcome to the DeuceSSH test server, have fun!\r\n") < 0)
		goto connection_done;
	/* The application does not use a copy of the authenticated username. */
	res = dssh_auth_server(sess, &auth_cbs, NULL, NULL);
	if (res != DSSH_AUTH_SUCCESS) {
		(void)log_message("auth failed: %d\n", res);
		goto connection_done;
	}
	if (!log_message("Client authenticated.\n"))
		goto connection_done;
	res = dssh_session_start(sess);
	if (res != 0) {
		(void)log_message("session_start failed: %d\n", res);
		goto connection_done;
	}
	if (!log_message("Handling channel...\n"))
		goto connection_done;
	ch = dssh_chan_accept(sess, &accept_cbs, -1);
	if (ch == NULL) {
		(void)log_message("chan_accept failed\n");
		goto connection_done;
	}
	enum dssh_chan_type ctype = dssh_chan_get_type(ch);
	if (ctype != DSSH_CHAN_EXEC && ctype != DSSH_CHAN_SHELL && ctype != DSSH_CHAN_SUBSYSTEM)
		goto connection_done;
	if (!log_message("  Type: %s\n", ctype == DSSH_CHAN_EXEC ? "exec"
	    : ctype == DSSH_CHAN_SHELL ? "shell" : "subsystem"))
		goto connection_done;
	status = 0;
#define WRITE(stream, bytes, length) do { \
	if (example_channel_write(ch, stream, bytes, length) < 0) { \
		status = 1; \
		goto connection_done; \
	} \
} while (0)

	if (ctype == DSSH_CHAN_EXEC) {
		/* Execute command -- respond and close */
		const char *cmd = dssh_chan_get_command(ch);

		if (cmd != NULL && strcmp(cmd, "ping") == 0) {
			WRITE(0, (const uint8_t *)"pong\n", 5);
			WRITE(1, (const uint8_t *)"gnop\n", 5);
		}
		else {
			WRITE(0, (const uint8_t *)"unknown command\n", 16);
		}
	}
	else {
		uint8_t input[4096];
		struct example_shell_line line = EXAMPLE_SHELL_INITIALIZER;
		WRITE(0, (const uint8_t *)"> ", 2);
		for (;;) {
			int events = dssh_chan_poll(ch, DSSH_POLL_READ, -1);
			if (events < 0) {
				status = 1;
				goto connection_done;
			}
			if ((events & DSSH_POLL_READ) == 0)
				continue;
			int64_t received = dssh_chan_read(ch, 0, input, sizeof(input));
			if (received == DSSH_ERROR_NOMORE)
				continue;
			if (received == 0)
				break;
			if (received < 0 || received > 4096) {
				status = 1;
				goto connection_done;
			}
			size_t count = (size_t)received;
			size_t echo_from = 0;
			for (size_t i = 0; i < count; i++) {
				enum example_shell_event event = example_shell_byte(&line, input[i]);
				if (event == EXAMPLE_SHELL_DATA)
					continue;
				/* Emit each contiguous span once, preserving binary input. */
				if (i > echo_from)
					WRITE(0, input + echo_from, i - echo_from);
				echo_from = i + 1;
				if (event == EXAMPLE_SHELL_SKIP_LF)
					continue;
				if (input[i] == '\r')
					WRITE(0, (const uint8_t *)"\r\n", 2);
				else
					WRITE(0, input + i, 1);
				if (event == EXAMPLE_SHELL_PING) {
					WRITE(0, (const uint8_t *)"pong\r\n", 6);
				}
				else if (event == EXAMPLE_SHELL_QUIT) {
					WRITE(0, (const uint8_t *)"bye\r\n", 5);
					goto connection_done;
				}
				else if (event == EXAMPLE_SHELL_STOP_PARENT) {
					WRITE(0, (const uint8_t *)"dying...\r\n", 10);
					if (kill(getppid(), SIGTERM) != 0)
						status = 1;
					goto connection_done;
				}
				else if (event == EXAMPLE_SHELL_UNKNOWN) {
					static const char help[] = "commands: ping, quit, exit\r\n";
					WRITE(0, (const uint8_t *)help, sizeof(help) - 1);
				}
				WRITE(0, (const uint8_t *)"> ", 2);
			}
			if (count > echo_from)
				WRITE(0, input + echo_from, count - echo_from);
		}
	}

#undef WRITE
connection_done:
	/* Keep the socket available until cleanup has joined the reader. */
	if (ch != NULL) {
		if (!log_message("Channel closed.\n"))
			status = 1;
		int closed = dssh_chan_close(ch, status);
		ch = NULL;
		if (closed != 0)
			status = 1;
	}
	if (sess != NULL) {
		dssh_session_cleanup(sess);
		sess = NULL;
	}
	if (conn_fd >= 0) {
		/* A failed close does not authorize retrying a possibly reused fd. */
		if (close(conn_fd) != 0) {
			(void)log_message("socket close failed\n");
			status = 1;
		}
		conn_fd = -1;
	}
	connection_io = (struct example_socket_io){ .descriptor = -1 };
	if (atomic_load(&diagnostic_failed))
		status = 1;
	return status;
}

/* ================================================================
 * Main
 * ================================================================ */
static volatile sig_atomic_t reap_failed;

static void
sigchld_handler(int sig)
{
	(void)sig;
	int saved_errno = errno;
	for (;;) {
		pid_t child = waitpid(-1, NULL, WNOHANG);
		if (child > 0)
			continue;
		if (child < 0 && errno == EINTR)
			continue;
		if (child < 0 && errno != ECHILD)
			reap_failed = 1;
		break;
	}
	errno = saved_errno;
}

int
main(int argc, char **argv)
{
	int status = 1;
	int listen_fd = -1;
	uint16_t port = 2222;
	reap_failed = 0;
	atomic_store(&diagnostic_failed, false);
	if (argc > 2 || (argc == 2 && !example_parse_port(argv[1], &port))) {
		(void)log_message("expected zero or one decimal port argument (0..65535)\n");
		return 1;
	}
	/* Socket write failures are reported through the callbacks. */
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		(void)log_message("SIGPIPE setup failed\n");
		return 1;
	}
	/* Set version */
	if (dssh_transport_set_version("Mozilla/5.0",
	    "(Windows NT 12.0; Win32; x32) AppleWebKit/540.70 (KHTML, like Gecko) Chrome/153.0.6626.357 Safari/540.70") != 0)
		return 1;

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
	if (dssh_register_dh_group16_sha512() != 0)
		return 1;
	if (dssh_register_dh_group14_sha256() != 0)
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

	if (res != 0) {
		(void)log_message("ed25519 key gen failed: %d\n", res);
		return 1;
	}
	res = dssh_rsa_sha2_256_generate_key(4096);
	if (res != 0) {
		(void)log_message("rsa key gen failed: %d\n", res);
		return 1;
	}

	/* Listen */
	listen_fd = socket(PF_INET, SOCK_STREAM, 0);

	if (listen_fd == -1) {
		(void)log_message("socket creation failed\n");
		return 1;
	}

	/* Bound observation of a reaper error even between its flag check and wait. */
	int flags = fcntl(listen_fd, F_GETFL);
	if (flags < 0 || fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK) != 0) {
		(void)log_message("listener configuration failed\n");
		goto listener_done;
	}
	int opt = 1;

	if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != 0) {
		(void)log_message("socket option failed\n");
		goto listener_done;
	}

	struct sockaddr_in sa = {
	    .sin_family = AF_INET,
	    .sin_port   = htons(port),
	    .sin_addr   = {htonl(INADDR_ANY)},
	};

	if (bind(listen_fd, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
		(void)log_message("bind failed\n");
		goto listener_done;
	}
	if (listen(listen_fd, 5) == -1) {
		(void)log_message("listen failed\n");
		goto listener_done;
	}

	/* Interrupt accept so a reaper failure reaches the ordinary owner path. */
	struct sigaction action = { .sa_handler = sigchld_handler };
	if (sigemptyset(&action.sa_mask) != 0 || sigaction(SIGCHLD, &action, NULL) != 0) {
		(void)log_message("SIGCHLD setup failed\n");
		goto listener_done;
	}
	unsigned displayed_port = port;
	if (!log_message("Listening on port %u...\n", displayed_port))
		goto listener_done;

	/* Accept loop: this process owns both descriptors until a successful fork. */
	for (;;) {
		if (reap_failed) {
			(void)log_message("child reaping failed\n");
			break;
		}
		struct pollfd incoming = { .fd = listen_fd, .events = POLLIN };
		int ready = poll(&incoming, 1, 1000);
		if (ready < 0) {
			if (errno == EINTR)
				continue;
			(void)log_message("listener wait failed\n");
			break;
		}
		if (ready == 0)
			continue;
		if (ready != 1 || (incoming.revents & (POLLNVAL | POLLERR | POLLHUP)) != 0) {
			(void)log_message("listener readiness failed\n");
			break;
		}
		if ((incoming.revents & POLLIN) == 0)
			continue;
		conn_fd = accept(listen_fd, NULL, NULL);
		if (conn_fd == -1) {
			if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
				continue;
			(void)log_message("accept failed\n");
			break;
		}
		if (reap_failed || !log_message("Connection accepted.\n"))
			break;
		if (fflush(NULL) != 0) {
			(void)log_message("pre-fork flush failed\n");
			break;
		}
		pid_t pid = fork();
		if (pid == 0) {
			/* The child must retire its listener before handling the connection. */
			if (close(listen_fd) != 0) {
				(void)log_message("child listener close failed\n");
				if (close(conn_fd) != 0)
					(void)log_message("child connection close failed\n");
				conn_fd = -1;
				_exit(1);
			}
			_exit(handle_connection());
		}
		bool fork_failed = pid < 0;
		if (fork_failed && !log_message("fork failed\n"))
			break;
		int closed = close(conn_fd);
		conn_fd = -1;
		if (closed != 0) {
			(void)log_message("parent connection close failed\n");
			break;
		}
		/* A transient fork refusal leaves the listener usable after release. */
	}
listener_done:
	if (conn_fd >= 0) {
		if (close(conn_fd) != 0)
			(void)log_message("connection close failed\n");
		conn_fd = -1;
	}
	if (listen_fd >= 0) {
		if (close(listen_fd) != 0)
			(void)log_message("listener close failed\n");
		listen_fd = -1;
	}
	return status;
}
