// syncretro_state_test.js -- the JS half of the snapshot staleness key.
// THE GOLDEN VALUE AND THE THREE INPUTS BELOW ARE THE SAME ONES IN
// src/doors/syncretro/test_statekey.c. Change one, change both -- and if the
// value ever moves, every existing snapshot on every install becomes
// unreachable, so it is not a number to adjust casually.
//
// Run: jsexec exec/tests/syncretro_state_test.js

load("syncretro_lib.js");

var failures = 0;

function check(cond, what)
{
	if (!cond) {
		print("FAIL: " + what);
		failures++;
	}
}

function check_str(got, want, what)
{
	if (String(got) !== String(want)) {
		print("FAIL: " + what + ": got \"" + got + "\", want \"" + want + "\"");
		failures++;
	}
}

var CORE_MD5 = "0123456789abcdef0123456789abcdef";
var ROM_MD5  = "fedcba9876543210fedcba9876543210";
var OPTS     = "mame2003-plus_skip_disclaimer=enabled\n"
    + "mame2003-plus_skip_warnings=enabled";

// The golden value, shared with the C half.
check_str(syncretro_state_key(CORE_MD5, ROM_MD5, OPTS), "38ed1f37",
          "golden key matches the C half");

// Every input participates.
check(syncretro_state_key("ffffffffffffffffffffffffffffffff", ROM_MD5, OPTS)
      !== syncretro_state_key(CORE_MD5, ROM_MD5, OPTS), "core hash matters");
check(syncretro_state_key(CORE_MD5, "ffffffffffffffffffffffffffffffff", OPTS)
      !== syncretro_state_key(CORE_MD5, ROM_MD5, OPTS), "rom hash matters");
check(syncretro_state_key(CORE_MD5, ROM_MD5, "")
      !== syncretro_state_key(CORE_MD5, ROM_MD5, OPTS), "options matter");

// Shape: 8 lowercase hex digits.
var k = syncretro_state_key(CORE_MD5, ROM_MD5, "");
check(k.length === 8, "key is 8 characters");
check(/^[0-9a-f]{8}$/.test(k), "key is lowercase hex");

// Flattening is order-independent in input and sorted in output, because two
// installs listing the same options differently must produce the same key.
check_str(syncretro_state_opts({ b: "2", a: "1" }), "a=1\nb=2",
          "options sort by name");
check_str(syncretro_state_opts({}), "", "no options is the empty string");
check_str(syncretro_state_opts(null), "", "absent options is the empty string");

// --- discovery carries the ROM hash ------------------------------------------
//
// syncretro_lobby_state_key() has no other way to reach a ROM's hash: it must
// not re-read every cartridge just to key one about to be played.
var romdir = system.temp_dir + "syncretro_statetest_roms/";
mkpath(romdir);
var f = new File(romdir + "Foo (2000) (Acme).rom"); f.open("wb"); f.write("cartridge bytes"); f.close();
var rules = syncretro_rules({ dir: "roms", ext: "rom" });
var discovered = syncretro_discover(romdir, rules, null);
check(discovered.length === 1, "discovery finds the test cartridge");
check(discovered.length && /^[0-9a-f]{32}$/.test(discovered[0].md5),
      "a discovered ROM object carries a 32-hex-character md5");

// --- the snapshot directory listing: one read, parsed into stem -> key. ------
var statedir = system.temp_dir + "syncretro_statetest/";

mkpath(statedir);
f = new File(statedir + "Astrosmash.a1b2c3d4.state"); f.open("wb"); f.write("x"); f.close();
f = new File(statedir + "Utopia.99887766.state");     f.open("wb"); f.write("x"); f.close();
f = new File(statedir + "notasnapshot.txt");          f.open("wb"); f.write("x"); f.close();

var list = syncretro_state_list(statedir);
check_str(list["Astrosmash"].key, "a1b2c3d4", "listing reads the key from the name");
check_str(list["Utopia"].key, "99887766", "listing finds the second snapshot");
check(list["notasnapshot"] === undefined, "non-.state files are ignored");
check(list["Astrosmash"].path.indexOf("Astrosmash.a1b2c3d4.state") >= 0,
      "listing carries the full path");

// A cartridge is suspended only when the key matches what would restore NOW.
check(syncretro_state_marked(list, "Astrosmash.int", "a1b2c3d4") === true,
      "matching key marks the cartridge");
check(syncretro_state_marked(list, "Astrosmash.int", "ffffffff") === false,
      "a stale key does not mark the cartridge");
check(syncretro_state_marked(list, "Nothing.int", "a1b2c3d4") === false,
      "a cartridge with no snapshot is not marked");

// --- sweeping dead snapshots --------------------------------------------------
//
// A snapshot is dead when its stem names no cartridge we can see, or when its
// key is not the key that cartridge would restore with now. Both are deleted
// in the same pass; a live, matching one is kept.
var sweepdir = system.temp_dir + "syncretro_statetest_sweep/";

mkpath(sweepdir);
f = new File(sweepdir + "Astrosmash.a1b2c3d4.state"); f.open("wb"); f.write("x"); f.close();  // stale key
f = new File(sweepdir + "Utopia.99887766.state");     f.open("wb"); f.write("x"); f.close();  // live
f = new File(sweepdir + "Gone.deadbeef.state");       f.open("wb"); f.write("x"); f.close();  // no cartridge

