// Run by limit.js in a child jsexec whose ctrl directory is a scratch copy
// with its own (new, empty) user base and mail base; writes
// system.max_msgs_waiting and every user's User.mailbox_full() answers as
// JSON to argv[0].
// Not a test on its own (the runner only runs *.js files).

load("sbbsdefs.js");
load("userdefs.js");
load("smbdefs.js");

// Users and mail are created below: refuse to run unless the data
// directory is inside the scratch tree that holds the ctrl directory
(function () {
	var root = fullpath(system.ctrl_dir + "..");
	if (fullpath(system.data_dir).indexOf(root) != 0 || root.length < 8)
		throw new Error("data_dir " + system.data_dir + " is not under " + root);
})();

var result = { limit: system.max_msgs_waiting, users: {} };
var mb = new MsgBase("mail");
if (!mb.open())
	throw new Error("open mail: " + mb.last_error);

// msgs: the attributes of each message to store for the user
function make_user(key, msgs, exemptions)
{
	// argv[1]: an alias prefix, for a second run in the same scratch tree
	var u = system.new_user((argv[1] || "Mboxfull") + String.fromCharCode(97 + Object.keys(result.users).length));
	if (typeof u != "object")
		throw new Error("new_user(" + key + ") returned " + u);
	if (exemptions)
		u.security.exemptions |= exemptions;
	msgs.forEach(function (attr, i) {
		var hdr = { to: u.alias, to_ext: String(u.number), from: "Test", subject: key + " " + i, attr: attr };
		if (!mb.save_msg(hdr, "body " + i + "\r\n"))
			throw new Error("save_msg for " + key + ": " + mb.last_error);
		// save_msg() does not store the deleted attribute
		if ((attr & MSG_DELETE) && !mb.remove_msg(mb.last_msg))
			throw new Error("remove_msg for " + key + ": " + mb.last_error);
	});
	u = new User(u.number);
	result.users[key] = {
		waiting: u.stats.mail_waiting,
		full: u.mailbox_full(),
		full_false: u.mailbox_full(false),
		full_system: u.mailbox_full(true)
	};
}

make_user("none", []);
make_user("under", [0, 0]);
make_user("at", [0, 0, 0]);
make_user("over", [0, 0, 0, 0]);
make_user("over_w", [0, 0, 0, 0], UFLAG_W);
make_user("spam", [0, 0, 0, MSG_SPAM]);
make_user("deleted", [0, 0, 0, MSG_DELETE]);
mb.close();

var f = new File(argv[0]);
if (!f.open("w"))
	throw new Error("Error " + f.error + " creating " + f.name);
f.write(JSON.stringify(result));
f.close();
