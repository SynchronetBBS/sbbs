// fetch-assets.js -- download the freeware Red Alert game data for SyncAlert.
//
// SyncAlert ships no game data. Electronic Arts released Command & Conquer:
// Red Alert as freeware on 2008-08-31, so this fetches the official RA95
// freeware CD image from archive.org, verifies its checksum, and extracts just
// the two MIX archives the engine needs (REDALERT.MIX + MAIN.MIX; the rest of
// the CD image isn't extracted). The door plays the game's FMV cutscenes --
// video plus A/V-synced audio, from these mixes via door/vqaaudio_termgfx.cpp.
// Run automatically (and prompted) by the installer via install-xtrn.ini's
// [exec:fetch-assets.js] step, or by hand:
//
//     jsexec ../xtrn/syncalert/fetch-assets.js
//
// Idempotent: if both MIX files are already present in the assets dir they are
// left alone, so re-running it does nothing. A download/extract failure is
// reported but non-fatal -- the sysop can always drop the MIX files in by hand.
//
// The download streams straight to disk (HTTPRequest.Download in load/http.js)
// and the checksum is computed streaming (File.sha1_hex), so the ~556 MB image
// is never buffered in memory. Synchronet's Archive object (libarchive) reads
// ISO9660 directly, so no bsdtar/mount is needed and this works on Windows too.
// SpiderMonkey 1.8.5-compatible (no modern ES).
//
// Copyright(C) 2026 Rob Swindell / syncalert. GPL-2.0.

load("http.js");

// The RA95 freeware CD1 image. SHA-1 rather than SHA-256 because SHA-1 is what
// Synchronet's JS File object can compute (File.sha1_hex, streamed).
//
// http:// (not https://) is deliberate and required: archive.org serves a
// certificate chain ending in the legacy 1024-bit "Go Daddy Class 2"
// cross-signed root, which cryptlib rejects ("Invalid certificate chain entry
// #3"), tearing down the handshake before any response -- HTTPRequest then
// throws "Unable to read status". The plaintext request is only the redirect
// lookup: archive.org 302s to an https:// storage node (whose chain cryptlib
// accepts) and the image itself is fetched over TLS. The bytes are verified
// against ISO_SHA1 below regardless, so integrity does not rest on the
// transport either way. Use the canonical /download/ URL -- the storage-node
// hostnames it redirects to (dn######.ca.archive.org) are not stable.
var ISO_URL  = "http://archive.org/download/CCRedAlert/CD1.iso";
var ISO_SHA1 = "4f347c38d087faf8ae28636d8a56e56c748eb8dd";
var ISO_FILE = "ra95-cd1.iso";

// What we want out of the image. member = the path inside the ISO;
// Archive.extract flattens it to the basename in the destination dir.
var MIXES = [
	{ name: "REDALERT.MIX", member: "INSTALL/REDALERT.MIX", size: 25046328 },
	{ name: "MAIN.MIX",     member: "MAIN.MIX",             size: 454605294 }
];

// The door's directory (where the binary and this script live). js.exec_dir is
// the running SCRIPT's own directory (xtrn/syncalert), whether launched by the
// installer or by hand with jsexec -- unlike js.startup_dir, which is the BBS/
// jsexec working dir. Fall back to startup_dir only if exec_dir is unset.
function door_dir()
{
	var d = js.exec_dir || js.startup_dir || "./";
	return backslash(d);   // ensure a trailing path separator
}

// door_io.c's door_resolve_assets_dir() looks for "<the binary's own dir>/
// assets", and the binary is deployed beside this script -- so the assets dir
// is simply assets/ under the door dir. Created if missing.
function assets_dir()
{
	var dir = backslash(door_dir() + "assets");
	if (!file_isdir(dir))
		mkpath(dir);
	return dir;
}

// Streamed SHA-1 of a file, or undefined if it can't be opened.
function sha1_of(fname)
{
	var f = new File(fname);
	if (!f.open("rb"))
		return undefined;
	var hash = f.sha1_hex;
	f.close();
	return hash;
}

function main()
{
	var dir = assets_dir();
	print("SyncAlert: assets directory is " + dir);

	var missing = [];
	for (var i = 0; i < MIXES.length; i++) {
		if (file_exists(dir + MIXES[i].name))
			print("  " + MIXES[i].name + " already present -- skipping");
		else
			missing.push(MIXES[i]);
	}
	if (!missing.length) {
		print("SyncAlert: Red Alert game data already installed -- nothing to do.");
		return 0;
	}

	// Downloaded into the door dir (not system.temp_dir): the image is ~556 MB
	// and temp_dir is often a small tmpfs or per-node scratch dir. .gitignore
	// covers *.iso here. A previous run's image is reused rather than re-fetched.
	var iso = door_dir() + ISO_FILE;
	var part = iso + ".part";
	var ok = false;

	try {
		if (file_exists(iso)) {
			print("  reusing the previously downloaded " + ISO_FILE);
		} else {
			print("  downloading the RA95 freeware CD image (~556 MB) from "
			    + ISO_URL + " ...");
			print("  (this will take a while)");
			var req = new HTTPRequest();
			req.follow_redirects = 5;   // archive.org redirects to a storage node
			var n = req.Download(ISO_URL, part);
			if (req.response_code != 200) {
				print("  ! download failed: HTTP " + req.response_code);
				return 1;
			}
			// Only name it ISO_FILE once it's complete, so an interrupted
			// download is never mistaken for a reusable image on the next run.
			if (!file_rename(part, iso)) {
				print("  ! could not rename " + part + " to " + iso);
				return 1;
			}
			print("  downloaded " + n + " bytes");
		}

		print("  verifying checksum ...");
		var hash = sha1_of(iso);
		if (hash != ISO_SHA1) {
			print("  ! checksum mismatch (got " + hash + ", expected " + ISO_SHA1
			    + ") -- refusing to use " + ISO_FILE);
			print("  ! delete " + iso + " and re-run to download it again");
			return 1;
		}

		print("  extracting the MIX archives ...");
		var ar = new Archive(iso);
		for (var m = 0; m < missing.length; m++) {
			ar.extract(dir, missing[m].member);
			var path = dir + missing[m].name;
			if (!file_exists(path)) {
				print("  ! failed to extract " + missing[m].name);
				return 1;
			}
			var sz = file_size(path);
			if (sz != missing[m].size)
				print("  ! warning: " + missing[m].name + " is " + sz
				    + " bytes (expected " + missing[m].size
				    + ") -- the file may be corrupt");
			else
				print("  installed " + missing[m].name + " (" + sz + " bytes)");
			// The game data is read-only content; make sure it stays readable
			// by the BBS user even if the door runs under a different account.
			file_chmod(path, 0444);
		}
		ok = true;
	} catch (e) {
		print("  ! error fetching the Red Alert assets: " + e);
	} finally {
		if (file_exists(part))
			file_remove(part);   // aborted mid-download
	}

	if (ok) {
		// ~556 MB of CD image we no longer need. Kept on failure so a re-run
		// doesn't have to download it a second time.
		if (file_remove(iso))
			print("  removed " + ISO_FILE + " (reclaimed ~556 MB)");
		print("SyncAlert: freeware Red Alert game data installed.");
	} else {
		print("SyncAlert: could not install the game data -- copy REDALERT.MIX "
		    + "and MAIN.MIX into " + dir + " by hand.");
	}
	return ok ? 0 : 1;
}

exit(main());
