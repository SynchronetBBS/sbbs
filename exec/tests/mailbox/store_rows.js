// Store, read side: list rows of shared messages come from the facts
// cache (mailbox.shared.json) once seen, so a page or a search reads no
// shared headers; read and replied come from the index on every call

load(js.exec_dir + "kit.js");
var JsonFiles = Kit.lib("mailbox_files");
var Tags = Kit.lib("mailbox_tags");
var Store = Kit.lib("mailbox");
var D = {};
load(D, Kit.exec_root() + "load/smbdefs.js");

var ALICE = 60001;

var S = Kit.scratch();
var shared = null;
var stores = [];

function save(mb, hdr, body)
{
	if (hdr.from === undefined)
		hdr.from = "Sender";
	if (hdr.to === undefined)
		hdr.to = "Rcpt";
	if (!mb.save_msg(hdr, body === undefined ? "body\r\n" : body))
		throw new Error("save_msg: " + mb.last_error);
	return mb.last_msg;
}

function item(list, id)
{
	for (var i = 0; i < list.items.length; i++)
		if (list.items[i].id == id)
			return list.items[i];
	return null;
}

function ids(list)
{
	var r = [];
	for (var i = 0; i < list.items.length; i++)
		r.push(list.items[i].id);
	return r;
}

// A Store whose shared header reads are counted
function open_store(usernum)
{
	var st = new Store({ usernum: usernum, alias: "u" + usernum, name: "User " + usernum,
		shared_path: S.shared_path, data_dir: S.data_dir });
	st.open();
	st.reads = 0;
	st.shared_header = function (num) {
		st.reads++;
		return Store.prototype.shared_header.call(this, num);
	};
	stores.push(st);
	return st;
}

// The list as a first call builds it: the facts cache removed
function rebuilt(st, folder_id, opts)
{
	var jf = new JsonFiles(S.user_dir(ALICE), "mailbox.");
	file_remove(jf.path("shared"));
	var before = st.reads;
	var r = st.list(folder_id, opts);
	Kit.assert(st.reads > before, "rows read from the headers");
	return r;
}

// An object's JSON with its keys sorted
function canon(o)
{
	var keys = [];
	for (var k in o)
		keys.push(k);
	keys.sort();
	var out = {};
	for (var i = 0; i < keys.length; i++)
		out[keys[i]] = o[keys[i]];
	return JSON.stringify(out);
}

