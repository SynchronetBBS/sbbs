// syncretro_getroms.js -- fetch freely-licensed homebrew ROMs for a SyncRetro
// console, so a fresh install is playable before the sysop supplies anything.
//
// Shared by every console: each one ships a short getroms.js naming ITS games and
// calling syncretro_getroms(). Only the table differs.
//
//     load("syncretro_getroms.js");
//     exit(syncretro_getroms(js.exec_dir, "NES", GAMES));
//
// Each GAMES entry is PINNED -- url, size, md5, licence, author -- and verified
// after download. Nothing is fetched by asking an API what the latest release is:
// GitHub's unauthenticated API allows 60 calls an hour and then refuses, which
// would make an installer fail for reasons the sysop cannot see or fix. A pinned
// URL either works or 404s, and a pinned hash means a ROM that changed upstream
// is REJECTED rather than silently installed under a licence we checked once.
//
// ONLY GAMES WHOSE AUTHOR GRANTED AN EXPLICIT FREE LICENCE ARE LISTED. That is a
// per-game fact, checked by hand, not a property of wherever the file is hosted:
// a collection may hold a hundred ROMs and be entitled to redistribute only some.
// If you add a game, read its licence first -- "it was on a homebrew site" is not
// a licence, and most homebrew states none at all, which means ordinary copyright.
//
// Idempotent and non-fatal: a ROM already present is left alone (so a sysop who
// deleted one gets it back only if he asks, and one he edited is never clobbered),
// and a download failure is reported but never aborts an install -- the door is
// perfectly usable with the sysop's own cartridges.
//
// SpiderMonkey 1.8.5: no let/const/arrows/template literals.
//
// Copyright(C) 2026 Rob Swindell / SyncRetro. GPL-2.0.

load("http.js");
load("xtrn_mirror.js");
load("syncretro_lib.js");   // syncretro_rules(): where this console keeps its ROMs

// Where the ROMs go: the console's [roms] dir (default "roms"), under the door.
function syncretro_roms_dir(door_dir)
{
	var dir = backslash(door_dir);
	var f   = new File(dir + "syncretro.ini");
	var sub = "roms";
	var ini;

	if (f.open("r")) {
		ini = f.iniGetObject("roms");
		f.close();
		if (ini && ini.dir)
			sub = String(ini.dir);
	}
	return backslash(dir + sub);
}

// Download one ROM to a temp file, verify it, and only then move it into place.
// A half-written or wrong file must never land in roms/, where the lobby would
// list it and the core would choke on it.
function syncretro_fetch_rom(game, dstdir)
{
	/* `as` is what it is INSTALLED as; `file` is only what upstream calls it. The
	 * lobby parses a game's display name out of its FILENAME, so a ROM fetched as
	 * "sgthelmet.nes" would sit in the picker as "sgthelmet", next to a properly
	 * named cartridge. Install it as "Sgt. Helmet Training Day.nes" instead. */
	var name = game.as ? game.as : game.file;
	var dst  = dstdir + name;
	var tmp  = backslash(system.temp_dir) + game.file;
	var n;

	if (file_exists(dst)) {
		print("  = " + game.title + " (already here)");
		return 0;                       /* 0 = nothing to do, not a failure */
	}

	/* The pinned size/md5 check runs inside the download helper's verify
	 * callback rather than after it, so bytes that fail it are treated exactly
	 * like an unreachable host: the fetch retries against the Synchronet mirror
	 * before giving up. A ROM that changed upstream is still never installed --
	 * the licence, and the fact that this is even the game we think it is, were
	 * verified against THESE bytes. */
	n = xtrn_mirror_download(game.url, tmp, {
		verify: function(p) {
			var got_size = file_size(p);
			var got_md5  = syncretro_file_md5(p, 0);

			if (got_size == game.size && got_md5 == game.md5)
				return true;
			print("  ! " + game.title + ": not the expected file (got "
			    + got_size + " bytes / " + String(got_md5).substr(0, 8)
			    + ", wanted " + game.size + " / " + game.md5.substr(0, 8)
			    + ")");
			return false;
		}
	});
	if (!n) {
		print("  ! " + game.title + ": could not fetch -- skipped");
		return -1;
	}

	if (!file_rename(tmp, dst) && !file_copy(tmp, dst)) {
		print("  ! " + game.title + ": could not install into " + dstdir);
		file_remove(tmp);
		return -1;
	}
	file_remove(tmp);
	print("  + " + game.title + "  (" + game.by + ", " + game.lic + ")");
	return 1;
}

function syncretro_getroms(door_dir, console_name, games)
{
	var dstdir = syncretro_roms_dir(door_dir);
	var got = 0, had = 0, failed = 0;
	var i, r;

	mkpath(dstdir);
	print("SyncRetro: installing freely-licensed " + console_name
	    + " homebrew into " + dstdir);
	print("Every game below is redistributable under the licence its AUTHOR chose;");
	print("none of it is commercial content. See README.md for the credits.");
	print("");

	for (i = 0; i < games.length; i++) {
		r = syncretro_fetch_rom(games[i], dstdir);
		if (r > 0)
			got++;
		else if (r === 0)
			had++;
		else
			failed++;
	}

	print("");
	print("SyncRetro: " + got + " installed, " + had + " already present, "
	    + failed + " failed.");
	if (failed)
		print("A failure here is not fatal -- the door runs on your own cartridges.");
	if (got)
		print("They appear in the lobby the next time a player enters it.");

	/* Never fail the install over ROMs: they are a bonus, not a dependency. */
	return 0;
}
