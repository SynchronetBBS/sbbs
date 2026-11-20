// The configured path end to end: a child jsexec loads a scratch ctrl
// directory (below system.temp_dir) whose msgs.ini has [domain:] sections,
// so the C loader, system.inet_domain_list (matched with system.findstr())
// and, through User.can_send_from_domain(), user_can_send_from_inet_domain()
// run against a real configuration, with users in a scratch data directory
// (a new, empty user base: never the real one).

var failures = [];
var sc = load({}, js.exec_dir + "scratch.inc").scratch_ctrl("cfg");
var ctrl = sc.ctrl;
var data = sc.data;
var as_file = sc.base + "domains_as_file.cfg";	// the same list, one pattern per line

function check(desc, got, expect)
{
	var g = JSON.stringify(got);
	var e = JSON.stringify(expect);
	if (g !== e)
		failures.push(format("%s = %s, expected %s", desc, g, e));
}

function setup()
{
	sc.setup();
	var list = [["plain-on.example", true, ""]
		, ["ars-on.example", true, "LEVEL 60"]
		, ["plain-off.example", false, ""]
		, ["!blocked.wild.example", false, ""]
		, ["*.wild.example", true, ""]
		, ["dup.example", false, ""]
		, ["DUP.example", true, ""]
		, ["bad_name.example", true, ""]
		, ["nodot", true, ""]
		, ["SYSADDR.example", false, ""]	// the system address, send_from off
	];
	sc.add_sections(list);
	var f = new File(as_file);
	if (!f.open("w") || !f.write(list.map(function (d) { return d[0]; }).join("\n") + "\n"))
		throw new Error("Error " + f.error + " writing " + f.name);
	f.close();
}

try {
	setup();
	var r = sc.run(js.exec_dir + "child.inc", [as_file]);
	check("child exit code", r.exit_code, 0);

	check("child ctrl dir", r.dirs.ctrl, ctrl);
	check("child data dir", r.dirs.data, data);
	check("child system address", r.inet_addr, "sysaddr.example");

	// The system address first, then every configured entry as is (the
	// one naming the system address again is kept)
	check("list", r.list, ["sysaddr.example", "plain-on.example", "ars-on.example", "plain-off.example"
		, "!blocked.wild.example", "*.wild.example", "dup.example", "DUP.example"
		, "bad_name.example", "nodot", "SYSADDR.example"]);
	check("list frozen", r.list_is_frozen, true);

	check("listed", r.listed, {
		"plain-on.example": true, "PLAIN-ON.Example": true, "ars-on.example": true
		, "plain-off.example": true, "a.wild.example": true, "x.y.wild.example": true
		, "wild.example": false, "blocked.wild.example": false, "dup.example": true
		, "bad_name.example": true, "nodot": true, "unlisted.example": false, "": false
		, "sysaddr.example": true	// the system address (also named by an entry)
	});
	// The array form gives what the old domains.cfg file gave for the
	// same lines (and what the mail server's matcher gives: both call
	// findstr_in_list() on the same names in the same order)
	check("listed (array) == listed (file)", r.listed, r.listed_file);

	var none = {
		"plain-on.example": false, "PLAIN-ON.EXAMPLE": false, "ars-on.example": false
		, "plain-off.example": false, "a.wild.example": false, "*.wild.example": false
		, "blocked.wild.example": false, "!blocked.wild.example": false, "dup.example": false
		, "bad_name.example": false, "nodot": false, "unlisted.example": false
		, "sysaddr.example": false, "SysAddr.Example": false
	};
	// The system's own address: always usable by a user who may send
	// Internet e-mail, even though its list entry has send_from off
	var sysaddr = ["sysaddr.example", "SysAddr.Example"];
	function only(list) {
		var row = JSON.parse(JSON.stringify(none));
		list.forEach(function (d) { row[d] = true; });
		return row;
	}
	check("can_send normal (level 50)", r.can_send.normal
		, only(["plain-on.example", "PLAIN-ON.EXAMPLE"].concat(sysaddr)));
	check("can_send high (level 60)", r.can_send.high
		, only(["plain-on.example", "PLAIN-ON.EXAMPLE", "ars-on.example"].concat(sysaddr)));
	check("can_send sysop", r.can_send.sysop
		, only(["plain-on.example", "PLAIN-ON.EXAMPLE", "ars-on.example"].concat(sysaddr)));
	check("can_send no Internet mail (restricted)", r.can_send.nonet, none);
	check("can_send guest", r.can_send.guest, none);
	check("can_send inactive", r.can_send.inactive, none);
	check("can_send deleted", r.can_send.deleted, none);
	// new User(0) is the "nobody" object: every answer false
	check("user 0 number", r.user_number.none, 0);
	check("can_send user 0", r.can_send.none, none);
	// new User() of a number beyond the last user cannot be made
	check("can_send unknown user", r.can_send.unknown, "new User() threw");
	// a deleted user's record still loads (number kept), the answer is false
	check("deleted user number", r.user_number.deleted, r.users.deleted);
}
finally {
	check("scratch directory removed", sc.remove(), true);
}

if (failures.length > 0)
	throw new Error("configured domain list test failures:\n  " + failures.join("\n  "));
