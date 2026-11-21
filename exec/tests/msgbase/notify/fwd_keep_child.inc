// Run by fwd_keep.js in a child jsexec whose ctrl directory is a scratch
// copy and whose data directory is a new, empty scratch tree: creates a
// user who forwards e-mail to an Internet address, sends that user a
// notice with system.notify() and records the e-mail headers, index
// records and short message it left. Writes the result as JSON to
// argv[0]. Not a test on its own (the runner only runs *.js files).

load("sbbsdefs.js");
load("userdefs.js");

var result = { data_dir: system.data_dir, msg_settings: msg_area.settings,
	sys_settings: system.settings };

// Everything below writes to the data directory: refuse to run unless it
// is the scratch tree next to this ctrl directory
(function () {
	var root = fullpath(system.ctrl_dir + "..");
	if (fullpath(system.data_dir).indexOf(root) != 0 || root.length < 8)
		throw new Error("data_dir " + system.data_dir + " is not under " + root);
})();

var mb = null;
try {
	var u = system.new_user("Fwdkeeper");
	if (typeof u != "object")
		throw new Error("new_user returned " + u);
	u.netmail = "fwdkeep@example.com";
	u.settings |= USER_NETMAIL;
	result.user = u.number;
	system.get_telegram(u.number);
	result.notified = system.notify(u.number, "Forward keep subject", "Forward keep body");
	result.telegram = system.get_telegram(u.number);
	result.mail_waiting = new User(u.number).stats.mail_waiting;

	mb = new MsgBase("mail");
	if (!mb.open())
		throw new Error("open mail: " + mb.last_error);
	result.mail_file = mb.file;
	result.total_msgs = mb.total_msgs;
	result.headers = [];
	var hdrs = mb.get_all_msg_headers(/* include votes: */ false, /* expand: */ false);
	for (var n in hdrs) {
		var h = hdrs[n];
		result.headers.push({
			number: h.number,
			to: h.to,
			to_ext: h.to_ext === undefined ? null : h.to_ext,
			to_net_type: h.to_net_type === undefined ? null : h.to_net_type,
			to_net_addr: h.to_net_addr === undefined ? null : h.to_net_addr,
			subject: h.subject,
			body: mb.get_msg_body(h.number)
		});
	}
	result.index = [];
	var idx = mb.get_index();
	for (var i = 0; i < idx.length; i++)
		result.index.push({ number: idx[i].number, to: idx[i].to });
} catch (e) {
	result.exception = String(e) + (e && e.fileName ? " " + e.fileName + ":" + e.lineNumber : "");
} finally {
	if (mb !== null)
		mb.close();
}

var f = new File(argv[0]);
if (!f.open("w"))
	throw new Error("Error " + f.error + " creating " + f.name);
f.write(JSON.stringify(result));
f.close();
