// deploy.js -- install the freshly-built syncmoo1 binary into the door bundle.
//
// Run via jsexec AFTER build.sh / build.bat:
//
//     jsexec src/doors/syncmoo1/deploy.js            (deploy to the live install
//                                                    beside system.ctrl_dir)
//     jsexec src/doors/syncmoo1/deploy.js <dir> ...  (also deploy to these dirs)
//
// This REPLACES the old deploy.sh / deploy.bat pair. The work is in
// exec/load/door_deploy.js, shared by every door -- because the deploy step and
// whatever LAUNCHES the door must agree, exactly, on where the binary lives, and a
// shell/batch pair would have to re-derive that from `uname` (whose spellings
// differ from system.platform/system.architecture) and drift out of agreement.
//
// SyncMOO1 has NO lobby: xtrn.ini launches the binary directly
// (`cmd = syncmoo1%. %f ...`), resolved against the door directory, and a fixed
// command line cannot probe a per-host sub-dir. So the binary must be FLAT.
// `subdir: false` -- and it stays false until the door grows a JS launcher that
// can do the probing. Two *nix hosts of different architecture therefore cannot
// share one SyncMOO1 install directory.
//
// SpiderMonkey 1.8.5: no let/const/arrows/template literals.
//
// Copyright(C) 2026 Rob Swindell. GPL-2.0.

load("door_deploy.js");

exit(door_deploy({
	name:   "syncmoo1",
	srcdir: js.exec_dir,
	xtrn:   "syncmoo1",
	subdir: false,         /* --subdir opts in, but see the note above */
	direct_launch: true
}));
