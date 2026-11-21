// Summary: cached summary of a private e-mail base, incremental updates,
// usage total

load(js.exec_dir + "kit.js");
var JsonFiles = Kit.lib("mailbox_files");
var Summary = Kit.lib("mailbox_summary");

var MSG_DELETE = (1 << 4);
var MSG_READ = (1 << 1);
var MSG_FILEATTACH = (1 << 1);	// auxattr
var MSG_HFIELDS_UTF8 = (1 << 13);	// auxattr
var RFC822HEADER = 0xb0;
var NET_INTERNET = 5;
var USER = 60001;
var T0 = 1700000000;

var S = Kit.scratch();
var mb = null;
var real_get = null;

// Wait until the clock is in a later second than 'since', so a write made
// after this lands in a new mtime second
function next_second(since)
{
	while (time() <= since)
		mswait(20);
}

function save(hdr, body)
{
	if (hdr.to === undefined)
		hdr.to = "Alice";
	if (hdr.to_ext === undefined)
		hdr.to_ext = String(USER);
	if (hdr.from === undefined)
		hdr.from = "Bob";
	if (!mb.save_msg(hdr, body === undefined ? "body text\r\n" : body))
		throw new Error("save_msg: " + mb.last_error);
	return mb.last_msg;
}

function retag(num, tags)
{
	var h = real_get.call(mb, false, num, false);	// not counted
	h.tags = tags;
	if (!mb.put_msg_header(false, num, h))
		throw new Error("put_msg_header: " + mb.last_error);
}

function write_text(path, text)
{
	var f = new File(path);
	if (!f.open("w"))
		throw new Error("create " + path);
	f.write(text);
	f.close();
}

function sum_sizes(list)
{
	var t = 0;
	for (var i = 0; i < list.length; i++)
		t += list[i].size;
	return t;
}

function find(list, num)
{
	for (var i = 0; i < list.length; i++)
		if (list[i].num == num)
			return list[i];
	return null;
}