try {
	shared = Kit.email_base(S.shared_path);
	var UTF8_SUBJ = "Caf\xC3\xA9 \xE6\x9D\xB1\xE4\xBA\xAC \xF0\x9F\x8E\x89";
	var A1 = save(shared, { to_ext: String(ALICE), from: "Carol", subject: "Hello zz",
		from_net_type: D.NET_INTERNET, from_net_addr: "carol@example.com",
		attr: D.MSG_READ | D.MSG_VALIDATED, priority: 1 });
	var A2 = save(shared, { to_ext: String(ALICE), from: "Zo\xC3\xAB", subject: UTF8_SUBJ,
		auxattr: D.MSG_HFIELDS_UTF8, mime_version: "1.0",
		content_type: "multipart/mixed; boundary=\"b\"" },
		"--b\r\nContent-Type: text/plain\r\n\r\nx\r\n--b--\r\n");
	var A3 = save(shared, { to_ext: String(ALICE), from: "D\x82j\x85 vu", subject: "cp437 zz" });
	var SENT = save(shared, { from_ext: String(ALICE), from: "Alice", to: "Tom",
		to_net_type: D.NET_INTERNET, to_net_addr: "tom@example.com", subject: "sent zz",
		id: "<sent@test>", netattr: D.NETMSG_SENT });

	var st = open_store(ALICE);
	var first = st.list("inbox");
	Kit.eq(ids(first), ["s" + A3, "s" + A2, "s" + A1], "inbox order");
	var sent1 = st.list("sent");

	// Warm: a page, a search and Sent read no shared header
	st.reads = 0;
	var warm = st.list("inbox");
	var found = st.list("all", { search: "\xE6\x9D\xB1\xE4\xBA\xAC" });
	var sent2 = st.list("sent");
	Kit.eq(st.reads, 0, "warm list, search and Sent read no shared headers");
	Kit.eq(ids(found), ["s" + A2], "search over cached rows");
	Kit.eq(item(found, "s" + A2).subject, UTF8_SUBJ, "UTF-8 subject kept byte for byte");
	Kit.eq(JSON.stringify(warm), JSON.stringify(first), "cached rows equal the first rows");
	Kit.eq(JSON.stringify(sent2), JSON.stringify(sent1), "cached Sent rows equal the first");

	// The cached rows equal a fresh build and the entries read from the headers
	Kit.eq(JSON.stringify(warm), JSON.stringify(rebuilt(st, "inbox")), "inbox: rebuilt alike");
	Kit.eq(JSON.stringify(sent2), JSON.stringify(rebuilt(st, "sent")), "sent: rebuilt alike");
	st.list("inbox");
	var facts = new JsonFiles(S.user_dir(ALICE), "mailbox.").read("shared", null).recs;
	var idx = shared.get_index();
	var nums = [A1, A2, A3, SENT];
	for (var i = 0; i < nums.length; i++) {
		var rec = null;
		for (var j = 0; j < idx.length; j++)
			if (idx[j].number == nums[i])
				rec = idx[j];
		var cached = st.summary_of({ kind: "s", num: nums[i], extra: nums[i] == SENT, rec: rec,
			fact: facts[nums[i]] });
		var read = st.summary_of({ kind: "s", num: nums[i], extra: nums[i] == SENT });
		Kit.eq(canon(cached), canon(read), "summary of #" + nums[i] + ": cache and header agree");
	}
	var a1 = item(warm, "s" + A1);
	Kit.eq([a1.from, a1.from_addr, a1.priority, a1.starred, a1.read], ["Carol",
		"carol@example.com", "high", true, true], "row fields");
	Kit.eq(item(warm, "s" + A2).attach, true, "attach");
	Kit.eq(item(warm, "s" + A3).from, "D\xC3\xA9j\xC3\xA0 vu", "CP437 name as UTF-8");

	// Read and replied set outside the library (no forget_shared): seen at once
	st.list("inbox");
	var h = shared.get_msg_header(false, A2, false);
	h.attr |= D.MSG_READ | D.MSG_REPLIED;
	if (!shared.put_msg_header(false, A2, h))
		throw new Error("put_msg_header: " + shared.last_error);
	st.reads = 0;
	var a2 = item(st.list("inbox"), "s" + A2);
	Kit.eq([a2.read, a2.replied, st.reads], [true, true, 0], "attributes from the index");

	// A header changed by the library calls forget_shared(): the row is read again
	h = shared.get_msg_header(false, A3, false);
	h.subject = "renamed zz";
	if (!shared.put_msg_header(false, A3, h))
		throw new Error("put_msg_header: " + shared.last_error);
	Kit.eq(item(st.list("inbox"), "s" + A3).subject, "cp437 zz", "cached until forgotten");
	st.forget_shared([A3]);
	st.reads = 0;
	Kit.eq(item(st.list("inbox"), "s" + A3).subject, "renamed zz", "read after forget_shared");
	Kit.eq(st.reads, 1, "only the forgotten row read");

	// A cached row of the wrong shape is not used
	var jf = new JsonFiles(S.user_dir(ALICE), "mailbox.");
	var c = jf.read("shared", null);
	c.recs[A1][7] = ["short"];
	jf.write("shared", c);
	st.reads = 0;
	Kit.eq(item(st.list("inbox"), "s" + A1).from, "Carol", "bad row: header read");
	Kit.assert(st.reads > 0, "bad row not used");

	// A cache of the earlier version is rebuilt
	c = jf.read("shared", null);
	c.v = 1;
	jf.write("shared", c);
	st.list("inbox");
	Kit.eq(jf.read("shared", null).v, 4, "cache rebuilt at the current version");
	Kit.eq(jf.read("shared", null).recs[A1].length, 8, "rebuilt facts hold the row");
	Kit.eq(item(st.list("sent"), "s" + SENT).delivery, { state: "sent" }, "delivery");
} finally {
	for (var k = 0; k < stores.length; k++)
		stores[k].close();
	if (shared)
		shared.close();
	S.cleanup();
}
