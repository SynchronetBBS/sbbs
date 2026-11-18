// MsgBase.get_msg_parts(number) lists the leaf MIME parts of a message and
// MsgBase.save_msg_part(number, part_id, path) writes one decoded part (or,
// for part_id "0", the whole undecoded body) to a file natively.

load("smbdefs.js");   /* for NET_FIDO */
mkpath(system.temp_dir);

var stamp = Date.now();
var base_path = system.temp_dir + "test_msgbase_parts_" + stamp;
var body_path = system.temp_dir + "test_msgbase_parts_body_" + stamp + ".txt";
var out_path = system.temp_dir + "test_msgbase_parts_out_" + stamp + ".bin";
var EXTENSIONS = [".shd", ".sdt", ".sid", ".sha", ".sda", ".ini", ".hash"];

function cleanup() {
	EXTENSIONS.forEach(function (ext) {
		try { file_remove(base_path + ext); } catch (e) { /* ignore */ }
	});
	[body_path, out_path].forEach(function (name) {
		try { file_remove(name); } catch (e) { /* ignore */ }
	});
}

function write_file(name, data) {
	var f = new File(name);
	if (!f.open("wb"))
		throw new Error("cannot create " + name);
	f.write(data);
	f.close();
}

function file_md5(name) {
	var f = new File(name);
	if (!f.open("rb"))
		throw new Error("cannot open " + name);
	var len = f.length;
	var md5 = f.md5_hex;
	f.close();
	return { length: len, md5: md5 };
}

function wrap76(str) {
	var out = "";
	for (var i = 0; i < str.length; i += 76)
		out += str.substr(i, 76) + "\r\n";
	return out;
}

var mb;

/* Save a message whose body is 'text' (read from a file) and return its number */
function save(hdr, text) {
	write_file(body_path, text);
	var h = { to: "Alice", to_ext: "60001", from: "Bob", from_ext: "60002", subject: "parts" };
	for (var k in hdr)
		h[k] = hdr[k];
	if (mb.save_msg_from_file(h, body_path) !== true)
		throw new Error("save_msg_from_file: " + mb.last_error);
	return mb.last_msg;
}

function mime(boundary) {
	return { mime_version: "1.0", content_type: 'multipart/mixed; boundary="' + boundary + '"' };
}

/* Write part 'id' of message 'num' and check its bytes against 'want' */
function check_saved(label, num, id, want) {
	if (mb.save_msg_part(num, id, out_path) !== true)
		throw new Error(label + ": save_msg_part(" + id + "): " + mb.last_error);
	var got = file_md5(out_path);
	if (got.length != want.length || got.md5 != md5_calc(want, /* hex: */ true))
		throw new Error(label + ": part " + id + " saved " + got.length + " bytes, expected " + want.length);
}

function check_part(label, part, want) {
	for (var k in want) {
		if (part[k] !== want[k])
			throw new Error(label + ": part " + part.id + " " + k + " is " + JSON.stringify(part[k])
				+ ", expected " + JSON.stringify(want[k]));
	}
}

/* Every part of a (possibly malformed) message is listed and saved */
function check_robust(label, num) {
	var parts = mb.get_msg_parts(num);
	if (!parts || parts.length < 1)
		throw new Error(label + ": get_msg_parts returned " + JSON.stringify(parts) + " (" + mb.last_error + ")");
	parts.forEach(function (part) {
		if (mb.save_msg_part(num, part.id, out_path) !== true)
			throw new Error(label + ": save_msg_part(" + part.id + "): " + mb.last_error);
		if (file_md5(out_path).length != part.size)
			throw new Error(label + ": part " + part.id + " size " + part.size + " differs from the saved file");
	});
	return parts;
}

