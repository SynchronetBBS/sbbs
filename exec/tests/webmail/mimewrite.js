// MimeWrite: the MIME body of a message being sent (webmail/lib/mimewrite.js),
// read back through the mail base and the mailbox library's MIME reader

load(js.exec_dir + "kit.js");
var MimeWrite = Kit.wlib("mimewrite");
var Store = Kit.lib("mailbox");
var D = {};
load(D, Kit.exec_root() + "load/smbdefs.js");

var ALICE = 60001;
var S = Kit.scratch();
var stores = [];
var logged = [];
MimeWrite.log = function (level, text) {
	logged.push(text);
};

function write_file(path, data)
{
	var f = new File(path);
	Kit.assert(f.open("wb"), "create " + path);
	f.write(data);
	f.close();
}

function read_file(path)
{
	var f = new File(path);
	Kit.assert(f.open("rb"), "open " + path);
	var t = f.read();
	f.close();
	return t;
}

function bytes(n, seed)
{
	var a = [];
	for (var i = 0; i < n; i++)
		a.push(String.fromCharCode((i * 31 + (seed || 0) + (i >> 8)) & 0xFF));
	return a.join("");
}

var seq = 0;
// Build, save to the scratch shared base to Alice, return {id, built, raw}
function round_trip(body, files)
{
	var out = S.dir + "built" + (seq++) + ".txt";
	var r = MimeWrite.build(body, files, out);
	var raw = read_file(out);
	var hdr = { to: "Alice", to_ext: String(ALICE), from: "Tester", subject: "rt " + seq,
		mime_version: r.mime_version, content_type: r.content_type, field_list: r.fields,
		auxattr: D.MSG_HFIELDS_UTF8 };
	var mb = Kit.email_base(S.shared_path);
	try {
		Kit.assert(mb.save_msg_from_file(hdr, out), "save: " + mb.last_error);
		var n = mb.last_msg;
	} finally {
		mb.close();
	}
	file_remove(out);
	return { id: "s" + n, built: r, raw: raw };
}

function store()
{
	var st = new Store({ usernum: ALICE, shared_path: S.shared_path, data_dir: S.data_dir });
	st.open();
	stores.push(st);
	return st;
}

function lines_ok(raw, max, what)
{
	var lines = raw.split("\r\n");
	for (var i = 0; i < lines.length; i++)
		Kit.assert(lines[i].length <= max, what + ": line " + i + " is " + lines[i].length);
	Kit.assert(!/[^\r]\n|\r[^\n]/.test(raw.replace(/\r\n$/, "")), what + ": only CRLF line breaks");
}

function strip_end(s)
{
	return s.replace(/(\r\n)+$/, "");
}

