// download.js -- download game content (the base GRP) for SyncDuke.
//
// SyncDuke ships no game data (the GRP is content, supplied by the sysop). The
// Full 1.3D / Plutonium 1.4 / Atomic 1.5 GRPs you own must be provided by hand,
// but the shareware Episode 1 (DUKE3D.GRP, 11,035,779 bytes, "SHAREWARE 1.3D")
// is freely redistributable, so this fetches it so the door is playable out of
// the box. Run automatically (and prompted) by the installer via
// install-xtrn.ini's [exec:download.js] step, or by hand:
//
//     jsexec ../xtrn/syncduke/download.js
//
// Idempotent: if DUKE3D.GRP is already present in the GRP dir it is left alone,
// so re-running it does nothing. A download/extract failure is reported but
// non-fatal -- the sysop can always drop a GRP in by hand.
//
// The download is the original 3D Realms shareware *installer* package, not a
// bare GRP, so the GRP is two archive layers deep:
//
//     3dduke13.zip  ->  DN3DSW13.SHR  ->  DUKE3D.GRP
//      (outer zip)      (MZ/PKLITE self-extractor, itself a ZIP)
//
// Synchronet's Archive (libarchive) reads both layers, so we extract the inner
// .SHR from the outer zip, then the GRP from the .SHR. The download streams
// straight to disk (HTTPRequest.Download in load/http.js), never buffered in
// memory. SpiderMonkey 1.8.5-compatible (no modern ES).
//
// Copyright(C) 2026 Rob Swindell / syncduke. GPL-2.0.

load("http.js");

// The shareware install package. The outer zip's inner self-extractor and the
// GRP inside it have fixed names; GRP_SIZE is the canonical shareware 1.3D GRP
// length, used as a post-extract sanity check.
var PKG_URL  = "https://dnr.duke4.net/dl/024fbc5/3dduke13.zip";
var SHR      = "DN3DSW13.SHR";    // member of the outer zip (a self-extracting ZIP)
var GRP      = "DUKE3D.GRP";      // member of the .SHR -- what we want
var GRP_SIZE = 11035779;          // SHAREWARE 1.3D

// The door's directory (where syncduke.ini and this script live). js.exec_dir is
// the running SCRIPT's own directory (xtrn/syncduke), whether launched by the
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

// Resolve the GRP directory from [grp] dir in syncduke.ini (default: the door's
// own dir, i.e. drop the GRP beside the binary), relative to the door dir unless
// absolute. Created if missing.
function grp_dir()
{
	var base = door_dir();
	var sub  = "";
	var f = new File(base + "syncduke.ini");
	if (f.open("r")) {
		var o = f.iniGetObject("grp");
		f.close();
		if (o && o.dir !== undefined && String(o.dir).length)
			sub = String(o.dir);
	}
	var dir = !sub.length ? base : (is_abs(sub) ? sub : (base + sub));
	dir = backslash(dir);
	if (!file_isdir(dir))
		mkpath(dir);
	return dir;
}

function main()
{
	var dir = grp_dir();
	print("SyncDuke: GRP directory is " + dir);

	if (file_exists(dir + GRP)) {
		print("  " + GRP + " already present -- nothing to download.");
		return 0;
	}

	var tmpzip = backslash(system.temp_dir) + "3dduke13.zip";
	var tmpshr = backslash(system.temp_dir) + SHR;
	var ok = false;

	print("  downloading the shareware install package (" + PKG_URL + ") ...");
	try {
		var req = new HTTPRequest();
		req.follow_redirects = 5;
		var n = req.Download(PKG_URL, tmpzip);
		if (req.response_code != 200) {
			print("  ! download failed: HTTP " + req.response_code);
			return 1;
		}
		print("  downloaded " + n + " bytes; extracting " + SHR + " ...");

		// Layer 1: pull the inner self-extractor out of the outer zip.
		new Archive(tmpzip).extract(backslash(system.temp_dir), SHR);
		if (!file_exists(tmpshr)) {
			print("  ! could not extract " + SHR + " from the package");
			return 1;
		}

		// Layer 2: pull the GRP out of the self-extractor (itself a ZIP).
		print("  extracting " + GRP + " ...");
		new Archive(tmpshr).extract(dir, GRP);
		if (!file_exists(dir + GRP)) {
			print("  ! could not extract " + GRP);
			return 1;
		}

		var sz = file_size(dir + GRP);
		if (sz != GRP_SIZE)
			print("  ! warning: " + GRP + " is " + sz + " bytes (expected "
			    + GRP_SIZE + ") -- the file may be corrupt");
		else
			print("  installed " + GRP + " (" + sz + " bytes)");
		ok = true;
	} catch (e) {
		print("  ! error fetching the GRP: " + e);
	} finally {
		if (file_exists(tmpzip))
			file_remove(tmpzip);
		if (file_exists(tmpshr))
			file_remove(tmpshr);
	}

	if (ok)
		print("SyncDuke: shareware DUKE3D.GRP installed.");
	else
		print("SyncDuke: could not install the GRP -- supply one by hand "
		    + "(see the [grp] section of syncduke.ini).");
	return ok ? 0 : 1;
}

exit(main());
