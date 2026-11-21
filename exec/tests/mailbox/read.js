// Reading one message: bodies and attachments from its MIME parts, file
// attachments, show original, a part or the raw message as a file

load(js.exec_dir + "kit.js");
var Tags = Kit.lib("mailbox_tags");
var Store = Kit.lib("mailbox");
var MimeRead = Kit.lib("mailbox_mime");
var D = {};
load(D, Kit.exec_root() + "load/smbdefs.js");

var ALICE = 60001;
var BOB = 60002;

var S = Kit.scratch();
var shared = null;
var stores = [];
var temp_before = null;

function save(mb, hdr, body)
{
	if (hdr.from === undefined)
		hdr.from = "Sender";
	if (hdr.to === undefined)
		hdr.to = "Alice";
	if (!mb.save_msg(hdr, body))
		throw new Error("save_msg: " + mb.last_error);
	return mb.last_msg;
}

function to_alice(hdr)
{
	hdr.to_ext = String(ALICE);
	return hdr;
}

function open_store(usernum, options)
{
	var st = new Store({ usernum: usernum, alias: "u" + usernum, name: "User " + usernum,
		shared_path: S.shared_path, data_dir: S.data_dir, options: options });
	st.open();
	stores.push(st);
	return st;
}

function read_file(path)
{
	var f = new File(path);
	if (!f.open("rb", /* shareable: */ true))
		throw new Error("cannot open " + path);
	var s = f.read();
	f.close();
	return s;
}

function write_file(path, data)
{
	var f = new File(path);
	if (!f.open("wb"))
		throw new Error("cannot create " + path);
	f.write(data);
	f.close();
}

function b64(body)
{
	return base64_decode(body.b64);
}

function part(parts, name)
{
	for (var i = 0; i < parts.length; i++)
		if (parts[i].name == name)
			return parts[i];
	return null;
}

// A header read through a new handle (a long-open handle can serve stale
// header bytes after another handle rewrote them)
function fresh_header(path, num)
{
	var mb = new MsgBase(path, true, true);
	if (!mb.open())
		throw new Error("open " + path + ": " + mb.last_error);
	try {
		return mb.get_msg_header(false, num, false);
	} finally {
		mb.close();
	}
}

// Call fn(mb) on every MsgBase the store opens (bases are reopened at
// the start of every public call), to instrument their methods
function instrument(st, fn)
{
	var open_shared = st.open_shared;
	var private_base = st.private_base;
	st.open_shared = function () {
		var r = open_shared.apply(this, arguments);
		fn(this.smb);
		return r;
	};
	st.private_base = function () {
		var had = this.pmb;
		var r = private_base.apply(this, arguments);
		if (r !== null && r !== had)
			fn(r);
		return r;
	};
}

// The lines of a header block (up to its blank line) and what follows
function split_raw(raw)
{
	var cut = raw.indexOf("\r\n\r\n");
	return { lines: raw.substr(0, cut).split("\r\n"), body: raw.substr(cut + 4) };
}

function temp_list()
{
	var list = directory(system.temp_dir + "*");
	list.sort();
	return list;
}

