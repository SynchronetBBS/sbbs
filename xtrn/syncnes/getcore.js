// getcore.js -- install the FCEUmm libretro core for this (NES) door.
//
// The work is in exec/load/syncretro_getcore.js, shared with every other
// SyncRetro console: only the core's name differs. FCEUmm is GPLv2 (freely
// redistributable), so -- unlike the cartridges, which the sysop supplies -- it
// CAN be fetched automatically. The NES needs no BIOS.
//
// Run by the installer (install-xtrn.ini's [exec:getcore.js] step), or by hand:
//     jsexec ../xtrn/syncnes/getcore.js
//
// Copyright(C) 2026 Rob Swindell / SyncRetro.  GPL-2.0.

load("syncretro_getcore.js");

exit(syncretro_getcore(js.exec_dir, "fceumm_libretro", "FCEUmm (NES)"));
