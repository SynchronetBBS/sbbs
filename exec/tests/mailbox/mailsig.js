// MailSig: mail signature with fallback to the general signature

load(js.exec_dir + "kit.js");
var MailSig = Kit.lib("mailsig");

function read_bytes(path)
{
	var f = new File(path);
	if (!f.open("rb"))
		return null;
	var s = f.read();
	f.close();
	return s;
}

function write_bytes(path, s)
{
	var f = new File(path);
	Kit.assert(f.open("wb"), "open " + path);
	if (s.length)
		Kit.assert(f.write(s), "write " + path);
	f.close();
}

function repeat(s, n)
{
	var r = "";
	for (var i = 0; i < n; i++)
		r += s;
	return r;
}

var N = 60001;
var sc = Kit.scratch();
try {
	var data = sc.data_dir;
	var gen = data + "user/0001.sig";
	var gen_path = data + format("user/%04u.sig", N);
	var mail_path = data + format("user/%04u.mail.sig", N);

	// neither file
	Kit.eq(MailSig.get(N, data), { text: "", source: "none" });

	// general only: CP437 line drawing (0xC4) and a Ctrl-A color code
	write_bytes(gen_path, "\x01hBest,\r\n\x01n" + "\xC4\xC4 Rob\r\n");
	var g = MailSig.get(N, data);
	Kit.eq(g.source, "general");
	Kit.eq(g.text, "Best,\n\xE2\x94\x80\xE2\x94\x80 Rob\n");

	// general signature that is already UTF-8 is not double-encoded
	var u = "Caf\xC3\xA9 \xE2\x82\xAC\r\nline2\r\n";
	write_bytes(gen_path, u);
	Kit.eq(MailSig.get(N, data), { text: "Caf\xC3\xA9 \xE2\x82\xAC\nline2\n", source: "general" });

	// pure ASCII, LF only and CR only line endings
	write_bytes(gen_path, "a\nb\rc\r\nd");
	Kit.eq(MailSig.get(N, data).text, "a\nb\nc\nd");

	// ANSI sequence removed, high bytes kept
	write_bytes(gen_path, "\x1b[1;31mred\x1b[0m\r\n");
	Kit.eq(MailSig.get(N, data).text, "red\n");

	// set() leaves the general file byte-identical, source becomes mail
	var before = "\x01hGeneral \xC4\r\n";
	write_bytes(gen_path, before);
	MailSig.set(N, "Mail sig \xC3\xA9\nsecond line", data);
	Kit.eq(read_bytes(gen_path), before, "general untouched after set");
	Kit.assert(file_exists(mail_path), "mail sig file exists");
	Kit.eq(read_bytes(mail_path), "Mail sig \xC3\xA9\r\nsecond line", "CRLF in file");
	Kit.eq(MailSig.get(N, data), { text: "Mail sig \xC3\xA9\nsecond line", source: "mail" });

	// CRLF and CR input normalize to LF on read, CRLF in the file
	MailSig.set(N, "a\r\nb\rc\nd", data);
	Kit.eq(read_bytes(mail_path), "a\r\nb\r\nc\r\nd");
	Kit.eq(MailSig.get(N, data).text, "a\nb\nc\nd");
	Kit.eq(read_bytes(gen_path), before);

	// empty string: zero-length file, mail source, no signature
	MailSig.set(N, "", data);
	Kit.assert(file_exists(mail_path));
	Kit.eq(file_size(mail_path), 0);
	Kit.eq(MailSig.get(N, data), { text: "", source: "mail" });
	Kit.eq(read_bytes(gen_path), before);

	// null removes the mail file, general returns
	MailSig.set(N, null, data);
	Kit.assert(!file_exists(mail_path));
	Kit.eq(MailSig.get(N, data).source, "general");
	Kit.eq(read_bytes(gen_path), before);
	MailSig.set(N, null, data);	// deleting nothing is fine

	// no leftover temp files
	var left = directory(data + "user/*");
	Kit.eq(left, [gen_path], "no stray files");
	file_remove(gen_path);
	Kit.eq(MailSig.get(N, data).source, "none");
	MailSig.set(N, null, data);
	Kit.eq(MailSig.get(N, data).source, "none");

	// length limit counts UTF-8 characters, not bytes
	var s2000 = repeat("\xC3\xA9", 2000);	// 4000 bytes
	MailSig.set(N, s2000, data);
	Kit.eq(MailSig.get(N, data).text, s2000);
	Kit.throws(function () { MailSig.set(N, s2000 + "\xC3\xA9", data); }, "invalid");
	Kit.eq(MailSig.get(N, data).text, s2000, "rejected set leaves old signature");
	Kit.throws(function () { MailSig.set(N, repeat("x", 2001), data); }, "invalid");
	MailSig.set(N, repeat("x", 2000), data);
	MailSig.set(N, null, data);

	// invalid UTF-8 cannot dodge the length limit
	MailSig.set(N, "keep", data);
	var badseq = [repeat("\x80", 100000), "ab\xC3", "\xC0\xAF", "\xE0\x80\x80", "\xFF"];
	for (var q = 0; q < badseq.length; q++) {
		(function (v) {
			Kit.throws(function () { MailSig.set(N, v, data); }, "invalid");
		})(badseq[q]);
	}
	Kit.eq(read_bytes(mail_path), "keep", "invalid sets write nothing");
	MailSig.set(N, null, data);

	// CP437 bytes that happen to be valid UTF-8 are taken as UTF-8
	write_bytes(gen_path, "\xC3\xA9\r\n");
	Kit.eq(MailSig.get(N, data), { text: "\xC3\xA9\n", source: "general" });
	file_remove(gen_path);

	// a set whose write cannot succeed leaves the previous signature
	MailSig.set(N, "previous", data);
	var sc2 = Kit.scratch();
	try {
		var blocker = sc2.data_dir + "user";
		rmdir(blocker);
		write_bytes(blocker, "x");	// user/ is a regular file here
		Kit.throws(function () { MailSig.set(N, "new", sc2.data_dir); }, "failed");
		try {
			MailSig.set(N, "new", sc2.data_dir);
		} catch (e) {
			Kit.assert(String(e.message).indexOf(sc2.dir) < 0, "no server path in the message: " + e.message);
		}
	} finally {
		file_remove(sc2.data_dir + "user");
		sc2.cleanup();
	}
	Kit.eq(read_bytes(mail_path), "previous");
	Kit.eq(directory(data + "user/*").length, 1, "no stray temp after failure");
	MailSig.set(N, null, data);

	// char code above 0xFF
	Kit.throws(function () { MailSig.set(N, "x" + String.fromCharCode(0x20AC), data); }, "invalid");
	Kit.throws(function () { MailSig.set(N, 42, data); }, "invalid");
	Kit.throws(function () { MailSig.set(N, undefined, data); }, "invalid");

	// invalid usernum
	var bad = [0, -1, 1.5, "1/../2", "12", NaN, Infinity, null, undefined, {}, [], true, "", 4294967296];
	for (var b = 0; b < bad.length; b++) {
		(function (v) {
			Kit.throws(function () { MailSig.get(v, data); }, "invalid");
			Kit.throws(function () { MailSig.set(v, "x", data); }, "invalid");
			Kit.throws(function () { MailSig.set(v, null, data); }, "invalid");
		})(bad[b]);
	}
	Kit.eq(directory(data + "user/*.sig").length, 0, "nothing written by invalid calls");
	Kit.eq(directory(sc.dir + "*.sig").length, 0);

	// enabled() mirrors MM_EMAILSIG (1<<17) of msg_area.settings
	Kit.eq(MailSig.enabled(), (msg_area.settings & (1 << 17)) != 0);
} finally {
	sc.cleanup();
}
