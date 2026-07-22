// getwads.js -- download the free Freedoom WAD set for SyncDOOM.
//
// SyncDOOM ships no game data (WADs are content, supplied by the sysop). The
// commercial doom.wad / doom2.wad must be provided by hand, but Freedoom and
// FreeDM are freely redistributable, so this fetches them so the door is
// playable out of the box. Run automatically (and prompted) by the installer
// via install-xtrn.ini's [exec:getwads.js] step, or by hand:
//
//     jsexec ../xtrn/syncdoom/getwads.js
//
// Idempotent: any WAD already present in the wads dir is left alone, so
// re-running it only fetches what's missing. A download/extract failure is
// reported but non-fatal -- the sysop can always drop WADs in by hand.
//
// Integrity: each release zip is verified against the PINNED sha256 on its set
// below before anything is extracted. This fetcher checked nothing at all
// previously, which was tolerable while GitHub was the only source; with the
// mirror fallback in play an unverified download would accept whatever either
// host handed it, so adding a second source would have weakened it rather than
// strengthened it. An archive that changed upstream is now REJECTED instead of
// silently installed.
//
// If a download fails or fails that check, exec/load/xtrn_mirror.js retries
// against Synchronet's asset mirror before giving up.
//
// Downloads stream straight to disk (HTTPRequest.Download in load/http.js), so
// the ~24 MB / ~11 MB archives are never buffered in memory. SpiderMonkey
// 1.8.5-compatible (no modern ES).
//
// Copyright(C) 2026 Rob Swindell / syncdoom. GPL-2.0.

load("http.js");
load("xtrn_mirror.js");

// Hex-encode a raw byte string (e.g. CryptContext.hashvalue).
function hexify(s)
{
	var out = "", i, c;

	for (i = 0; i < s.length; i++) {
		c = s.charCodeAt(i) & 0xff;
		out += (c < 16 ? "0" : "") + c.toString(16);
	}
	return out;
}

// SHA-256 of a file's contents, streamed in chunks so a 24 MB archive is never
// buffered twice. CryptContext(CryptContext.ALGO.SHA2) defaults to 256-bit
// output; File.read() returns a byte-per-char string that round-trips through
// encrypt() losslessly, high bit and embedded NULs included.
function sha256_of_file(path)
{
	var f = new File(path);
	var cc, chunk;

	if (!f.open("rb"))
		return null;
	cc = new CryptContext(CryptContext.ALGO.SHA2);
	try {
		while ((chunk = f.read(65536)) != null && chunk.length > 0)
			cc.encrypt(chunk);
	} finally {
		f.close();
	}
	cc.encrypt("");   // empty final call: finalizes the hash (cryptlib convention)
	return hexify(cc.hashvalue);
}

// Pinned Freedoom release. Bump VER on a new release (the asset names and the
// in-zip top-level directory both follow the freedoom-<VER>/ , freedm-<VER>/
// pattern). https://github.com/freedoom/freedoom/releases
var VER  = "0.13.0";
var BASE = "https://github.com/freedoom/freedoom/releases/download/v" + VER + "/";

// Each release zip and the WAD(s) to pull out of it (member = the path inside
// the zip; Archive.extract flattens it to the basename in the destination).
var SETS = [
	{
		zip: "freedoom-" + VER + ".zip",
		sha256: "3f9b264f3e3ce503b4fb7f6bdcb1f419d93c7b546f4df3e874dd878db9688f59",
		wads: [
			{ name: "freedoom1.wad", member: "freedoom-" + VER + "/freedoom1.wad" },
			{ name: "freedoom2.wad", member: "freedoom-" + VER + "/freedoom2.wad" }
		]
	},
	{
		zip: "freedm-" + VER + ".zip",
		sha256: "b420f13508ef745d7b38e83d15e55e0fc0b09d9a503c96741cddd9773d43f7c9",
		wads: [
			{ name: "freedm.wad", member: "freedm-" + VER + "/freedm.wad" }
		]
	}
];

