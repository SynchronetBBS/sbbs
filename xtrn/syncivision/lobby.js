// lobby.js -- xtrn/syncivision: SyncRetro's Intellivision install.
//
// This file IS the console definition. Everything else -- discovery, the picker,
// the activity board, the door command line -- lives in exec/load/syncretro_lobby.js
// and is shared with every other SyncRetro console, because they all run the one
// door binary against a different libretro core. Adding a console is adding one of
// these files, not forking a lobby.
//
// See src/doors/syncretro/M3_MULTICORE.md and LAUNCHER.md.
//
// SpiderMonkey 1.8.5: no let/const/arrows/template literals.
//
// Copyright(C) 2026 Rob Swindell / SyncRetro. GPL-2.0.

load("syncretro_lobby.js");

syncretro_lobby({
	dir:      js.exec_dir,               /* this door dir: core, BIOS, roms/ */
	name:     "Intellivision",
	/* `short` derives the id ("intv") that names the per-user save dir and the
	 * ROM cache. It must STAY "Intv": change it and every existing player's SRAM
	 * and save states are orphaned under data/user/####/intv. */
	short:    "Intv",
	core:     "freeintv_libretro",       /* no extension: .so / .dll / .dylib */
	profile:  "intv",                    /* keypad on the disc angle; see M2_INPUT.md */

	/* What an Intellivision cartridge looks like. The size band is the whole
	 * cartridge range -- anything outside it is metadata litter, not a game. */
	ext:      ["int", "bin", "rom"],
	min_size: 2 * 1024,
	max_size: 64 * 1024,

	/* The console BIOS, in this door dir (FreeIntv's system dir). Rejected from
	 * the picker by name AND by content, so a re-dropped ROM set cannot smuggle
	 * one back in under a cartridge's name. */
	bios:       ["exec.bin", "grom.bin"],
	bios_names: ["exec.bin", "grom.bin"],
	bios_md5:   ["62e761035cb657903761800f4437b8af",    /* exec.bin, 8192 bytes */
	             "0cd5946c6473e42e8e4c2137785e427f"]    /* grom.bin, 2048 bytes */
});
