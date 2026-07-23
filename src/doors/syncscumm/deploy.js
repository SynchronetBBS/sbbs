// deploy.js -- install the freshly-built SyncSCUMM (ScummVM) binary into
// every live SyncSCUMM title package.
//
// Run via jsexec AFTER build.sh / build.bat:
//
//     jsexec src/doors/syncscumm/deploy.js            (deploy to the live install)
//     jsexec src/doors/syncscumm/deploy.js <dir> ...  (also deploy to these dirs)
//
// ONE BINARY, MANY TITLES: build.sh produces a single `syncscumm` binary that
// plays every SyncSCUMM title (Flight of the Amazon Queen, Beneath a Steel
// Sky, ...) -- each title is its own individually-installed xtrn/sync<game>/
// package (see xtrn/syncqueen/README.md), but every one of them runs the
// SAME binary. So this deploys to every SyncSCUMM package that exists in the
// live install, rather than the one fixed xtrn dir most doors' deploy.js
// copies into (compare src/doors/syncmoo1/deploy.js).
//
// PACKAGES ARE DISCOVERED, NOT WHITELISTED. This script used to carry a
// hand-maintained list of xtrn dir names, on the theory that -- unlike
// SyncRetro, whose deploy.js finds every console by the lobby.js that loads
// syncretro_lobby.js -- "a SyncSCUMM title has nothing in its bare xtrn dir to
// detect it by." That was wrong twice over, and a whitelist goes stale (a
// title added or hand-installed later is silently skipped until someone edits
// this file). syncscumm_dirs() below finds the packages the same self-updating
// way SyncRetro does, from two independent markers whose UNION covers every
// case a list could not:
//
//   1. The dir already holds a syncscumm binary (door_deploy_current() then
//      makes the copy a no-op if it is already this build). This is the
//      "keep the binary current" case -- and it is the ONLY marker for a door
//      a sysop registered by hand in SCFG rather than via install-xtrn: a
//      COMMERCIAL title (e.g. a Maniac Mansion door, whose game data the sysop
//      supplies) ships no install-xtrn.ini, so nothing but the deployed binary
//      itself identifies it.
//   2. Its install-xtrn.ini has a program command line that LAUNCHES the
//      syncscumm binary. This is the first-install case: a freeware package
//      fetched but not yet deployed has no binary yet, but its installer data
//      names the door, so its first binary lands here too. The test is the
//      invoked-binary token, not any mention of the word, so a package whose
//      installer names "SyncSCUMM" in its Name/Desc but launches some other
//      binary is not mistaken for a deploy target.
//
// (1) alone would miss a never-deployed freeware package; (2) alone would miss
// a hand-registered commercial title. Neither marker can go stale the way the
// old list did.
//
// Every title is launched DIRECTLY from xtrn.ini, with no lobby (see
// xtrn/syncqueen/install-xtrn.ini's `cmd =` line: `syncscumm%. %f ... queen`,
// run from the package dir as its own CWD), so the binary must be FLAT in
// each package dir -- the same reasoning as
// src/doors/syncmoo1/deploy.js, not the per-target sub-dir SyncRetro's lobby
// can probe for. That is why door_deploy_into()'s `subdir` argument is false.
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

// The LIVE install's xtrn/ -- the one anything actually launches from.
// system.ctrl_dir names the install we are running against, under jsexec and
// in the BBS alike.
var LIVE = backslash(system.ctrl_dir) + "../xtrn/";

// The in-tree bundle: js.exec_dir is this script's own dir
// (src/doors/syncscumm/), and xtrn/ is three up. Only a fallback -- see main().
var BUNDLE = js.exec_dir + "../../../xtrn/";

// Marker 1: does this dir already hold a syncscumm binary? EITHER platform's
// name -- a directory is a SyncSCUMM package whether the copy sitting there is
// this host's or the other's (a shared install serving a *nix and a Windows
// host has both), and a *nix deploy should still recognize and update a dir
// that currently holds only the Windows syncscumm.exe.
function dir_has_syncscumm_binary(dir)
{
	return file_exists(dir + "syncscumm") || file_exists(dir + "syncscumm.exe");
}

// Marker 2: does an install-xtrn.ini here have a program command line that
// LAUNCHES the syncscumm binary? Matches the invoked-binary token
// (`syncscumm%.`, `syncscumm.exe`, or `syncscumm ` at a word boundary), not a
// bare mention of the word -- so the M2 test skeleton, whose cmd launches
// ../build/scummvm while its Name says "SyncSCUMM", is correctly excluded.
// `scummvm` never matches: it does not contain the substring "syncscumm".
function install_xtrn_launches_syncscumm(ini)
{
	var f, text, lines, i;

	if (!file_exists(ini))
		return false;
	f = new File(ini);
	if (!f.open("r"))
		return false;
	text = f.read();
	f.close();
	lines = String(text).split("\n");
	for (i = 0; i < lines.length; i++) {
		if (/^\s*cmd\b/i.test(lines[i]) && /\bsyncscumm(%|\.exe|\s|$)/i.test(lines[i]))
			return true;
	}
	return false;
}

// Every SyncSCUMM package under `root`: any xtrn dir that either already holds
// the door binary (marker 1) or whose install-xtrn.ini launches it (marker 2).
// See the header comment for why the union is the right definition and why it
// cannot go stale.
function syncscumm_dirs(root)
{
	var out = [];

	directory(backslash(root) + "*").forEach(function (path) {
		var dir = backslash(path);

		if (!file_isdir(dir))
			return;
		if (dir_has_syncscumm_binary(dir)
		    || install_xtrn_launches_syncscumm(dir + "install-xtrn.ini"))
			out.push(dir);
	});
	return out;
}

function main()
{
	var exename = door_exe_name("syncscumm", system.platform);
	var exe     = door_find_built(js.exec_dir, exename);
	var ok      = true;
	var dirs, i;

	if (exe === null) {
		print("[deploy] ERROR: no " + exename + " found under " + fullpath(js.exec_dir)
		    + " -- run " + (/^win/i.test(system.platform) ? "build.bat" : "./build.sh") + " first");
		return 1;
	}
	print("[deploy] " + fullpath(exe) + " -> every live SyncSCUMM package (flat, direct launch)");

	// 1. Every SyncSCUMM package in the LIVE install -- discovered, not listed.
	dirs = syncscumm_dirs(LIVE);

	// 2. No live package at all: a bare checkout with no BBS (packaging, CI), or
	//    a BBS where no SyncSCUMM title has been installed yet. Fall back to the
	//    in-tree bundle the same way every other door's deploy.js does, so a
	//    from-scratch build still has somewhere to land.
	if (!dirs.length) {
		dirs = syncscumm_dirs(BUNDLE);
		if (dirs.length)
			print("[deploy] No SyncSCUMM package under " + fullpath(LIVE)
			    + " -- deploying into the in-tree bundle instead"
			    + " (run install-xtrn there for a first install)");
		else
			print("[deploy] WARNING: no SyncSCUMM package found under "
			    + fullpath(LIVE) + " or " + fullpath(BUNDLE));
	}
	for (i = 0; i < dirs.length; i++) {
		print("[deploy] package: " + fullpath(dirs[i]));
		if (!door_deploy_into(dirs[i], exe, exename, false))
			ok = false;
	}

	// 3. Any extra live bundle dir(s) given on the command line (copy installs).
	for (i = 0; i < argc; i++) {
		if (!door_deploy_into(argv[i], exe, exename, false))
			ok = false;
	}

	return ok ? 0 : 1;
}

exit(main());
