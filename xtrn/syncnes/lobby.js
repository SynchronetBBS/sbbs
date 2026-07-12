// lobby.js -- xtrn/syncnes: SyncRetro's Nintendo Entertainment System install.
//
// This file IS the console definition. Everything else -- discovery, the picker,
// the activity board, the door command line -- lives in exec/load/syncretro_lobby.js
// and is shared with every other SyncRetro console, because they all run the one
// door binary against a different libretro core. Adding a console is adding one of
// these files, not forking a lobby.
//
// See src/doors/syncretro/M3_MULTICORE.md.
//
// SpiderMonkey 1.8.5: no let/const/arrows/template literals.
//
// Copyright(C) 2026 Rob Swindell / SyncRetro. GPL-2.0.

load("syncretro_lobby.js");

syncretro_lobby({
	dir:      js.exec_dir,               /* this door dir: core, roms/ */
	name:     "Nintendo Entertainment System",
	short:    "NES",                     /* -> id "nes": the save dir and ROM cache */
	core:     "fceumm_libretro",         /* no extension: .so / .dll / .dylib */
	/* A NES controller IS a plain RetroPad -- d-pad, B, A, Select, Start -- so it
	 * needs no console-specific C at all. See M3_MULTICORE.md sec 4. */
	profile:  "pad",

	/* What a NES cartridge looks like. fceumm's own valid_extensions are
	 * "fds|nes|unf|unif" -- but .fds (Famicom Disk System) is DELIBERATELY absent
	 * here: an FDS image needs the disksys.rom BIOS in this directory, which is
	 * copyrighted content the sysop must supply. A sysop who has it can add "fds"
	 * to [roms] ext in syncretro.ini and drop disksys.rom here. Without it fceumm
	 * fails the load, and the player just sees a door that will not start. */
	ext:      ["nes", "unf", "unif"],
	/* The NES cartridge range: a 8 KB single-bank game up to a 4 MB late mapper.
	 * Nothing outside it is a game (a .nes under 8 KB is a header and no PRG). */
	min_size: 8 * 1024,
	max_size: 4 * 1024 * 1024,

	/* NO BIOS. Unlike the Intellivision, an NES boots from the cartridge alone --
	 * so there is nothing for the sysop to supply, and nothing to reject from the
	 * picker. (The exception is .fds, above.) */
	bios:     []

	/* To run this door through the BBS's stdin/stdout instead of a socket -- the
	 * way Mystic does on *nix, and the way to exercise the door's -stdio path --
	 * add:  stdio: true
	 * Under Synchronet the socket is the better path (no pty hop), so it is the
	 * default. Both are live-tested. */
});
