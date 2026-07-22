// getdata.js -- fetch the freeware Lure of the Temptress game data for the
// synclure door.
//
// Lure of the Temptress was released as freeware by Revolution Software and
// ScummVM hosts it, so this script downloads it -- straight from ScummVM's own
// distribution point -- rather than this repository bundling copyrighted data
// or a sysop supplying a copy.
//
// Fetches lure-1.1.zip (the English build) and unpacks just the VGA disk
// resources (Disk1.vga .. Disk4.vga) and the freeware LICENSE.txt into THIS
// door directory, flattening the archive's lure/ subfolder. The EGA disks,
// Lure.exe and the PDFs the archive also carries are deliberately left out:
// ScummVM's lure engine runs the VGA data through lure.dat (see below), and
// taking VGA alone makes it detect a single game rather than the VGA/EGA pair
// the full archive would.
//
// lure.dat: the lure engine's own ScummVM engine-data file (ScummVM generates
// it from the game) is NOT part of this download -- it ships committed in this
// package directory, and the door finds it via --path.
//
// Integrity: the archive is verified before it is unpacked against the PINNED
// sha256 below -- the value ScummVM publishes alongside it -- using
// CryptContext(CryptContext.ALGO.SHA2) (cryptlib's SHA-2 context defaults to
// 256-bit output).
//
// Pinned rather than fetched at run time, which is what this did before. The
// mirror fallback means the archive can arrive from a second host, and a hash
// retrieved from whichever host served the file proves nothing about it. A pin
// in git is checked once, by a person, and an archive that changed upstream is
// REJECTED rather than silently installed.
//
// If the download fails or fails that check, exec/load/xtrn_mirror.js retries
// against Synchronet's asset mirror before giving up.
//
// Run automatically (and prompted) by the installer via install-xtrn.ini's
// [exec:getdata.js] step, or by hand any time:
//
//     jsexec ../xtrn/synclure/getdata.js
//
// Idempotent: if Disk1.vga already exists the download is skipped. SpiderMonkey
// 1.8.5 compatible (no modern ES) -- runs unchanged on Windows.
//
// Copyright (C) 2026 Rob Swindell / synclure.  GPL-2.0.

load("http.js");
load("xtrn_mirror.js");
load("sha256.js");

var BASE   = "https://downloads.scummvm.org/frs/extras/Lure%20of%20the%20Temptress";
var ZIP    = "lure-1.1.zip";
var MARKER = "Disk1.vga";
var SHA256 = "f3178245a1483da1168c3a11e70b65d33c389f1f5df63d4f3a356886c1890108";

// The door's own directory (where this script lives). js.exec_dir is the
// running SCRIPT's own dir -- correct whether launched by the installer or by
// hand via jsexec -- unlike js.startup_dir (the BBS/jsexec working dir).
function door_dir()
{
	var d = js.exec_dir || js.startup_dir || "./";
	return backslash(d);
}

function main()
{
	var root = door_dir();
	print("Lure of the Temptress (synclure) game-data fetcher");
	print("  door directory: " + root);

	if (file_exists(root + MARKER)) {
		print("game: present");
		return 0;
	}

	var url = BASE + "/" + ZIP;
	var tmp = backslash(system.temp_dir) + ZIP;

	print("downloading " + ZIP + " ...");
	var n = xtrn_mirror_download(url, tmp, {
		verify: function(p) {
			var got = sha256_of_file(p);
			if (got == SHA256)
				return true;
			print("  ! sha256 MISMATCH for " + ZIP);
			print("    expected: " + SHA256);
			print("    got:      " + got);
			return false;
		}
	});
	if (!n) {
		print("  ! could not fetch " + ZIP);
		return 1;
	}
	print("  downloaded " + n + " bytes; sha256 verified");

	print("  extracting into " + root + " ...");
	try {
		// with_path=false + trailing recurse=true: flatten the archive's lure/
		// subfolder, taking only the VGA disks and the freeware license.
		var ar = new Archive(tmp);
		var got_n = ar.extract(root, false, true, 0, "Disk*.vga", "LICENSE.txt", true);
		print("  extracted " + got_n + " file(s)");
	} catch (e) {
		print("  ! extraction failed: " + e);
		file_remove(tmp);
		return 1;
	}
	file_remove(tmp);

	if (!file_exists(root + MARKER)) {
		print("  ! extracted, but " + MARKER + " is still missing -- something's off");
		return 1;
	}
	print("game: installed");
	return 0;
}

if (typeof SYNCLURE_GETDATA_NO_MAIN == "undefined")
	exit(main());
