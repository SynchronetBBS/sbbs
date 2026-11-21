// Max Messages Waiting (max_msgs_waiting in the [mail] section of
// msgs.ini, system.max_msgs_waiting) and User.mailbox_full()
// (user_mailbox_full(), which the mail server, the terminal's e-mail
// commands and WebMail call before storing mail for a local user),
// against scratch configurations with a scratch user base and mail base:
// a child jsexec per configuration (see child.inc for the users; the
// limit there is 3 unless said otherwise).
//
// A mailbox is full with MORE than the limit of messages (spam included,
// deleted messages not), unless the limit is 0 or the user has the W
// exemption; mail from the system itself is never refused.
//
// Until msgs.ini has the key, the limit is the mail server's old
// MaxMsgsWaiting in sbbs.ini; update.js (max_msgs_waiting_upgrade.js)
// carries a non-default value over, once.

load(js.exec_dir + "../../../load/max_msgs_waiting_upgrade.js");

var failures = [];
var lib = load({}, js.exec_dir + "../../inet_domains/config/scratch.inc");

function check(desc, got, expect)
{
	var g = JSON.stringify(got);
	var e = JSON.stringify(expect);
	if (g !== e)
		failures.push(format("%s = %s, expected %s", desc, g, e));
}

function read_text(path)
{
	var f = new File(path);
	if (!f.open("r"))
		return null;
	var text = f.read();
	f.close();
	return text;
}

// The [mail] max_msgs_waiting value of the scratch msgs.ini (undefined
// when absent)
function new_key(sc)
{
	var f = new File(sc.ctrl + "msgs.ini");
	if (!f.open("r"))
		throw new Error("Error " + f.error + " opening " + f.name);
	var value = f.iniGetValue("mail", "max_msgs_waiting");
	f.close();
	return value;
}

// now, old: the msgs.ini and sbbs.ini values (undefined: no key);
// steps(sc, child): the checks, where child() runs child.inc
function scratch(name, now, old, steps)
{
	var sc = lib.scratch_ctrl("mboxfull_" + name);
	try {
		sc.setup();
		lib.ini_edit(sc.ctrl + "msgs.ini", function (f) {
			f.iniRemoveKey("mail", "max_msgs_waiting");
			if (now !== undefined)
				f.iniSetValue("mail", "max_msgs_waiting", now);
		});
		// The file this host's servers would read (sbbs.<hostname>.ini
		// when the tree has one)
		lib.ini_edit(file_cfgname(sc.ctrl, "sbbs.ini"), function (f) {
			f.iniRemoveKey("Mail", "MaxMsgsWaiting");
			if (old !== undefined)
				f.iniSetValue("Mail", "MaxMsgsWaiting", old);
		});
		var runs = 0;
		steps(sc, function (desc) {
			// A new set of users per run (aliases must be unique)
			var r = sc.run(js.exec_dir + "child.inc", ["Mboxrun" + String.fromCharCode(97 + runs++)]);
			check(name + " " + desc + ": child exit code", r.exit_code, 0);
			return r;
		});
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

// The limit in msgs.ini (the old key, set differently, is ignored)
scratch("limit3", 3, 1, function (sc, child) {
	var r = child("");
	check("limit3: system.max_msgs_waiting", r.limit, 3);
	check("limit3: waiting", [r.users.none.waiting, r.users.under.waiting, r.users.at.waiting
		, r.users.over.waiting, r.users.spam.waiting, r.users.deleted.waiting], [0, 2, 3, 4, 4, 3]);
	check("limit3: full", fulls(r), { none: 0, under: 0, at: 0, over: 1, over_w: 0, spam: 1, deleted: 0 });
	for (var key in r.users) {
		check("limit3: " + key + " (false) as no argument", r.users[key].full_false, r.users[key].full);
		check("limit3: " + key + " from the system", r.users[key].full_system, 0);
	}
});

// 0: no limit
scratch("limit0", 0, 3, function (sc, child) {
	var r = child("");
	check("limit0: system.max_msgs_waiting", r.limit, 0);
	check("limit0: full", fulls(r), { none: 0, under: 0, at: 0, over: 0, over_w: 0, spam: 0, deleted: 0 });
});

// Neither key: the default
scratch("default", undefined, undefined, function (sc, child) {
	var before = read_text(sc.ctrl + "msgs.ini");
	check("default: update status", upgrade_max_msgs_waiting(sc.ctrl).status, "none");
	check("default: msgs.ini unchanged", read_text(sc.ctrl + "msgs.ini") === before, true);
	var r = child("");
	check("default: system.max_msgs_waiting", r.limit, 100);
	check("default: full", fulls(r).over, 0);
});

// The old key at its default: not carried over
scratch("old_default", undefined, 100, function (sc, child) {
	var before = read_text(sc.ctrl + "msgs.ini");
	check("old default: update status", upgrade_max_msgs_waiting(sc.ctrl).status, "none");
	check("old default: msgs.ini unchanged", read_text(sc.ctrl + "msgs.ini") === before, true);
	check("old default: no key", new_key(sc), undefined);
});

// Only the old key: used as it is before update.js, carried over by it,
// once, and the same limit afterwards
scratch("old", undefined, 3, function (sc, child) {
	var old_path = file_cfgname(sc.ctrl, "sbbs.ini");
	var old_before = read_text(old_path);
	var before = read_text(sc.ctrl + "msgs.ini");
	var r = child("before");
	check("old: limit before update", r.limit, 3);
	check("old: full before update", fulls(r), { none: 0, under: 0, at: 0, over: 1, over_w: 0, spam: 1, deleted: 0 });

	var result = upgrade_max_msgs_waiting(sc.ctrl, /* dry_run: */ true);
	check("old: dry run", [result.status, result.value], ["converted", 3]);
	check("old: dry run changes nothing", read_text(sc.ctrl + "msgs.ini") === before, true);

	result = upgrade_max_msgs_waiting(sc.ctrl);
	check("old: update", [result.status, result.value], ["converted", 3]);
	check("old: new key", new_key(sc), 3);
	check("old: sbbs.ini unchanged", read_text(old_path) === old_before, true);

	var after = read_text(sc.ctrl + "msgs.ini");
	check("old: second update", upgrade_max_msgs_waiting(sc.ctrl).status, "none");
	check("old: second update changes nothing", read_text(sc.ctrl + "msgs.ini") === after, true);

	// The old key no longer matters
	lib.ini_edit(old_path, function (f) { f.iniSetValue("Mail", "MaxMsgsWaiting", 50); });
	check("old: third update", upgrade_max_msgs_waiting(sc.ctrl).status, "none");
	r = child("after");
	check("old: limit after update", r.limit, 3);
	check("old: full after update", fulls(r).over, 1);
});

// An old "no limit" is carried over too
scratch("old0", undefined, 0, function (sc, child) {
	check("old 0: limit before update", child("before").limit, 0);
	var result = upgrade_max_msgs_waiting(sc.ctrl);
	check("old 0: update", [result.status, result.value], ["converted", 0]);
	check("old 0: new key", new_key(sc), 0);
	check("old 0: limit after update", child("after").limit, 0);
});

// A missing msgs.ini is an error and nothing is created
scratch("no_msgs_ini", undefined, 3, function (sc, child) {
	file_remove(sc.ctrl + "msgs.ini");
	check("no msgs.ini: status", upgrade_max_msgs_waiting(sc.ctrl).status, "error");
	check("no msgs.ini: not created", file_exists(sc.ctrl + "msgs.ini"), false);
});

if (failures.length)
	throw new Error(failures.length + " failure(s):\n" + failures.join("\n"));
