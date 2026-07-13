// deploy.js -- install the freshly-built syncduke binary into the door bundle.
//
// Run via jsexec AFTER build.sh / build.bat:
//
//     jsexec src/doors/syncduke/deploy.js            (deploy to the live install
//                                                    beside system.ctrl_dir)
//     jsexec src/doors/syncduke/deploy.js <dir> ...  (also deploy to these dirs)
//
// This REPLACES the old deploy.sh / deploy.bat pair. The work is in
// exec/load/door_deploy.js, shared by every door -- because the deploy step and
// whatever LAUNCHES the door must agree, exactly, on where the binary lives, and a
// shell/batch pair would have to re-derive that from `uname` (whose spellings
// differ from system.platform/system.architecture) and drift out of agreement.
//
// SyncDuke is launched BOTH ways: `?lobby` (which probes the per-target sub-dir)
// and a direct single-player xtrn.ini entry (which cannot). So `subdir: true`
// deploys to both -- the sub-dir a multi-*nix-host install needs, and the flat copy
// the direct entry needs.
//
// SpiderMonkey 1.8.5: no let/const/arrows/template literals.
//
// Copyright(C) 2026 Rob Swindell. GPL-2.0.

load("door_deploy.js");

exit(door_deploy({
	name:   "syncduke",
	srcdir: js.exec_dir,
	xtrn:   "syncduke",
	subdir: false          /* --subdir opts in; see door_deploy.js */
}));
