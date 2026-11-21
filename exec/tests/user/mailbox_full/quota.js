// Mailbox Quota (mailbox_quota in the [mail] section of msgs.ini,
// system.mailbox_quota) as User.mailbox_full() applies it
// (user_mailbox_full(), which the mail server, the terminal's e-mail
// commands and WebMail call before storing mail for a local user),
// against scratch configurations with a scratch user base and mail base:
// a child jsexec per configuration (see quota_child.inc for the users;
// their mail is built to exact sizes around a quota of 16384 bytes).
//
// The answer is 2 when the user's mail has REACHED the quota: the header
// and data blocks of the mail waiting in the Inbox (spam included, deleted
// messages not) plus the private base, whose size is the "bytes" value
// the mailbox library writes to data/user/NNNN/mailbox.usage.ini, or the
// sizes of mailbox.shd and mailbox.sdt without that value, or nothing
// without a base.  Not with a quota of 0, the W exemption, or mail from
// the system itself.  Max Messages Waiting is asked first and answers 1.

var failures = [];
var lib = load({}, js.exec_dir + "../../inet_domains/config/scratch.inc");
var QUOTA = 16384;

function check(desc, got, expect)
{
	var g = JSON.stringify(got);
	var e = JSON.stringify(expect);
	if (g !== e)
		failures.push(format("%s = %s, expected %s", desc, g, e));
}

function scratch(name, quota, limit, steps)
{
	var sc = lib.scratch_ctrl("mboxquota_" + name);
	try {
		sc.setup();
		lib.ini_edit(sc.ctrl + "msgs.ini", function (f) {
			f.iniSetValue("mail", "mailbox_quota", quota);
			f.iniSetValue("mail", "max_msgs_waiting", limit);
		});
		var r = sc.run(js.exec_dir + "quota_child.inc", []);
		check(name + ": child exit code", r.exit_code, 0);
		check(name + ": system.mailbox_quota", r.quota, quota);
		check(name + ": system.max_msgs_waiting", r.limit, limit);
		steps(r);
	} finally {
		if (!sc.remove())
			failures.push(name + ": scratch directory not removed: " + sc.base);
	}
}

function fulls(r)
{
	var out = {};
	for (var key in r.users)
		out[key] = r.users[key].full;
	return out;
}

// The bytes each mailbox was built to hold
var BYTES = { none: 0, under: QUOTA - 256, at: QUOTA, over: QUOTA + 256, over_w: QUOTA + 256,
	spam: QUOTA, deleted: null, three_under: QUOTA - 256, three_at: QUOTA,
	ini_at: QUOTA, ini_under: QUOTA - 1, ini_alone: QUOTA, ini_no_base: QUOTA - 256,
	files_at: QUOTA, files_under: QUOTA - 1, files_no_key: QUOTA };

function built(name, r)
{
	for (var key in BYTES) {
		if (r.users[key] === undefined)
			failures.push(name + ": no user " + key);
		else if (BYTES[key] !== null)
			check(name + ": bytes of " + key, r.users[key].bytes, BYTES[key]);
	}
	check(name + ": deleted is under the quota", r.users.deleted && r.users.deleted.bytes < QUOTA, true);
	for (key in r.users) {
		check(name + ": " + key + " (false) as no argument", r.users[key].full_false, r.users[key].full);
		check(name + ": " + key + " from the system", r.users[key].full_system, 0);
	}
}

// The quota alone
scratch("quota", QUOTA, 0, function (r) {
	built("quota", r);
	check("quota: full", fulls(r), { none: 0, under: 0, at: 2, over: 2, over_w: 0, spam: 2, deleted: 0,
		three_under: 0, three_at: 2, ini_at: 2, ini_under: 0, ini_alone: 2, ini_no_base: 0,
		files_at: 2, files_under: 0, files_no_key: 2 });
});

// With Max Messages Waiting: each limit answers with its own reason, the
// message count first
scratch("both", QUOTA, 2, function (r) {
	built("both", r);
	check("both: waiting", [r.users.at.waiting, r.users.three_at.waiting, r.users.deleted.waiting], [2, 3, 2]);
	check("both: full", fulls(r), { none: 0, under: 0, at: 2, over: 2, over_w: 0, spam: 2, deleted: 0,
		three_under: 1, three_at: 1, ini_at: 2, ini_under: 0, ini_alone: 2, ini_no_base: 0,
		files_at: 2, files_under: 0, files_no_key: 2 });
});

// No quota: Max Messages Waiting as before
scratch("limit", 0, 2, function (r) {
	built("limit", r);
	var expect = {};
	for (var key in BYTES)
		expect[key] = 0;
	expect.three_under = 1;
	expect.three_at = 1;
	check("limit: full", fulls(r), expect);
});

// Neither: nobody's mail is refused
scratch("neither", 0, 0, function (r) {
	built("neither", r);
	var expect = {};
	for (var key in BYTES)
		expect[key] = 0;
	check("neither: full", fulls(r), expect);
});

if (failures.length)
	throw new Error(failures.length + " failure(s):\n" + failures.join("\n"));
