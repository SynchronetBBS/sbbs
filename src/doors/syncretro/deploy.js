// deploy.js -- install the freshly-built syncretro binary into the door bundle.
//
// Run via jsexec AFTER build.sh / build.bat:
//
//     jsexec src/doors/syncretro/deploy.js            (deploy to the live install)
//     jsexec src/doors/syncretro/deploy.js <dir>      (also deploy to a live bundle dir)
//
// This REPLACES the old deploy.sh / deploy.bat. The binary and the libretro core
// go where the lobby looks for them: FLAT at the door root on Windows (a
// .exe/.dll never collides with a *nix host's names), or in an <os>-<arch>
// sub-directory on *nix (linux-x64, linux-arm64, darwin-arm64, ...) so a shared
// install can serve several *nix hosts without their same-named binaries
// colliding. That location is syncretro_target(system.platform, system.architecture)
// ("" == flat) -- and the whole reason this is a jsexec script and not a
// shell/batch pair is so it computes that token with the SAME code the lobby
// (lobby.js) and the core installer (getcore.js) use. A shell/batch deploy would
// have to re-derive it from `uname`, whose spellings differ from
// system.platform/system.architecture, and drift out of agreement with the lobby.
//
// ONE BINARY, MANY CONSOLES: the same door hosts any libretro core, so every
// console install (xtrn/syncivision, xtrn/syncnes, ...) needs a copy of it. This
// finds them all rather than naming one -- a console added later is deployed to
// without touching this script. They are found in the LIVE install (the xtrn/
// beside system.ctrl_dir), which is the only copy anything ever runs; a bare
// checkout with no BBS installed falls back to the in-tree bundle. See
// console_dirs().
//
// Deploys only the frontend binary; getcore.js installs each console's core, and
// the BIOS + cartridges are sysop-supplied. SpiderMonkey 1.8.5.
//
// Copyright(C) 2026 Rob Swindell / SyncRetro. GPL-2.0.

load("syncretro_lib.js");   // syncretro_target() (which delegates to door_deploy.js)
load("door_deploy.js");     // door_deploy_into(): the same copy logic every door uses

// The LIVE install's xtrn/ -- the one anything actually launches from.
// system.ctrl_dir names the install we are running against, under jsexec and in
// the BBS alike.
var LIVE = backslash(system.ctrl_dir) + "../xtrn/";

// The in-tree bundle: js.exec_dir is this script's own dir (src/doors/syncretro/),
// and xtrn/ is three up. Only a fallback -- see main().
var BUNDLE = js.exec_dir + "../../../xtrn/";

// Every SyncRetro console install under `root`: an xtrn dir whose lobby.js drives
// the shared lobby. That is the definition of "a SyncRetro console", so it cannot
// go stale -- a console added later is deployed to without touching this script.
function console_dirs(root)
{
	var out = [];

	directory(backslash(root) + "*").forEach(function (path) {
		var dir = backslash(path);
		var f, text;

		if (!file_isdir(dir) || !file_exists(dir + "lobby.js"))
			return;
		f = new File(dir + "lobby.js");
		if (!f.open("r"))
			return;
		text = f.read();
		f.close();
		if (String(text).indexOf("syncretro_lobby.js") >= 0)
			out.push(dir);
	});
	return out;
}

function is_win() { return /^win/i.test(system.platform); }

// Where build.sh / build.bat leave the binary: a single-config tree on *nix,
// the multi-config Release sub-dir on Windows/MSVC.
function built_exe()
{
	return is_win() ? js.exec_dir + "build-msvc/Release/syncretro.exe"
	                : js.exec_dir + "build/syncretro";
}