// The door's directory (where syncdoom.ini and this script live). js.exec_dir is
// the running SCRIPT's own directory (xtrn/syncdoom), whether launched by the
// installer or by hand with jsexec -- unlike js.startup_dir, which is the BBS/
// jsexec working dir. Fall back to startup_dir only if exec_dir is unset.
function door_dir()
{
	var d = js.exec_dir || js.startup_dir || "./";
	return backslash(d);   // ensure a trailing path separator
}

// True if a path is absolute (POSIX "/...", or Windows "X:\..." / "\...").
function is_abs(p)
{
	return p.charAt(0) == "/" || p.charAt(0) == "\\" || /^[A-Za-z]:/.test(p);
}

// Resolve the WAD directory from [wads] dir in syncdoom.ini (default "wads"),
// relative to the door dir unless absolute. Created if missing.
function wad_dir()
{
	var base = door_dir();
	var sub  = "wads";
	var f = new File(base + "syncdoom.ini");
	if (f.open("r")) {
		var o = f.iniGetObject("wads");
		f.close();
		if (o && o.dir !== undefined && String(o.dir).length)
			sub = String(o.dir);
	}
	var dir = is_abs(sub) ? sub : (base + sub);
	dir = backslash(dir);
	if (!file_isdir(dir))
		mkpath(dir);
	return dir;
}

function main()
{
	var wads = wad_dir();
	print("SyncDOOM: WAD directory is " + wads);

	var any_missing = false;
	var ok = true;
	var s, w, m;

	for (s = 0; s < SETS.length; s++) {
		var set = SETS[s];

		// Which of this zip's WADs are not already installed?
		var missing = [];
		for (w = 0; w < set.wads.length; w++) {
			if (file_exists(wads + set.wads[w].name))
				print("  " + set.wads[w].name + " already present -- skipping");
			else
				missing.push(set.wads[w]);
		}
		if (!missing.length)
			continue;
		any_missing = true;

		var url    = BASE + set.zip;
		var tmpzip = backslash(system.temp_dir) + set.zip;
		print("  downloading " + set.zip + " (" + url + ") ...");

		try {
			// xtrn_mirror_download() follows redirects (GitHub sends asset
			// URLs to a CDN) and, if the pinned sha256 does not match,
			// treats that exactly like an unreachable host: it retries
			// against the mirror rather than accepting the bytes.
			var n = xtrn_mirror_download(url, tmpzip, {
				verify: function(p) {
					var got = sha256_of_file(p);
					if (got == set.sha256)
						return true;
					print("  ! sha256 MISMATCH for " + set.zip);
					print("    expected: " + set.sha256);
					print("    got:      " + got);
					return false;
				}
			});
			if (!n) {
				print("  ! could not fetch " + set.zip);
				ok = false;
				continue;
			}
			print("  downloaded " + n + " bytes; sha256 verified; extracting ...");
			var ar = new Archive(tmpzip);
			for (m = 0; m < missing.length; m++) {
				ar.extract(wads, missing[m].member);
				if (file_exists(wads + missing[m].name))
					print("  installed " + missing[m].name);
				else {
					print("  ! failed to extract " + missing[m].name);
					ok = false;
				}
			}
		} catch (e) {
			print("  ! error fetching " + set.zip + ": " + e);
			ok = false;
		} finally {
			if (file_exists(tmpzip))
				file_remove(tmpzip);
		}
	}

	if (!any_missing)
		print("SyncDOOM: all Freedoom WADs already present -- nothing to download.");
	else if (ok)
		print("SyncDOOM: Freedoom WAD set installed.");
	else
		print("SyncDOOM: some WADs could not be installed -- add them by hand "
		    + "(see the [wads] section of syncdoom.ini).");

	return ok ? 0 : 1;
}

if (typeof SYNCDOOM_GETWADS_NO_MAIN == "undefined")
	exit(main());
