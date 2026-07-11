// getcore.js -- install the FreeIntv libretro core for the SyncRetro
// (Intellivision) door.  LOCAL-FIRST: use a core already installed, or one the
// sysop dropped into the door directory (a loose file or an archive), and only
// DOWNLOAD from the libretro buildbot when neither is present.
//
// FreeIntv is GPLv2+ (freely redistributable), so -- unlike the Intellivision
// BIOS (exec.bin/grom.bin) and the cartridges, which are content the sysop
// supplies -- this core CAN be fetched automatically.  The core is a
// platform-specific shared library, installed into the SAME place the door
// binary lives (sv_target()): FLAT at the door root on Windows, or in an
// <os>-<arch> sub-directory on *nix, so one shared install can serve several
// hosts without their same-named cores colliding:
//
//   <door>/freeintv_libretro.dll            (Windows -- flat; .dll disambiguates)
//   <door>/darwin-arm64/freeintv_libretro.dylib
//   <door>/linux-x64/freeintv_libretro.so   (and linux-arm64/, freebsd-x64/, ...)
//
// A Win32 door MUST load a 32-bit (windows/x86) core regardless of the host's
// bitness, so the download always fetches the x86 Windows build.  lobby.js picks
// the same location + filename (SYNCRETRO_CORE), so whatever this installs is
// what the door loads.
//
// Run automatically (and prompted) by the installer via install-xtrn.ini's
// [exec:getcore.js] step, or by hand any time:
//
//     jsexec ../xtrn/syncivision/getcore.js
//
// Idempotent and non-fatal: an already-present core is left alone, and a
// download/extract failure is reported but never aborts the install -- the sysop
// can always drop the core into the door dir by hand.  Downloads stream straight
// to disk (HTTPRequest.Download in load/http.js), so the archive is never
// buffered in memory.  SpiderMonkey 1.8.5-compatible (no modern ES).
//
// Copyright(C) 2026 Rob Swindell / SyncRetro.  GPL-2.0.

load("http.js");
load(js.exec_dir + "syncivision_lib.js");   // sv_platform(), sv_core_ext(), sv_target()

// libretro nightly buildbot. The per-platform core zip contains just the core
// library at the top level (e.g. freeintv_libretro.dll), so extracting by its
// basename drops it straight into the destination.
var BUILDBOT = "https://buildbot.libretro.com/nightly/";

// Archive extensions we'll look inside for a dropped-in core (libarchive-backed
// Archive reads all of these). The buildbot ships a plain .zip.
var ARCHIVE_EXT = [".zip", ".7z", ".rar", ".tar", ".tar.gz", ".tgz", ".tar.xz",
                   ".tar.bz2", ".gz", ".xz", ".bz2"];

// This script's own directory (the door root, where install-xtrn.ini /
// syncretro.ini / the per-platform sub-dirs live), trailing-slashed. js.exec_dir
// is the running SCRIPT's dir whether launched by the installer or by hand --
// unlike js.startup_dir (the BBS/jsexec cwd). Fall back to startup_dir if unset.
function door_dir()
{
	return backslash(js.exec_dir || js.startup_dir || "./");
}

// The libretro buildbot sub-path for THIS host. Windows is pinned to windows/x86
// (the door is Win32); *nix/macOS follow the host architecture, since build.sh
// builds the door for the host.
function buildbot_sub()
{
	var p   = system.platform;
	var a   = String(system.architecture || "").toLowerCase();
	var b64 = /64/.test(a);            // x86_64 / amd64 / x64 / aarch64
	var arm = /arm|aarch/.test(a);

	if (/^win/i.test(p))
		return "windows/x86";                       // door is Win32
	if (/darwin|mac|osx/i.test(p))
		return "apple/osx/" + (arm ? "arm64" : "x86_64");
	return "linux/" + (b64 ? "x86_64" : "x86");
}

// Basename of a path (handles both separators; an archive may use '/').
function basename(p)
{
	var s = String(p).replace(/\\/g, "/");
	var i = s.lastIndexOf("/");
	return i >= 0 ? s.substr(i + 1) : s;
}

