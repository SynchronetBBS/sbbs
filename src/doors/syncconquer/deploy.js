// deploy.js -- install the freshly-built SyncConquer door binaries into their
// bundles. The family has two titles built from this one tree: syncalert (Red
// Alert) and syncdawn (Tiberian Dawn). Each is deployed into its OWN xtrn
// bundle (xtrn/syncalert, xtrn/syncdawn). syncdawn is deployed only when it was
// built (build.sh without "ra"); a Red-Alert-only build just skips it.
//
// Run via jsexec AFTER build.sh / build.bat:
//
//     jsexec src/doors/syncconquer/deploy.js            (the in-tree bundle, and the live
//                                                    install beside system.ctrl_dir)
//     jsexec src/doors/syncconquer/deploy.js <dir> ...  (also deploy to these dirs)
//
// This REPLACES the old deploy.sh / deploy.bat pair. The work is in
// exec/load/door_deploy.js, shared by every door -- because the deploy step and
// whatever LAUNCHES the door must agree, exactly, on where the binary lives, and a
// shell/batch pair would have to re-derive that from `uname` (whose spellings
// differ from system.platform/system.architecture) and drift out of agreement.
//
// SyncConquer has NO lobby: xtrn.ini launches the binary directly
// (`cmd = syncalert%. %f ...`), and a fixed command line cannot probe a per-host
// sub-dir. So the binary must be FLAT. `subdir: false` -- and it stays false until
// the door grows a JS launcher that can do the probing. Two *nix hosts of different
// architecture therefore cannot share one install directory.
//
// SpiderMonkey 1.8.5: no let/const/arrows/template literals.
//
// Copyright(C) 2026 Rob Swindell. GPL-2.0.

load("door_deploy.js");

var rc = door_deploy({
	name:   "syncalert",
	srcdir: js.exec_dir,
	xtrn:   "syncalert",
	subdir: false,         /* --subdir opts in, but see the note above */
	direct_launch: true
});

/* SyncDawn (Tiberian Dawn) -- built alongside syncalert when BUILD_VANILLATD=ON
 * (the default of both build.sh and build.bat; "ra" skips it). Deploy it too when
 * present; a Red-Alert-only build produced no syncdawn, so this is a clean no-op
 * there (rather than door_deploy printing a "not found -- run build.sh" error).
 * Its own xtrn bundle (xtrn/syncdawn) and, like syncalert, a direct (non-lobby)
 * launch, so FLAT.
 *
 * Ask door_find_built() where the binary is rather than spelling out "build/" --
 * the answer is build/ on *nix but build-msvc/<Config>/ for build.bat, and
 * hardcoding the *nix one is precisely the bug that made this gate silently skip
 * SyncDawn on every Windows build. */
var td_exe = door_find_built(js.exec_dir, door_exe_name("syncdawn", system.platform));
if (td_exe !== null) {
	var rc2 = door_deploy({
		name:   "syncdawn",
		srcdir: js.exec_dir,
		xtrn:   "syncdawn",
		subdir: false,
		direct_launch: true
	});
	if (rc2 != 0)
		rc = rc2;
} else {
	print("[deploy] no syncdawn build present -- skipping SyncDawn (both titles are"
	    + " the default: ./build.sh or build.bat; \"ra\" builds Red Alert alone)");
}

exit(rc);
