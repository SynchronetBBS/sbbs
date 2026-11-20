// The notices and semaphore of a message saved to a recipient list: a
// child jsexec loads a scratch copy of the configuration whose data
// directory is a new, empty tree (users, mail base and short messages all
// scratch) and saves mail through MsgBase("mail") with save_msg(), with
// and without a recipient list.
//
// For each recipient of a list, save_msg() does what it does for a
// single recipient: a local recipient of a message that is not anonymous
// gets the UserSentYouMail short message, and a FidoNet recipient touches
// the netmail semaphore. Under jsexec there is no client, so a sender who
// is also a recipient gets the notice too (only a client's own user is
// skipped), with or without a list.

var failures = [];
var scratch = load({}, js.exec_dir + "scratch.inc");
var sc = scratch.scratch_ctrl("msgnotify");

function check(desc, got, expect)
{
	var g = JSON.stringify(got);
	var e = JSON.stringify(expect);
	if (g !== e)
		failures.push(format("%s = %s, expected %s", desc, g, e));
}

try {
	sc.setup();
	// Where short messages are written (data/msgs/, as on an installed system)
	if (!mkpath(sc.data + "msgs/"))
		throw new Error("Error creating " + sc.data + "msgs/");
	scratch.ini_edit(sc.ctrl + "msgs.ini", function (f) {
		f.iniSetValue("FidoNet", "netmail_sem", "%jnotify_test_netmail.sem");
	});
	var r = sc.run(js.exec_dir + "rcpt_list_child.inc");
	if (argv.indexOf("-v") >= 0)
		print(JSON.stringify(r, null, 1));
	check("child exit code", r.exit_code, 0);
	check("child data dir", r.data_dir, sc.data);
	check("no exception", r.exception, undefined);
	check("mail base file", r.mail_file, sc.data + "mail");
	check("semaphore path", r.sem, sc.data + "notify_test_netmail.sem");
	check("a notice was left", typeof r.notice == "string" && r.notice.length > 0, true);

	var c = r.cases || {};
	// One message to a local user, the sender and a FidoNet address
	check("list: saved", c.list && c.list.saved, true);
	check("list: copies", c.list && c.list.copies, 3);
	check("list: recipient notified", c.list && c.list.rcpt, r.notice);
	check("list: sender (no client) notified", c.list && c.list.sender, r.notice);
	check("list: semaphore touched", c.list && c.list.sem, true);
	// Anonymous: the same list, no one notified; the semaphore still touched
	check("anonymous list: saved", c.anon_list && c.anon_list.saved, true);
	check("anonymous list: recipient", c.anon_list && c.anon_list.rcpt, null);
	check("anonymous list: sender", c.anon_list && c.anon_list.sender, null);
	check("anonymous list: semaphore touched", c.anon_list && c.anon_list.sem, true);
	// A list of one local user, no FidoNet recipient
	check("local list: recipient notified", c.local_list && c.local_list.rcpt, r.notice);
	check("local list: semaphore not touched", c.local_list && c.local_list.sem, false);
	// Single recipient (no list), as before
	check("single: recipient notified", c.single && c.single.rcpt, r.notice);
	check("single: semaphore not touched", c.single && c.single.sem, false);
	check("single to self: sender (no client) notified", c.single_self && c.single_self.sender, r.notice);
	check("single FidoNet: semaphore touched", c.single_fido && c.single_fido.sem, true);
	check("single FidoNet: no notices", c.single_fido && [c.single_fido.rcpt, c.single_fido.sender], [null, null]);
	check("single anonymous: recipient", c.single_anon && c.single_anon.rcpt, null);
	// A mail base opened by path is not the configured mail base: no notice
	check("path base list: saved", c.path_list && c.path_list.saved, true);
	check("path base list: recipient", c.path_list && c.path_list.rcpt, null);
	check("path base single: recipient", c.path_single && c.path_single.rcpt, null);
	check("notice text", /sent you E-mail/.test(r.notice) && r.notice.indexOf("Notifysender") >= 0, true);
} finally {
	check("scratch directory removed", sc.remove(), true);
}

if (failures.length > 0)
	throw new Error("recipient list notice failures:\n  " + failures.join("\n  "));
