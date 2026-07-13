// door_deploy.js -- install a freshly-built game-door binary where the door
// expects to find it. Shared by src/doors/*/deploy.js.
//
// This replaces the deploy.sh + deploy.bat pairs each door used to carry. The
// reason it is JavaScript and not a shell/batch pair is not tidiness: it is that
// the deploy step and the LOBBY must agree, exactly, on where the binary lives.
// A shell script would have to re-derive that from `uname`, whose spellings
// differ from Synchronet's system.platform / system.architecture, and drift out
// of agreement with the lobby that has to find the thing. Sharing one function
// makes disagreement impossible rather than unlikely.
//
// THE PER-TARGET SUB-DIRECTORY, and why it is optional.
//
// A *nix door binary is named the same on every OS and architecture -- plain
// "syncdoom" -- so two *nix hosts sharing one install directory (a BBS spanning,
// say, an x86_64 box and an arm64 box over a mount) collide: whichever deployed
// last wins, and the other host runs an executable it cannot exec. Putting each
// host's binary in an "<os>-<arch>" sub-dir (linux-x64, linux-arm64, freebsd-x64,
// darwin-arm64, ...) removes the collision. Windows needs none: a .exe never
// collides with an extension-less *nix name, and the door is Win32-only anyway,
// so Windows is always FLAT.
//
// But a sub-dir can only be found by something that LOOKS for it, and only a
// door's JS lobby can: `ctrl/xtrn.ini` launches a native door with a fixed
// command line ("syncmoo1%. %f ..."), resolved against the door's directory, and
// it cannot probe per host. So:
//
//   * A lobby-launched door can use the sub-dir; its lobby probes <target>/ and
//     falls back to the flat door dir.
//   * A door registered as a DIRECT native launch needs the binary flat.
//   * A door that is both (SyncDOOM, SyncDuke) needs BOTH -- which is what
//     `subdir: true` does: it deploys to the sub-dir AND leaves the flat copy the
//     direct entry needs.
//
// SpiderMonkey 1.8.5: no let/const/arrows/template literals.
//
// Copyright(C) 2026 Rob Swindell. GPL-2.0.

// --- the target token -------------------------------------------------------
//
// Normalizing Synchronet's own values into a stable, filesystem-safe token --
// NOT matching any external tool's spelling. `uname` is deliberately not
// consulted: the lobby cannot run it, and the two must agree.

function door_platform(platname)
{
	var p = String(platname);

	if (/^win/i.test(p))
		return "win32";                                  // doors are Win32 on every Windows host
	if (/darwin|mac|osx/i.test(p))
		return "darwin";                                 // macOS / MacOSX -> one stable name
	return p.toLowerCase().replace(/[^a-z0-9]+/g, "");   // linux, freebsd, netbsd, ...
}

function door_arch(archname)
{
	var a = String(archname).toLowerCase();

	if (/x86[_-]?64|amd64|x64/.test(a))
		return "x64";
	if (/aarch64|arm64|armv8/.test(a))
		return "arm64";                                  // armv8 IS aarch64
	if (/arm/.test(a))
		return "arm";
	if (/i[3-7]86|x86/.test(a))
		return "x86";
	return a.replace(/[^a-z0-9]+/g, "");                 // riscv64, ppc64, ... verbatim
}

// The per-target sub-dir name, or "" when the door dir itself is the answer.
// Windows is always "" -- see the header.
function door_target(platname, archname)
{
	var plat = door_platform(platname);

	if (plat == "win32")
		return "";
	return plat + "-" + door_arch(archname);
}

// The built binary's filename for this host ("syncdoom" / "syncdoom.exe").
function door_exe_name(name, platname)
{
	return name + (/^win/i.test(String(platname)) ? ".exe" : "");
}

