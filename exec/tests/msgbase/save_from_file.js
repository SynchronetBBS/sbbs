// MsgBase.save_msg_from_file(header, body_path [, rcpt_list]): the body is
// read natively from a file (never a JS string) and stored as the message
// body; an entry of rcpt_list may carry its own "netattr", which replaces
// the header's network attributes for that recipient's copy only (for
// save_msg() as well).

load("smbdefs.js");   /* for NETMSG_KILLSENT */
mkpath(system.temp_dir);

var stamp = Date.now();
var base_path = system.temp_dir + "test_msgbase_savefile_" + stamp;
var body_path = system.temp_dir + "test_msgbase_savefile_body_" + stamp + ".txt";
var nul_path = system.temp_dir + "test_msgbase_savefile_nul_" + stamp + ".txt";
var EXTENSIONS = [".shd", ".sdt", ".sid", ".sha", ".sda", ".ini", ".hash"];

function cleanup() {
	EXTENSIONS.forEach(function (ext) {
		try { file_remove(base_path + ext); } catch (e) { /* ignore */ }
	});
	[body_path, nul_path].forEach(function (name) {
		try { file_remove(name); } catch (e) { /* ignore */ }
	});
}

function write_file(name, data) {
	var f = new File(name);
	if (!f.open("wb"))
		throw new Error("cannot create " + name);
	f.write(data);
	f.close();
}

