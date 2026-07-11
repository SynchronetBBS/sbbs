// getroms.js -- fetch freely-licensed NES homebrew for this door, so a fresh
// install has games in it before you supply a single cartridge.
//
// The work is in exec/load/syncretro_getroms.js, shared with every other
// SyncRetro console. This file is just the LIST -- and every entry on it is a
// game whose AUTHOR published it under a free licence (LGPL, GPL, MIT, zlib, or
// Creative Commons). Each is pinned by URL, size and MD5, and verified after
// download: a ROM that changed upstream is rejected rather than installed under
// a licence that was checked against different bytes.
//
// WHAT IS DELIBERATELY NOT HERE: the great majority of NES homebrew states no
// licence at all, which means ordinary copyright and no right to redistribute it
// -- including most of the collection two of these games are fetched from. Being
// downloadable is not a licence. If you add a game here, read its licence first.
//
// The four Creative Commons BY-NC-SA titles are NON-COMMERCIAL: fine on a hobby
// BBS, but if you charge for access to your board, leave them out.
//
// Run by the installer (install-xtrn.ini's [exec:getroms.js] step), or by hand:
//     jsexec ../xtrn/syncnes/getroms.js
//
// Copyright(C) 2026 Rob Swindell / SyncRetro.  GPL-2.0.

load("syncretro_getroms.js");

// The retrobrews collection redistributes these with each author's licence text
// beside the ROM; the licence is the author's grant, not the collection's.
var RETROBREWS = "https://raw.githubusercontent.com/retrobrews/nes-games/master/";

// Fetched from the author's OWN release page -- the cleanest provenance there is.
var NOVA    = "https://github.com/NovaSquirrel/NovaTheSquirrel/releases/download/v1.0.6a/";
var THWAITE = "https://github.com/pinobatch/thwaite-nes/releases/download/v0.04/";

