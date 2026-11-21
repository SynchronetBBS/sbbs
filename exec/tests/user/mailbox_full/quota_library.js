// User.mailbox_full() with a private base the mailbox library made
// (exec/load/mailbox.js): the library records the base's size in
// data/user/NNNN/mailbox.usage.ini when it saves a Sent copy, and
// user_mailbox_full() adds that figure to the mail waiting in the Inbox.
// The library's own total (Store.usage()) and the answer agree at the
// quota and one block under it.  See quota.js and quota_child.inc.

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

var sc = lib.scratch_ctrl("mboxquota_library");
try {
	sc.setup();
	lib.ini_edit(sc.ctrl + "msgs.ini", function (f) {
		f.iniSetValue("mail", "mailbox_quota", QUOTA);
		f.iniSetValue("mail", "max_msgs_waiting", 0);
	});
	var r = sc.run(js.exec_dir + "quota_child.inc", ["library"]);
	check("child exit code", r.exit_code, 0);
	check("system.mailbox_quota", r.quota, QUOTA);
	var at = r.users.library_at || {};
	var under = r.users.library_under || {};
	check("bytes at the quota", at.bytes, QUOTA);
	check("full at the quota", [at.full, at.full_system], [2, 0]);
	check("bytes under the quota", under.bytes, QUOTA - 256);
	check("not full under the quota", under.full, 0);
} finally {
	if (!sc.remove())
		failures.push("scratch directory not removed: " + sc.base);
}

if (failures.length)
	throw new Error(failures.length + " failure(s):\n" + failures.join("\n"));