// --- finding what was just built --------------------------------------------
//
// WHERE the build leaves the binary is not one path, and assuming it was one is a
// bug this function exists to end: every door's deploy looked for "build/<exe>",
// which is right on *nix and WRONG ON WINDOWS -- build.bat configures a Visual
// Studio (multi-config) generator into build-msvc/, so the binary lands in
// build-msvc/Release/. Deploy therefore failed on every Windows host with
// "not found -- run build.bat first" immediately AFTER a successful build.bat.
//
// So probe, in the order a sysop is likely to have produced them, and let mtime
// break the tie (below): whatever was built LAST is what a deploy should push.
function door_built_candidates(exename)
{
	if (/^win/i.test(String(system.platform)))
		return [ "build-msvc/Release/" + exename,   // build.bat (the documented path)
		         "build-msvc/Debug/" + exename,
		         "build/Release/" + exename,        // hand-run VS generator (see COMPILING.md)
		         "build/Debug/" + exename,
		         "build/" + exename ];              // single-config generator (Ninja/NMake)
	return [ "build/" + exename,                    // build.sh
		 "build/Release/" + exename,            // a multi-config generator (Ninja Multi-Config)
		 "build/Debug/" + exename ];
}

// The built binary, or null when nothing was built. `built` (spec.built) pins the
// location explicitly and skips the probe entirely.
//
// When several builds exist -- a stale build/Debug beside a fresh build-msvc/Release,
// say -- take the NEWEST and SAY SO. Silently preferring a fixed order would deploy a
// binary the sysop did not just build, and the door would then behave like a change
// that "didn't take", which is exactly the class of bug that is miserable to chase.
function door_find_built(srcdir, exename, built)
{
	var dir = backslash(srcdir);
	var cands, found = [], best, i;

	if (built)
		return file_exists(dir + built) ? dir + built : null;

	cands = door_built_candidates(exename);
	for (i = 0; i < cands.length; i++) {
		if (file_exists(dir + cands[i]))
			found.push(dir + cands[i]);
	}
	if (!found.length)
		return null;

	best = found[0];
	for (i = 1; i < found.length; i++) {
		if (file_date(found[i]) > file_date(best))
			best = found[i];
	}
	if (found.length > 1) {
		print("[deploy] " + found.length + " builds of " + exename + " present;"
		    + " deploying the most recently built: " + best);
	}
	return best;
}

// --- the copy ---------------------------------------------------------------

// Is `dst` already this exact build? Compared by CONTENT, and that is not
// fastidiousness -- it is the only test that cannot destroy the build output.
//
// The usual dev install has the door's live entry SYMLINKED to the build output
// (./build.sh and the change is live). Copy onto that symlink and file_copy()
// opens the destination for writing -- which IS the source -- truncating the
// binary to zero bytes before reading a byte of it. The source is gone, and the
// "copy" is a 0-byte file. Both doors this happened to also had a build system
// that then saw a target newer than its sources and cheerfully declined to relink.
//
// The two cheaper tests both fail to see it, which is exactly how it got shipped:
//
//   * fullpath(dst) == fullpath(exe) -- fullpath() NORMALIZES a path, it does not
//     RESOLVE it, so a symlink and its target never compare equal.
//   * size + mtime -- correct on one filesystem, and wrong across the SMB mount a
//     live install is usually reached through: CIFS reported the same file's mtime
//     one second off from the local view, so the guard missed and the copy ran.
//
// The shell scripts this replaced got it right with `[ "$EXE" -ef "$DST" ]` -- a
// device+inode identity test. Synchronet's JS has no stat(), but a content hash
// answers a strictly stronger question, and answers it through symlinks, mounts and
// clock skew alike: if dst's bytes ARE exe's bytes, there is nothing to copy --
// whether dst is a symlink to it, a hardlink, or an identical previous copy. The
// destructive case (dst resolves to exe) is IDENTICAL BY CONSTRUCTION, so it is
// always taken by this branch and the copy below can never eat its own source.
function door_deploy_current(exe, dst)
{
	var a, b, ha, hb;

	if (!file_exists(dst) || file_size(dst) != file_size(exe))
		return false;                   // cheap reject before hashing megabytes

	a = new File(exe);
	b = new File(dst);
	if (!a.open("rb"))
		return false;
	if (!b.open("rb")) {
		a.close();
		return false;
	}
	// BOTH hashes must be read while the files are OPEN. A closed File's md5_hex is
	// undefined -- and reading it after close() is not a harmless nit here: the
	// comparison silently goes false, the guard never fires, and the copy below eats
	// its own source. That is not hypothetical either; it is how this function
	// destroyed four door binaries before it was tested.
	ha = a.md5_hex;
	hb = b.md5_hex;
	a.close();
	b.close();
	return ha !== undefined && ha === hb;
}