try {
	/* a 6 MB MIME text body, without trailing white space */
	var part = "--x\r\nContent-Type: text/plain\r\n\r\n"
		+ "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+-\r\n";
	var text = part;
	while (text.length < 6 * 1024 * 1024)
		text += text;
	text += "--x--";
	write_file(body_path, text);
	var want = md5_calc(text);

	var mb = new MsgBase(base_path, /* is_path: */ true, /* is_email: */ true);
	if (!mb.open())
		throw new Error("open: " + mb.last_error);

	var hdr = { to: "Alice", to_ext: "60001", from: "Bob", from_ext: "60002",
		subject: "big", mime_version: "1.0",
		content_type: 'multipart/mixed; boundary="x"' };
	if (mb.save_msg_from_file(hdr, body_path) !== true)
		throw new Error("save_msg_from_file: " + mb.last_error);
	var num = mb.last_msg;
	var h = mb.get_msg_header(num);
	if (h.subject != "big" || h.to_ext != "60001" || h.mime_version != "1.0" || h.content_type != hdr.content_type)
		throw new Error("header not saved: " + JSON.stringify(h));
	/* the message store terminates the body with one CRLF */
	var stored = mb.get_msg_body(num, false, false, false);
	if (stored.slice(-2) != "\r\n" || md5_calc(stored.slice(0, -2)) != want)
		throw new Error("stored body differs from the file (" + stored.length + " vs " + text.length + ")");

	/* save_msg() stores the MIME header properties the same way */
	if (!mb.save_msg(hdr, "plain body"))
		throw new Error("save_msg with MIME headers: " + mb.last_error);
	h = mb.get_msg_header(mb.last_msg);
	if (h.mime_version != "1.0" || h.content_type != hdr.content_type)
		throw new Error("save_msg MIME headers: " + JSON.stringify(h));

	/* a recipient list yields one message per entry */
	var before = mb.total_msgs;
	if (mb.save_msg_from_file({ from: "Bob", from_ext: "60002", subject: "two" }, body_path
		, [ { to: "Carol", to_ext: "60003" }, { to: "Dave", to_ext: "60004" } ]) !== true)
		throw new Error("save with rcpt_list: " + mb.last_error);
	if (mb.total_msgs != before + 2)
		throw new Error("rcpt_list: " + (mb.total_msgs - before) + " messages saved (expected 2)");
	if (mb.last_error)
		throw new Error("last_error after a successful rcpt_list save: " + mb.last_error);

	/* a nonexistent file, an empty-recipient array and a bad argument fail */
	var missing = system.temp_dir + "no_such_file_" + stamp;
	if (mb.save_msg_from_file(hdr, missing) !== false)
		throw new Error("a missing file did not return false");
	if (!/opening body file/.test(mb.last_error))
		throw new Error("missing file last_error: " + mb.last_error);
	before = mb.total_msgs;
	write_file(nul_path, "a\0b");
	if (mb.save_msg_from_file(hdr, nul_path) !== false)
		throw new Error("a file containing NUL did not return false");
	if (mb.total_msgs != before)
		throw new Error("a failed save_msg_from_file stored a message");
	if (!/NUL/.test(mb.last_error))
		throw new Error("NUL file last_error: " + mb.last_error);

	/* MIME header lines are not duplicated by round trips through
	 * put_msg_header() or by saving a header read back from the base */
	function check_once(label, hh) {
		var mime = 0, ctype = 0;
		hh.field_list.forEach(function (f) {
			if (/^MIME-Version:/i.test(f.data))
				mime++;
			else if (/^Content-Type:/i.test(f.data))
				ctype++;
		});
		if (mime != 1 || ctype != 1)
			throw new Error(label + ": " + mime + " MIME-Version and " + ctype + " Content-Type lines");
		if (hh.mime_version != "1.0" || hh.content_type != "text/plain")
			throw new Error(label + ": properties " + hh.mime_version + " / " + hh.content_type);
	}
	function roundtrip(label, save_hdr) {
		if (!mb.save_msg(save_hdr, "mime body"))
			throw new Error(label + " save: " + mb.last_error);
		var n = mb.last_msg;
		check_once(label + " saved", mb.get_msg_header(n));
		for (var r = 0; r < 3; r++) {
			var g = mb.get_msg_header(false, n, false);
			if (!g || !mb.put_msg_header(g))
				throw new Error(label + " put_msg_header: " + mb.last_error);
		}
		check_once(label + " after 3 round trips", mb.get_msg_header(n));
		var rb = mb.get_msg_header(false, n, false);
		if (!mb.save_msg(rb, "again"))
			throw new Error(label + " resave: " + mb.last_error);
		check_once(label + " resaved", mb.get_msg_header(mb.last_msg));
	}
	var common = { to: "Alice", to_ext: "60001", from: "Bob", from_ext: "60002", subject: "rt" };
	var by_props = { mime_version: "1.0", content_type: "text/plain" };
	var by_list = { field_list: [ { type: "OtherHeader", data: "MIME-Version: 1.0" },
		{ type: "OtherHeader", data: "Content-Type: text/plain" } ] };
	for (var k in common) {
		by_props[k] = common[k];
		by_list[k] = common[k];
	}
	roundtrip("properties", by_props);
	roundtrip("field_list only", by_list);

	/* per-recipient netattr, from a file and from a string */
	var nh = { from: "Bob", from_ext: "60002", subject: "kill", netattr: 0 };
	var rcpts = [ { to: "Carol", to_ext: "60003", netattr: NETMSG_KILLSENT },
		{ to: "Dave", to_ext: "60004" } ];
	var saves = [
		function () { return mb.save_msg_from_file(nh, body_path, rcpts); },
		function () { return mb.save_msg(nh, "body", rcpts); }
	];
	saves.forEach(function (save, i) {
		var first = mb.total_msgs;
		var last_before = mb.last_msg;
		if (save() !== true)
			throw new Error("save " + i + ": " + mb.last_error);
		var hdrs = [];
		for (var n = last_before + 1; n <= mb.last_msg; n++) {
			var x = mb.get_msg_header(n);
			if (x)
				hdrs.push(x);
		}
		if (hdrs.length != 2)
			throw new Error("save " + i + ": " + hdrs.length + " messages");
		if (!(hdrs[0].netattr & NETMSG_KILLSENT) || hdrs[0].to != "Carol")
			throw new Error("save " + i + ": first copy lacks KILLSENT: " + JSON.stringify(hdrs[0]));
		if ((hdrs[1].netattr & NETMSG_KILLSENT) || hdrs[1].to != "Dave")
			throw new Error("save " + i + ": second copy has KILLSENT: " + JSON.stringify(hdrs[1]));
	});

	/* without a per-recipient value the copy inherits the header's */
	var before_last = mb.last_msg;
	if (!mb.save_msg({ from: "Bob", from_ext: "60002", subject: "inh", netattr: NETMSG_KILLSENT }, "body"
		, [ { to: "Carol", to_ext: "60003" } ]))
		throw new Error("inherit save: " + mb.last_error);
	if (!(mb.get_msg_header(mb.last_msg).netattr & NETMSG_KILLSENT))
		throw new Error("the copy did not inherit the header's netattr");

	mb.close();
} finally {
	cleanup();
}
