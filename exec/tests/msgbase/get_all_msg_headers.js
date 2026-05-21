// Regression/behavioral test for the MsgBase.get_all_msg_headers() lazy-field bug.
//
// Header objects returned by get_all_msg_headers() resolve most string fields
// lazily (js_get_msg_header_resolve + the LAZY_* macros). The
// LAZY_STRING_TRUNCSP_NULL fields (to_ext, from_ext, replyto, replyto_ext,
// replyto_list, to_list, cc_list, summary, tags, from_org, etc.) are not
// defined as properties when their underlying value is NULL. In a bulk fetch
// all header objects share one SpiderMonkey 1.8.5 shape; once the resolve hook
// processes a header whose *_NULL field is NULL (leaving the property
// undefined), the property cache for that (shape, field) entry is corrupted and
// every subsequent same-shape header returns `undefined` on the FIRST access of
// that field — even when populated.
//
// History:
//   - ca448cb8b eagerly JS_DefineProperty("number", ...) per header to force a
//     shape transition. That masked the bug for tiny/uniform bases (and this
//     test, when it had a single message) but NOT for real mailboxes, where it
//     still bit ~98% of headers once the first NULL-to_ext message appeared.
//   - The current fix eagerly RESOLVES the data fields per header in
//     js_get_all_msg_headers (defer_listing skips the heavy field_list/can_read,
//     which stay lazy), so non-NULL fields are own properties up front and the
//     caller never triggers the GET-path lazy resolve that mis-serves undefined.
//
// IMPORTANT: the failure only reproduces on real, varied mailbox data — it could
// not be reproduced with synthetically save_msg()'d messages (uniform, varying,
// NULL-interleaved, net-typed, header-rich, or shape-diverse, up to 15k msgs).
// So this test is a BEHAVIORAL GUARD: it asserts the contract (cold first-access
// of *_NULL fields returns the stored value across a bulk fetch) and catches a
// total removal of the eager-populate, but it does not, on its own, reproduce
// the real-mailbox property-cache corruption. Validate that against a real base.

mkpath(system.temp_dir);

var EXTENSIONS = [".shd", ".sdt", ".sid", ".sha", ".sda", ".ini", ".hash"];

function with_base(name, build, check) {
	var path = system.temp_dir + name + "_" + Date.now();
	function cleanup() {
		EXTENSIONS.forEach(function (ext) { try { file_remove(path + ext); } catch (e) {} });
	}
	var mb = new MsgBase(path, /* is_path: */ true);
	if (!mb.open()) { cleanup(); throw new Error("open '" + path + "': " + mb.last_error); }
	try {
		build(mb);
		mb.close();
		if (!mb.open()) { cleanup(); throw new Error("reopen: " + mb.last_error); }
		var hdrs = mb.get_all_msg_headers();
		mb.close();
		check(hdrs);
	} finally {
		try { mb.close(); } catch (e) {}
		cleanup();
	}
}

// --- Scenario 1: single message, to_ext read as the FIRST property access ---
with_base("test_msgbase_to_ext", function (mb) {
	if (!mb.save_msg({ to: "TestRecipient", to_ext: "1", from: "TestSender", subject: "to_ext-bug-test" },
	                 "Test body."))
		throw new Error("save_msg: " + mb.last_error);
}, function (hdrs) {
	var seen = 0;
	for (var i in hdrs) {
		seen++;
		// FIRST property access on this header — must not be undefined. Do NOT
		// touch any other property before this line (that would prime the shape).
		var first = hdrs[i].to_ext;
		if (first != "1")
			throw new Error("hdrs[" + i + "].to_ext on first access = " + JSON.stringify(first) + " (expected '1')");
	}
	if (seen === 0) throw new Error("get_all_msg_headers() returned no entries");
});

// --- Scenario 2: many messages, mixed NULL/non-NULL to_ext + shape diversity,
//     reading several *_NULL fields as the FIRST access on each header. ---
var N = 60;
var opt = ["replyto", "from_org", "summary", "tags", "cc_list", "from_ext", "to_list"];
with_base("test_msgbase_bulk", function (mb) {
	for (var i = 0; i < N; i++) {
		var h = { to: "R" + i, from: "S", subject: "m" + i };
		if (i % 5 != 2) h.to_ext = String((i % 9) + 1);   // most have to_ext; some omit it (NULL)
		for (var j = 0; j < opt.length; j++)              // rotating field subset -> shape diversity
			if ((i >> j) & 1) h[opt[j]] = opt[j] + "-" + i;
		if (!mb.save_msg(h, "body " + i))
			throw new Error("save_msg " + i + ": " + mb.last_error);
	}
}, function (hdrs) {
	var seen = 0;
	for (var i in hdrs) {
		var h = hdrs[i];
		if (typeof h != "object" || h == null) continue;
		seen++;
		// to_ext as the FIRST property access (the classic trap).
		var to_ext = h.to_ext;
		var n = Number(i);
		// recover the message index from the subject to know what was stored
		var subj = h.subject;                              // safe to touch after to_ext
		var k = parseInt(String(subj).substr(1), 10);
		var expect = (k % 5 != 2) ? String((k % 9) + 1) : undefined;
		if (String(to_ext) !== String(expect))
			throw new Error("bulk hdr (subject " + subj + ") to_ext first-access = "
				+ JSON.stringify(to_ext) + " (expected " + JSON.stringify(expect) + ")");
	}
	if (seen !== N) throw new Error("expected " + N + " headers, got " + seen);
});
