// Shared helpers for the web client library tests (exec/tests/webmail/*.js).
// Loads the mailbox kit (exec/tests/mailbox/kit.js) and adds to the same
// Kit object.  Run on its own (as the test runner does) this file only
// defines things.  A test loads it with:  load(js.exec_dir + "kit.js");
// js.exec_dir is the directory of the running test, <exec>/tests/webmail/.

"use strict";

load(js.exec_dir + "../mailbox/kit.js");	// defines the global Kit

(function () {
	var scratch = null;

	// Load webmail/lib/<name>.js from this worktree (the directory above
	// exec/, found from the test location) and return its namespace
	Kit.wlib = function (name) {
		var path = Kit.exec_root() + "../webmail/lib/" + name + ".js";
		if (!file_exists(path))
			throw new Error("Kit.wlib: no such library: " + path);
		return load({}, path);
	};

	// A scratch stand-in for data/user/####/webmail/, created if absent.
	// It lies inside a Kit.scratch() tree (data/user/<####>/webmail/), so
	// it goes away with that tree's cleanup().
	//   Kit.webmail_dir(usernum, scr)  inside the scratch tree 'scr' (the
	//                                  caller's, cleaned by scr.cleanup())
	//   Kit.webmail_dir(usernum)       inside one tree this kit makes on
	//                                  first use; remove it with
	//                                  Kit.webmail_cleanup()
	Kit.webmail_dir = function (usernum, scr) {
		if (!scr) {
			if (!scratch)
				scratch = Kit.scratch();
			scr = scratch;
		}
		var d = scr.user_dir(usernum) + "webmail/";
		mkpath(d);
		return d;
	};

	// Removes the tree made by Kit.webmail_dir(usernum) (no scratch given)
	Kit.webmail_cleanup = function () {
		if (scratch)
			scratch.cleanup();
		scratch = null;
	};
})();
