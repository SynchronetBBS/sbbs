// getcore.js -- install the FreeIntv libretro core for this (Intellivision) door.
//
// The work is in exec/load/syncretro_getcore.js, shared with every other
// SyncRetro console: only the core's name differs. FreeIntv is GPLv2+ (freely
// redistributable), so -- unlike the Intellivision BIOS (exec.bin/grom.bin) and
// the cartridges, which are content the sysop supplies -- it CAN be fetched
// automatically.
//
// Run by the installer (install-xtrn.ini's [exec:getcore.js] step), or by hand:
//     jsexec ../xtrn/syncivision/getcore.js
//
// Copyright(C) 2026 Rob Swindell / SyncRetro.  GPL-2.0.

load("syncretro_getcore.js");

exit(syncretro_getcore(js.exec_dir, "freeintv_libretro", "FreeIntv (Intellivision)"));