try {
	mb = new MsgBase(base_path, /* is_path: */ true, /* is_email: */ true);
	if (!mb.open())
		throw new Error("open: " + mb.last_error);

	/* (a) multipart/alternative inside multipart/mixed, a base64 PNG and a
	 * quoted-printable text attachment */
	var png = "\x89PNG\r\n\x1a\n\x00\x00\x00\x0dIHDR";
	for (var r = 0; r < 3; r++)
		for (var b = 0; b < 256; b++)
			png += String.fromCharCode(b);
	png += "IEND\xaeB`\x82";
	var qp_text = "Caf=E9 =3D 5\r\nsoft=\r\nbreak\r\ntrailing  \r\nend=0D=0Abytes=00=FF=";
	var notes = "Caf\xe9 = 5\r\nsoftbreak\r\ntrailing\r\nend\r\nbytes\x00\xff";
	var html = "<p>Hello \xe9</p>";
	var body = "This is a multi-part message in MIME format.\r\n"
		+ "--outer\r\n"
		+ "Content-Type: multipart/alternative; boundary=\"inner\"\r\n"
		+ "\r\n"
		+ "--inner\r\n"
		+ "Content-Type: text/plain; charset=UTF-8\r\n"
		+ "Content-Transfer-Encoding: 7bit\r\n"
		+ "\r\n"
		+ "Hello plain\r\n"
		+ "--inner\r\n"
		+ "content-type: TEXT/HTML; charset=\"ISO-8859-1\"\r\n"
		+ "Content-Transfer-Encoding: quoted-printable\r\n"
		+ "\r\n"
		+ "<p>Hello =E9</p>\r\n"
		+ "--inner--\r\n"
		+ "\r\n"
		+ "--outer\r\n"
		+ "Content-Type: image/png; name=\"pic.png\"\r\n"
		+ "Content-Transfer-Encoding: base64\r\n"
		+ "Content-Disposition: inline; filename=\"dot.png\"\r\n"
		+ "Content-ID: <img1@example>\r\n"
		+ "\r\n"
		+ wrap76(base64_encode(png))
		+ "--outer\r\n"
		+ "Content-Type: text/plain; charset=windows-1252;\r\n"
		+ "\tname=notes.txt\r\n"
		+ "Content-Transfer-Encoding: Quoted-Printable\r\n"
		+ "Content-Disposition: ATTACHMENT;\r\n"
		+ " filename=\"C:\\\\temp\\\\notes.txt\"\r\n"
		+ "\r\n"
		+ qp_text + "\r\n"
		+ "--outer--";
	var num = save(mime("outer"), body);
	var parts = mb.get_msg_parts(num);
	if (!parts || parts.length != 4)
		throw new Error("(a) expected 4 parts, got " + JSON.stringify(parts) + " (" + mb.last_error + ")");
	check_part("(a)", parts[0], { id: "1.1", content_type: "text/plain", charset: "utf-8", name: "",
		content_id: "", disposition: "", size: 11 });
	check_part("(a)", parts[1], { id: "1.2", content_type: "text/html", charset: "iso-8859-1",
		size: html.length });
	check_part("(a)", parts[2], { id: "2", content_type: "image/png", charset: "", name: "dot.png",
		content_id: "img1@example", disposition: "inline", size: png.length });
	check_part("(a)", parts[3], { id: "3", content_type: "text/plain", charset: "windows-1252",
		name: "notes.txt", disposition: "attachment", size: notes.length });
	check_saved("(a)", num, "1.1", "Hello plain");
	check_saved("(a)", num, "1.2", html);
	check_saved("(a)", num, "2", png);
	check_saved("(a)", num, "3", notes);
	/* the message store terminates the body with one CRLF */
	check_saved("(a)", num, "0", body + "\r\n");

	/* a part that does not exist, a container and bad message numbers */
	["4", "1", "1.3", "", "x", new Array(50).join("1.") + "1"].forEach(function (id) {
		if (mb.save_msg_part(num, id, out_path) !== false)
			throw new Error("save_msg_part of part \"" + id + "\" did not return false");
		if (!mb.last_error)
			throw new Error("save_msg_part of part \"" + id + "\": no last_error");
	});
	[0, "1", num + 100].forEach(function (n) {
		if (mb.get_msg_parts(n) !== null)
			throw new Error("get_msg_parts(" + JSON.stringify(n) + ") did not return null");
		if (n !== num + 100 && !/number/.test(mb.last_error))
			throw new Error("get_msg_parts(" + JSON.stringify(n) + ") last_error: " + mb.last_error);
		if (mb.save_msg_part(n, "1", out_path) !== false)
			throw new Error("save_msg_part(" + JSON.stringify(n) + ") did not return false");
		if (!mb.last_error)
			throw new Error("save_msg_part(" + JSON.stringify(n) + "): no last_error");
	});

	/* a single-part MIME message */
	num = save({ mime_version: "1.0", content_type: "application/pdf; name=\"doc.pdf\"",
		field_list: [ { type: "OtherHeader", data: "Content-Transfer-Encoding: base64" } ] },
		wrap76(base64_encode(png)));
	parts = mb.get_msg_parts(num);
	if (!parts || parts.length != 1)
		throw new Error("single-part: " + JSON.stringify(parts) + " (" + mb.last_error + ")");
	check_part("single-part", parts[0], { id: "1", content_type: "application/pdf", name: "doc.pdf",
		size: png.length });
	check_saved("single-part", num, "1", png);

	/* (b) non-MIME messages: the charset follows the message's origin */
	var plain = "Plain text body.\r\nSecond line.";
	var cases = [
		{ label: "local", hdr: {}, charset: "cp437" },
		{ label: "internet", hdr: { from_net_addr: "bob@example.com" }, charset: "windows-1252" },
		{ label: "utf-8", hdr: { from_net_addr: "bob@example.com", ftn_charset: "UTF-8 4" },
			charset: "utf-8" },
		{ label: "fidonet", hdr: { from_net_type: NET_FIDO, from_net_addr: "1:103/705" },
			charset: "cp437" },
		{ label: "ftn charset", hdr: { from_net_type: NET_FIDO, from_net_addr: "1:103/705",
			ftn_charset: "LATIN-1 2" }, charset: "latin-1" }
	];
	cases.forEach(function (c) {
		var n = save(c.hdr, plain);
		var p = mb.get_msg_parts(n);
		if (!p || p.length != 1)
			throw new Error("(b) " + c.label + ": " + JSON.stringify(p) + " (" + mb.last_error + ")");
		check_part("(b) " + c.label, p[0], { id: "1", content_type: "text/plain", charset: c.charset,
			name: "", content_id: "", disposition: "", size: plain.length + 2 });
		check_saved("(b) " + c.label, n, "1", plain + "\r\n");
	});

	/* (c) malformed MIME: each yields an array of parts, no crash or hang */
	parts = check_robust("missing closing boundary", save(mime("b"),
		"--b\r\nContent-Type: text/plain\r\n\r\nhello\r\n--b\r\nContent-Type: text/html\r\n\r\n<b>unterminated"));
	if (parts.length != 2 || parts[1].content_type != "text/html")
		throw new Error("missing closing boundary: " + JSON.stringify(parts));

	/* 12 levels of multipart: the innermost text part is reached */
	/* (boundary "n1" is a prefix of "n12": only a whole boundary delimits) */
	var text = "--n12\r\nContent-Type: text/plain\r\n\r\ndeep\r\n--n12--";
	for (var i = 11; i >= 1; i--)
		text = "--n" + i + "\r\nContent-Type: multipart/mixed; boundary=n" + (i + 1)
			+ "\r\n\r\n" + text + "\r\n--n" + i + "--";
	parts = check_robust("12 nested", save(mime("n1"), text));
	if (parts.length != 1 || parts[0].id != "1.1.1.1.1.1.1.1.1.1.1.1" || parts[0].size != 4)
		throw new Error("12 nested: " + JSON.stringify(parts));

	/* Multiparts nested 'levels' deep (boundaries m1..mN), the innermost
	 * holding one text/html part */
	function nest(levels) {
		var t = "--m" + levels + "\r\nContent-Type: text/html\r\n\r\ndeep\r\n--m" + levels + "--";
		for (var lvl = levels - 1; lvl >= 1; lvl--)
			t = "--m" + lvl + "\r\nContent-Type: multipart/mixed; boundary=m" + (lvl + 1)
				+ "\r\n\r\n" + t + "\r\n--m" + lvl + "--";
		return save(mime("m1"), t);
	}
	/* 16 levels are walked; a 17th (or deeper) level is one opaque part */
	var id16 = new Array(17).join("1.").slice(0, -1);
	[ { levels: 16, type: "text/html", size: 4 },
		{ levels: 17, type: "application/octet-stream" },
		{ levels: 20, type: "application/octet-stream" } ].forEach(function (c) {
		var p = check_robust(c.levels + " nested", nest(c.levels));
		if (p.length != 1 || p[0].content_type != c.type || p[0].id != id16
			|| (c.size !== undefined && p[0].size != c.size))
			throw new Error(c.levels + " nested: " + JSON.stringify(p));
	});

	/* a nested multipart without a matching delimiter is one opaque part */
	num = save(mime("b"), "--b\r\nContent-Type: text/plain\r\n\r\nfirst\r\n"
		+ "--b\r\nContent-Type: multipart/alternative; boundary=zz\r\n\r\nHIDDEN TEXT\r\n--b--");
	parts = check_robust("unmatched nested boundary", num);
	if (parts.length != 2 || parts[1].id != "2" || parts[1].content_type != "application/octet-stream")
		throw new Error("unmatched nested boundary: " + JSON.stringify(parts));
	check_saved("unmatched nested boundary", num, "2", "HIDDEN TEXT");

	/* as is one whose boundary is longer than 255 characters */
	var long_b = new Array(301).join("x");
	parts = check_robust("long nested boundary", save(mime("b"), "--b\r\nContent-Type: text/plain\r\n\r\nfirst\r\n"
		+ "--b\r\nContent-Type: multipart/mixed; boundary=\"" + long_b + "\"\r\n\r\n"
		+ "--" + long_b + "\r\nContent-Type: text/plain\r\n\r\ninner\r\n--" + long_b + "--\r\n--b--"));
	if (parts.length != 2 || parts[1].content_type != "application/octet-stream")
		throw new Error("long nested boundary: " + JSON.stringify(parts));

	/* deep and wide: thousands of empty multiparts before each nested level
	 * (which would give part ids too long for their field); every listed id
	 * is unique */
	var chunks = [];
	var lvl;
	for (lvl = 1; lvl <= 16; lvl++) {
		var empties = (lvl == 1) ? 10000 : 1200;
		for (var e = 0; e < empties; e++)
			chunks.push("--w" + lvl + "\r\nContent-Type: multipart/mixed; boundary=z\r\n");
		if (lvl < 16)
			chunks.push("--w" + lvl + "\r\nContent-Type: multipart/mixed; boundary=w" + (lvl + 1) + "\r\n\r\n");
	}
	chunks.push("--w16\r\n\r\nleaf one\r\n--w16\r\n\r\nleaf two\r\n--w16--");
	parts = mb.get_msg_parts(save(mime("w1"), chunks.join("")));
	if (!parts || parts.length < 1 || parts.length > 1000)
		throw new Error("deep and wide: " + (parts ? parts.length : parts) + " parts (" + mb.last_error + ")");
	var seen = {};
	parts.forEach(function (part) {
		if (seen[part.id])
			throw new Error("deep and wide: duplicate part id " + part.id);
		seen[part.id] = true;
	});

	parts = check_robust("no headers", save(mime("b"), "--b\r\n\r\nno headers\r\n--b--"));
	check_part("no headers", parts[0], { id: "1", content_type: "text/plain", charset: "", size: 10 });

	check_robust("no blank line", save(mime("b"), "--b\r\nContent-Type: text/plain\r\n--b--"));
	check_robust("boundary absent", save(mime("zzz"), "no delimiter here at all"));
	check_robust("no boundary parameter", save({ mime_version: "1.0", content_type: "multipart/mixed" },
		"--b\r\n\r\nx\r\n--b--"));
	check_robust("bad base64", save(mime("b"),
		"--b\r\nContent-Transfer-Encoding: base64\r\n\r\n!!*\xff\xfe=QUJD====\r\n--b--"));
	check_robust("bad quoted-printable", save(mime("b"),
		"--b\r\nContent-Transfer-Encoding: quoted-printable\r\n\r\n=\r\n==G1=4=\r\n--b--"));
	check_robust("unterminated parameters", save(mime("b"),
		"--b\r\nContent-Type: text/plain; charset=\"utf-8; name=\"\r\nContent-ID: <\r\n\r\nx\r\n--b--"));
	check_robust("delimiter at end", save(mime("b"), "--b"));

	mb.close();
} finally {
	cleanup();
}
