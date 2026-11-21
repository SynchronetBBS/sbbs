// Run by rules.js in a child jsexec whose ctrl directory is a scratch copy
// with its own (new, empty) user base; writes every user's
// User.can_send_mail() answers as JSON to argv[0].
// Not a test on its own (the runner only runs *.js files).

load("sbbsdefs.js");
load("userdefs.js");
load("smbdefs.js");

// Users are created below: refuse to run unless the ctrl and data
// directories are both inside the scratch tree, which is where the parent
// asked for the result file (argv[0])
(function () {
	var base = fullpath(String(argv[0]).replace(/[^\/\\]*$/, ""));
	var ctrl = fullpath(system.ctrl_dir), data = fullpath(system.data_dir);
	if (base.length < 8 || ctrl.indexOf(base) != 0 || data.indexOf(base) != 0)
		throw new Error("ctrl_dir " + ctrl + " or data_dir " + data + " is not under " + base);
})();

var result = { users: {}, can: {} };

// sent: [count, feedback] for User.sent_email(), which counts the
// messages against the totals and against today
function make_user(key, level, restrictions, exemptions, sent)
{
	// Not named for the key: an alias containing "sysop" is refused
	var u = system.new_user("Cansend" + String.fromCharCode(97 + Object.keys(result.users).length));
	if (typeof u != "object")
		throw new Error("new_user(" + key + ") returned " + u);
	// Assigned, not added: a new user starts with the system's new-user
	// defaults, which differ from one system to the next
	u.security.level = level;
	u.security.restrictions = restrictions || 0;
	u.security.exemptions = exemptions || 0;
	if (sent && !u.sent_email(sent[0], sent[1]))
		throw new Error("sent_email failed for " + key);
	result.users[key] = u.number;
}

// The numbers rules.js relies on: 1 and 3 are sysops, 2 and 4 are not
make_user("sysop", 90);
make_user("normal", 50);
make_user("sysop2", 90);
make_user("normal2", 50);
make_user("e", 50, UREST_EMAIL);
make_user("s", 50, UREST_EMAIL_SYSOP);
make_user("es", 50, UREST_EMAIL | UREST_EMAIL_SYSOP);
make_user("m", 50, UREST_SEND_NETMAIL);
make_user("sysop_es", 90, UREST_EMAIL | UREST_EMAIL_SYSOP);
make_user("exempt", 50, 0, UEXEMPT_EMAIL_LIMIT, [2, false]);
make_user("at_limit", 50, 0, 0, [2, false]);
make_user("es_mailed", 50, UREST_EMAIL | UREST_EMAIL_SYSOP, 0, [1, false]);
make_user("es_fed_back", 50, UREST_EMAIL | UREST_EMAIL_SYSOP, 0, [1, true]);
make_user("sysop_at_limit", 90, 0, 0, [1, false]);

for (var key in result.users) {
	var u = new User(result.users[key]);
	var row = {
		fido: u.can_send_mail(NET_FIDO),
		inet: u.can_send_mail(NET_INTERNET),
		qwk: u.can_send_mail(NET_QWK),
		local: u.can_send_mail(NET_NONE),
		local_x2: u.can_send_mail(NET_NONE, 0, 2),
		local_x3: u.can_send_mail(NET_NONE, 0, 3)
	};
	for (var to = 1; to <= 4; to++)
		row["to" + to] = u.can_send_mail(NET_NONE, to);
	result.can[key] = row;
}

var f = new File(argv[0]);
if (!f.open("w"))
	throw new Error("Error " + f.error + " creating " + f.name);
f.write(JSON.stringify(result));
f.close();
