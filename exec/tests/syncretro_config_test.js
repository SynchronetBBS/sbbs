// syncretro_config_test.js -- the lobby's half of the shipped-ini + sysop-overlay
// read. Run: jsexec exec/tests/syncretro_config_test.js
//
// SpiderMonkey 1.8.5: no let/const, no arrow functions, no template literals.

load("syncretro_lobby.js");

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

var dir = system.temp_dir + "syncretro_cfgtest/";
var f;

mkpath(dir);

f = new File(dir + "syncretro.ini");
f.open("w");
f.write("[console]\n"
    + "name = Intellivision\n"
    + "short = Intv\n"
    + "core = freeintv_libretro\n"
    + "profile = intv\n"
    + "shared_saves = false\n"
    + "\n"
    + "[roms]\n"
    + "dir = roms\n"
    + "ext = int, bin, rom\n"
    + "min_size = 2048\n"
    + "max_size = 65536\n"
    + "\n"
    + "[lobby]\n"
    + "enter_sound = ding.mid\n"
    + "\n"
    + "[idle]\n"
    + "timeout = 15m\n");
f.close();

f = new File(dir + "syncretro.local.ini");
f.open("w");
f.write("[console]\n"
    + "short = Intellivision\n"
    + "\n"
    + "[roms]\n"
    + "exclude = BIOS\n"
    + "\n"
    + "[lobby]\n"
    + "enter_sound = \n"
    + "\n"
    + "[text]\n"
    + "header = mine.asc\n");
f.close();

var ini = syncretro_lobby_ini(dir);

// A shipped key with no local twin survives.
check_str(ini.console.name, "Intellivision", "console.name survives");
check_str(ini.console.core, "freeintv_libretro", "console.core survives");
check_str(ini.roms.ext, "int, bin, rom", "roms.ext survives");
check_str(ini.idle.timeout, "15m", "idle.timeout survives");

// A local key wins at the same scope.
check_str(ini.console.short, "Intellivision", "console.short overridden");

// A local-only key arrives.
check_str(ini.roms.exclude, "BIOS", "roms.exclude arrives");

// A local-only SECTION arrives.
check_str(ini.text.header, "mine.asc", "text.header arrives");

// A key present but EMPTY in the local file is a real override in [lobby] and
// [text] ("draw nothing"), not an absent key.
check(ini.lobby.enter_sound === "", "empty enter_sound overrides");

// Neither file: every section is an object, never null.
var empty = system.temp_dir + "syncretro_cfgtest_empty/";
mkpath(empty);
var none = syncretro_lobby_ini(empty);
check(none.console && typeof none.console === "object", "console object when no files");
check(none.roms && typeof none.roms === "object", "roms object when no files");
check(none.lobby && typeof none.lobby === "object", "lobby object when no files");
check(none.text && typeof none.text === "object", "text object when no files");
check(none.idle && typeof none.idle === "object", "idle object when no files");

// A full copy as the overlay resolves identically to no overlay at all -- the
// property that makes the spec's migration safe.
var copydir = system.temp_dir + "syncretro_cfgtest_copy/";
mkpath(copydir);
file_copy(dir + "syncretro.ini", copydir + "syncretro.ini");
file_copy(dir + "syncretro.ini", copydir + "syncretro.local.ini");
var copied = syncretro_lobby_ini(copydir);
check_str(copied.console.short, "Intv", "full-copy overlay: console.short");
check_str(copied.roms.ext, "int, bin, rom", "full-copy overlay: roms.ext");

// The console and rules objects now come from the ini, not a JS literal.
var con = syncretro_console(ini.console);
check_str(con.name, "Intellivision", "console name from ini");
check_str(con.short, "Intellivision", "console short from ini (local wins)");
check_str(con.profile, "intv", "console profile from ini");
check_str(con.core, "freeintv_libretro", "console core from ini");
check_str(con.id, "intellivision", "console id derived from short");
check(con.shared_saves === false, "shared_saves false from ini");

var rules = syncretro_rules(ini.roms);
check_str(rules.dir, "roms", "rules dir from ini");
check_str(rules.ext.join(","), "int,bin,rom", "rules ext parsed from ini");
check_str(rules.exclude.join(","), "BIOS", "rules exclude from ini");
check(rules.min_size === 2048, "rules min_size from ini");
check(rules.max_size === 65536, "rules max_size from ini");

// shared_saves is a string in an ini, not a boolean. "true" must mean true and
// "false" must mean false -- the naive truthiness test makes both true.
check(syncretro_console({ shared_saves: "true" }).shared_saves === true,
      "shared_saves \"true\" is true");
check(syncretro_console({ shared_saves: "false" }).shared_saves === false,
      "shared_saves \"false\" is false");

// dir_name was the spec's key for the roms sub-directory; the ini spells it
// `dir`, which is what [roms] already used. Both must reach rules.dir.
check_str(syncretro_rules({ dir: "carts" }).dir, "carts", "roms dir key");

print(failures ? "FAILED: " + failures + " failure(s)" : "ok: 0 failures");
exit(failures ? 1 : 0);
