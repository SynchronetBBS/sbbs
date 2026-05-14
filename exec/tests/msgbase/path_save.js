// Regression test for the segfault in savemsg() when an is_path
// MsgBase posts a message with to_ext set and the recipient resolves
// to a real local user.
//
// Before the fix in src/sbbs3/postmsg.cpp (commit e5ddda76d), savemsg()
// dereferenced cfg->sub[smb->subnum] without a subnum_is_valid() guard
// when emitting the "MsgPostedToYou" notification. For an is_path
// MsgBase, the constructor sets smb->subnum = scfg->total_subs (one
// past the end of cfg->sub[]) — so save_msg({to_ext:"<real-user>"})
// crashed with SIGSEGV every time.
//
// Fix: gate the notification's else branch on subnum_is_valid(); for
// ad-hoc/is_path msgbases skip the local "you have a message"
// notification entirely.
//
// This test reproduces the original crashing call. With the fix,
// save_msg returns true; without it, jsexec dies with exit 139.

mkpath(system.temp_dir);

var path = system.temp_dir + "test_msgbase_is_path_to_ext_" + Date.now();
var EXTENSIONS = [".shd", ".sdt", ".sid", ".sha", ".sda", ".ini", ".hash"];

function cleanup() {
	EXTENSIONS.forEach(function (ext) {
		try { file_remove(path + ext); } catch (e) { /* ignore */ }
	});
}

try {
	var mb = new MsgBase(path, /* is_path: */ true);
	if (!mb.open())
		throw new Error("open failed: " + mb.last_error);

	/* to_ext "1" => atoi() == 1 == sysop's user number on every install,
	 * which guarantees the notification path fires. */
	var ok = mb.save_msg(
		{ to: "Sysop", to_ext: "1", from: "Tester", subject: "is_path-to_ext-bug" },
		"body"
	);
	if (!ok)
		throw new Error("save_msg returned false: " + mb.last_error);

	mb.close();
} finally {
	cleanup();
}
