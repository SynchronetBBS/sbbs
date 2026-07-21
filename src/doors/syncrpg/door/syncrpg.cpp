/* SyncRPG -- door entry point.
 *
 * Boots the vendored EasyRPG Player headless through our BaseUi_termgfx
 * backend (ui_term.cpp), the door analogue of syncscumm's OSystem_Synchronet.
 * termgfx_termio_init() parses DOOR32.SYS / -s<fd> and stands up the terminal
 * session; we then hand EasyRPG a clean argv (never the raw door argv -- the
 * engine's own parser would reject -s<fd>/a DOOR32 path) and run it.
 *
 * Task 3 got the engine loading RPG_RT.ldb/.lmt and reaching the title
 * without crashing, with our UI installed; Task 4 wired video, Task 5 wired
 * keyboard input, and Task 6 (M3) wires audio -- EasyRPG now runs with its
 * real audio backend (no --no-audio), routed to termgfx_stream by
 * ui_term.cpp / audio_term.h.
 *
 * The game to run is taken from the SYNCRPG_GAME environment variable, or the
 * first door-argv token termgfx_termio_init() did not consume -- a test seam
 * until getdata.js (Task 7) provides the real per-package project path.
 *
 * Task 8 (M4) adds per-user saves: a door-level `--save-path=<dir>` argument
 * (the install-xtrn cmd passes `%juser/%4/yumenikki`, mirroring
 * synclure/syncscumm's `--savepath=%juser/%4/lure`) is translated to
 * EasyRPG's OWN `--save-path PATH` option (player.cpp; verified against
 * `easyrpg/src/player.cpp`'s `--help` text: "Instead of storing save files in
 * the game directory, store them in PATH"). Like ScummVM's --savepath,
 * EasyRPG's FileFinder::Root().Create() only resolves an EXISTING directory
 * -- data/user/<####>/yumenikki/ does not exist yet on a user's first launch,
 * so we mkpath() it here before Player::Init() ever looks at it (same
 * reasoning as syncscumm.cpp's savepath mkpath(), see that file's comment).
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "player.h"

extern "C" {
#include "termgfx_termio.h"
#include "dirwrap.h"
}

int main(int argc, char **argv)
{
	termgfx_termio_init(argc, argv);
	atexit(termgfx_termio_shutdown);

	/* Resolve the game/project path (env first, else the first non-option
	 * argv token the door layer did not claim) and the optional per-user
	 * --save-path=<dir>, from whatever argv tokens termgfx_termio_init() left
	 * for us. */
	const char *game = getenv("SYNCRPG_GAME");
	std::string game_path;
	std::string save_path;
	static const char save_path_opt[] = "--save-path=";

	if (game != NULL && *game != '\0')
		game_path = game;
	for (int i = 1; i < argc; i++) {
		if (termgfx_termio_consumed(i))
			continue;
		if (strncmp(argv[i], save_path_opt, sizeof(save_path_opt) - 1) == 0) {
			save_path = argv[i] + sizeof(save_path_opt) - 1;
			continue;
		}
		if (game_path.empty())
			game_path = argv[i];
	}
	if (game_path.empty()) {
		fputs("syncrpg: no game path (set SYNCRPG_GAME or pass a project dir)\n",
			stderr);
		return 1;
	}

	/* Fail closed on a broken/partial install: only proceed if game_path is a
	 * real RM2k/2k3 project (has RPG_RT.ldb). If it doesn't, EasyRPG's
	 * Player::Init() falls through to Scene_GameBrowser -- its command window
	 * exposes Settings/About, engine UI a locked-down door must never surface
	 * -- so we must catch this BEFORE any EasyRPG call, not rely on the engine
	 * to do the right thing. */
	std::string ldb_path = game_path;
	if (!ldb_path.empty() && !IS_PATH_DELIM(ldb_path[ldb_path.size() - 1]))
		ldb_path += PATH_DELIM;
	ldb_path += "RPG_RT.ldb";
	if (!fexist(ldb_path.c_str())) {
		fprintf(stderr,
			"syncrpg: %s is not a valid RPG Maker project (missing %s)\n",
			game_path.c_str(), ldb_path.c_str());
		return 1;
	}

	/* Build EasyRPG's own argv from scratch. args[0] is the program name (the
	 * engine's command-line parser skips it); the rest are real EasyRPG
	 * options only. No --no-audio: Task 6 (M3) wires GetAudio() to a real
	 * termgfx-backed audio interface (see ui_term.cpp / audio_term.h), so the
	 * engine is left free to use it. */
	std::vector<std::string> args;
	args.push_back("syncrpg");
	args.push_back("--project-path");
	args.push_back(game_path);
	if (!save_path.empty()) {
		mkpath(save_path.c_str());
		args.push_back("--save-path");
		args.push_back(save_path);
	}

	Player::Init(std::move(args));
	Player::Run();

	termgfx_termio_shutdown();
	return 0;
}
