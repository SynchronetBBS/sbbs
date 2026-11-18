// MsgBase.purge() frees the messages flagged deleted in a path-opened
// base (headers, data and index records) and returns how many it freed.
// Surviving messages keep their numbers; data shared by several headers
// (a message saved with a recipient list) is freed by its last header.
// The system mail base and configured sub-boards are refused.
//
// Set the environment variable MSGBASE_TEST_KEEP to keep the scratch
// base (its path is printed) for inspection with chksmb.

load("sbbsdefs.js");  /* for MSG_DELETE, MSG_PERMANENT */
mkpath(system.temp_dir);

var keep = typeof env == "object" && env.MSGBASE_TEST_KEEP;
var stamp = Date.now();
var path = system.temp_dir + "test_msgbase_purge_" + stamp;
var file_path = system.temp_dir + "test_msgbase_purge_file_" + stamp;
var EXTENSIONS = [".shd", ".sdt", ".sid", ".sha", ".sda", ".ini", ".hash", ".lock"];

function cleanup() {
	[path, file_path].forEach(function (base) {
		EXTENSIONS.forEach(function (ext) {
			try { file_remove(base + ext); } catch (e) { /* ignore */ }
		});
	});
}

function body(n) {
	return "Body of message " + n + ".\r\nLine 2 of message " + n + ".\r\n";
}

function save(mb, n) {
	if (!mb.save_msg({ to: "Alice", to_ext: "60001", from: "Bob", from_ext: "60002",
		subject: "s" + n }, body(n)))
		throw new Error("save_msg " + n + ": " + mb.last_error);
	return mb.last_msg;
}

function check_msg(mb, number, n) {
	var h = mb.get_msg_header(number);
	if (h == null)
		throw new Error("message #" + number + " is missing: " + mb.last_error);
	if (h.subject != "s" + n)
		throw new Error("message #" + number + " subject: " + h.subject);
	var b = mb.get_msg_body(number);
	if (b != body(n))
		throw new Error("message #" + number + " body: " + JSON.stringify(b));
}

/* purge() must fail with last_error matching 'reason' and not 'other' */
var NOT_ALLOWED = /not allowed on the mail base or a sub-board/;
var NOT_OPEN = /not open/;
var FILE_BASE = /file base/;
var SMB_FILE_DIRECTORY = (1 << 3); /* smbdefs.h status attribute */

function expect_refused(mb, label, reason, other) {
	if (mb.purge() !== false)
		throw new Error("purge() of " + label + " was not refused");
	if (!reason.test(mb.last_error) || other.test(mb.last_error))
		throw new Error("purge() of " + label + " refused for the wrong reason: "
			+ mb.last_error);
}

function expect_maintained(code_or_path, is_path, label) {
	expect_refused(new MsgBase(code_or_path, is_path), label, NOT_ALLOWED, NOT_OPEN);
}

