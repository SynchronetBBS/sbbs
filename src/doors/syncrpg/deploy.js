// deploy.js -- install the freshly-built syncrpg binary into the door bundle.
//
// Run via jsexec AFTER build.sh / build.bat:
//
//     jsexec src/doors/syncrpg/deploy.js            (deploy to the live install
//                                                   beside system.ctrl_dir)
//     jsexec src/doors/syncrpg/deploy.js <dir> ...  (also deploy to these dirs)
//
// This mirrors src/doors/syncmoo1/deploy.js: the work is in
// exec/load/door_deploy.js, shared by every door -- because the deploy step
// and whatever LAUNCHES the door must agree, exactly, on where the binary
// lives, and a shell/batch pair would have to re-derive that from `uname`
// (whose spellings differ from system.platform/system.architecture) and
// drift out of agreement.
//
// SyncRPG is a single-flagship door (Yume Nikki, v1): xtrn.ini launches the
// binary directly (`cmd = syncrpg%. %f ...`, see xtrn/yumenikki/install-xtrn.ini),
// resolved against the door directory, and a fixed command line cannot probe
// a per-host sub-dir. So the binary must be FLAT. `subdir: false` -- and it
// stays false until the door grows a JS launcher that can do the probing
// (the way SyncSCUMM's discovery-based deploy.js does once more than one
// EasyRPG title ships).
//
// SpiderMonkey 1.8.5: no let/const/arrows/template literals.
//
// Copyright(C) 2026 Rob Swindell / SyncRPG. GPL-2.0.

load("door_deploy.js");

exit(door_deploy({
	name:   "syncrpg",
	srcdir: js.exec_dir,
	xtrn:   "yumenikki",
	subdir: false,         /* --subdir opts in, but see the note above */
	direct_launch: true
}));