// Copy `exe` into <door>/<target>/syncretro[.exe], or straight into <door> when
// `target` is "" (flat, Windows) -- backslash(backslash(door) + "") collapses to
// the door dir.
//
// The COPY ITSELF is door_deploy_file()'s, and this function must never grow its
// own again: the private copy that used to live here compared the destination by
// path and timestamp, which cannot see that the destination is a SYMLINK back to
// the build output -- so it copied the binary onto itself and truncated it to zero.
// The shared one compares CONTENT. See exec/load/door_deploy.js.
function deploy_to(door, exe, exename, target)
{
	var dstdir = backslash(backslash(door) + target);
	var flat   = backslash(door) + exename;
	var ok;

	mkpath(dstdir);
	ok = door_deploy_file(exe, dstdir + exename);

	// An older install may have a FLAT binary beside the sub-dir. The lobby probes
	// <target>/ first and falls back to flat, so that copy is inert -- until someone
	// removes the sub-dir and silently gets a stale door. Keep an existing one in
	// step; don't create one that wasn't there.
	if (target !== "" && file_exists(flat))
		ok = door_deploy_file(exe, flat) && ok;
	return ok;
}

function main()
{
	var exe     = built_exe();
	var exename = is_win() ? "syncretro.exe" : "syncretro";
	var target  = syncretro_target(system.platform, system.architecture);
	var ok      = true;
	var i;

	if (!file_exists(exe)) {
		print("[deploy] ERROR: " + exe + " not found -- run "
		    + (is_win() ? "build.bat" : "./build.sh") + " first");
		return 1;
	}
	print("[deploy] target: " + (target ? target + "/" : "flat (door dir)") + "  (" + exename + ")");

	// 1. Every console install in the LIVE install. One door binary, so each
	//    console gets the same one.
	//
	// This used to scan the in-tree bundle (<checkout>/xtrn/) AS WELL, and that was
	// pure confusion. On the recommended install (install-sbbs.mk SYMLINK=1 does
	// "ln -sf $(REPODIR)/xtrn $(SBBSDIR)") the live xtrn IS a symlink to the repo's,
	// so the two scans address ONE directory -- and the guard meant to collapse them
	// could not see it: fullpath() NORMALIZES a path, it does not RESOLVE it, so a
	// symlink and its target never compare equal, and Synchronet's JS has no stat()
	// to compare inodes with. The second pass then found the bytes the first had just
	// written and printed "already this build -- nothing to copy" directly after
	// "Deployed" -- which reads as a contradiction precisely when a sysop is asking
	// whether their build landed.
	//
	// Where the two ARE different directories (a copy-style install, or a checkout
	// that is not the install), the in-tree copy was pointless anyway: nothing
	// launches from the checkout -- the lobby runs the door out of the live console
	// dir -- so it only littered the working tree with a gitignored multi-megabyte
	// binary. The same reasoning, and the same fix, as door_deploy() in
	// exec/load/door_deploy.js, which every other door uses. (SyncRetro needs its own
	// because one binary serves N console dirs, not one.)
	var dirs = console_dirs(LIVE);
	if (!dirs.length) {
		// No console in the live install: a bare checkout with no BBS (packaging,
		// CI), or a BBS that has not installed a console yet. The in-tree bundle is
		// then the only sensible destination.
		dirs = console_dirs(BUNDLE);
		if (dirs.length)
			print("[deploy] No SyncRetro console under " + fullpath(LIVE)
			    + " -- deploying into the in-tree bundle instead"
			    + " (run install-xtrn there for a first install)");
		else
			print("[deploy] WARNING: no SyncRetro console install found under "
			    + fullpath(LIVE) + " or " + fullpath(BUNDLE));
	}
	for (i = 0; i < dirs.length; i++) {
		print("[deploy] console: " + fullpath(dirs[i]));
		if (!deploy_to(dirs[i], exe, exename, target))
			ok = false;
	}

	// 2. Any extra live bundle dir(s) given on the command line (copy installs).
	for (i = 0; i < argc; i++) {
		if (!deploy_to(argv[i], exe, exename, target))
			ok = false;
	}

	return ok ? 0 : 1;
}

exit(main());
