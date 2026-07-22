// lobby.js -- xtrn/syncarcade: SyncRetro's arcade install (MAME 2003-Plus).
//
// This file IS the console definition. Everything else -- discovery, the picker,
// the activity board, the door command line -- lives in exec/load/syncretro_lobby.js
// and is shared with every other SyncRetro console, because they all run the one
// door binary against a different libretro core. Adding a console is adding one of
// these files, not forking a lobby.
//
// An arcade "console" is a cabinet, and that changes two things a cartridge
// console does not care about: the ROM is a .zip whose NAME is significant (see
// `ext`), and the high-score table belongs to the MACHINE rather than to each
// player (see `shared_saves`).
//
// See src/doors/syncretro/M3_MULTICORE.md.
//
// SpiderMonkey 1.8.5: no let/const/arrows/template literals.
//
// Copyright(C) 2026 Rob Swindell / SyncRetro. GPL-2.0.

load("syncretro_lobby.js");

syncretro_lobby({
	dir:      js.exec_dir,               /* this door dir: core, roms/ */
	name:     "Arcade",
	short:    "Arcade",                  /* -> id "arcade": the save dir and ROM cache */
	core:     "mame2003_plus_libretro",  /* no extension: .so / .dll / .dylib */

	/* An arcade control panel is a stick, a coin slot and a row of buttons, which
	 * IS a plain RetroPad -- so the `arcade` profile BINDS exactly what `pad`
	 * binds, and exists for the words rather than the wiring. A player told that
	 * Backspace is "Select" does not know it is the coin slot, and on a cabinet
	 * nothing happens at all until a coin goes in. It also drops the Tab
	 * controller-swap, which here only moves the player onto player two's
	 * controls and leaves him with a dead machine.
	 *
	 * Insert a coin, then press start. */
	profile:  "arcade",

	/* A romset is a .zip, and -- unlike every other console -- its FILENAME IS
	 * DATA. MAME identifies the game by the zip's basename ("puckman.zip" ->
	 * the puckman driver), so a romset must NOT be renamed to something more
	 * readable. That is why the picker gets its display names from names.json
	 * rather than from the filename the way a cartridge console does.
	 *
	 * MAME also opens the .zip itself (block_extract = true), so nothing here or
	 * in the lobby should try to look inside one. */
	ext:      ["zip"],

	/* Measured across a 272-romset collection: 1.8 KB (a tiny clone that shares
	 * its parent's data) to 15 MB, median 50 KB. The band is deliberately loose
	 * at both ends -- unlike a cartridge console, where the size range IS a
	 * meaningful filter, a romset's size says nothing about whether it is a
	 * game. The core is the only real judge, and it rejects cleanly. */
	min_size: 1024,
	max_size: 64 * 1024 * 1024,

	/* NO BIOS. A few romsets need a parent set present (mspacman.zip resolves
	 * puckman.zip in a split set), but that is another romset in roms/, not a
	 * BIOS image the sysop has to find -- so there is nothing for the picker to
	 * reject. */
	bios:     [],

	/* THE HIGH-SCORE TABLE IS THE MACHINE'S, NOT THE PLAYER'S.
	 *
	 * Every player shares one save directory, so the score you set is the score
	 * the next caller has to beat -- which is the whole point of an arcade
	 * cabinet, and the reason this console does not want the per-user sandbox a
	 * cartridge console does. MAME 2003-Plus writes its hiscore files (and its
	 * input cfg) under the save directory the frontend hands it, so this is all
	 * it takes.
	 *
	 * KNOWN LIMITATION, worth stating out loud: two nodes playing the SAME game
	 * at the same time each hold that game's scores in memory and write them at
	 * exit, so the second one out wins and the first one's scores are lost. It
	 * is a lost update, not a corrupt file, and it needs a busy BBS and one
	 * popular game to happen at all. Fixing it properly means arbitrating the
	 * score file across nodes, which is a feature, not a bug-fix. */
	shared_saves: true

	/* CORE OPTIONS ARE NOT PINNED HERE -- they are in syncretro.ini's [options]
	 * section, and that is forced rather than chosen. The lobby builds the door's
	 * command line into a BBS-owned buffer that truncates SILENTLY at 260
	 * characters (xtrn.cpp's `fullcmdline[MAX_PATH + 1]`), and one arcade game
	 * already spends ~240 of them. Two pinned options took it to 334 and cut the
	 * ROM argument clean off the end -- the door reported "(no ROM)" for a file
	 * whose full path the BBS had just logged, because the BBS logs the whole
	 * string and passes the truncated one.
	 *
	 * MAME opens on a copyright/disclaimer screen, so this console does need
	 * skip_disclaimer and skip_warnings; see syncretro.example.ini. */
});
