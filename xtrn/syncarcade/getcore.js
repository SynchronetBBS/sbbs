// getcore.js -- install the MAME 2003-Plus libretro core for this (arcade) door.
//
// The work is in exec/load/syncretro_getcore.js, shared with every other
// SyncRetro console: only the core's name differs.
//
// LICENCE NOTE, and it differs from the sibling consoles. FreeIntv and FCEUmm are
// GPL, so their getcore.js can say "the cores are GPL" and leave it there. MAME
// 2003-Plus is under the MAME licence, which is NOT GPL and restricts commercial
// redistribution. Nothing here redistributes it: this script downloads it, at the
// sysop's prompting, from the libretro buildbot -- the same thing a sysop would do
// by hand. A BBS that charges for access should read the licence first.
//
// Run by the installer (install-xtrn.ini's [exec:getcore.js] step), or by hand:
//     jsexec ../xtrn/syncarcade/getcore.js
//
// Copyright(C) 2026 Rob Swindell / SyncRetro.  GPL-2.0.

load("syncretro_getcore.js");

exit(syncretro_getcore(js.exec_dir, "mame2003_plus_libretro", "MAME 2003-Plus (arcade)"));