try {
	// --- text(): every line break as CRLF
	Kit.eq(MimeWrite.text("a\rb\nc\r\nd\n\re"), "a\r\nb\r\nc\r\nd\r\n\r\ne", "line breaks");

	// --- encoding()
	Kit.eq(MimeWrite.encoding("Hello\r\n.\r\nworld"), "7bit", "a lone dot is 7bit");
	Kit.eq(MimeWrite.encoding("Caf\xc3\xa9"), "8bit", "UTF-8 is 8bit");
	var long = new Array(5001).join("x");
	Kit.eq(MimeWrite.encoding("a\r\n" + long), "quoted-printable", "5000-byte line");
	Kit.eq(MimeWrite.encoding("a\x00b"), "quoted-printable", "NUL");
	Kit.eq(MimeWrite.encoding("x\r\nFrom here"), "quoted-printable", "From line");
	Kit.eq(MimeWrite.encoding("trailing \r\nx"), "quoted-printable", "trailing space");
	Kit.eq(MimeWrite.encoding("a\x1bb"), "quoted-printable", "ESC");
	Kit.eq(MimeWrite.encoding("tab\there"), "7bit", "tab");

	// --- qp(): lines of at most 76, decodes back
	var hostile = "Hi\r\n.\r\nFrom me\r\n" + long + "\r\nnul\x00nul\r\nend \t\r\n=?UTF-8?B?eA==?= \xc3\xa9\r\n";
	var q = MimeWrite.qp(MimeWrite.text(hostile));
	lines_ok(q, 76, "qp");
	Kit.assert(q.indexOf("\r\n.\r\n") < 0 && q.indexOf("\r\n.") < 0, "no line starts with a dot");
	Kit.assert(q.indexOf("\r\nFrom ") < 0, "no line starts with From");
	Kit.assert(!/[\x00-\x08\x0b-\x0c\x0e-\x1f\x7f-\xff]/.test(q), "qp is 7-bit");

	// --- param(): file names never break out of their parameter
	Kit.eq(MimeWrite.param("filename", "plain name.txt"), 'filename="plain name.txt"', "plain");
	var evil = MimeWrite.param("filename", 'a"\r\nBcc: x@evil.example;\x00 b.exe');
	Kit.assert(!/[\r\n\x00]/.test(evil), "no line break or NUL: " + evil);
	Kit.assert(/^filename="[^"]*"$/.test(evil), "one quoted value: " + evil);
	Kit.eq(MimeWrite.param("name", ""), 'name="attachment"', "empty name");
	Kit.eq(MimeWrite.param("name", "\xe2\x80\xaefdp.exe"), 'name="fdp.exe"', "bidi control removed");
	Kit.assert(/^name="=\?UTF-8\?B\?/.test(MimeWrite.param("name", "r\xc3\xa9sum\xc3\xa9.pdf")), "UTF-8 encoded");
	Kit.assert(/^name="=\?UTF-8\?B\?/.test(MimeWrite.param("name", "=?UTF-8?B?ZXZpbA==?=")), "a raw word is re-encoded");
	Kit.eq(MimeWrite.safe_type("text/html\r\nX-Evil: 1"), "application/octet-stream", "bad type");
	Kit.eq(MimeWrite.safe_type("image/png"), "image/png", "good type");
	Kit.eq(MimeWrite.safe_type("multipart/mixed"), "application/octet-stream", "multipart type");
	Kit.eq(MimeWrite.safe_type("message/rfc822"), "application/octet-stream", "message type");

	var st = store();

	// --- single part, each hostile body read back through the Store
	var bodies = [
		["plain", "Hello Alice\r\nsecond line\r\n"],
		["dot", "a\r\n.\r\nb\r\n..\r\n"],
		["bare CR and LF", "one\rtwo\nthree\r\n"],
		["long line", "x\r\n" + long + "\r\ny\r\n"],
		["NUL", "a\x00b\r\n\x00\r\n"],
		["From", "From the start\r\nFrom: header-like\r\n"],
		["UTF-8", "Caf\xc3\xa9 \xe2\x82\xac \xf0\x9f\x98\x80\r\n"],
		["trailing white space", "keep this  \r\nand\t\r\n"],
		["encoded word text", "=?UTF-8?B?ZXZpbA==?= =3D\r\n"]
	];
	for (var i = 0; i < bodies.length; i++) {
		var label = bodies[i][0];
		var rt = round_trip(bodies[i][1], []);
		Kit.eq(rt.built.content_type, "text/plain; charset=utf-8", label + ": type");
		Kit.eq(rt.built.mime_version, "1.0", label + ": MIME-Version");
		lines_ok(rt.raw, 998, label);
		var g = st.get(rt.id);
		Kit.assert(g.text !== null, label + ": a text body");
		Kit.eq(strip_end(base64_decode(g.text.b64)), strip_end(MimeWrite.text(bodies[i][1])), label + ": read back");
		Kit.eq(g.parts.length, 0, label + ": no parts");
	}
	var rtq = round_trip("x\r\n" + long, []);
	Kit.eq(rtq.built.encoding, "quoted-printable", "long line: qp");
	Kit.eq(rtq.built.fields, [{ type: D.RFC822HEADER, data: "Content-Transfer-Encoding: quoted-printable" }], "CTE field");
	Kit.eq(round_trip("plain ascii", []).built.fields, [], "7bit: no CTE field");
	Kit.eq(round_trip("\xc3\xa9", []).built.fields[0].data, "Content-Transfer-Encoding: 8bit", "8bit field");

	// --- attachments: hostile name and type, binary bytes, sizes
	var p1 = S.dir + "a1.bin";
	var p2 = S.dir + "a2.bin";
	var p3 = S.dir + "a3.bin";
	var b1 = bytes(MimeWrite.PIECE * 2 + 1000, 1);	// more than two pieces
	var b2 = bytes(0, 0);
	var b3 = bytes(57, 3);
	write_file(p1, b1);
	write_file(p2, b2);
	write_file(p3, b3);
	var files = [
		{ name: 'evil"\r\nBcc: x@evil.example;\x00 b.exe', type: "text/html\r\nX-Evil: 1", path: p1, size: b1.length },
		{ name: "r\xc3\xa9sum\xc3\xa9.pdf", type: "application/pdf", path: p2, size: 0 },
		{ name: "CON.txt", type: "text/plain", path: p3, size: b3.length }
	];
	var body = "See attached.\r\n--=_wm0000\r\n" + long + "\r\n";
	var ra = round_trip(body, files);
	var m = ra.built.content_type.match(/^multipart\/mixed; boundary="([^"]+)"$/);
	Kit.assert(m !== null, "multipart type: " + ra.built.content_type);
	var b = m[1];
	Kit.assert(b.substr(0, 4) == "=_wm", "boundary form");
	Kit.eq(ra.raw.split("--" + b).length - 1, files.length + 2, "the boundary appears only as delimiters");
	lines_ok(ra.raw, 998, "multipart");
	Kit.assert(ra.raw.indexOf("\r\nBcc:") < 0 && ra.raw.indexOf("\r\nX-Evil") < 0, "no injected header line");
	Kit.eq(ra.built.fields, [], "multipart: no CTE field");
	// base64 lines are 76 columns
	var b64_lines = ra.raw.split("\r\n").filter(function (l) {
		return /^[A-Za-z0-9+\/]+=*$/.test(l) && l.length > 60;
	});
	Kit.assert(b64_lines.length > 10, "base64 lines found");
	for (i = 0; i < b64_lines.length; i++)
		Kit.assert(b64_lines[i].length <= 76, "base64 line length");

	var ga = st.get(ra.id);
	Kit.eq(strip_end(base64_decode(ga.text.b64)), strip_end(MimeWrite.text(body)), "text part read back");
	Kit.eq(ga.parts.length, 3, "three attachments: " + JSON.stringify(ga.parts));
	Kit.assert(!/[\r\n"\x00]/.test(ga.parts[0].name), "safe name: " + ga.parts[0].name);
	Kit.assert(ga.parts[0].name.indexOf("Bcc x@evil.example") >= 0, "name kept as text: " + ga.parts[0].name);
	Kit.eq(ga.parts[0].type, "application/octet-stream", "bad type replaced");
	Kit.eq(ga.parts[1].name, "r\xc3\xa9sum\xc3\xa9.pdf", "UTF-8 name read back");
	Kit.eq(ga.parts[1].type, "application/pdf", "type");
	Kit.eq(ga.parts[2].name, "_CON.txt", "device name made safe by the reader");
	var want = [b1, b2, b3];
	for (i = 0; i < 3; i++) {
		var pf = st.part_file(ra.id, ga.parts[i].pid);
		try {
			Kit.eq(md5_calc(read_file(pf.path), true), md5_calc(want[i], true), "attachment " + i + " bytes");
			Kit.eq(pf.size, want[i].length, "attachment " + i + " size");
		} finally {
			if (pf.temp)
				file_remove(pf.path);
		}
	}

	// --- estimate() is an upper bound
	Kit.assert(MimeWrite.estimate(body, files) >= ra.raw.length, "estimate covers the output");

	// --- a missing or short file fails; the output is removed
	var out = S.dir + "fail.txt";
	Kit.throws(function () {
		MimeWrite.build("x", [{ name: "n", type: "text/plain", path: S.dir + "missing", size: 1 }], out);
	}, "failed");
	Kit.assert(!file_exists(out), "output removed on failure");
	Kit.throws(function () {
		MimeWrite.build("x", [{ name: "n", type: "text/plain", path: p3, size: 58 }], out);
	}, "failed");
	Kit.assert(!file_exists(out), "output removed after a size mismatch");
	for (i = 0; i < logged.length; i++)
		Kit.assert(/^mimewrite: /.test(logged[i]), "logged: " + logged[i]);
} finally {
	for (var si = 0; si < stores.length; si++)
		stores[si].close();
	S.cleanup();
}
