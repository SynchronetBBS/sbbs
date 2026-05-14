// Regression test for MsgBase.get_all_msg_headers() lazy-property bug.
//
// Prior to the fix in js_get_all_msg_headers (js_msgbase.cpp), header
// objects returned by get_all_msg_headers() exhibited a SpiderMonkey
// shape-cache quirk: the first JS access to any LAZY_STRING_TRUNCSP_NULL
// field (to_ext, from_ext, replyto, replyto_ext, replyto_list, to_list,
// cc_list, summary, tags, from_org, etc.) returned `undefined` even when
// p->msg.<field> was populated. Touching ANY other property first (e.g.
// h.number, h.attr) triggered a JS_DefineProperty inside a LAZY_*
// macro, transitioning the object's shape, after which subsequent
// resolve calls worked normally.
//
// The fix: eagerly JS_DefineProperty("number", ...) right after
// JS_SetPrivate in js_get_all_msg_headers, forcing the shape transition
// once at construction time.
//
// This test creates a temp message base, saves a message with a
// known to_ext value, then accesses to_ext as the FIRST property
// touched on the bulk-fetched header. Without the fix, to_ext is
// undefined and the test throws. With the fix, to_ext is the saved
// value.

/* system.temp_dir may not exist yet on a fresh install / CI runner — create it. */
mkpath(system.temp_dir);

var path = system.temp_dir + "test_msgbase_to_ext_" + Date.now();
var EXTENSIONS = [".shd", ".sdt", ".sid", ".sha", ".sda", ".ini", ".hash"];

function cleanup() {
	EXTENSIONS.forEach(function (ext) {
		try { file_remove(path + ext); } catch (e) { /* ignore */ }
	});
}

var mb = new MsgBase(path, /* is_path: */ true);
if (!mb.open()) {
	cleanup();
	throw new Error("Failed to open temp msgbase '" + path + "': " + mb.last_error);
}

var saved = mb.save_msg(
	{ to: "TestRecipient", to_ext: "1", from: "TestSender", subject: "to_ext-bug-test" },
	"Test body for the get_all_msg_headers to_ext regression test."
);
mb.close();

if (!saved) {
	cleanup();
	throw new Error("MsgBase.save_msg failed: " + mb.last_error);
}

if (!mb.open()) {
	cleanup();
	throw new Error("Reopen failed after save: " + mb.last_error);
}

var hdrs = mb.get_all_msg_headers();
mb.close();
cleanup();

var seen = 0;
for (var i in hdrs) {
	seen++;
	var h = hdrs[i];
	// FIRST property access on this header object — must not return
	// undefined. (Accessing any other property first would prime the
	// shape and mask the bug, so don't touch h.* before this line.)
	var first_to_ext = h.to_ext;
	if (first_to_ext != "1") {
		throw new Error(
			"get_all_msg_headers(): hdrs[" + i + "].to_ext on first access " +
			"returned " + JSON.stringify(first_to_ext) + " (expected '1'). " +
			"Bug is in js_get_all_msg_headers (src/sbbs3/js_msgbase.cpp); " +
			"fix forces a shape transition by JS_DefineProperty('number', ...) " +
			"immediately after JS_SetPrivate."
		);
	}
}

if (seen === 0) {
	throw new Error("get_all_msg_headers() returned no entries from a base with 1 saved msg");
}
