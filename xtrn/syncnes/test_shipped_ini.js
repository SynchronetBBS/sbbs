// test_shipped_ini.js -- the shipped syncretro.ini must reproduce exactly what
// the deleted lobby.js spec and console.ini declared. `id` in particular names
// the per-user save directory, so a typo silently strands every player's saves.
// Run: jsexec xtrn/syncnes/test_shipped_ini.js

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

check_str(con.name, "Nintendo Entertainment System", "name");
check_str(con.short, "NES", "short");
check_str(con.id, "nes", "id (names the save dir)");
check_str(con.core, "fceumm_libretro", "core");
check_str(con.profile, "pad", "profile");
check(con.shared_saves === false, "shared_saves false");
check_str(rules.ext.join(","), "nes,unf,unif", "ext");
check(rules.min_size === 8192, "min_size");
check(rules.max_size === 4194304, "max_size");

// .fds is DELIBERATELY absent: an FDS image needs disksys.rom, which is
// copyrighted content the sysop supplies. Without it fceumm fails the load and
// the player just sees a door that will not start.
check(rules.ext.join(",").indexOf("fds") < 0, "fds not enabled by default");

print(failures ? "FAILED: " + failures + " failure(s)" : "ok: 0 failures");
exit(failures ? 1 : 0);
