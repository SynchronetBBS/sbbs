// syncretro_getcore.js -- install a libretro core for a SyncRetro console door.
//
// Shared by every console install: each one ships a three-line getcore.js that
// names ITS core and calls syncretro_getcore(). Forking 180 lines of downloader
// per console would be absurd -- only the core's name differs.
//
//     load("syncretro_getcore.js");
//     syncretro_getcore(js.exec_dir, "fceumm_libretro", "FCEUmm (NES)");
//
// LOCAL-FIRST: use a core already installed, or one the sysop dropped into the
// door directory (a loose file or an archive), and only DOWNLOAD from the
// libretro buildbot when neither is present.
//
// The cores are GPL (freely redistributable), so -- unlike a console BIOS or the
// cartridges, which are content the sysop supplies -- a core CAN be fetched
// automatically. It is a platform-specific shared library, installed into the
// SAME place the door binary lives (syncretro_target()): FLAT at the door root on
// Windows, or in an <os>-<arch> sub-directory on *nix, so one shared install can
// serve several hosts without their same-named cores colliding:
//
//   <door>/fceumm_libretro.dll            (Windows -- flat; .dll disambiguates)
//   <door>/darwin-arm64/fceumm_libretro.dylib
//   <door>/linux-x64/fceumm_libretro.so   (and linux-arm64/, freebsd-x64/, ...)
//
// A Win32 door MUST load a 32-bit (windows/x86) core regardless of the host's
// bitness, so the download always fetches the x86 Windows build. The lobby picks
// the same location + filename, so whatever this installs is what the door loads.
//
// Idempotent and non-fatal: an already-present core is left alone, and a
// download/extract failure is reported but never aborts the install -- the sysop
// can always drop the core into the door dir by hand. Downloads stream straight
// to disk (HTTPRequest.Download in load/http.js), so the archive is never
// buffered in memory. SpiderMonkey 1.8.5-compatible (no modern ES).
//
// Copyright(C) 2026 Rob Swindell / SyncRetro.  GPL-2.0.

load("http.js");
load("xtrn_mirror.js");
load("syncretro_lib.js");   // syncretro_platform(), syncretro_core_ext(), syncretro_target()

// libretro nightly buildbot. The per-platform core zip contains just the core
// library at the top level (e.g. fceumm_libretro.dll), so extracting by its
// syncretro_basename drops it straight into the destination.
var SYNCRETRO_BUILDBOT = "https://buildbot.libretro.com/nightly/";

// Archive extensions we'll look inside for a dropped-in core (libarchive-backed
// Archive reads all of these). The buildbot ships a plain .zip.
var SYNCRETRO_ARCHIVE_EXT = [".zip", ".7z", ".rar", ".tar", ".tar.gz", ".tgz", ".tar.xz",
                   ".tar.bz2", ".gz", ".xz", ".bz2"];

// WHICH HOST ARE WE INSTALLING FOR? Not necessarily this one.
//
// A BBS can span machines that share one install directory over a mount -- a
// Linux box and a Windows box both serving /sbbs is the case this door was built
// for -- and each needs its OWN core: a .so for one, a .dll for the other, and
// they sit side by side. But this script only ever knew about the host it was
// RUNNING on, so a sysop who ran it on Linux installed the .so, saw "core
// installed", and left the Windows half of his BBS with a door that could not
// start. (That is not hypothetical: it is exactly how the NES door came to be
// broken on the Windows node of the BBS this was written on.)
//
// So the platform is overridable:  jsexec getcore.js -platform win32
// Accepted: win32, linux, darwin/macos, freebsd  (+ -arch x64|x86|arm64).
function syncretro_getcore_platform()
{
	var i;

	for (i = 0; i < argc - 1; i++)
		if (argv[i] == "-platform" || argv[i] == "--platform")
			return String(argv[i + 1]);
	return system.platform;
}

function syncretro_getcore_arch()
{
	var i;

	for (i = 0; i < argc - 1; i++)
		if (argv[i] == "-arch" || argv[i] == "--arch")
			return String(argv[i + 1]);
	return system.architecture;
}

// The libretro buildbot sub-path for a host. Windows is pinned to windows/x86
// (the door is Win32); *nix/macOS follow the architecture.
function syncretro_buildbot_sub()
{
	var p   = syncretro_getcore_platform();
	var a   = String(syncretro_getcore_arch() || "").toLowerCase();
	var b64 = /64/.test(a);            // x86_64 / amd64 / x64 / aarch64
	var arm = /arm|aarch/.test(a);

	if (/^win/i.test(p))
		return "windows/x86";                       // door is Win32
	if (/darwin|mac|osx/i.test(p))
		return "apple/osx/" + (arm ? "arm64" : "x86_64");
	return "linux/" + (b64 ? "x86_64" : "x86");
}

// Basename of a path (handles both separators; an archive may use '/').
function syncretro_basename(p)
{
	var s = String(p).replace(/\\/g, "/");
	var i = s.lastIndexOf("/");
	return i >= 0 ? s.substr(i + 1) : s;
}