try {
	var udir = S.user_dir(USER);
	var opts = { data_dir: S.data_dir };
	var jf = new JsonFiles(udir, "mailbox.", { timeout: 2000 });
	mb = Kit.email_base(udir + "mailbox");

	// Count header reads by the library
	var reads = 0;
	real_get = mb.get_msg_header;
	mb.get_msg_header = function () {
		reads++;
		return real_get.apply(mb, arguments);
	};

	// 50 live messages, arrival times not in number order
	var nums = [];
	for (var i = 0; i < 50; i++) {
		var hdr = {
			subject: "msg " + i,
			when_written_time: T0 + i,
			when_imported_time: T0 + ((i * 37) % 50) * 60,
			tags: "t" + i
		};
		if (i == 1)
			hdr.from = "Ren\x82";	// CP437 e-acute
		if (i == 2) {
			hdr.from = "Ren\xC3\xA9";	// already UTF-8
			hdr.auxattr = MSG_HFIELDS_UTF8;
		}
		if (i == 3)
			hdr.priority = 1;
		if (i == 4) {
			hdr.field_list = [
				{ type: RFC822HEADER, data: "MIME-Version: 1.0" },
				{ type: RFC822HEADER, data: "Content-Type: multipart/mixed; boundary=\"xx\"" }
			];
		}
		if (i == 5) {
			hdr.auxattr = MSG_FILEATTACH;
			hdr.subject = "a.txt \"b c.txt\" ../../missing.txt";
		}
		if (i == 6) {
			hdr.from_net_type = NET_INTERNET;
			hdr.from_net_addr = "bob@example.com";
		}
		nums.push(save(hdr));
	}
	// One more, flagged deleted: never in the summary
	var deleted = save({ subject: "deleted", when_imported_time: T0 });
	Kit.assert(mb.remove_msg(deleted), "remove_msg");

	// File attachment files for message 5
	var fdir = S.data_dir + format("file/%04u.in/", USER);
	mkpath(fdir);
	write_text(fdir + "a.txt", new Array(101).join("x"));	// 100 bytes
	write_text(fdir + "b c.txt", new Array(201).join("y"));	// 200 bytes

	// --- get: cold
	reads = 0;
	var list = Summary.get(mb, jf, opts);
	Kit.eq(list.length, 50, "50 messages");
	Kit.eq(reads, 50, "cold get reads each live header once");
	Kit.assert(find(list, deleted) === null, "deleted message excluded");
	for (i = 1; i < list.length; i++)
		Kit.assert(list[i - 1].arrived > list[i].arrived, "newest arrival first at " + i);
	Kit.eq(list[0].arrived, T0 + 49 * 60, "newest first");

	var m0 = find(list, nums[0]);
	Kit.eq(m0.subject, "msg 0", "subject");
	Kit.eq(m0.from, "Bob", "from");
	Kit.eq(m0.to, "Alice", "to");
	Kit.eq(m0.tags, "t0", "tags");
	Kit.eq(m0.date, T0, "date is when written");
	Kit.eq(m0.arrived, T0, "arrived is when imported");
	Kit.eq(typeof m0.zone, "number", "zone");
	Kit.eq(m0.priority, 0, "unspecified priority");
	Kit.eq(m0.attach, false, "no attachment");
	Kit.eq(m0.attr & MSG_DELETE, 0, "attr");
	Kit.assert(typeof m0.msgid == "string" && m0.msgid.length > 0, "msgid: " + m0.msgid);
	Kit.eq(m0.from_addr, "", "no from address");
	Kit.eq(m0.size % 256, 0, "size is whole blocks");
	Kit.assert(m0.size >= 512, "size counts header and data blocks: " + m0.size);

	Kit.eq(find(list, nums[1]).from, "Ren\xC3\xA9", "CP437 converted to UTF-8");
	Kit.eq(find(list, nums[2]).from, "Ren\xC3\xA9", "UTF-8 header kept");
	Kit.eq(find(list, nums[3]).priority, 1, "priority");
	Kit.eq(find(list, nums[4]).attach, true, "MIME multipart/mixed is an attachment");
	var fa = find(list, nums[5]);
	Kit.eq(fa.attach, true, "file attachment");
	Kit.eq(fa.size, m0.size + 300, "attached file sizes count (missing file is 0)");
	Kit.eq(find(list, nums[6]).from_addr, "bob@example.com", "from address");

	// --- get: warm, no header reads
	reads = 0;
	var list2 = Summary.get(mb, jf, opts);
	Kit.eq(reads, 0, "warm get reads no headers");
	Kit.eq(list2, list, "warm get equals cold");

	// --- an outside header change (a later second) forces a rebuild
	// (the file's date and size and the clock, before and after, are in
	// the message: the cache goes by them, and file systems differ in when
	// a write shows in a file's date)
	var shd = mb.file + ".shd";
	var was = [file_date(shd), file_size(shd), time()];
	next_second(was[0]);
	retag(nums[10], "changed");
	var now = [file_date(shd), file_size(shd), time()];
	reads = 0;
	list = Summary.get(mb, jf, opts);
	Kit.eq(reads, 50, "outside change rebuilds (.shd date " + was[0] + " -> " + now[0] + ", size " + was[1]
		+ " -> " + now[1] + ", clock " + was[2] + " -> " + now[2] + ", date after the get " + file_date(shd) + ")");
	Kit.eq(find(list, nums[10]).tags, "changed", "rebuild shows new tag");

	// --- a corrupt summary file is rebuilt, not thrown
	write_text(jf.path("summary"), "{\"version\":1,\"stamps\":[1,");
	reads = 0;
	list = Summary.get(mb, jf, opts);
	Kit.eq(list.length, 50, "rebuilt after corruption");
	Kit.eq(reads, 50, "corrupt cache rebuilds");
	write_text(jf.path("summary"), "{\"version\":1,\"stamps\":\"x\",\"list\":7}");
	Kit.eq(Summary.get(mb, jf, opts).length, 50, "wrong-shaped cache rebuilt");

	// --- usage
	var u = Summary.usage(mb, jf, opts);
	Kit.eq(u, { bytes: sum_sizes(list), count: 50 }, "usage is the sum of sizes");

	// --- modify: one added, one removed, one retagged (cache valid)
	var added;
	var removed = nums[20];
	var removed_size = find(list, removed).size;
	reads = 0;
	var changes = null;
	var mu = Summary.modify(mb, jf, function () {
		added = save({ subject: "new one", when_imported_time: T0 + 100000 });
		Kit.assert(mb.remove_msg(removed), "remove_msg");
		retag(nums[30], "retagged");
		changes = { added: [added], removed: [removed], changed: [nums[30]] };
		return changes;
	}, opts);
	Kit.eq(reads, 2, "modify reads exactly the added and changed headers");
	Kit.assert(mu.result === changes, "modify returns fn's result");
	var applied = Summary.get(mb, jf, opts);
	Kit.eq(reads, 2, "no rebuild after modify");
	Kit.eq(mu.bytes, sum_sizes(applied), "modify returns bytes");
	Kit.eq(mu.count, 50, "modify returns count");
	Kit.eq(applied.length, 50, "modify result length");
	Kit.eq(applied[0].num, added, "added message is newest");
	Kit.eq(find(applied, nums[30]).tags, "retagged", "retag applied");
	Kit.assert(find(applied, removed) === null, "removed gone");
	// usage after modify comes from the state file without a rebuild
	u = Summary.usage(mb, jf, opts);
	Kit.eq(u, { bytes: sum_sizes(applied), count: 50 }, "usage after modify");
	Kit.eq(reads, 2, "usage reads no headers");

	// the cache after modify equals a fresh rebuild
	file_remove(jf.path("summary"));
	var fresh = Summary.get(mb, jf, opts);
	Kit.eq(applied, fresh, "modify equals fresh rebuild");

	// a no-change modify reads nothing and keeps the cache
	reads = 0;
	mu = Summary.modify(mb, jf, function () { return null; }, opts);
	Kit.eq(reads, 0, "no-change modify reads nothing");
	Kit.eq(mu.result, null, "no-change result");
	Kit.eq(mu.count, 50, "no-change count");
	Summary.get(mb, jf, opts);
	Kit.eq(reads, 0, "cache still valid after no-change modify");

	// usage drops by a removed message's size
	var before = Summary.usage(mb, jf, opts).bytes;
	var victim = find(fresh, nums[40]);
	Summary.modify(mb, jf, function () {
		Kit.assert(mb.remove_msg(victim.num), "remove_msg");
		return { removed: [victim.num] };
	}, opts);
	u = Summary.usage(mb, jf, opts);
	Kit.eq(u.bytes, before - victim.size, "usage drops by the removed size");
	Kit.eq(u.count, 49, "count drops");
	Kit.assert(removed_size > 0, "removed size");

	// usage recomputed when the state file is missing
	var expect = u;
	file_remove(jf.path("state"));
	Kit.eq(Summary.usage(mb, jf, opts), expect, "usage without state file");
	Kit.assert(file_exists(jf.path("state")), "state file rewritten");

	// usage recomputed when the state file is stale (stamps differ)
	jf.write("state", { bytes: 1, count: 1, stamps: [0, 0, 0, 0], other: "kept" });
	Kit.eq(Summary.usage(mb, jf, opts), expect, "usage with stale state");
	var st = jf.read("state", null);
	Kit.eq(st.other, "kept", "other state fields are preserved");
	Kit.eq(st.bytes, expect.bytes, "stale state rewritten");

	// usage trusts a state file whose stamps match (no recompute)
	st.bytes = 12345;
	jf.write("state", st);
	Kit.eq(Summary.usage(mb, jf, opts).bytes, 12345, "matching state is used as is");
	st.bytes = expect.bytes;
	jf.write("state", st);

	// garbage state file
	write_text(jf.path("state"), "not json");
	Kit.eq(Summary.usage(mb, jf, opts), expect, "usage with corrupt state");

	// --- two JsonFiles objects on the same directory, each adding one
	var jf1 = new JsonFiles(udir, "mailbox.", { timeout: 2000 });
	var jf2 = new JsonFiles(udir, "mailbox.", { timeout: 2000 });
	var a, b;
	reads = 0;
	Summary.modify(mb, jf1, function () {
		a = save({ subject: "from one", when_imported_time: T0 + 200000 });
		return { added: [a] };
	}, opts);
	Summary.modify(mb, jf2, function () {
		b = save({ subject: "from two", when_imported_time: T0 + 200001 });
		return { added: [b] };
	}, opts);
	Kit.eq(reads, 2, "no rebuild for either modify");
	list = Summary.get(mb, jf1, opts);
	Kit.assert(find(list, a) !== null, "first modify kept");
	Kit.assert(find(list, b) !== null, "second modify kept");
	Kit.eq(list.length, 51, "both added");

	// --- a cache already stale before modify is rebuilt, not marked valid:
	// x retagged with no modify (in a later second, see the same-second
	// limitation in mailbox_summary.js), then a modify adding y
	var x = nums[12];
	next_second(file_date(mb.file + ".shd"));
	retag(x, "outside");
	var y;
	reads = 0;
	Summary.modify(mb, jf, function () {
		y = save({ subject: "y", when_imported_time: T0 + 300000 });
		return { added: [y] };
	}, opts);
	Kit.assert(reads > 2, "stale cache rebuilt by modify");
	list = Summary.get(mb, jf, opts);
	Kit.eq(find(list, x).tags, "outside", "x shows its new tags");
	Kit.assert(find(list, y) !== null, "y present");
	Kit.eq(list.length, 52, "count after stale modify");

	// --- fn throws: exception unchanged, lock released, next get correct
	var boom = { code: "boom" };
	var thrown = null;
	var z;
	try {
		Summary.modify(mb, jf, function () {
			z = save({ subject: "half done", when_imported_time: T0 + 300001 });
			throw boom;
		}, opts);
	} catch (e) {
		thrown = e;
	}
	Kit.assert(thrown === boom, "exception propagates unchanged");
	Kit.assert(!file_exists(jf.lock_path("summary")), "summary lock released");
	list = Summary.get(mb, jf, opts);
	Kit.assert(find(list, z) !== null, "next get sees the write");
	Kit.eq(list.length, 53, "count after throwing modify");

	// --- a nested call fails at once rather than waiting for its own lock
	var t = system.timer;
	thrown = null;
	try {
		Summary.modify(mb, jf, function () {
			Summary.get(mb, jf, opts);
			return null;
		}, opts);
	} catch (e) {
		thrown = e;
	}
	Kit.assert(thrown instanceof Error && /nested|reentrant/.test(thrown.message), "nested call throws: " + thrown);
	Kit.assert(system.timer - t < 1, "nested call fails fast");
	Kit.assert(!file_exists(jf.lock_path("summary")), "lock released after nested call");

	// --- state lock held during modify: modify succeeds, state skipped
	var jfs = new JsonFiles(udir, "mailbox.", { timeout: 200 });
	var q;
	Kit.assert(file_mutex(jfs.lock_path("state"), "held", 60), "hold state lock");
	try {
		Summary.modify(mb, jfs, function () {
			q = save({ subject: "q", when_imported_time: T0 + 300002 });
			return { added: [q] };
		}, opts);
	} finally {
		file_remove(jfs.lock_path("state"));
	}
	reads = 0;
	list = Summary.get(mb, jf, opts);
	Kit.eq(reads, 0, "summary was saved despite the state lock");
	Kit.assert(find(list, q) !== null, "q present");
	Kit.eq(Summary.usage(mb, jf, opts), { bytes: sum_sizes(list), count: list.length }, "usage after skipped state write");

	// --- summary lock held: get and usage still answer, without saving
	var w = save({ subject: "w outside", when_imported_time: T0 + 300003 });	// cache now stale
	var saved = jf.read("summary", null);
	Kit.assert(file_mutex(jfs.lock_path("summary"), "held", 60), "hold summary lock");
	try {
		list = Summary.get(mb, jfs, opts);
		Kit.assert(find(list, w) !== null, "get under a held lock sees w");
		Kit.eq(list.length, 55, "count under held lock");
		Kit.eq(Summary.usage(mb, jfs, opts), { bytes: sum_sizes(list), count: list.length }, "usage under held lock");
		Kit.eq(jf.read("summary", null), saved, "nothing saved without the lock");
	} finally {
		file_remove(jfs.lock_path("summary"));
	}
	file_remove(jf.path("summary"));
	Kit.eq(Summary.get(mb, jf, opts), list, "unlocked result equals a fresh rebuild");

	// --- a message both added and removed in one call is absent
	var gone;
	Summary.modify(mb, jf, function () {
		gone = save({ subject: "gone", when_imported_time: T0 + 300004 });
		Kit.assert(mb.remove_msg(gone), "remove_msg");
		return { added: [gone], removed: [gone] };
	}, opts);
	list = Summary.get(mb, jf, opts);
	Kit.assert(find(list, gone) === null, "added then removed is absent");
	Kit.eq(Summary.usage(mb, jf, opts).count, list.length, "usage count matches");
	Kit.assert(typeof Summary.apply == "undefined", "apply is gone");

	// --- fn rewrites a header in place (same size) in the same second as
	// the recorded stamps, then throws: the cache is invalidated, so the
	// change is seen (the stamps alone could not show it)
	next_second(time());
	var rt = save({ subject: "retag then throw", tags: "aaaa", when_imported_time: T0 + 300005 });
	Summary.modify(mb, jf, function () { return { changed: [rt] }; }, opts);
	thrown = null;
	try {
		Summary.modify(mb, jf, function () {
			retag(rt, "bbbb");
			throw boom;
		}, opts);
	} catch (e) {
		thrown = e;
	}
	Kit.assert(thrown === boom, "exception after an in-place change propagates unchanged");
	Kit.eq(find(Summary.get(mb, jf, opts), rt).tags, "bbbb", "in-place change seen after a throwing fn");

	// --- the summary cannot be saved (rename and remove fail, as when a
	// reader on Windows has it open) after fn rewrote a header in place:
	// modify still returns fn's result, and the next get sees the change
	var real_rename = file_rename;
	var real_remove = file_remove;
	var spath = jf.path("summary");
	next_second(time());
	retag(rt, "bbbc");
	Summary.modify(mb, jf, function () { return { changed: [rt] }; }, opts);
	var r;
	file_rename = function (from, to) { return to == spath ? false : real_rename(from, to); };
	file_remove = function (p) { return p == spath ? false : real_remove(p); };
	try {
		r = Summary.modify(mb, jf, function () {
			retag(rt, "cccc");
			return { changed: [rt] };
		}, opts);
	} finally {
		file_rename = real_rename;
		file_remove = real_remove;
	}
	Kit.eq(r.result, { changed: [rt] }, "fn's result returned although the save failed");
	Kit.assert(!file_exists(jf.lock_path("summary")), "lock released after a failed save");
	Kit.eq(directory(udir + "mailbox.summary.json.*.tmp").length, 0, "no temp file left");
	Kit.eq(find(Summary.get(mb, jf, opts), rt).tags, "cccc", "change seen after a failed save");
} finally {
	if (mb)
		mb.close();
	S.cleanup();
}
