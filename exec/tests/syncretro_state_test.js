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

print(failures ? "FAILED: " + failures + " failure(s)" : "ok: 0 failures");
exit(failures ? 1 : 0);
