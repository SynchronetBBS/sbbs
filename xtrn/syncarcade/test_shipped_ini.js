// test_shipped_ini.js -- the shipped syncretro.ini must reproduce exactly what
// the deleted lobby.js spec and console.ini declared. shared_saves is the one
// that matters most here: false would give every player a private cabinet and
// silently empty the machine's high-score table.
// Run: jsexec xtrn/syncarcade/test_shipped_ini.js

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

check_str(con.name, "Arcade", "name");
check_str(con.short, "Arcade", "short");
check_str(con.id, "arcade", "id (names the shared save dir)");
check_str(con.core, "mame2003_plus_libretro", "core");
check_str(con.profile, "arcade", "profile");
check(con.shared_saves === true, "shared_saves TRUE: one cabinet, one table");
check_str(rules.ext.join(","), "zip", "ext");
check(rules.min_size === 1024, "min_size");
check(rules.max_size === 67108864, "max_size");

print(failures ? "FAILED: " + failures + " failure(s)" : "ok: 0 failures");
exit(failures ? 1 : 0);