var GAMES = [
	{ file: "31in1realgame-multicart.nes",
	  title: "31 in 1 Real Game!",
	  by: "Mojon Twins", lic: "LGPL-3.0",
	  size: 393232, md5: "bf016011d7da3db0f24698486b781bb8",
	  url: RETROBREWS + "31in1realgame-multicart.nes" },

	{ file: "jetpaco.nes",
	  title: "Jet Paco - Space Agent!",
	  by: "Mojon Twins", lic: "LGPL-3.0",
	  size: 40976, md5: "543788a352dec60850bc2d8506f6812d",
	  url: RETROBREWS + "jetpaco.nes" },

	{ file: "lala.nes",
	  title: "Lala the Magical",
	  by: "Mojon Twins", lic: "LGPL-3.0",
	  size: 81936, md5: "f6afa5154d44c683c5863afb850535af",
	  url: RETROBREWS + "lala.nes" },

	{ file: "sgthelmet.nes",
	  title: "Sgt. Helmet Training Day",
	  by: "Mojon Twins", lic: "LGPL-3.0",
	  size: 40976, md5: "2f7369665ee6e9d70d2594cb5de912b7",
	  url: RETROBREWS + "sgthelmet.nes" },

	{ file: "sir-ababol-remastered.nes",
	  title: "Sir Ababol Remastered",
	  by: "Mojon Twins", lic: "LGPL-3.0",
	  size: 40976, md5: "e377b81e08bca177d73f29531fe4a100",
	  url: RETROBREWS + "sir-ababol-remastered.nes" },

	{ file: "wo-xiang-niao-niao.nes",
	  title: "Wo Xiang Niao Niao",
	  by: "Mojon Twins", lic: "LGPL-3.0",
	  size: 40976, md5: "3609c7a53a9bf31936e75db50f613df3",
	  url: RETROBREWS + "wo-xiang-niao-niao.nes" },

	{ file: "yun.nes",
	  title: "Yun",
	  by: "Mojon Twins", lic: "LGPL-3.0",
	  size: 65552, md5: "ad1cf569c8cf113beeec3362581b9f0a",
	  url: RETROBREWS + "yun.nes" },

	{ file: "bootee.nes",
	  title: "Bootee",
	  by: "Mojon Twins", lic: "CC BY-NC-SA 3.0",
	  size: 40976, md5: "33ddb5a56308f4af1e5893a484b86e74",
	  url: RETROBREWS + "bootee.nes" },

	{ file: "cheril-the-goddess.nes",
	  title: "Cheril the Goddess",
	  by: "Mojon Twins", lic: "CC BY-NC-SA 3.0",
	  size: 65552, md5: "ab7e44ff4ee120dabe42e9a9afe3322f",
	  url: RETROBREWS + "cheril-the-goddess.nes" },

	{ file: "miedow.nes",
	  title: "Miedow",
	  by: "Mojon Twins", lic: "CC BY-NC-SA 3.0",
	  size: 65552, md5: "284d0240e9b2e13158af4915e7037552",
	  url: RETROBREWS + "miedow.nes" },

	{ file: "superuwol.nes",
	  title: "Super Uwol",
	  by: "Mojon Twins", lic: "CC BY-NC-SA 3.0",
	  size: 40976, md5: "e8f16cb9b4f45c994a9b45f25074a10b",
	  url: RETROBREWS + "superuwol.nes" },

	{ file: "falling.nes",
	  title: "Falling",
	  by: "Tragicmuffin", lic: "MIT",
	  size: 40976, md5: "150c8567ce53f1a677b17c6896c9b6fc",
	  url: RETROBREWS + "falling.nes" },

	{ file: "fff.nes",
	  title: "F-FF",
	  by: "Pubby", lic: "MIT",
	  size: 65552, md5: "19b57d580140a6189ceda83e5d584d16",
	  url: RETROBREWS + "fff.nes" },

	{ file: "lunarlimit.nes",
	  title: "Lunar Limit",
	  by: "Wendell Scott", lic: "MIT",
	  size: 40976, md5: "1949dfe158787352cf8f33c07b35ba25",
	  url: RETROBREWS + "lunarlimit.nes" },

	{ file: "ralph4.nes",
	  title: "RALPH 4",
	  by: "Michael Chaney", lic: "MIT",
	  size: 24592, md5: "dbc6085c0bbc3f1777400e6d699f24ef",
	  url: RETROBREWS + "ralph4.nes" },

	{ file: "starevil.nes",
	  title: "Star Evil",
	  by: "Pubby", lic: "MIT",
	  size: 40976, md5: "b6395ce3c1de6438b9041a26197e9d74",
	  url: RETROBREWS + "starevil.nes" },

	{ file: "dabg.nes",
	  title: "Double Action Blaster Guys",
	  by: "NovaSquirrel", lic: "GPL-3.0",
	  size: 24592, md5: "70148551a679cbb9ce6b4e16042d8599",
	  url: RETROBREWS + "dabg.nes" },

	{ file: "croom.nes",
	  title: "Concentration Room",
	  by: "Damian Yerrick", lic: "GPL-3.0",
	  size: 24592, md5: "a02eefb66205f41faf3facb9275e29c0",
	  url: RETROBREWS + "croom.nes" },

	{ file: "nesertbus.nes",
	  title: "NesertBus",
	  by: "Wendell Scott", lic: "CC BY-SA 4.0",
	  size: 24592, md5: "7e08712d0d6003415fa921fcfd9cb3e8",
	  url: RETROBREWS + "nesertbus.nes" },

	{ file: "robotfindskitten.nes",
	  title: "RobotFindsKitten",
	  by: "Damian Yerrick", lic: "zlib",
	  size: 32784, md5: "00ce7661dfd2e73cc44f393e2dac5476",
	  url: RETROBREWS + "robotfindskitten.nes" },

	/* Straight from their authors' release pages. */
	{ file: "nova.nes",
	  title: "Nova the Squirrel",
	  by: "NovaSquirrel", lic: "GPL-3.0 (code) / CC BY-NC-SA 4.0 (art)",
	  size: 262160, md5: "ecc08609f53c236e16731e75c857c934",
	  url: NOVA + "nova.nes" },

	{ file: "thwaite.nes",
	  title: "Thwaite",
	  by: "Damian Yerrick", lic: "GPL-3.0",
	  size: 24592, md5: "60c2367d470abaa52a47605effe68352",
	  url: THWAITE + "thwaite.nes" }
];

exit(syncretro_getroms(js.exec_dir, "NES", GAMES));