// Pull `core` out of the archive at `path` into `dstdir`. Matches the member by
// basename, case-insensitively (a sysop-made zip may nest or re-case it), and
// renames the extracted file to the canonical lower-case core name if needed.
// Returns true iff the core ends up present in `dstdir`.
function extract_core_from(path, dstdir, core)
{
	var i, names, base, entry;

	try {
		var ar = new Archive(path);
		names = ar.list();                         // array of { name, type, ... }
		for (i = 0; i < names.length; i++) {
			entry = names[i];
			if (entry.type && entry.type != "file")
				continue;
			base = basename(entry.name);
			if (base.toLowerCase() != core)
				continue;
			ar.extract(dstdir, entry.name);        // flattens to the basename
			if (base != core && file_exists(dstdir + base) && !file_exists(dstdir + core))
				file_rename(dstdir + base, dstdir + core);
			if (file_exists(dstdir + core)) {
				print("  installed " + core + " from " + basename(path));
				return true;
			}
		}
	} catch (e) {
		print("  ! " + basename(path) + ": " + e);
	}
	return false;
}

// Try every archive the sysop may have dropped into the door root, extracting
// the core into `dstdir`. The common case is the buildbot's own
// freeintv_libretro.<ext>.zip dropped in as-is.
function from_dropped_archive(root, dstdir, core)
{
	var list = directory(root + "*");
	var i, j, lower;

	for (i = 0; i < list.length; i++) {
		lower = list[i].toLowerCase();
		for (j = 0; j < ARCHIVE_EXT.length; j++) {
			if (lower.length >= ARCHIVE_EXT[j].length
			    && lower.substr(lower.length - ARCHIVE_EXT[j].length) == ARCHIVE_EXT[j]) {
				if (extract_core_from(list[i], dstdir, core))
					return true;
				break;
			}
		}
	}
	return false;
}

// A loose core file the sysop dropped in (at the door root or already in the
// platform sub-dir), possibly under a different case. Move/rename it into place.
function from_loose_file(root, dstdir, core)
{
	var i, hit = directory(root + "freeintv_libretro.*").concat(
	             directory(dstdir + "freeintv_libretro.*"));

	// Any hit is in the wrong place or the wrong case (an exact dstdir/core was
	// already caught by main()'s step 1), so move it to the canonical target.
	for (i = 0; i < hit.length; i++) {
		if (basename(hit[i]).toLowerCase() != core)
			continue;
		file_rename(hit[i], dstdir + core);
		if (file_exists(dstdir + core)) {
			print("  installed " + core + " (from " + hit[i] + ")");
			return true;
		}
	}
	return false;
}

// Download the correct per-platform build and extract the core into `dstdir`.
function from_buildbot(dstdir, core, sub)
{
	var url = BUILDBOT + sub + "/latest/" + core + ".zip";
	var tmp = backslash(system.temp_dir) + core + ".zip";
	var ok  = false;

	print("  downloading " + core + " (" + url + ") ...");
	try {
		var req = new HTTPRequest();
		req.follow_redirects = 5;
		var n = req.Download(url, tmp);
		if (req.response_code != 200) {
			print("  ! download failed: HTTP " + req.response_code);
		} else {
			print("  downloaded " + n + " bytes; extracting ...");
			ok = extract_core_from(tmp, dstdir, core);
			if (!ok)
				print("  ! " + core + " not found in the downloaded archive");
		}
	} catch (e) {
		print("  ! error fetching " + core + ": " + e);
	} finally {
		if (file_exists(tmp))
			file_remove(tmp);
	}
	return ok;
}

function main()
{
	var root   = door_dir();
	var target = sv_target(system.platform, system.architecture);   // "" (flat, Windows), linux-x64, ...
	var core   = "freeintv_libretro." + sv_core_ext(sv_platform(system.platform));
	var dst    = target ? backslash(root + target) : root;          // <door>/<target>/ or the flat door dir
	var where  = target ? (target + "/") : "the door dir";
	var sub    = buildbot_sub();

	// 1. Already installed?
	if (file_exists(dst + core)) {
		print("SyncRetro: " + core + " is already present in " + where + " -- nothing to do.");
		return 0;
	}

	print("SyncRetro: installing the FreeIntv core (" + core + ") into " + dst);
	mkpath(dst);

	// 2. A copy the sysop dropped in (loose file, or an archive at the door root).
	if (from_loose_file(root, dst, core))
		return 0;
	if (from_dropped_archive(root, dst, core))
		return 0;

	// 3. Download from the libretro buildbot (FreeIntv is GPLv2+).
	if (from_buildbot(dst, core, sub)) {
		print("SyncRetro: FreeIntv core installed.");
		return 0;
	}

	print("SyncRetro: could not install the core automatically. Drop "
	    + core + " (or its buildbot .zip) into " + root + " -- it is at\n"
	    + "  " + BUILDBOT + sub + "/latest/" + core + ".zip\n"
	    + "then re-run:  jsexec ../xtrn/syncivision/getcore.js");
	return 1;
}

exit(main());