// The copy failed. On a live BBS the overwhelmingly likely reason is that the door
// is RUNNING: an executable that is currently mapped cannot be written in place
// (ETXTBSY), and somebody is always playing something.
//
// SAY SO, AND CHANGE NOTHING. The tempting trick -- copy alongside, rename over the
// top, the way a package manager replaces a running binary -- is WRONG here, and
// not theoretically: the door directory is reached over an SMB mount, where
// rename() onto a file another process is executing DELETES THE TARGET AND THEN
// FAILS. This function tried exactly that on its first outing and left the live
// install with a deleted-but-still-open phantom where the door binary had been.
//
// A failed deploy must leave the working binary working.
function door_deploy_busy_note(dst)
{
	print("[deploy] ERROR: " + dst + " is IN USE -- somebody is in this door right"
	    + " now, and a running binary cannot be overwritten.");
	print("[deploy]        Nothing was changed: the old binary is intact and still"
	    + " runs. Re-run this deploy once the door is idle.");
}

function door_deploy_file(exe, dst)
{
	var was = file_size(exe);

	if (door_deploy_current(exe, dst)) {
		print("[deploy] " + dst + " is already this build -- nothing to copy");
		return true;
	}
	if (!file_copy(exe, dst)) {
		if (file_exists(dst))
			door_deploy_busy_note(dst);
		else
			print("[deploy] ERROR: failed to copy " + exe + " -> " + dst);
		return false;
	}
	// The tripwire for the failure above: if the destination resolved back to the
	// source after all, the source is now the truncated one. It cannot be undone
	// here, but it must never again be SILENT -- a 0-byte door is a door that dies
	// at the player's first keystroke, and the build that produced it looks clean.
	if (file_size(exe) != was || file_size(dst) != was) {
		print("[deploy] ERROR: " + exe + " was " + was + " bytes and is now "
		    + file_size(exe) + "; the copy at " + dst + " is " + file_size(dst)
		    + " bytes. The destination resolves back to the source (a symlink?)"
		    + " -- REBUILD before running this door.");
		return false;
	}
	print("[deploy] Deployed: " + fullpath(dst));
	return true;
}

// Install `exe` into one door directory: the flat dir, and -- when `subdir` and
// the host is not Windows -- the <os>-<arch> sub-dir as well.
function door_deploy_into(door_dir, exe, exename, subdir)
{
	var dir = backslash(door_dir);
	var tgt = door_target(system.platform, system.architecture);
	var ok  = true;

	mkpath(dir);
	if (!door_deploy_file(exe, dir + exename))
		ok = false;

	if (subdir && tgt !== "") {
		var sub = backslash(dir + tgt);

		mkpath(sub);
		if (!door_deploy_file(exe, sub + exename))
			ok = false;
	}
	return ok;
}

// --- the entry point --------------------------------------------------------
//
// spec:
//   name    "syncdoom"                 -- the binary's base name
//   srcdir  js.exec_dir                -- src/doors/<door>/ (the deploy script's own dir)
//   built   "build/syncdoom"           -- OPTIONAL: pin the built binary's location,
//                                         relative to srcdir. Omit it (every door does)
//                                         and door_find_built() probes the per-platform
//                                         build dirs -- build/ on *nix, build-msvc/<Config>/
//                                         for build.bat. Only pin it for a door whose
//                                         build leaves the binary somewhere unusual.
//   xtrn    "syncdoom"                 -- the xtrn/<dir> bundle name
//   subdir  true|false                 -- also install into <os>-<arch>/ (see the header)
//
// The destination is the LIVE install (the xtrn/<door> dir beside system.ctrl_dir) --
// the only copy anything ever runs. A bare checkout with no BBS installed falls back
// to the in-tree bundle. Extra destinations may be given on the command line.
// Is the per-target sub-dir wanted? The spec's default, overridden by the command
// line (--subdir / --no-subdir).
//
// FLAT IS THE DEFAULT, and that is deliberate. A sub-dir copy is a SNAPSHOT, and a
// door dir whose flat entry is a SYMLINK to the build output (the usual dev setup:
// ./build.sh and the change is live, no deploy step) would start preferring the
// snapshot the moment one existed -- so a rebuild would silently stop reaching the
// player. Ask for the sub-dir when you actually need it: a shared install serving
// two *nix hosts of different architecture.
function door_want_subdir(spec)
{
	var i;

	for (i = 0; i < argc; i++) {
		if (argv[i] == "--subdir" || argv[i] == "-subdir" || argv[i] == "--target")
			return true;
		if (argv[i] == "--no-subdir")
			return false;
	}
	return spec.subdir ? true : false;
}