try {
	shared = Kit.email_base(S.shared_path);
	var F = Kit.mime_fixtures;
	var alice = open_store(ALICE);
	var bob = open_store(BOB);
	temp_before = temp_list();

	// ---- MimeRead.model on part lists ----
	var m = MimeRead.model([
		{ id: "1.1", content_type: "text/plain", charset: "utf-8", name: "", content_id: "",
			disposition: "", size: 5 },
		{ id: "1.2", content_type: "text/html", charset: "", name: "", content_id: "",
			disposition: "", size: 9 },
		{ id: "2", content_type: "text/plain", charset: "", name: "=?utf-8?B?Y2Fmw6kudHh0?=",
			content_id: "", disposition: "attachment", size: 3 },
		{ id: "3", content_type: "image/png", charset: "", name: "=?iso-8859-1?Q?d=E9j=E0_vu?= .png",
			content_id: "c1", disposition: "inline", size: 7 },
		{ id: "4", content_type: "application/pdf", charset: "", name: "..", content_id: "",
			disposition: "attachment", size: 1 },
		{ id: "5", content_type: "text/plain", charset: "", name: "a/b\\..\x01x:y*?\"<>|.txt",
			content_id: "", disposition: "", size: 1 },
		{ id: "6", content_type: "application/octet-stream", charset: "",
			name: "=?x-unknown?Q?abc?=", content_id: "", disposition: "", size: 1 },
		{ id: "7", content_type: "text/plain", charset: "",
			name: "=?utf-8?q?caf=C3=A9?= =?utf-8?q?_au_lait?= menu.txt", content_id: "",
			disposition: "", size: 1 },
		{ id: "8", content_type: "text/plain", charset: "", name: "...\x7f" + new Array(300).join("\xc3\xa9"),
			content_id: "", disposition: "", size: 1 }
	]);
	Kit.eq(m.text_id, "1.1", "text_id");
	Kit.eq(m.html_id, "1.2", "html_id");
	Kit.eq(m.attachments.length, 7, "attachment count");
	Kit.eq(m.attachments[0], { pid: "2", name: "caf\xc3\xa9.txt", type: "text/plain", size: 3, cid: "",
		inline: false }, "encoded-word B name");
	Kit.eq(m.attachments[1].name, "d\xc3\xa9j\xc3\xa0 vu .png", "encoded-word Q name");
	Kit.eq(m.attachments[1].inline, true, "content id: inline");
	Kit.eq(m.attachments[1].cid, "c1", "content id");
	Kit.eq(m.attachments[2].name, "attachment.pdf", "\"..\" name");
	Kit.eq(m.attachments[3].name, "xy.txt", "base name, unsafe characters removed");
	// an unknown charset is left encoded (then sanitized: '?' removed)
	Kit.eq(MimeRead.decode_words("=?x-unknown?Q?abc?="), "=?x-unknown?Q?abc?=", "unknown charset left encoded");
	Kit.eq(m.attachments[4].name, "=x-unknownQabc=", "unknown charset name sanitized");
	Kit.eq(m.attachments[5].name, "caf\xc3\xa9 au lait menu.txt", "adjacent encoded words");
	var long_name = m.attachments[6].name;
	Kit.assert(long_name.length <= 200 && long_name.length >= 198, "length cap: " + long_name.length);
	Kit.assert(str_is_utf8(long_name) && long_name.charAt(0) == "\xc3", "UTF-8 boundary, no leading dots");
	Kit.eq(MimeRead.model(null), { text_id: null, html_id: null, attachments: [] }, "model(null)");

	// a multibyte character split across two encoded words
	Kit.eq(MimeRead.decode_words("=?utf-8?q?=E2=82?= =?UTF-8?q?=AC?=5"), "\xe2\x82\xac5", "split character");
	Kit.eq(MimeRead.decode_words("=?utf-8?b?4oI=?=\t=?utf-8?q?=AC_x?= y"), "\xe2\x82\xac x y", "split, B then Q");
	Kit.eq(MimeRead.decode_words("=?utf-8?q?=E2=82?= =?iso-8859-1?q?=E9?="),
		"=?utf-8?q?=E2=82?= \xc3\xa9", "different charsets are not joined");

	// file name hardening
	Kit.eq(MimeRead.safe_name("in\xe2\x80\x8bvoice\xe2\x80\xaefdp.exe\xef\xbb\xbf", "text/plain"),
		"invoicefdp.exe", "format characters removed");
	Kit.eq(MimeRead.safe_name("a\xe2\x81\xa6b\xe2\x81\xa9c\xe2\x80\x8fd\xe2\x80\xaae.txt", ""),
		"abcde.txt", "isolates and marks removed");
	Kit.eq(MimeRead.safe_name("report.pdf. . ", ""), "report.pdf", "trailing dots and spaces");
	Kit.eq(MimeRead.safe_name("con.txt", ""), "_con.txt", "device name with extension");
	Kit.eq(MimeRead.safe_name("LPT9", ""), "_LPT9", "device name");
	Kit.eq(MimeRead.safe_name("Nul.tar.gz", ""), "_Nul.tar.gz", "device name, two extensions");
	Kit.eq(MimeRead.safe_name("console.txt", ""), "console.txt", "not a device name");
	Kit.eq(MimeRead.safe_name("com10.txt", ""), "com10.txt", "COM10 is not a device");

	// ---- text + html + attachments ----
	var fx = F.mixed();
	var n_mixed = save(shared, to_alice(fx.hdr), fx.body);
	var g = alice.get("s" + n_mixed);
	Kit.eq(g.id, "s" + n_mixed, "summary id");
	Kit.eq(g.folder, "inbox", "summary folder");
	Kit.eq(b64(g.text), fx.plain, "text body");
	Kit.eq(g.text.charset, "utf-8", "text charset");
	Kit.eq(b64(g.html), fx.html, "html body");
	Kit.eq(g.html.charset, "iso-8859-1", "html charset");
	Kit.eq(g.parts.length, 2, "parts: " + JSON.stringify(g.parts));
	Kit.eq(g.parts[0], { pid: "2", name: "dot.png", type: "image/png", size: fx.png.length,
		cid: "img1@example", inline: true }, "inline image");
	Kit.eq(g.parts[1].name, "notes.txt", "attachment name");
	Kit.eq(g.parts[1].size, fx.notes.length, "attachment size");
	Kit.eq(g.read, false, "not marked read without mark_read");

	// part_file: decoded bytes to a temp file
	var pf = alice.part_file("s" + n_mixed, "3");
	Kit.eq(pf.temp, true, "MIME part is a temp file");
	Kit.eq(pf.name, "notes.txt", "part_file name");
	Kit.eq(pf.type, "text/plain", "part_file type");
	Kit.eq(pf.size, fx.notes.length, "part_file size");
	Kit.eq(md5_calc(read_file(pf.path), true), md5_calc(fx.notes, true), "part_file bytes");
	Kit.assert(pf.path.indexOf("notes") < 0, "temp path does not use the part name");
	file_remove(pf.path);
	pf = alice.part_file("s" + n_mixed, "2");
	Kit.eq(pf.type, "image/png", "binary part type");
	Kit.eq(pf.size, fx.png.length, "binary part size");
	Kit.eq(md5_calc(read_file(pf.path), true), md5_calc(fx.png, true), "binary part bytes");
	file_remove(pf.path);
	Kit.throws(function () { alice.part_file("s" + n_mixed, "9"); }, "notfound");
	Kit.throws(function () { alice.part_file("s" + n_mixed, "1"); }, "notfound");
	Kit.throws(function () { alice.part_file("s" + n_mixed, "../x"); }, "invalid");
	Kit.throws(function () { bob.part_file("s" + n_mixed, "2"); }, "forbidden");
	Kit.throws(function () { bob.get("s" + n_mixed); }, "forbidden");

	// mark_read sets the read attribute once
	g = alice.get("s" + n_mixed, { mark_read: true });
	Kit.eq(g.read, true, "marked read");
	var h = fresh_header(S.shared_path, n_mixed);
	Kit.assert((h.attr & D.MSG_READ) != 0, "MSG_READ set in the base");
	var shd_md5 = md5_calc(read_file(S.shared_path + ".shd"), true);
	g = alice.get("s" + n_mixed, { mark_read: true });
	Kit.eq(md5_calc(read_file(S.shared_path + ".shd"), true), shd_md5, "no second write");

	// ---- a text part above max_text_part_bytes ----
	var big = F.big_text(2 * 1024 * 1024);
	var n_big = save(shared, to_alice(big.hdr), big.body);
	g = alice.get("s" + n_big);
	Kit.eq(g.text, null, "oversized text body");
	Kit.eq(b64(g.html), "<p>small</p>", "small html body");
	Kit.eq(g.parts.length, 1, "oversized part listed");
	Kit.eq(g.parts[0].pid, "1", "oversized part id");
	Kit.eq(g.parts[0].size, big.size, "oversized part size");

	// ---- an HTML-only message ----
	var n_html = save(shared, to_alice({ mime_version: "1.0", content_type: "text/html; charset=utf-8" }),
		"<p>only html</p>\r\n");
	g = alice.get("s" + n_html);
	Kit.eq(g.text, null, "html-only: no text");
	Kit.eq(b64(g.html), "<p>only html</p>\r\n", "html-only body");
	Kit.eq(g.html.charset, "utf-8", "html-only charset");
	Kit.eq(g.parts, [], "html-only: no parts");

	// ---- a part whose saved size is over the cap, its reported size not ----
	var liar = open_store(ALICE, { max_text_part_bytes: 100 });
	instrument(liar, function (mb) {
		var get_parts = mb.get_msg_parts;
		mb.get_msg_parts = function () {
			var list = get_parts.apply(this, arguments);
			for (var k = 0; list && k < list.length; k++)
				list[k].size = 1;
			return list;
		};
	});
	var n_liar = save(shared, to_alice({}), new Array(301).join("y") + "\r\n");
	var temp_now = temp_list();
	g = liar.get("s" + n_liar);
	Kit.eq(g.text, null, "saved size over the cap");
	Kit.eq(g.parts.length, 1, "over-cap part listed");
	Kit.eq(g.parts[0].pid, "1", "over-cap part id");
	Kit.eq(temp_list(), temp_now, "no temp file left by the over-cap part");

	// ---- CP437 text written on the BBS ----
	var n_cp = save(shared, to_alice({ from: "Sysop", from_ext: "1" }),
		"Box \xC4\xC4\xB3 \x01hred\x01n and \x1b[1;31mansi\x1b[0m done\r\n");
	g = alice.get("s" + n_cp);
	Kit.eq(g.text.charset, "utf-8", "cp437 returned as utf-8");
	Kit.eq(b64(g.text), "Box \xe2\x94\x80\xe2\x94\x80\xe2\x94\x82 red and ansi done\r\n", "cp437 text");
	Kit.eq(g.html, null, "no html");
	Kit.eq(g.headers.net_type, "local", "local net_type");
	Kit.eq(g.headers.to_domain, "", "local to_domain");
	Kit.eq(g.headers.to_addr, "", "local to_addr");
	Kit.eq(g.headers.from_local_addr, system.lastuser >= 1 && !(new User(1).settings & ((1 << 0) | (1 << 6)))
		? new User(1).email : "", "a local sender's address is that user's");
	g = alice.get("s" + save(shared, to_alice({ from: "Nobody", from_ext: String(system.lastuser + 1) }), "x\r\n"));
	Kit.eq(g.headers.from_local_addr, "", "no address for a sender who is not a user");
	g = alice.get("s" + save(shared, to_alice({ from: "Out", from_ext: "1", from_net_type: D.NET_INTERNET, from_net_addr: "out@far.example" }), "x\r\n"));
	Kit.eq(g.headers.from_local_addr, "", "no local address for Internet mail");

	// a non-MIME message flagged UTF-8
	var n_u8 = save(shared, to_alice({ ftn_charset: "UTF-8 4" }), "caf\xc3\xa9 \x01hbold\r\n");
	g = alice.get("s" + n_u8);
	Kit.eq(g.text.charset, "utf-8", "utf-8 charset");
	Kit.eq(b64(g.text), "caf\xc3\xa9 bold\r\n", "utf-8 text without Ctrl-A");

	// ---- header display values ----
	var n_inet = save(shared, to_alice({ from: "bob", from_net_type: D.NET_INTERNET,
		from_net_addr: "bob@example.com", to_list: "Alice <alice@bbs.example>, Carol <c@x.example>",
		cc_list: "dave@y.example", replyto_list: "list@z.example", id: "<inet@test>",
		reply_id: "<orig@test>", forward_path: "<alice@bbs.example>",
		field_list: [ { type: D.RFC822FROM, data: "\"=?utf-8?q?Bob_Sm=C3=AFth?=\" <bob@example.com>" },
			{ type: D.SMB_COMMENT, data: "Forwarded by Carol" } ] }), "hi\r\n");
	g = alice.get("s" + n_inet);
	Kit.eq(g.headers.from, "Bob Sm\xc3\xafth", "from display name");
	Kit.eq(g.headers.from_addr, "bob@example.com", "from_addr");
	Kit.eq(g.headers.to, "Alice <alice@bbs.example>, Carol <c@x.example>", "to list");
	Kit.eq(g.headers.cc, "dave@y.example", "cc list");
	Kit.eq(g.headers.reply_to, "list@z.example", "reply_to");
	Kit.eq(g.headers.message_id, "<inet@test>", "message_id");
	Kit.eq(g.headers.reply_id, "<orig@test>", "reply_id");
	Kit.eq(g.headers.net_type, "internet", "internet net_type");
	Kit.eq(g.headers.to_domain, "bbs.example", "to_domain");
	Kit.eq(g.headers.to_addr, "alice@bbs.example", "to_addr: the address it was delivered to");
	Kit.eq(g.comment, "Forwarded by Carol", "comment");
	Kit.eq(g.text.charset, "windows-1252", "internet non-MIME charset");
	Kit.eq(g.headers.to_list, [{ name: "Alice", addr: "alice@bbs.example" }, { name: "Carol", addr: "c@x.example" }],
		"to_list entries");
	Kit.eq(g.headers.cc_list, [{ name: "", addr: "dave@y.example" }], "cc_list entries");
	Kit.eq(g.headers.reply_to_list, [{ name: "", addr: "list@z.example" }], "reply_to_list entries");

	// ---- an encoded word can never add a recipient or change an address:
	// structure from the raw field, the name decoded as text afterwards
	// (B encoding: in Q form the "<" and "," would be in the raw field,
	// which then really names two mailboxes, as any parser reads it)
	var CRAFT = "=?UTF-8?B?" + base64_encode("Alice <alice@x.example>, Mallory") + "?= <mallory@evil.example>";
	var n_craft = save(shared, to_alice({ from: "m", from_net_type: D.NET_INTERNET, from_net_addr: "m@evil.example",
		to_list: CRAFT, cc_list: '"Q, \\"x\\"" <q@q.example>, (c (n) \\) m) r@r.example; =?UTF-8?B?' + base64_encode("Bob <b@b.example>; Eve <e@e.example>") + '?= <z@z.example>',
		replyto_list: CRAFT, field_list: [{ type: D.RFC822FROM,
			data: "=?UTF-8?B?" + base64_encode("Bank <service@bank.example>") + "?= <m@evil.example>" }] }), "x\r\n");
	g = alice.get("s" + n_craft);
	var MAL = [{ name: "Alice <alice@x.example>, Mallory", addr: "mallory@evil.example" }];
	Kit.eq(g.headers.to_list, MAL, "crafted To: one entry, the real address, the name as text");
	Kit.eq(g.headers.reply_to_list, MAL, "crafted Reply-To: the same");
	Kit.eq(g.headers.cc_list, [{ name: 'Q, "x"', addr: "q@q.example" }, { name: "c (n) ) m", addr: "r@r.example" },
		{ name: "Bob <b@b.example>; Eve <e@e.example>", addr: "z@z.example" }], "cc: quoted, comment, encoded");
	Kit.eq(g.headers.to, '"Alice <alice@x.example>, Mallory" <mallory@evil.example>', "displayed To names one address");
	Kit.eq(g.headers.from, "Bank <service@bank.example>", "a From name is text");
	Kit.eq(g.headers.from_addr, "m@evil.example", "beside the real address");

	// bidirectional controls (a right-to-left override can make a name or
	// subject read as something else) are removed from names and subjects
	var RLO = "\xE2\x80\xAE", LRI = "\xE2\x81\xA6", ALM = "\xD8\x9C", LRM = "\xE2\x80\x8E";
	var n_bidi = save(shared, to_alice({ from: "Ev" + RLO + "il", from_net_type: D.NET_INTERNET, from_net_addr: "e@evil.example",
		auxattr: D.MSG_HFIELDS_UTF8, subject: "Invoice" + RLO + "fdp.exe" + LRI + ALM + LRM,
		to_list: "=?UTF-8?B?" + base64_encode("Bo" + RLO + "b") + "?= <b@b.example>",
		field_list: [{ type: D.RFC822FROM, data: "=?UTF-8?B?" + base64_encode("Ma" + RLO + "llory") + "?= <e@evil.example>" }] }), "x\r\n");
	g = alice.get("s" + n_bidi);
	Kit.eq(g.headers.from, "Mallory", "no bidi control in a From name");
	Kit.eq(g.headers.to_list, [{ name: "Bob", addr: "b@b.example" }], "none in a recipient name");
	Kit.eq(g.subject, "Invoicefdp.exe", "none in the subject (reader)");
	var bl = alice.list("inbox", { offset: 0, limit: 500 }).items.filter(function (x) { return x.id === "s" + n_bidi; })[0];
	Kit.eq([bl.from, bl.subject], ["Evil", "Invoicefdp.exe"], "none in a list row");
	Kit.eq(g.headers.from_addr_bad, false, "a clean sender address is not marked");
	// the same in the fallbacks: no RFC822 From field, no to_list; and an
	// ADDRESS holding a control character is shown stripped and marked
	// malformed (it can never match anything the user listed)
	var n_bidi2 = save(shared, to_alice({ from: "Ev" + RLO + "e", from_net_type: D.NET_INTERNET,
		from_net_addr: "photos@pics" + RLO + ".example", auxattr: D.MSG_HFIELDS_UTF8, to: "Al" + LRI + "ice",
		subject: "plain" }), "x\r\n");
	g = alice.get("s" + n_bidi2);
	Kit.eq([g.headers.from, g.headers.to], ["Eve", "Alice"], "no bidi control in the fallback from and to");
	Kit.eq(g.headers.from_addr, "photos@pics.example", "the address shown stripped");
	Kit.eq(g.headers.from_addr_bad, true, "and marked malformed");
	bl = alice.list("inbox", { offset: 0, limit: 500 }).items.filter(function (x) { return x.id === "s" + n_bidi2; })[0];
	Kit.eq([bl.from, bl.from_addr, bl.from_addr_bad], ["Eve", "photos@pics.example", true], "a list row: stripped, marked");
	var n_c0 = save(shared, to_alice({ from: "Tab", from_net_type: D.NET_INTERNET, from_net_addr: "a\tb@x.example",
		subject: "c0" }), "x\r\n");
	g = alice.get("s" + n_c0);
	Kit.eq([g.headers.from_addr, g.headers.from_addr_bad], ["ab@x.example", true], "a C0 control in an address: malformed");

	// ---- show original and the raw message ----
	var o = alice.original("s" + n_mixed);
	Kit.assert(/(^|\r\n)Subject: /.test(o.rfc822), "Subject line: " + o.rfc822);
	Kit.assert(/(^|\r\n)Message-ID: </.test(o.rfc822), "Message-ID line");
	Kit.assert(o.stored instanceof Array && o.stored.length > 0, "stored list");
	Kit.eq(o.external_files, false, "no external files");
	Kit.throws(function () { bob.original("s" + n_mixed); }, "forbidden");

	var stored_body = system.temp_dir + "read_test_body_" + n_mixed + "_" + random(100000);
	if (!shared.save_msg_part(n_mixed, "0", stored_body))
		throw new Error("save_msg_part 0: " + shared.last_error);
	var want_md5 = md5_calc(read_file(stored_body), true);
	file_remove(stored_body);
	var rf = alice.raw_file("s" + n_mixed);
	Kit.eq(rf.temp, true, "raw_file temp");
	Kit.assert(/\.eml$/.test(rf.name), "raw_file name: " + rf.name);
	var raw = read_file(rf.path);
	Kit.eq(rf.size, raw.length, "raw_file size");
	var cut = raw.indexOf("\r\n\r\n");
	Kit.assert(cut > 0, "blank line in raw file");
	Kit.eq(md5_calc(raw.substr(cut + 4), true), want_md5, "raw body md5");
	file_remove(rf.path);
	Kit.throws(function () { bob.raw_file("s" + n_mixed); }, "forbidden");

	// CR/LF in stored header fields cannot forge header lines
	var n_evil = save(shared, to_alice({ subject: "evil\r\nBcc: x@y\r\n\r\nbody?", id: "<a\r\nX-Inj: 1>",
		from: "Mal\nX-From-Inj: 2" }), "evil body\r\n");
	stored_body = system.temp_dir + "read_test_body_" + n_evil + "_" + random(100000);
	if (!shared.save_msg_part(n_evil, "0", stored_body))
		throw new Error("save_msg_part 0: " + shared.last_error);
	want_md5 = md5_calc(read_file(stored_body), true);
	file_remove(stored_body);
	function check_block(label, lines) {
		var subjects = 0;
		for (var k = 0; k < lines.length; k++) {
			Kit.assert(!/^(Bcc|X-Inj|X-From-Inj):/i.test(lines[k]), label + ": forged line " + lines[k]);
			Kit.assert(lines[k] !== "", label + ": blank line inside the block");
			Kit.assert(/^[!-9;-~]+:/.test(lines[k]) || /^[ \t]/.test(lines[k]), label + ": bad line " + lines[k]);
			if (/^Subject:/.test(lines[k])) {
				subjects++;
				var value = lines[k];
				for (var c = k + 1; c < lines.length && /^[ \t]/.test(lines[c]); c++)
					value += lines[c];
				Kit.assert(value.indexOf("evil") >= 0 && value.indexOf("Bcc: x@y") >= 0
					&& value.indexOf("body?") >= 0, label + ": subject text: " + value);
			}
		}
		Kit.eq(subjects, 1, label + ": one Subject field");
	}
	rf = alice.raw_file("s" + n_evil);
	raw = read_file(rf.path);
	file_remove(rf.path);
	var parts_raw = split_raw(raw);
	check_block("raw_file", parts_raw.lines);
	Kit.eq(md5_calc(parts_raw.body, true), want_md5, "raw body md5 after a hostile header");
	Kit.eq(parts_raw.body.substr(0, 2) == "\r\n", false, "exactly one blank line");
	o = alice.original("s" + n_evil);
	Kit.eq(o.rfc822.substr(-4), "\r\n\r\n", "original ends with one blank line");
	Kit.eq(o.rfc822.indexOf("\r\n\r\n"), o.rfc822.length - 4, "original: no blank line inside");
	check_block("original", o.rfc822.substr(0, o.rfc822.length - 4).split("\r\n"));
	for (var sl = 0; sl < o.stored.length; sl++)
		Kit.assert(!/[\r\n]/.test(o.stored[sl]), "stored line holds CR/LF: " + o.stored[sl]);

	// a device name as a subject
	var n_con = save(shared, to_alice({ subject: "CON" }), "x\r\n");
	rf = alice.raw_file("s" + n_con);
	file_remove(rf.path);
	Kit.eq(rf.name, "_CON.eml", ".eml name from a device-name subject");

	// ---- file attachments ----
	var in_dir = S.data_dir + format("file/%04u.in/", ALICE);
	mkpath(in_dir);
	write_file(in_dir + "photo.jpg", "JPEGDATA");
	write_file(in_dir + "my notes.txt", "some notes");
	var n_fa = save(shared, to_alice({ subject: "photo.jpg \"my notes.txt\" ../../etc/passwd",
		auxattr: D.MSG_FILEATTACH }), "see attached\r\n");
	g = alice.get("s" + n_fa);
	Kit.eq(g.subject, "photo.jpg \"my notes.txt\" ../../etc/passwd", "subject as stored");
	Kit.eq(g.parts, [ { pid: "f1", name: "photo.jpg", type: "image/jpeg", size: 8 },
		{ pid: "f2", name: "my notes.txt", type: "text/plain", size: 10 },
		{ pid: "f3", name: "passwd", type: "application/octet-stream", size: -1 } ], "file parts");
	pf = alice.part_file("s" + n_fa, "f1");
	Kit.eq(pf.temp, false, "file attachment is not a temp file");
	Kit.eq(pf.path, in_dir + "photo.jpg", "real path");
	Kit.eq(pf.size, 8, "file size");
	Kit.throws(function () { alice.part_file("s" + n_fa, "f3"); }, "notfound");
	Kit.throws(function () { alice.part_file("s" + n_fa, "f4"); }, "notfound");
	Kit.throws(function () { bob.part_file("s" + n_fa, "f1"); }, "forbidden");
	o = alice.original("s" + n_fa);
	Kit.eq(o.external_files, true, "original: external files");

	alice.folder_create("Pics");
	var mv = alice.move(["s" + n_fa], "u:" + Tags.encode("Pics"));
	Kit.assert(mv[0].ok, "move: " + JSON.stringify(mv));
	var pid_fa = mv[0].new_id;
	Kit.assert(file_exists(in_dir + "photo.jpg") && file_exists(in_dir + "my notes.txt"), "files kept on move");
	var puts = 0;
	var counted = open_store(ALICE);
	instrument(counted, function (mb) {
		var put = mb.put_msg_header;
		mb.put_msg_header = function () {
			puts++;
			return put.apply(this, arguments);
		};
	});
	// head: marked read, the list item only
	g = counted.get(pid_fa, { mark_read: true, head: true });
	Kit.eq(puts, 1, "private mark_read with head: one header write");
	Kit.eq([g.id, g.read, g.folder], [pid_fa, true, "u:" + Tags.encode("Pics")], "head: the item, read");
	Kit.eq([g.headers, g.text, g.html, g.parts], [undefined, undefined, undefined, undefined],
		"head: no headers, bodies or parts");
	g = counted.get(pid_fa, { mark_read: true });
	Kit.eq(puts, 1, "private mark_read: one header write");
	g = counted.get(pid_fa, { mark_read: true });
	Kit.eq(puts, 1, "private mark_read: no second write");
	Kit.eq(g.parts[0].size, 8, "file part from the private base");
	Kit.eq(g.read, true, "private mark_read");
	Kit.assert(fresh_header(S.data_dir + format("user/%04u/mailbox", ALICE), parseInt(pid_fa.substr(1), 10)).attr
		& D.MSG_READ, "private MSG_READ");
	Kit.eq(alice.part_file(pid_fa, "f2").path, in_dir + "my notes.txt", "private part_file");
	var rm = alice.remove([pid_fa]);
	Kit.assert(rm[0].ok, "to Trash: " + JSON.stringify(rm));
	rm = alice.remove([pid_fa]);
	Kit.assert(rm[0].ok && rm[0].new_id === null, "for real: " + JSON.stringify(rm));
	Kit.assert(!file_exists(in_dir + "photo.jpg") && !file_exists(in_dir + "my notes.txt"), "files removed");

	// a sent extra: no mark_read, files are not the sender's
	var n_sent = save(shared, { to: "Bob", to_ext: String(BOB), from: "Alice", from_ext: String(ALICE),
		subject: "for bob" }, "hello bob\r\n");
	g = alice.get("s" + n_sent, { mark_read: true });
	Kit.eq(g.extra, true, "sent extra");
	Kit.eq(fresh_header(S.shared_path, n_sent).attr & D.MSG_READ, 0, "sent extra not marked read");

	// ---- malformed fixtures ----
	var bad = F.malformed();
	for (var i = 0; i < bad.length; i++) {
		var nb = save(shared, to_alice(bad[i].hdr), bad[i].body);
		var parts = shared.get_msg_parts(nb);
		var model = MimeRead.model(parts);
		Kit.assert(model && model.attachments instanceof Array, bad[i].label + ": model");
		g = alice.get("s" + nb);
		Kit.assert(g.parts instanceof Array, bad[i].label + ": get");
	}

	// no temp file left behind
	Kit.eq(temp_list(), temp_before, "temp dir unchanged");
} finally {
	for (var s = 0; s < stores.length; s++)
		stores[s].close();
	if (shared !== null)
		shared.close();
	S.cleanup();
}
