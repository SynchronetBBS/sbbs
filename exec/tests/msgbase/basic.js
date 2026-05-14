// Basic MsgBase round-trip: open, save_msg, reopen, get_msg_header,
// get_msg_body, remove_msg, close. Exercises the core API surface
// against an ad-hoc (is_path) message base so the test doesn't touch
// the live install's mail or any sub-board.

load("sbbsdefs.js");  /* for MSG_DELETE */
mkpath(system.temp_dir);

var path = system.temp_dir + "test_msgbase_basic_" + Date.now();
var EXTENSIONS = [".shd", ".sdt", ".sid", ".sha", ".sda", ".ini", ".hash"];

function cleanup() {
	EXTENSIONS.forEach(function (ext) {
		try { file_remove(path + ext); } catch (e) { /* ignore */ }
	});
}

var SUBJECT = "msgbase-basic-test " + Date.now();
var BODY = "Hello from the MsgBase basic round-trip test.\nLine 2.\n";

try {
	var mb = new MsgBase(path, /* is_path: */ true);
	if (!mb.open())
		throw new Error("open failed: " + mb.last_error);

	if (!mb.save_msg({ to: "Alice", from: "Bob", subject: SUBJECT }, BODY))
		throw new Error("save_msg failed: " + mb.last_error);
	mb.close();

	if (!mb.open())
		throw new Error("reopen failed: " + mb.last_error);

	if (mb.total_msgs != 1)
		throw new Error("total_msgs = " + mb.total_msgs + " (expected 1)");
	if (mb.first_msg != 1)
		throw new Error("first_msg = " + mb.first_msg + " (expected 1)");
	if (mb.last_msg != 1)
		throw new Error("last_msg = " + mb.last_msg + " (expected 1)");

	var hdr = mb.get_msg_header(/* by_offset: */ false, 1);
	if (hdr === null)
		throw new Error("get_msg_header(1) returned null: " + mb.last_error);
	if (hdr.subject != SUBJECT)
		throw new Error("subject = " + JSON.stringify(hdr.subject) + " (expected " + JSON.stringify(SUBJECT) + ")");
	if (hdr.to != "Alice")
		throw new Error("to = " + JSON.stringify(hdr.to) + " (expected 'Alice')");
	if (hdr.from != "Bob")
		throw new Error("from = " + JSON.stringify(hdr.from) + " (expected 'Bob')");
	if (hdr.number != 1)
		throw new Error("number = " + hdr.number + " (expected 1)");

	var hdr_by_offset = mb.get_msg_header(/* by_offset: */ true, 0);
	if (hdr_by_offset === null || hdr_by_offset.number != 1)
		throw new Error("get_msg_header(by_offset=true, 0) failed");

	var body = mb.get_msg_body(false, 1);
	if (body === null)
		throw new Error("get_msg_body(1) returned null: " + mb.last_error);
	if (body.indexOf("Hello from the MsgBase basic round-trip test.") < 0)
		throw new Error("body missing expected content; got: " + JSON.stringify(body));

	if (!mb.remove_msg(false, 1))
		throw new Error("remove_msg(1) failed: " + mb.last_error);

	mb.close();
	if (!mb.open())
		throw new Error("reopen-after-remove failed: " + mb.last_error);

	/* remove_msg only marks for deletion; the index entry remains until the
	 * base is packed. total_msgs reflects index entries, so it stays 1 here.
	 * Verify the header reads back as deleted. */
	var hdr_after_remove = mb.get_msg_header(false, 1);
	if (hdr_after_remove !== null && !(hdr_after_remove.attr & MSG_DELETE))
		throw new Error("MSG_DELETE attr not set on removed message: attr=" + (hdr_after_remove ? hdr_after_remove.attr : "<null>"));

	mb.close();
} finally {
	cleanup();
}