function door_deploy(spec)
{
	var exename = door_exe_name(spec.name, system.platform);
	var exe     = door_find_built(spec.srcdir, exename, spec.built);
	var bundle  = backslash(spec.srcdir) + "../../../xtrn/" + (spec.xtrn || spec.name);
	var tgt     = door_target(system.platform, system.architecture);
	var subdir  = door_want_subdir(spec);
	var ok      = true;
	var live, i, looked;

	if (exe === null) {
		looked = spec.built ? [ spec.built ] : door_built_candidates(exename);
		print("[deploy] ERROR: no " + exename + " found under "
		    + fullpath(backslash(spec.srcdir)) + " -- run "
		    + (/^win/i.test(system.platform) ? "build.bat" : "./build.sh") + " first");
		print("[deploy]        Looked for: " + looked.join(", "));
		return 1;
	}
	print("[deploy] " + fullpath(exe) + " -> "
	    + (subdir && tgt ? ("flat + " + tgt + "/") : "the door dir (flat)")
	    + (subdir || !tgt ? "" : "   [--subdir also installs into " + tgt + "/]"));
	if (subdir && spec.direct_launch)
		print("[deploy] NOTE: " + spec.name + " is launched directly from xtrn.ini,"
		    + " whose fixed command line cannot find a sub-dir. The flat copy is what"
		    + " runs; the sub-dir is inert until the door grows a JS launcher.");

	// THE destination is the LIVE install -- the only copy anything ever runs.
	//
	// It used to also copy into the in-tree bundle (<checkout>/xtrn/<door>/), and that
	// was pure confusion. On the recommended install (install-sbbs.mk SYMLINK=1) the
	// live xtrn IS a symlink to the repo's, so the two destinations are ONE FILE: the
	// second pass found the bytes the first had just written and reported "already this
	// build -- nothing to copy" straight after "Deployed", which reads as a
	// contradiction to anyone checking whether their build landed. And where they are
	// genuinely two directories (a copy install; a checkout that is not the install),
	// nothing ever launches from the checkout -- xtrn.ini runs the door out of the LIVE
	// dir, and install-xtrn fetches its binary via get-binary.js -- so that copy only
	// littered the working tree with a gitignored multi-megabyte binary.
	//
	// system.ctrl_dir names the install we are actually running against, in both jsexec
	// and in-BBS contexts (the environment's $SBBSCTRL exists only under jsexec).
	live = backslash(system.ctrl_dir) + "../xtrn/" + (spec.xtrn || spec.name);
	if (file_isdir(live)) {
		if (!door_deploy_into(live, exe, exename, subdir))
			ok = false;
	} else if (file_isdir(bundle)) {
		// No live install: a bare checkout with no BBS (building for packaging, CI).
		// The in-tree bundle is then the only sensible destination.
		print("[deploy] No live install at " + fullpath(backslash(live))
		    + " -- deploying into the in-tree bundle instead"
		    + " (run install-xtrn there for a first install)");
		if (!door_deploy_into(bundle, exe, exename, subdir))
			ok = false;
	} else {
		print("[deploy] ERROR: no live install at " + fullpath(backslash(live))
		    + " and no bundle at " + fullpath(backslash(bundle)) + " -- nothing to deploy to");
		ok = false;
	}

	// 3. Any extra directories given on the command line.
	for (i = 0; i < argc; i++) {
		if (argv[i].charAt(0) == "-")
			continue;                       // an option, not a directory
		if (!door_deploy_into(argv[i], exe, exename, subdir))
			ok = false;
	}
	return ok ? 0 : 1;
}