var sweep_list = syncretro_state_list(sweepdir);
var sweep_roms = [ { name: "Astrosmash.int" }, { name: "Utopia.int" } ];
var current    = { "Astrosmash.int": "ffffffff", "Utopia.int": "99887766" };
var deleted    = syncretro_state_sweep(sweep_list, sweep_roms, current);

check(deleted === 2, "sweep deletes the stale-key and orphaned snapshots (got " + deleted + ")");
check(!file_exists(sweep_list["Astrosmash"].path), "the stale-key snapshot is gone");
check(!file_exists(sweep_list["Gone"].path), "the orphaned snapshot is gone");
check(file_exists(sweep_list["Utopia"].path), "the live, matching snapshot survives");

// --- the core binary's hash, cached on size + mtime --------------------------
var coredir = system.temp_dir + "syncretro_statetest_core/";

mkpath(coredir);
var core_path  = coredir + "fake_core.so";
var cache_path = coredir + "core.cache.json";

f = new File(core_path); f.open("wb"); f.write("core-bytes-v1"); f.close();
var hash1 = syncretro_core_md5(core_path, cache_path);
check(/^[0-9a-f]{32}$/.test(hash1), "core hash is 32 hex characters");
check(file_exists(cache_path), "the core hash cache is written");

// Overwrite the cache with a deliberately WRONG md5 for the file's CURRENT
// size + date (unchanged since the write above). A correctly-warm cache
// trusts size+date and returns this wrong value rather than re-reading the
// file -- proving the cache is consulted at all, not just that its shape is
// right.
var forged = "ffffffffffffffffffffffffffffffff";
f = new File(cache_path);
f.open("w");
f.write(JSON.stringify({ path: core_path, size: file_size(core_path),
                         date: file_date(core_path), md5: forged }));
f.close();
check_str(syncretro_core_md5(core_path, cache_path), forged,
          "an unchanged size+date reuses the cached hash without re-reading");

// Changing the file's SIZE invalidates the cache and forces a re-hash back to
// the real value.
f = new File(core_path); f.open("wb"); f.write("core-bytes-v1-longer"); f.close();
var hash2 = syncretro_core_md5(core_path, cache_path);
check(hash2 !== forged && /^[0-9a-f]{32}$/.test(hash2),
      "a changed size invalidates the cache and re-hashes for real");

// A missing core resolves to "", never throws.
check_str(syncretro_core_md5(coredir + "nope.so", cache_path), "", "a missing core file yields \"\"");

// --- resolving the installed core file ----------------------------------------
//
// Mirrors syncretro_config.c's sr_find_core(): the named core (or the lone
// "*_libretro" match) at the door root, or one level down in any sub-dir.
var coreresdir = system.temp_dir + "syncretro_statetest_coreres/";

mkpath(coreresdir);
mkpath(coreresdir + "linux-x64/");
// A re-run of this suite leaves fixtures behind (system.temp_dir is not
// wiped between invocations), and this section's first assertion depends on
// NONE of them existing yet -- clear the exact set this section creates.
file_remove(coreresdir + "foo_libretro.so");
file_remove(coreresdir + "linux-x64/foo_libretro.so");
file_remove(coreresdir + "linux-x64/bar_libretro.so");

check_str(syncretro_core_path(coreresdir, "foo_libretro", "so"), "",
          "no candidate resolves to \"\"");

f = new File(coreresdir + "foo_libretro.so"); f.open("wb"); f.write("x"); f.close();
check(syncretro_core_path(coreresdir, "foo_libretro", "so").indexOf("foo_libretro.so") >= 0,
      "the named core is found at the door root");

file_remove(coreresdir + "foo_libretro.so");
mkpath(coreresdir + "linux-x64/");
f = new File(coreresdir + "linux-x64/foo_libretro.so"); f.open("wb"); f.write("x"); f.close();
check(syncretro_core_path(coreresdir, "foo_libretro", "so").indexOf("foo_libretro.so") >= 0,
      "the named core is found one level down, whatever the sub-dir is called");

check(syncretro_core_path(coreresdir, "", "so").indexOf("foo_libretro.so") >= 0,
      "the lone \"*_libretro\" match is found when no core is named");

f = new File(coreresdir + "linux-x64/bar_libretro.so"); f.open("wb"); f.write("x"); f.close();
check_str(syncretro_core_path(coreresdir, "", "so"), "",
          "two \"*_libretro\" candidates resolve to \"\" rather than guessing");

// --- the cabinet preference --------------------------------------------------
//
// The pure key/default only -- syncretro_lobby_private() (a live session
// answer, in syncretro_lobby.js) is what a player actually sees.
check_str(syncretro_cabinet_key("arcade"), "cabinet.arcade",
          "preference key is per console");
check(syncretro_cabinet_default() === "public",
      "an absent preference means the shared cabinet");

print(failures ? "FAILED: " + failures + " failure(s)" : "ok: 0 failures");
exit(failures ? 1 : 0);
