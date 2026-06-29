// get-binary.js -- ensure the SyncDuke executable is present before the door is installed.
//
// SyncDuke's binary is NOT shipped in the Synchronet package (size + the GPL source-
// distribution obligation; the source lives in the public repo under src/doors/syncduke).
// The installer runs this as install-xtrn.ini's [pre-exec:get-binary.js] with required=true,
// so a non-zero return ABORTS the install -- i.e. the door won't register unless its
// executable (or a symlink to it) is present in this directory.
//
//   - Windows: if syncduke.exe is missing, download it from a well-known URL on Vertrauen
//     (an alias to the current versioned build) and extract it here.
//   - *nix: the binary is produced by build.sh and placed/symlinked here by deploy.sh; this
//     script does not auto-download it, it just verifies it's present.
//
// Run by hand:  jsexec ../xtrn/syncduke/get-binary.js
//
// Copyright(C) 2026 Rob Swindell / syncduke. GPL-2.0.

load("http.js");

// Alias on synchro.net to the current versioned Win32 build (e.g. syncduke-0.1.zip), so the
// installer never needs to know the version number.
var ZIP_URL = "http://synchro.net/Synchronet/syncduke.zip";

// This script's own directory (where install-xtrn.ini / the binary live), trailing-slashed.
function door_dir()
{
	return backslash(js.exec_dir || js.startup_dir || "./");
}

function main()
{
	var dir = door_dir();
	var win = /^win/i.test(system.platform);
	var exe = win ? "syncduke.exe" : "syncduke";

	if (file_exists(dir + exe)) {
		print("SyncDuke: " + exe + " is present.");
		return 0;
	}

	if (!win) {
		print("SyncDuke: '" + exe + "' is missing. Build it (./build.sh in src/doors/syncduke) "
		    + "and install it here (./deploy.sh), then re-run the installer.");
		return 1;
	}

	// Windows: fetch the zip and extract the exe beside this script.
	var tmp = backslash(system.temp_dir) + "syncduke.zip";
	print("SyncDuke: " + exe + " not found -- downloading " + ZIP_URL + " ...");
	try {
		var req = new HTTPRequest();
		req.follow_redirects = 5;
		var n = req.Download(ZIP_URL, tmp);
		if (req.response_code != 200)
			print("  ! download failed: HTTP " + req.response_code);
		else {
			print("  downloaded " + n + " bytes; extracting " + exe + " ...");
			new Archive(tmp).extract(dir, exe);
		}
	} catch (e) {
		print("  ! error fetching " + exe + ": " + e);
	} finally {
		if (file_exists(tmp))
			file_remove(tmp);
	}

	if (file_exists(dir + exe)) {
		print("SyncDuke: installed " + exe + ".");
		return 0;
	}
	print("SyncDuke: could not obtain '" + exe + "'. Build it from src/doors/syncduke or place "
	    + "it in " + dir + " by hand, then re-run the installer.");
	return 1;
}

exit(main());
