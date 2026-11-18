// MsgBase.copy_msg(number, dest [, header_changes]) and
// MsgBase.move_msg(number, dest [, header_changes]) between two
// path-opened e-mail bases: the copy keeps every header field and the
// body as stored, gets a number and index record of the destination's
// own, and has its thread links cleared; a move also flags the source
// deleted (and drops its claim on any attached files).

load("sbbsdefs.js");  /* for MSG_DELETE */
load("smbdefs.js");   /* for MSG_FILEATTACH, MSG_KILLFILE */
mkpath(system.temp_dir);

var stamp = Date.now();
var src_path = system.temp_dir + "test_msgbase_copy_src_" + stamp;
var dst_path = system.temp_dir + "test_msgbase_copy_dst_" + stamp;
var plain_path = system.temp_dir + "test_msgbase_copy_plain_" + stamp;
var EXTENSIONS = [".shd", ".sdt", ".sid", ".sha", ".sda", ".ini", ".hash"];

function cleanup() {
	[src_path, dst_path, plain_path].forEach(function (base) {
		EXTENSIONS.forEach(function (ext) {
			try { file_remove(base + ext); } catch (e) { /* ignore */ }
		});
	});
}

var BODY = "Hello from the copy_msg test.\r\nLine 2.\r\n";

try {
	var src = new MsgBase(src_path, /* is_path: */ true, /* is_email: */ true);
	var dst = new MsgBase(dst_path, /* is_path: */ true, /* is_email: */ true);
	if (!src.open())
		throw new Error("open src: " + src.last_error);
	if (!dst.open())
		throw new Error("open dst: " + dst.last_error);

	/* a plain copy */
	if (!src.save_msg({ to: "Alice", to_ext: "60001", from: "Bob", from_ext: "60002",
		subject: "s1", tags: "a b" }, BODY))
		throw new Error("save_msg: " + src.last_error);
	var first = src.last_msg;
	var n = src.copy_msg(first, dst);
	if (typeof n != "number" || n < 1)
		throw new Error("copy_msg: " + n + " " + src.last_error);
	var sh = src.get_msg_header(first);
	var h = dst.get_msg_header(n);
	if (h.subject != "s1" || h.to != "Alice" || h.to_ext != "60001"
		|| h.from != "Bob" || h.from_ext != "60002" || h.tags != "a b"
		|| h.id != sh.id || h.when_written_time != sh.when_written_time
		|| h.when_imported_time != sh.when_imported_time || h.attr != sh.attr)
		throw new Error("header not preserved: " + JSON.stringify(h));
	if (dst.get_msg_body(n) != src.get_msg_body(first))
		throw new Error("body not preserved");
	var idx = dst.get_msg_index(n);
	if (idx.to != 60001 || idx.from != 60002)
		throw new Error("index not built for the e-mail base: " + JSON.stringify(idx));

	/* number 0 or a non-number must not select the oldest message */
	var src_total = src.total_msgs;
	var dst_total = dst.total_msgs;
	var bad = [
		["copy_msg", 0], ["copy_msg", undefined], ["copy_msg", NaN], ["copy_msg", "1"],
		["move_msg", 0], ["move_msg", undefined], ["move_msg", "bogus"]
	];
	bad.forEach(function (b) {
		if (src[b[0]](b[1], dst) !== false)
			throw new Error(b[0] + "(" + b[1] + ") did not fail");
		if (!/message number/.test(src.last_error))
			throw new Error(b[0] + "(" + b[1] + ") failed with last_error: " + src.last_error);
	});
	if (src.total_msgs != src_total || dst.total_msgs != dst_total)
		throw new Error("a refused copy/move wrote a message");
	if (src.get_msg_header(first).attr & MSG_DELETE)
		throw new Error("a refused move flagged the oldest message deleted");

	if (src.copy_msg(999999, dst) !== false)
		throw new Error("copy_msg of a nonexistent message did not fail");
	if (!/999999 not found/.test(src.last_error))
		throw new Error("copy_msg of a nonexistent message: last_error: " + src.last_error);

	/* a large body round-trips byte for byte */
	var line = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+-\r\n";
	var big = line;
	while (big.length < 8 * 1024 * 1024)
		big += big;
	big += "end";
	if (!src.save_msg({ to: "Alice", to_ext: "60001", from: "Bob", subject: "big" }, big))
		throw new Error("save_msg big: " + src.last_error);
	var big_num = src.last_msg;
	var big_copy = src.copy_msg(big_num, dst);
	if (typeof big_copy != "number")
		throw new Error("copy_msg big: " + src.last_error);
	if (md5_calc(dst.get_msg_body(big_copy)) != md5_calc(src.get_msg_body(big_num)))
		throw new Error("large body changed by the copy");
	if (dst.get_msg_body(big_copy).indexOf(big) != 0)
		throw new Error("large body differs from what was saved");

	/* thread links are cleared; Message-ID and reply-ID are kept */
	if (!src.save_msg({ to: "Bob", to_ext: "60002", from: "Alice", subject: "re: s1",
		thread_back: first, reply_id: sh.id, id: "<reply-test@example>" }, "reply"))
		throw new Error("save_msg reply: " + src.last_error);
	var reply = src.last_msg;
	var rh = src.get_msg_header(reply);
	if (rh.thread_back != first)
		throw new Error("test setup: thread_back = " + rh.thread_back);
	var rc = src.copy_msg(reply, dst);
	var rch = dst.get_msg_header(rc);
	if (rch.thread_back || rch.thread_next || rch.thread_first)
		throw new Error("thread links not cleared: " + JSON.stringify(rch));
	if (rch.thread_id && rch.thread_id != rc)
		throw new Error("thread_id = " + rch.thread_id + " (expected 0 or " + rc + ")");
	if (rch.id != "<reply-test@example>" || rch.reply_id != sh.id)
		throw new Error("id/reply_id not kept: " + rch.id + " " + rch.reply_id);

	/* header changes are applied to the copy only */
	var c = src.copy_msg(first, dst, { tags: "x y" });
	if (typeof c != "number" || dst.get_msg_header(c).tags != "x y")
		throw new Error("copy_msg header_changes: " + c + " " + src.last_error);
	if (src.get_msg_header(first).tags != "a b")
		throw new Error("source changed by copy_msg");
	dst_total = dst.total_msgs;
	if (src.copy_msg(first, dst, { no_such_property: 1 }) !== false)
		throw new Error("unsupported header change accepted");
	if (!/Unsupported header change: "no_such_property"/.test(src.last_error)
		|| src.status == 0)
		throw new Error("unsupported header change: last_error: " + src.last_error
			+ ", status: " + src.status);
	if (dst.total_msgs != dst_total)
		throw new Error("an unsupported header change wrote a copy");

	/* move */
	var m = src.move_msg(first, dst, { tags: "moved" });
	if (typeof m != "number" || dst.get_msg_header(m).tags != "moved")
		throw new Error("move_msg: " + m + " " + src.last_error);
	/* removal flags the source deleted; it is not gone until purged */
	var moved_src = src.get_msg_header(first);
	if (!moved_src || !(moved_src.attr & MSG_DELETE))
		throw new Error("source not flagged deleted");
	if (moved_src.tags != "a b")
		throw new Error("source tags changed by move_msg");
	if (dst.get_msg_header(m).attr & MSG_DELETE)
		throw new Error("moved copy flagged deleted");
	var dst_total = dst.total_msgs;
	if (src.move_msg(first, dst) !== false)
		throw new Error("moved a deleted message");
	if (!/is deleted/.test(src.last_error))
		throw new Error("move of a deleted message: last_error: " + src.last_error);
	if (src.move_msg(src.last_msg, src) !== false)
		throw new Error("moved within the same base");
	if (src.get_msg_header(src.last_msg).attr & MSG_DELETE)
		throw new Error("same-base move flagged the source deleted");
	if (dst.total_msgs != dst_total)
		throw new Error("a refused move wrote a copy");

	/* a source already flagged deleted is copied, and the copy is not
	 * flagged deleted, even when asked to be through attr */
	if (!src.save_msg({ to: "Alice", to_ext: "60001", from: "Bob", subject: "del" }, "deleted"))
		throw new Error("save_msg del: " + src.last_error);
	var del = src.last_msg;
	if (!src.remove_msg(del))
		throw new Error("remove_msg del: " + src.last_error);
	var dc = src.copy_msg(del, dst, { attr: src.get_msg_header(del).attr | MSG_DELETE });
	if (typeof dc != "number")
		throw new Error("copy_msg of a deleted message: " + src.last_error);
	if (dst.get_msg_header(dc).attr & MSG_DELETE)
		throw new Error("copy of a deleted message is flagged deleted");

	/* file-attached messages: copy refuses, move hands the files over */
	if (!src.save_msg({ to: "Alice", to_ext: "60001", from: "Bob", subject: "a.txt",
		auxattr: MSG_FILEATTACH | MSG_KILLFILE }, "file attached"))
		throw new Error("save_msg attach: " + src.last_error);
	var att = src.last_msg;
	if (!(src.get_msg_header(att).auxattr & MSG_FILEATTACH))
		throw new Error("test setup: MSG_FILEATTACH not saved");
	dst_total = dst.total_msgs;
	if (src.copy_msg(att, dst) !== false)
		throw new Error("copy_msg of a file-attached message did not fail");
	if (!/has attached files/.test(src.last_error))
		throw new Error("copy_msg refused a file attachment with last_error: " + src.last_error);
	if (dst.total_msgs != dst_total)
		throw new Error("a refused copy wrote a message");
	var am = src.move_msg(att, dst);
	if (typeof am != "number")
		throw new Error("move_msg attach: " + src.last_error);
	var amh = dst.get_msg_header(am);
	if ((amh.auxattr & (MSG_FILEATTACH | MSG_KILLFILE)) != (MSG_FILEATTACH | MSG_KILLFILE))
		throw new Error("copy lost the file-attachment attributes: " + amh.auxattr);
	var ash = src.get_msg_header(att);
	if (!(ash.attr & MSG_DELETE))
		throw new Error("file-attached source not flagged deleted");
	if (ash.auxattr & (MSG_FILEATTACH | MSG_KILLFILE))
		throw new Error("source still claims the attached files: " + ash.auxattr);

	/* into a plain (not e-mail) path base that was never saved to: it is
	 * created, and its index holds name CRCs; copied back into an e-mail
	 * base, the index holds the user numbers again */
	var plain = new MsgBase(plain_path, /* is_path: */ true);
	if (!plain.open())
		throw new Error("open plain: " + plain.last_error);
	if (!src.save_msg({ to: "Alice", to_ext: "60001", from: "Bob", from_ext: "60002",
		subject: "plain" }, BODY))
		throw new Error("save_msg plain: " + src.last_error);
	var pn = src.copy_msg(src.last_msg, plain);
	if (typeof pn != "number")
		throw new Error("copy_msg into a new plain base: " + src.last_error);
	if (plain.attributes & SMB_EMAIL)
		throw new Error("the plain base was created as an e-mail base");
	var pidx = plain.get_msg_index(pn);
	if (pidx.to != crc16_calc("alice") || pidx.from != crc16_calc("bob"))
		throw new Error("plain index not by name CRC: " + JSON.stringify(pidx));
	var ph = plain.get_msg_header(pn);
	if (ph.subject != "plain" || ph.to_ext != "60001" || ph.from_ext != "60002"
		|| plain.get_msg_body(pn) != BODY)
		throw new Error("copy into the plain base not preserved: " + JSON.stringify(ph));
	var back = plain.copy_msg(pn, dst);
	if (typeof back != "number")
		throw new Error("copy_msg from the plain base: " + plain.last_error);
	var bidx = dst.get_msg_index(back);
	if (bidx.to != 60001 || bidx.from != 60002)
		throw new Error("index not by user number after copying back: " + JSON.stringify(bidx));
	if (dst.get_msg_header(back).subject != "plain" || dst.get_msg_body(back) != BODY)
		throw new Error("copy back from the plain base not preserved");
	plain.close();

	/* both bases must be open */
	dst.close();
	if (src.copy_msg(src.last_msg, dst) !== false)
		throw new Error("copy_msg to a closed base did not fail");
	src.close();
} finally {
	cleanup();
}