try {
	/* bases with their own maintenance are refused before the open check,
	 * so none of them is ever opened here */
	var sep = system.data_dir.slice(-1); /* the platform's own separator */
	var mail_path = system.data_dir + "mail";
	expect_maintained("mail", false, "the mail base");
	expect_maintained(mail_path, true, "the mail base by path");
	expect_maintained(mail_path.toUpperCase(), true
		, "the mail base by path in another letter case");
	expect_maintained(system.data_dir + "subs" + sep + ".." + sep + "mail", true
		, "the mail base by a path with a .. component");
	var code;
	for (code in msg_area.sub)
		break;
	if (code !== undefined) {
		expect_maintained(code, false, "sub-board " + code);
		expect_maintained(msg_area.sub[code].data_dir + code, true
			, "sub-board " + code + " by path");
	}

	/* a file base (status attribute SMB_FILE_DIRECTORY) has larger index
	 * records than a message base: it is refused before its index is read.
	 * Made here by setting the attribute in a scratch message base's status
	 * (smbstatus_t.attr, 22 bytes in, after the 8-byte smbhdr_t) */
	var fb = new MsgBase(file_path, /* is_path: */ true);
	if (!fb.open())
		throw new Error("open file base: " + fb.last_error);
	save(fb, 1);
	save(fb, 2);
	if (!fb.remove_msg(fb.last_msg))
		throw new Error("remove_msg in file base: " + fb.last_error);
	fb.close();
	var shd = new File(file_path + ".shd");
	if (!shd.open("r+b"))
		throw new Error("open " + shd.name + ": " + shd.error);
	shd.position = 30;
	var attr = shd.readBin(2);
	shd.position = 30;
	shd.writeBin(attr | SMB_FILE_DIRECTORY, 2);
	shd.close();
	var sid_size = file_size(file_path + ".sid");
	if (!fb.open())
		throw new Error("reopen file base: " + fb.last_error);
	if (!(fb.attributes & SMB_FILE_DIRECTORY))
		throw new Error("test setup: attributes = " + fb.attributes);
	expect_refused(fb, "a file base", FILE_BASE, NOT_ALLOWED);
	if (fb.total_msgs != 2 || file_size(file_path + ".sid") != sid_size)
		throw new Error("a refused purge changed the file base");
	fb.close();

	var mb = new MsgBase(path, /* is_path: */ true, /* is_email: */ true);
	expect_refused(mb, "a base that is not open", NOT_OPEN, NOT_ALLOWED);
	if (!mb.open())
		throw new Error("open: " + mb.last_error);
	if (mb.purge() !== 0)
		throw new Error("purge() of an empty base did not return 0: " + mb.last_error);

	/* five messages, the second and fourth removed */
	var num = [];
	var i;
	for (i = 1; i <= 5; i++)
		num[i] = save(mb, i);
	if (!mb.remove_msg(num[2]) || !mb.remove_msg(num[4]))
		throw new Error("remove_msg: " + mb.last_error);

	/* a base locked by another process is not purged, nor its lock taken */
	var lock = new File(path + ".lock");
	if (!lock.open("w"))
		throw new Error("create " + lock.name + ": " + lock.error);
	lock.close();
	var retry_time = mb.retry_time;
	mb.retry_time = 1;
	var purged = mb.purge();
	mb.retry_time = retry_time;
	if (!file_exists(lock.name))
		throw new Error("purge() removed another process's lock file");
	file_remove(lock.name);
	if (purged !== false || !mb.last_error)
		throw new Error("purge() of a locked base: " + purged + " " + mb.last_error);
	if (mb.total_msgs != 5 || !(mb.get_msg_header(num[2]).attr & MSG_DELETE))
		throw new Error("a refused purge changed the base");

	purged = mb.purge();
	if (file_exists(lock.name))
		throw new Error("purge() left the base locked");
	if (purged !== 2)
		throw new Error("purge() returned " + purged + " (expected 2): " + mb.last_error);
	if (mb.total_msgs != 3)
		throw new Error("total_msgs is " + mb.total_msgs + " after the purge");
	if (mb.get_msg_header(num[2]) !== null || mb.get_msg_header(num[4]) !== null)
		throw new Error("a purged message still has a header");
	check_msg(mb, num[1], 1);
	check_msg(mb, num[3], 3);
	check_msg(mb, num[5], 5);
	var idx = mb.get_index();
	if (idx.length != 3 || idx[0].number != num[1] || idx[1].number != num[3]
		|| idx[2].number != num[5])
		throw new Error("index after the purge: " + JSON.stringify(idx));
	if (mb.purge() !== 0)
		throw new Error("a second purge() did not return 0: " + mb.last_error);

	/* unlike daily mail maintenance, a deleted permanent message is purged;
	 * a permanent message not deleted is untouched */
	var n;
	for (n = 6; n <= 8; n += 2) {
		if (!mb.save_msg({ to: "Alice", to_ext: "60001", from: "Bob", subject: "s" + n,
			attr: MSG_PERMANENT }, body(n)))
			throw new Error("save_msg permanent: " + mb.last_error);
		num[n] = mb.last_msg;
	}
	if (!mb.remove_msg(num[8]))
		throw new Error("remove_msg permanent: " + mb.last_error);
	if ((mb.get_msg_header(num[8]).attr & (MSG_PERMANENT | MSG_DELETE))
		!= (MSG_PERMANENT | MSG_DELETE))
		throw new Error("test setup: message not flagged permanent and deleted");
	purged = mb.purge();
	if (purged !== 1)
		throw new Error("purge() of a deleted permanent message returned " + purged
			+ ": " + mb.last_error);
	if (mb.get_msg_header(num[8]) !== null)
		throw new Error("the deleted permanent message still has a header");
	check_msg(mb, num[6], 6);
	if ((mb.get_msg_header(num[6]).attr & (MSG_PERMANENT | MSG_DELETE)) != MSG_PERMANENT)
		throw new Error("the permanent message's flags changed");

	/* two headers sharing one body */
	var before = mb.total_msgs;
	if (!mb.save_msg({ from: "Bob", from_ext: "60002", subject: "shared" }, "Shared body.\r\n"
		, [ { to: "Carol", to_ext: "60003" }, { to: "Dave", to_ext: "60004" } ]))
		throw new Error("save_msg with rcpt_list: " + mb.last_error);
	if (mb.total_msgs != before + 2)
		throw new Error("rcpt_list saved " + (mb.total_msgs - before) + " messages");
	var shared = mb.get_index().slice(-2);
	var first = shared[0].number;
	var second = shared[1].number;
	if (mb.get_msg_header(first).data_offset != mb.get_msg_header(second).data_offset)
		throw new Error("test setup: the recipients do not share one body");
	if (!mb.remove_msg(first))
		throw new Error("remove_msg shared: " + mb.last_error);
	if (mb.purge() !== 1)
		throw new Error("purge() of one shared header: " + mb.last_error);
	if (mb.get_msg_header(first) !== null)
		throw new Error("the purged recipient's header remains");
	if (mb.get_msg_body(second) != "Shared body.\r\n")
		throw new Error("the other recipient's body was freed: "
			+ JSON.stringify(mb.get_msg_body(second)));
	if (!mb.remove_msg(second))
		throw new Error("remove_msg second shared: " + mb.last_error);
	if (mb.purge() !== 1)
		throw new Error("purge() of the last shared header: " + mb.last_error);

	/* a message saved afterwards reads back */
	num[7] = save(mb, 7);
	check_msg(mb, num[7], 7);
	check_msg(mb, num[1], 1);
	check_msg(mb, num[3], 3);
	check_msg(mb, num[5], 5);
	check_msg(mb, num[6], 6);
	if (mb.total_msgs != 5)
		throw new Error("total_msgs is " + mb.total_msgs + " at the end");
	mb.close();
	if (keep)
		print("scratch base kept: " + path);
} finally {
	if (!keep)
		cleanup();
}
