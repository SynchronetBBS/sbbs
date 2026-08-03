// test_shipped_ini.js -- the shipped syncretro.ini must reproduce exactly what
// the deleted lobby.js spec and console.ini declared. `id` in particular names
// the per-user save directory, so a typo silently strands every player's saves.
// This is the most complex of the three shipped inis (four bios_md5 hashes,
// bios_names, bios_words), and its sibling test_lobby_headless.js cannot run
// in this checkout (it refuses while a live data file exists) -- so this file
// is its only executable guard.
// Run: jsexec xtrn/syncivision/test_shipped_ini.js

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

var shipped = syncretro_lobby_ini(js.exec_dir);
var con = syncretro_console(shipped.console);
var rules = syncretro_rules(shipped.roms);

check_str(con.name, "Intellivision", "name");
check_str(con.short, "Intv", "short");
check_str(con.id, "intv", "id (names the save dir)");
check_str(con.core, "freeintv_libretro", "core");
check_str(con.profile, "intv", "profile");
check(con.shared_saves === false, "shared_saves false");
check_str(rules.ext.join(","), "int,bin,rom", "ext");
check(rules.min_size === 2048, "min_size");
check(rules.max_size === 65536, "max_size");
check_str(rules.bios_names.join(","), "exec.bin,grom.bin", "bios_names");
check_str(rules.bios_words.join(","), "bios", "bios_words");

check(rules.bios_md5.length === 4, "bios_md5 has all four hashes");
check_str(rules.bios_md5[0], "62e761035cb657903761800f4437b8af", "bios_md5[0] (exec.bin)");
check_str(rules.bios_md5[1], "0cd5946c6473e42e8e4c2137785e427f", "bios_md5[1] (grom.bin)");
check_str(rules.bios_md5[2], "d5530f74681ec6e0f282dab42e6b1c5f", "bios_md5[2] (IntelliVoice)");
check_str(rules.bios_md5[3], "8590d338a1bb5e0feed3e8a8cd493035", "bios_md5[3] (Sears exec)");

check_str(shipped.console.bios, "exec.bin, grom.bin", "[console] bios");

print(failures ? "FAILED: " + failures + " failure(s)" : "ok: 0 failures");
exit(failures ? 1 : 0);
