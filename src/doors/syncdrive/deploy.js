// deploy.js -- install the freshly-built syncdrive binary into the door dir.
//
//     jsexec src/doors/syncdrive/deploy.js
//
// xtrn.ini launches the binary directly (see xtrn/testdrive/install-xtrn.ini),
// so it must be FLAT in the door dir. SpiderMonkey 1.8.5.
//
// Copyright(C) 2026 Rob Swindell. GPL-2.0.

load("door_deploy.js");

exit(door_deploy({
	name:   "syncdrive",
	srcdir: js.exec_dir,
	xtrn:   "testdrive",
	subdir: false,
	direct_launch: true
}));
