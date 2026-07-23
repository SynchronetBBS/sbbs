// Unit test for exec/load/game_lobby.js parse_duration() -- pure, no network.
//
//     jsexec /home/rswindell/sbbs/xtrn/tests/game_lobby_test.js
//
// Alongside xtrn_mirror_test.js rather than under exec/tests/, matching the
// convention already set there.

var gl = load({}, "game_lobby.js");

var fail = 0;
function ok(cond, msg) { if (!cond) { fail++; print("FAIL: " + msg); } else print("ok: " + msg); }

/* The historical default is unchanged -- a bare number is DAYS. activity_max_age
   depends on this, so a regression here silently changes the activity feed. */
ok(gl.parse_duration("2") === 172800, "bare number still defaults to days");
ok(gl.parse_duration("0.5") === 43200, "fractional days");

/* Seconds as the explicit default: what the [idle] keys pass, so this parser
   and xpdev's parse_duration() agree on the same ini string. */
ok(gl.parse_duration("900", "s") === 900, "bare number with dflt_unit s");
ok(gl.parse_duration("2", "d") === 172800, "explicit dflt_unit d matches the old default");

/* An explicit suffix always wins over the default unit, in both directions. */
ok(gl.parse_duration("15m", "s") === 900, "suffix m overrides dflt_unit s");
ok(gl.parse_duration("60s", "d") === 60, "suffix s overrides dflt_unit d");
ok(gl.parse_duration("1h", "s") === 3600, "suffix h");
ok(gl.parse_duration("2w", "s") === 1209600, "suffix w");
ok(gl.parse_duration("1y", "s") === 31536000, "suffix y");

/* Blank/invalid/non-positive is 0 ("no limit") whatever the default unit. */
ok(gl.parse_duration("", "s") === 0, "empty string");
ok(gl.parse_duration("0", "s") === 0, "zero");
ok(gl.parse_duration("-5", "s") === 0, "negative");
ok(gl.parse_duration("abc", "s") === 0, "non-numeric");
ok(gl.parse_duration(undefined, "s") === 0, "undefined");

print(fail ? ("\n" + fail + " FAILURES") : "\nALL PASS");
exit(fail ? 1 : 0);
