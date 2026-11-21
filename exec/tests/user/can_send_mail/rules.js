// The rules of User.can_send_mail() (user_can_send_mail(), which the
// terminal server's e-mail and netmail commands call too), against
// scratch configurations and a scratch user base: a child jsexec per
// configuration (see child.inc for the users).
//
// The E restriction (no local e-mail) applies when the recipient is not a
// sysop, the S restriction (no feedback) when it is; "a sysop" is any
// user at sysop level, not only user #1. Either way a first message (no
// e-mail or feedback sent yet) to the new user validation user is allowed.
// FidoNet netmail needs a configured FidoNet address, for sysops too;
// "netmail allowed" being off does not stop a sysop. Sysops keep their
// own restrictions.

var failures = [];
var lib = load({}, js.exec_dir + "../../inet_domains/config/scratch.inc");

function check(desc, got, expect)
{
	var g = JSON.stringify(got);
	var e = JSON.stringify(expect);
	if (g !== e)
		failures.push(format("%s = %s, expected %s", desc, g, e));
}

// opts: valuser, fido_addrs, fido_allowed, inet_allowed
function run(name, opts)
{
	var sc = lib.scratch_ctrl("cansend_" + name);
	try {
		sc.setup();
		lib.ini_edit(sc.ctrl + "main.ini", function (f) {
			f.iniSetValue(null, "valuser", opts.valuser);
			f.iniSetValue("level:50", "emailperday", 2);
			f.iniSetValue("level:90", "emailperday", 1);
		});
		lib.ini_edit(sc.ctrl + "msgs.ini", function (f) {
			f.iniSetValue("FidoNet", "addr_list", opts.fido_addrs);
			f.iniSetValue("FidoNet", "netmail_settings", opts.fido_allowed ? 1 : 0);
			f.iniSetValue("Internet", "netmail_settings", opts.inet_allowed ? 1 : 0);
		});
		var r = sc.run(js.exec_dir + "child.inc");
		check(name + ": child exit code", r.exit_code, 0);
		check(name + ": user numbers", [r.users.sysop, r.users.normal, r.users.sysop2, r.users.normal2], [1, 2, 3, 4]);
		return r.can;
	} finally {
		if (!sc.remove())
			failures.push(name + ": scratch directory not removed: " + sc.base);
	}
}

// expect: { user: { case: result } }
function expect(name, can, table)
{
	for (var u in table) {
		for (var c in table[u])
			check(name + ": " + u + " " + c, can[u][c], table[u][c]);
	}
}

// The validation user is sysop #1; both kinds of netmail allowed
var can = run("valuser1", { valuser: 1, fido_addrs: "1:2/3", fido_allowed: true, inet_allowed: true });
expect("valuser1", can, {
	normal: { local: true, to1: true, to2: true, to3: true, to4: true, fido: true, inet: true, qwk: true
		, local_x2: true, local_x3: false },
	// E: to sysops only, whichever user number they have
	e: { local: true, to1: true, to2: false, to3: true, to4: false, fido: true, inet: true, qwk: true },
	// S: not to sysops, except the first message to the validation user
	s: { local: true, to1: true, to2: true, to3: false, to4: true },
	es: { to1: true, to2: false, to3: false, to4: false },
	// The exception ends with the first e-mail or feedback sent
	es_mailed: { to1: false, to2: false, to3: false },
	es_fed_back: { to1: false, to2: false, to3: false },
	// A sysop's own restrictions apply
	sysop_es: { to1: true, to2: false, to3: false },
	m: { to2: true, fido: false, inet: false, qwk: false },
	at_limit: { local: false, to2: false, fido: false, inet: false, qwk: false },
	exempt: { local: true, to2: true, fido: true, local_x3: true },
	sysop_at_limit: { local: true, to2: true, fido: true, inet: true, local_x3: true }
});

// The validation user is not a sysop; no FidoNet address; Internet mail off
can = run("valuser2", { valuser: 2, fido_addrs: "", fido_allowed: true, inet_allowed: false });
expect("valuser2", can, {
	e: { to1: true, to2: true, to3: true, to4: false },
	s: { to1: false, to2: true, to3: false, to4: true },
	es: { to1: false, to2: true, to3: false, to4: false },
	es_mailed: { to2: false },
	normal: { fido: false, inet: false, qwk: true },
	sysop: { fido: false, inet: true },
	sysop2: { fido: false, inet: true },
	m: { inet: false }
});

// No validation user; a FidoNet address but FidoNet netmail off
can = run("novaluser", { valuser: 0, fido_addrs: "1:2/3", fido_allowed: false, inet_allowed: true });
expect("novaluser", can, {
	e: { local: true, to1: true, to2: false, to3: true },
	s: { local: true, to1: false, to2: true, to3: false },
	normal: { fido: false, inet: true },
	sysop: { fido: true, inet: true },
	sysop_es: { fido: true }
});

if (failures.length)
	throw new Error(failures.length + " failure(s):\n" + failures.join("\n"));
