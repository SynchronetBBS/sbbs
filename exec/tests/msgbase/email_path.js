// A path-opened MsgBase can be an e-mail base:
// new MsgBase(path, /* is_path: */ true, /* is_email: */ true)
// creates a new base with the SMB_EMAIL status attribute, so its index
// records carry user numbers (from to_ext/from_ext) and duplicate
// messages are not rejected. For an existing base the stored attribute
// is what counts: is_email=true on a non-e-mail base fails to open.
// Saving into such a base must not send a notice to the recipient.

load("smbdefs.js");
mkpath(system.temp_dir);

var stamp = Date.now();
var path = system.temp_dir + "test_msgbase_email_path_" + stamp;
var path2 = system.temp_dir + "test_msgbase_email_path2_" + stamp;
var EXTENSIONS = [".shd", ".sdt", ".sid", ".sha", ".sda", ".ini", ".hash"];

function cleanup() {
	[path, path2].forEach(function (base) {
		EXTENSIONS.forEach(function (ext) {
			try { file_remove(base + ext); } catch (e) { /* ignore */ }
		});
	});
}

/* A notice to user 60001 would be appended to this file by savemsg(),
 * provided the directory exists (it does on any running BBS) */
mkpath(system.data_dir + "msgs");
var notice = system.data_dir + "msgs/60001.msg";
var notice_size = file_size(notice);

try {
	var mb = new MsgBase(path, /* is_path: */ true, /* is_email: */ true);
	if (!mb.open())
		throw new Error("open: " + mb.last_error);
	if (!(mb.attributes & SMB_EMAIL))
		throw new Error("not e-mail type: attributes = " + mb.attributes);
	var H = { to: "A", to_ext: "60001", from: "B", from_ext: "60002",
		subject: "s", id: "<same@example>" };
	if (!mb.save_msg(H, "same body"))
		throw new Error(mb.last_error);
	if (!mb.save_msg(H, "same body"))
		throw new Error("dupe rejected: " + mb.last_error);
	var idx = mb.get_index();
	if (idx.length != 2 || idx[0].to != 60001 || idx[0].from != 60002
		|| idx[1].to != 60001 || idx[1].from != 60002)
		throw new Error("index not by user number: " + JSON.stringify(idx));
	mb.close();

	if (file_size(notice) != notice_size)
		throw new Error("a notice was sent to the recipient: " + notice);

	/* reopening without is_email still reports SMB_EMAIL */
	mb = new MsgBase(path, /* is_path: */ true);
	if (!mb.open())
		throw new Error("reopen: " + mb.last_error);
	if (!(mb.attributes & SMB_EMAIL))
		throw new Error("reopened base not e-mail type: attributes = " + mb.attributes);
	if (mb.total_msgs != 2)
		throw new Error("total_msgs = " + mb.total_msgs + " (expected 2)");
	mb.close();

	/* a base created as non-e-mail cannot be opened with is_email=true */
	var mb2 = new MsgBase(path2, /* is_path: */ true, /* is_email: */ false);
	if (!mb2.open())
		throw new Error("open non-e-mail: " + mb2.last_error);
	if (!mb2.save_msg({ to: "A", from: "B", subject: "s" }, "body"))
		throw new Error("save non-e-mail: " + mb2.last_error);
	if (mb2.attributes & SMB_EMAIL)
		throw new Error("non-e-mail base has SMB_EMAIL");
	mb2.close();

	mb2 = new MsgBase(path2, /* is_path: */ true, /* is_email: */ true);
	if (mb2.open()) {
		mb2.close();
		throw new Error("opened a non-e-mail base with is_email=true");
	}
	if (!mb2.last_error)
		throw new Error("open failed without setting last_error");
} finally {
	cleanup();
}
