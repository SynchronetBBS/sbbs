// deploy.js -- install the freshly-built SyncSCUMM (ScummVM) binary into
// every live SyncSCUMM title package.
//
// Run via jsexec AFTER build.sh / build.bat:
//
//     jsexec src/doors/syncscumm/deploy.js            (deploy to the live install)
//     jsexec src/doors/syncscumm/deploy.js <dir> ...  (also deploy to these dirs)
//
// ONE BINARY, MANY TITLES: build.sh produces a single `scummvm` binary that
// plays every SyncSCUMM title (Flight of the Amazon Queen, Beneath a Steel
// Sky, ...) -- each title is its own individually-installed xtrn/sync<game>/
// package (see xtrn/syncqueen/README.md), but every one of them runs the
// SAME binary. So this deploys to every SyncSCUMM package that exists in the
// live install, rather than the one fixed xtrn dir most doors' deploy.js
// copies into (compare src/doors/syncmoo1/deploy.js).
//
// PACKAGES below is a small, explicit, hand-maintained list -- unlike
// SyncRetro's deploy.js (src/doors/syncretro/deploy.js), which auto-discovers
// every console by scanning the live xtrn/ for a lobby.js that loads
// syncretro_lobby.js. SyncSCUMM titles have no lobby to probe for: each is a
// fixed, individually-installed package with its own install-xtrn.ini, so
// there is nothing in a bare xtrn/<dir> to detect "this is a SyncSCUMM title"
// by. Add a new title's xtrn dir name here when its package ships; a package
// that is not installed yet (syncbass, before M5 Task 5 lands it) is skipped,
// not an error -- this script must not fail just because not every title has
// been fetched yet.
//
// Every title is launched DIRECTLY from xtrn.ini, with no lobby (see
// xtrn/syncqueen/install-xtrn.ini's `cmd =` line: `scummvm%. %f --path=. ...`),
// so the binary must be FLAT in each package dir -- the same reasoning as
// src/doors/syncmoo1/deploy.js, not the per-target sub-dir SyncRetro's lobby
// can probe for.
//
// The COPY ITSELF, and the symlink-vs-timestamp caveat, are
// exec/load/door_deploy.js's -- shared by every door's deploy.js so the
// destructive "copy truncates its own source through a symlinked live entry"
// bug (see that file's door_deploy_current() comment) cannot recur here.
// This script only supplies the "for every SyncSCUMM package" loop
// door_deploy() itself does not have, since door_deploy() assumes one door,
// one xtrn dir.
//
// SpiderMonkey 1.8.5: no let/const/arrows/template literals.
//
// Copyright(C) 2026 Rob Swindell / SyncSCUMM. GPL-2.0.

load("door_deploy.js");   // door_exe_name(), door_find_built(), door_deploy_into()

// Every SyncSCUMM title package -- the xtrn/<dir> name, matching each
// title's own install-xtrn.ini. See the header comment above.
var PACKAGES = [ "syncqueen", "syncbass" ];

// The LIVE install's xtrn/ -- the one anything actually launches from.
// system.ctrl_dir names the install we are running against, under jsexec and
// in the BBS alike.
var LIVE = backslash(system.ctrl_dir) + "../xtrn/";

// The in-tree bundle: js.exec_dir is this script's own dir
// (src/doors/syncscumm/), and xtrn/ is three up. Only a fallback -- see main().
var BUNDLE = js.exec_dir + "../../../xtrn/";

function main()
{
	var exename = door_exe_name("scummvm", system.platform);
	var exe     = door_find_built(js.exec_dir, exename);
	var ok      = true;
	var deployed = 0;
	var i, dir;

	if (exe === null) {
		print("[deploy] ERROR: no " + exename + " found under " + fullpath(js.exec_dir)
		    + " -- run " + (/^win/i.test(system.platform) ? "build.bat" : "./build.sh") + " first");
		return 1;
	}
	print("[deploy] " + fullpath(exe) + " -> every live SyncSCUMM package (flat, direct launch)");

	// 1. Every SyncSCUMM package in the LIVE install. One door binary, so
	//    each title gets the same one -- see the header comment.
	for (i = 0; i < PACKAGES.length; i++) {
		dir = backslash(LIVE) + PACKAGES[i];
		if (!file_isdir(dir))
			continue;                 // not installed yet (e.g. syncbass pre-Task 5)
		print("[deploy] package: " + fullpath(dir));
		if (!door_deploy_into(dir, exe, exename, false))
			ok = false;
		deployed++;
	}

	// 2. No live package existed at all: a bare checkout with no BBS
	//    (packaging, CI), or a BBS where no SyncSCUMM title has been
	//    installed yet. Fall back to the in-tree bundle the same way every
	//    other door's deploy.js does, so a from-scratch build still has
	//    somewhere to land.
	if (!deployed) {
		var found = false;

		for (i = 0; i < PACKAGES.length; i++) {
			dir = backslash(BUNDLE) + PACKAGES[i];
			if (!file_isdir(dir))
				continue;
			if (!found) {
				print("[deploy] No SyncSCUMM package under " + fullpath(LIVE)
				    + " -- deploying into the in-tree bundle instead"
				    + " (run install-xtrn there for a first install)");
			}
			found = true;
			print("[deploy] package (bundle): " + fullpath(dir));
			if (!door_deploy_into(dir, exe, exename, false))
				ok = false;
		}
		if (!found) {
			print("[deploy] WARNING: no SyncSCUMM package found under "
			    + fullpath(LIVE) + " or " + fullpath(BUNDLE));
		}
	}

	// 3. Any extra live bundle dir(s) given on the command line (copy installs).
	for (i = 0; i < argc; i++) {
		if (!door_deploy_into(argv[i], exe, exename, false))
			ok = false;
	}

	return ok ? 0 : 1;
}

exit(main());
