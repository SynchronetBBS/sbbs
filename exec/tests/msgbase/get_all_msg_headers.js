// Regression/behavioral test for the MsgBase.get_all_msg_headers() *_NULL
// cold-access bug (gitlab issue #1143).
//
// Symptom: header objects returned by get_all_msg_headers() resolve string
// fields lazily (js_get_msg_header_resolve + the LAZY_* macros). The
// LAZY_STRING_TRUNCSP_NULL fields (to_ext, from_ext, replyto, replyto_ext,
// replyto_list, to_list, cc_list, summary, tags, from_org, etc.) are not
// defined as properties when their underlying value is NULL. All bulk-fetched
// header objects share one SpiderMonkey 1.8.5 shape, so the property set
// diverges across the shared shape (a *_NULL field is an own property on some
// headers, absent on others). The TraceMonkey trace-JIT records a shape-
// guarded GETPROP on the hot for..in dot-access loop; once a header whose
// *_NULL field is absent contributes to the recorded trace, the replay yields
// `undefined` for that field on every subsequent same-shape header — even
// when populated. Touching any non-NULL field first dodges it.
//
// Root cause: TraceMonkey (JSOPTION_JIT, bit 11 = 0x800), not the interpreter
// PropertyCache or the lazy resolve hook. The interpreter cache invalidates
// correctly on shape transitions; the trace recorder does not, for this
// shared-shape / conditional-resolve pattern.
//
// Fix: JSOPTION_JIT was dropped from JAVASCRIPT_OPTIONS in sbbsdefs.h (changed
// from 0x810 to 0x10). JSOPTION_COMPILE_N_GO is kept. TraceMonkey is dead
// upstream in SpiderMonkey, and on builds that compile it in (Linux, Windows
// via MSVC) leaving it enabled produces this and likely other shape-guarded
// trace-replay surprises.
//
// History (workaround attempts, both now reverted in favor of the root-cause
// fix above):
//   - ca448cb8b eagerly JS_DefineProperty("number", ...) per header to force a
//     shape transition. Masked the bug for tiny/uniform bases but not for
//     real mailboxes.
//   - 666ff71ce eagerly RESOLVED the data fields per header, which did fix the
//     symptom but at the cost of the lazy design and on the wrong diagnosis
//     (named the interpreter PropertyCache, but the misprediction is in the
//     trace JIT).
//
// What this test does:
//   - Asserts JSOPTION_JIT is NOT set in js.options. If a future change re-
//     enables it, this fails fast with a clear message before getting to the
//     behavioral check.
//   - Behavioral check: builds a bulk message base with mixed NULL/non-NULL
//     to_ext + rotating LAZY_STRING_TRUNCSP_NULL fields (for shape diversity),
//     bulk-fetches, and reads to_ext as the FIRST property access on each
//     header. Expects every populated to_ext to come back correctly and every
//     genuine NULL to come back as undefined — no spurious mispredicts.
//
// IMPORTANT: the symptom only reproduces robustly on real, varied mailbox
// data — it could not be reproduced reliably with synthetic save_msg()'d
// messages even at 15k+ entries. The behavioral scenario here is a contract
// guard, not a faithful reproduction of the original Linux/Windows misfire.
// For that, see probe_to_ext.js / probe_enum.js attached to issue #1143 and
// run against a real mail base.

mkpath(system.temp_dir);

var JSOPTION_JIT = 0x800;
if ((js.options & JSOPTION_JIT) !== 0) {
	throw new Error(
		"JSOPTION_JIT (0x800) is set in js.options (=0x" + js.options.toString(16) + "). " +
		"This re-enables TraceMonkey, which is the root cause of issue #1143 " +
		"(cold *_NULL field access mis-served as undefined across bulk-fetched " +
		"headers). See sbbsdefs.h: JAVASCRIPT_OPTIONS must omit bit 0x800."
	);
}

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

// Bulk: mixed NULL/non-NULL to_ext + rotating LAZY_STRING_TRUNCSP_NULL fields
// (shape diversity). to_ext is read as the FIRST property access on each
// header — the original trap.
var N = 60;
var opt = ["replyto", "from_org", "summary", "tags", "cc_list", "from_ext", "to_list"];
with_base("test_msgbase_bulk", function (mb) {
	for (var i = 0; i < N; i++) {
		var h = { to: "R" + i, from: "S", subject: "m" + i };
		if (i % 5 != 2) h.to_ext = String((i % 9) + 1);   // most have to_ext; some omit it (NULL)
		for (var j = 0; j < opt.length; j++)              // rotating subset -> shape diversity
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
