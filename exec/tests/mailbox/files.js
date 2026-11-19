// JsonFiles: JSON side files with locked read-modify-write

load(js.exec_dir + "kit.js");
var JsonFiles = Kit.lib("mailbox_files");
var S = Kit.scratch();
try {
	var dir = S.dir + "side/";	// does not exist yet
	var jf = new JsonFiles(dir, "t_", { timeout: 300 });

	Kit.eq(jf.read("a", { x: 1 }), { x: 1 }, "missing returns fallback");
	Kit.assert(!file_isdir(dir), "read must not create the directory");

	jf.write("a", { n: [1, 2, 3], s: "str" });
	Kit.assert(file_exists(dir + "t_a.json"), "file name");
	Kit.eq(jf.read("a", null), { n: [1, 2, 3], s: "str" }, "round trip");
	jf.write("a", { n: 2 });
	Kit.eq(jf.read("a", null), { n: 2 }, "overwrite existing");

	// leftover temp file from a crashed writer
	var junk = dir + "t_a.json.12345-1-0.tmp";
	var f = new File(junk);
	f.open("w");
	f.write('{"half');
	f.close();
	Kit.eq(jf.read("a", null), { n: 2 }, "leftover temp ignored");
	jf.write("a", { n: 3 });
	Kit.eq(jf.read("a", null), { n: 3 }, "write after leftover temp");
	Kit.assert(file_exists(junk), "leftover temp not touched by write");
	file_remove(junk);

	// no temp file remains after successful writes and updates
	var dir2 = S.dir + "clean/";
	var jf3 = new JsonFiles(dir2, "c_", { timeout: 300 });
	jf3.write("x", { a: 1 });
	jf3.write("x", { a: 2 });
	jf3.update("y", {}, function (o) { o.b = 1; return o; });
	var names = [];
	var ents = directory(dir2 + "*");
	for (var k = 0; k < ents.length; k++)
		names.push(ents[k].substr(dir2.length));
	names.sort();
	Kit.eq(names, ["c_x.json", "c_y.json"], "only expected files remain");

	// write failure: directory path is occupied by a regular file
	var blocker = S.dir + "blocker";
	f = new File(blocker);
	f.open("w");
	f.write("x");
	f.close();
	var jf4 = new JsonFiles(blocker, "b_");
	var msg = "";
	try {
		jf4.write("z", {});
	} catch (e) {
		msg = String(e.message);
	}
	Kit.assert(msg.indexOf("blocker") >= 0, "write error names the path: " + msg);
	Kit.assert(file_size(blocker) == 1, "blocker untouched");

	// unparsable file returns fallback
	f = new File(dir + "t_bad.json");
	f.open("w");
	f.write("not json");
	f.close();
	Kit.eq(jf.read("bad", "fb"), "fb", "unparsable");

	// update
	var r = jf.update("c", { count: 0 }, function (o) { o.count++; return o; });
	Kit.eq(r, { count: 1 }, "update return");
	jf.update("c", { count: 0 }, function (o) { o.count++; return o; });
	Kit.eq(jf.read("c", null), { count: 2 }, "update persisted");
	Kit.assert(!file_exists(dir + "t_c.lock"), "lock released");

	// re-entrant update from inside fn conflicts
	var t0 = system.timer;
	Kit.throws(function () {
		jf.update("d", {}, function (o) {
			jf.update("d", {}, function (p) { return p; });
			return o;
		});
	}, "conflict");
	Kit.assert(system.timer - t0 >= 0.25, "waited for the timeout");
	Kit.assert(!file_exists(dir + "t_d.lock"), "lock released after conflict");

	// fn returning undefined throws, leaves the file alone, releases the lock
	var before = jf.read("c", null);
	var threw = false;
	try {
		jf.update("c", { count: 0 }, function (o) { o.count = 99; });
	} catch (e) {
		threw = e instanceof TypeError;
	}
	Kit.assert(threw, "undefined result throws TypeError");
	Kit.eq(jf.read("c", null), before, "stored file unchanged");
	Kit.assert(!file_exists(dir + "t_c.lock"), "lock released after undefined");

	// fn throwing releases the lock
	Kit.throws(function () {
		jf.update("e", {}, function (o) { throw new Error("boom"); });
	});
	Kit.assert(!file_exists(dir + "t_e.lock"), "lock released after fn throws");

	// stale lock taken over
	var jf2 = new JsonFiles(dir, "t_", { timeout: 300, max_age: 1 });
	Kit.assert(file_mutex(dir + "t_s.lock"), "make lock");
	Kit.throws(function () { jf2.update("s", {}, function (o) { return o; }); }, "conflict");
	mswait(2100);
	Kit.eq(jf2.update("s", {}, function (o) { o.ok = 1; return o; }), { ok: 1 }, "stale lock taken over");

	// a holder that outlived max_age does not remove the new holder's lock
	var jf5 = new JsonFiles(dir, "t_", { timeout: 300, max_age: 1 });
	jf5.update("o", {}, function (o) {
		mswait(2100);
		// lock is now stale: another holder takes it over
		Kit.assert(file_mutex(dir + "t_o.lock", "other-holder", 1), "takeover");
		return o;
	});
	Kit.assert(file_exists(dir + "t_o.lock"), "new holder's lock survives");
	var lf = new File(dir + "t_o.lock");
	lf.open("r");
	Kit.eq(lf.read(), "other-holder", "new holder's token intact");
	lf.close();

	// a rename that fails a few times (as when a reader on Windows has the
	// file open) is retried; one that keeps failing gives up in bounded
	// time, and leaves no temp file
	var real_rename = file_rename;
	var fails = 3;
	file_rename = function (from, to) {
		if (fails > 0 && to == jf.path("r")) {
			fails--;
			return false;
		}
		return real_rename(from, to);
	};
	try {
		jf.write("r", { n: 1 });
		Kit.eq(jf.read("r", null), { n: 1 }, "written after failed renames");
		Kit.eq(fails, 0, "every failure retried");
		fails = 1000000;
		var t_start = system.timer;
		Kit.throws(function () { jf.write("r", { n: 2 }); });
		Kit.assert(system.timer - t_start < 2, "bounded retry (" + (system.timer - t_start) + " s)");
	} finally {
		file_rename = real_rename;
	}
	Kit.eq(directory(dir + "t_r.json.*.tmp").length, 0, "no temp file left by a failed write");
} finally {
	S.cleanup();
}
