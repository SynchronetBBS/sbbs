// UserData: a user's web client JSON files (webmail/lib/userdata.js)

load(js.exec_dir + "kit.js");
var UserData = Kit.wlib("userdata");
var JsonFiles = Kit.lib("mailbox_files");	// this worktree's copy

var scr = Kit.scratch();
try {
	var dir = Kit.webmail_dir(60001, scr);
	Kit.assert(dir.indexOf(scr.dir) === 0, "webmail_dir lies inside the scratch tree");
	var ud = new UserData(dir, JsonFiles);

	// Empty directory: the fallback
	Kit.eq(ud.read("prefs", { a: 1 }), { a: 1 });
	Kit.assert(ud.read("prefs") === undefined, "no fallback");

	// Round trip
	ud.write("prefs", { theme: "dark", n: [1, 2] });
	Kit.eq(ud.read("prefs", null), { theme: "dark", n: [1, 2] });

	// update: function applied under the lock, result returned and stored
	var r = ud.update("counter", { n: 0 }, function (o) {
		o.n++;
		return o;
	});
	Kit.eq(r, { n: 1 });
	r = ud.update("counter", { n: 0 }, function (o) {
		o.n += 10;
		return o;
	});
	Kit.eq(r, { n: 11 });
	Kit.eq(ud.read("counter", null), { n: 11 });

	// fn returning undefined throws and writes nothing
	Kit.throws(function () {
		ud.update("counter", {}, function (o) {});
	});
	Kit.eq(ud.read("counter", null), { n: 11 });

	// A per-call lock timeout: a held lock fails the update quickly, and
	// the instance's own timeout is left as it was
	var lock = ud.files.lock_path("counter");
	Kit.assert(file_mutex(lock, "held by test", 60), "take the counter lock");
	try {
		var started = system.timer;
		Kit.throws(function () {
			ud.update("counter", {}, function (o) {
				return o;
			}, { timeout: 100 });
		}, "conflict");
		var took = system.timer - started;
		// well under the 5 second default (the per-call wait is 100 ms)
		Kit.assert(took < 3, "per-call timeout honored: " + took + " s");
		Kit.eq(ud.files.timeout, 5000, "instance timeout unchanged");
	} finally {
		file_remove(lock);
	}
	r = ud.update("counter", { n: 0 }, function (o) {
		o.n++;
		return o;
	}, {});
	Kit.eq(r, { n: 12 }, "empty options");

	// Separate names are separate files; files carry the "webmail." prefix
	ud.write("other", 5);
	Kit.eq(ud.read("other", 0), 5);
	Kit.assert(file_exists(dir + "webmail.other.json"), "file name");

	// Without an injected JsonFiles the library finds its own
	var ud2 = new UserData(dir);
	Kit.eq(ud2.read("other", 0), 5);
	Kit.eq(ud2.dir, dir);
	// ...and that lookup used the exec/load copy beside this worktree's
	// webmail/, not the load() fallback
	Kit.assert(UserData.source !== "mailbox_files.js", "fell back to load(): " + UserData.source);
	Kit.assert(file_exists(UserData.source), "source exists: " + UserData.source);
	Kit.assert(UserData.source.indexOf(Kit.exec_root().replace(/\/$/, "")) === 0
		|| UserData.source.indexOf("exec/load/mailbox_files.js") >= 0, "source: " + UserData.source);

	// A second kit-made directory is cleaned by Kit.webmail_cleanup()
	var d2 = Kit.webmail_dir(60002);
	Kit.assert(file_isdir(d2), "kit scratch dir");
	Kit.webmail_cleanup();
	Kit.assert(!file_isdir(d2), "kit scratch dir removed");
} finally {
	scr.cleanup();
	Kit.webmail_cleanup();
}