// Pull `core` out of the archive at `path` into `dstdir`. Matches the member by
// syncretro_basename, case-insensitively (a sysop-made zip may nest or re-case it), and
// renames the extracted file to the canonical lower-case core name if needed.
// Returns true iff the core ends up present in `dstdir`.
function syncretro_extract_core_from(path, dstdir, core)
{
	var i, names, base, entry;

	try {
		var ar = new Archive(path);
		names = ar.list();                         // array of { name, type, ... }
		for (i = 0; i < names.length; i++) {
			entry = names[i];
			if (entry.type && entry.type != "file")
				continue;
			base = syncretro_basename(entry.name);
			if (base.toLowerCase() != core)
				continue;
			ar.extract(dstdir, entry.name);        // flattens to the syncretro_basename
			if (base != core && file_exists(dstdir + base) && !file_exists(dstdir + core))
				file_rename(dstdir + base, dstdir + core);
			if (file_exists(dstdir + core)) {
				print("  installed " + core + " from " + syncretro_basename(path));
				return true;
			}
		}
	} catch (e) {
		print("  ! " + syncretro_basename(path) + ": " + e);
	}
	return false;
}

// Try every archive the sysop may have dropped into the door root, extracting
// the core into `dstdir`. The common case is the buildbot's own
// <core>_libretro.<ext>.zip dropped in as-is.
function syncretro_from_dropped_archive(root, dstdir, core)
{
	var list = directory(root + "*");
	var i, j, lower;

	for (i = 0; i < list.length; i++) {
		lower = list[i].toLowerCase();
		for (j = 0; j < SYNCRETRO_ARCHIVE_EXT.length; j++) {
			if (lower.length >= SYNCRETRO_ARCHIVE_EXT[j].length
			    && lower.substr(lower.length - SYNCRETRO_ARCHIVE_EXT[j].length) == SYNCRETRO_ARCHIVE_EXT[j]) {
				if (syncretro_extract_core_from(list[i], dstdir, core))
					return true;
				break;
			}
		}
	}
	return false;
}

// A loose core file the sysop dropped in (at the door root or already in the
// platform sub-dir), possibly under a different case. Move/rename it into place.
function syncretro_from_loose_file(root, dstdir, core, base)
{
	var i, hit = directory(root + base + ".*").concat(
	             directory(dstdir + base + ".*"));

	// Any hit is in the wrong place or the wrong case (an exact dstdir/core was
	// already caught by main()'s step 1), so move it to the canonical target.
	for (i = 0; i < hit.length; i++) {
		if (syncretro_basename(hit[i]).toLowerCase() != core)
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
function syncretro_from_buildbot(dstdir, core, sub)
{
	var url = SYNCRETRO_BUILDBOT + sub + "/latest/" + core + ".zip";
	var tmp = backslash(system.temp_dir) + core + ".zip";
	var ok  = false;

	// The buildbot serves every platform's build under the same name, so the
	// mirrored copy is platform-qualified to coexist in one flat directory:
	// fceumm_libretro.so-linux-x86_64.zip, and so on. `core` already carries
	// the .so/.dll extension by this point (see the caller), which is why the
	// mirror name keeps it too.
	//
	// Only the platforms the mirror actually stocks will resolve; anywhere else
	// the fallback 404s and the buildbot failure stands, as before. No checksum
	// is pinned because these are NIGHTLY builds -- what the mirror holds is a
	// snapshot for when the buildbot is unreachable, not a substitute for it.
	var mirror_name = core + "-" + sub.replace(/\//g, "-") + ".zip";

	print("  downloading " + core + " (" + url + ") ...");
	try {
		var n = xtrn_mirror_download(url, tmp, { name: mirror_name });
		if (!n) {
			print("  ! could not fetch " + core);
		} else {
			print("  downloaded " + n + " bytes; extracting ...");
			ok = syncretro_extract_core_from(tmp, dstdir, core);
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

// `door_dir` is the console's install dir (its getcore.js passes js.exec_dir).
// `base` is the core's filename without the extension ("fceumm_libretro").
// `label` is what the sysop is told we are installing ("FCEUmm (NES)").
function syncretro_getcore(door_dir, base, label)
{
	var root   = backslash(door_dir || js.startup_dir || "./");
	var plat   = syncretro_getcore_platform();       // this host, or -platform <name>
	var arch   = syncretro_getcore_arch();
	var target = syncretro_target(plat, arch);       // "" (flat, Windows), linux-x64, ...
	var core   = base + "." + syncretro_core_ext(syncretro_platform(plat));
	var dst    = target ? backslash(root + target) : root;          // <door>/<target>/ or the flat door dir
	var where  = target ? (target + "/") : "the door dir";
	var sub    = syncretro_buildbot_sub();

	if (!label)
		label = base;
	if (plat != system.platform)
		print("SyncRetro: installing for " + plat + " (not this host) -- "
		    + "a shared install serving several hosts needs a core for each.");

	// 1. Already installed?
	if (file_exists(dst + core)) {
		print("SyncRetro: " + core + " is already present in " + where + " -- nothing to do.");
		return 0;
	}

	print("SyncRetro: installing the " + label + " core (" + core + ") into " + dst);
	mkpath(dst);

	// 2. A copy the sysop dropped in (loose file, or an archive at the door root).
	if (syncretro_from_loose_file(root, dst, core, base))
		return 0;
	if (syncretro_from_dropped_archive(root, dst, core))
		return 0;

	// 3. Download from the libretro buildbot (the cores are GPL).
	if (syncretro_from_buildbot(dst, core, sub)) {
		print("SyncRetro: " + label + " core installed.");
		return 0;
	}

	print("SyncRetro: could not install the core automatically. Drop "
	    + core + " (or its buildbot .zip) into " + root + " -- it is at\n"
	    + "  " + SYNCRETRO_BUILDBOT + sub + "/latest/" + core + ".zip\n"
	    + "then re-run this door's getcore.js");
	return 1;
}
