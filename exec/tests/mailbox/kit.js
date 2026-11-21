// Shared helpers for the mailbox library tests (exec/tests/mailbox/*.js).
// Run on its own (as the test runner does) this file only defines Kit.
// A test loads it with:  load(js.exec_dir + "kit.js");
// js.exec_dir is the directory of the running test, which is
// <exec>/tests/mailbox/ both under the runner and when run directly.

"use strict";

var Kit = (function () {
	var here = js.exec_dir;
	var seq = 0;
	var EXTENSIONS = [".shd", ".sdt", ".sid", ".sha", ".sda", ".ini", ".hash"];

	function slash(p) {
		var c = p.charAt(p.length - 1);
		return (c == '/' || c == '\\') ? p : p + '/';
	}

	// This worktree's exec/ directory, derived from the test location
	// (<exec>/tests/mailbox/)
	function exec_root() {
		var d = slash(here);
		d = d.substr(0, d.length - 1);	// strip the trailing separator
		d = d.substr(0, Math.max(d.lastIndexOf('/'), d.lastIndexOf('\\')) + 1);	// up from mailbox
		d = d.substr(0, d.length - 1);
		d = d.substr(0, Math.max(d.lastIndexOf('/'), d.lastIndexOf('\\')) + 1);	// up from tests
		return d;
	}

	var Kit = {};

	Kit.exec_root = exec_root;

	// Load exec/load/<name>.js from this worktree, return its namespace
	Kit.lib = function (name) {
		var path = exec_root() + "load/" + name + ".js";
		if (!file_exists(path))
			throw new Error("Kit.lib: no such library: " + path);
		return load({}, path);
	};

	Kit.assert = function (cond, msg) {
		if (!cond)
			throw new Error("assertion failed: " + (msg || ""));
	};

	Kit.eq = function (actual, expected, msg) {
		var a = JSON.stringify(actual);
		var e = JSON.stringify(expected);
		if (a !== e)
			throw new Error((msg ? msg + ": " : "") + "expected " + e + " got " + a);
	};

	// Run fn, expecting it to throw an object (or Error) whose .code
	// is 'code' (any throw if code is omitted)
	Kit.throws = function (fn, code) {
		var thrown = false;
		try {
			fn();
		} catch (e) {
			thrown = true;
			if (code !== undefined && (e === null || e === undefined || e.code !== code))
				throw new Error("expected throw with code " + code + ", got " + JSON.stringify(e)
					+ (e && e.message ? " " + e.message : ""));
		}
		if (!thrown)
			throw new Error("expected a throw" + (code !== undefined ? " with code " + code : ""));
	};

	// Open (creating) an e-mail-type base at path (no extension)
	// How this file system dates a file that is written: { open, closed }.
	// open: a write shows in the file's date while the file is still open;
	// closed: it shows once the file is closed.  Some (network) file
	// systems give the new date only after the close.  Takes up to a
	// second; the answer is kept.
	var dates = null;
	Kit.file_dates = function () {
		if (dates !== null)
			return dates;
		mkpath(system.temp_dir);
		var path = system.temp_dir + format("mailbox_test_dates_%u_%u", time(), random(1000000));
		var f = new File(path);
		if (!f.open("w+"))
			throw new Error("Kit.file_dates: cannot create " + path);
		f.write("a");
		f.flush();
		var first = file_date(path);
		while (time() <= first)
			mswait(20);
		f.write("b");
		f.flush();
		var open = file_date(path);
		f.close();
		var closed = file_date(path);
		file_remove(path);
		dates = { open: open > first, closed: closed > first };
		return dates;
	};

	Kit.email_base = function (path) {
		var mb = new MsgBase(path, /* is_path: */ true, /* is_email: */ true);
		if (!mb.open())
			throw new Error("Kit.email_base open " + path + ": " + mb.last_error);
		return mb;
	};

	function rmtree(dir) {
		var list = directory(dir + "*");
		for (var i = 0; i < list.length; i++) {
			var p = list[i];
			var c = p.charAt(p.length - 1);
			if (c == '/' || c == '\\') {
				if (p.substr(-3) == "/./" || p.substr(-4) == "/../")
					continue;
				rmtree(p);
				rmdir(p);
			} else
				file_remove(p);
		}
		rmdir(dir);
	}

	// A scratch tree under system.temp_dir:
	//   dir/data/{user,file}/   data_dir stand-in
	//   shared_path             e-mail-type base path (not yet created)
	Kit.scratch = function () {
		var root = slash(system.temp_dir) + "mailbox_test_" + system.timer.toString().replace('.', '') + "_" + random(0x7fffffff) + "_" + (seq++) + "/";
		mkpath(root);
		var data = root + "data/";
		mkpath(data + "user");
		mkpath(data + "file");
		var shared = root + "shared_mail";
		return {
			dir: root,
			data_dir: data,
			shared_path: shared,
			user_dir: function (usernum) {
				var n = String(usernum);
				while (n.length < 4)
					n = '0' + n;
				var d = data + "user/" + n + "/";
				mkpath(d);
				return d;
			},
			cleanup: function () {
				for (var i = 0; i < EXTENSIONS.length; i++)
					file_remove(shared + EXTENSIONS[i]);
				rmtree(root);
			}
		};
	};

	// MIME fixture messages (from exec/tests/msgbase/parts.js), each
	// {hdr, body}: save with mb.save_msg(hdr-with-to/from, body)
	function wrap76(str) {
		var out = "";
		for (var i = 0; i < str.length; i += 76)
			out += str.substr(i, 76) + "\r\n";
		return out;
	}

	function mime_hdr(boundary) {
		return { mime_version: "1.0", content_type: 'multipart/mixed; boundary="' + boundary + '"' };
	}

	Kit.mime_fixtures = {
		// multipart/alternative (text + html) inside multipart/mixed, a
		// base64 PNG (inline, with a content id) and a quoted-printable
		// text attachment. Also returns the decoded contents.
		mixed: function () {
			var png = "\x89PNG\r\n\x1a\n\x00\x00\x00\x0dIHDR";
			for (var r = 0; r < 3; r++)
				for (var b = 0; b < 256; b++)
					png += String.fromCharCode(b);
			png += "IEND\xaeB`\x82";
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
				+ "Caf=E9 =3D 5\r\nsoft=\r\nbreak\r\ntrailing  \r\nend=0D=0Abytes=00=FF=\r\n"
				+ "--outer--";
			return { hdr: mime_hdr("outer"), body: body, plain: "Hello plain",
				html: "<p>Hello \xe9</p>", png: png,
				notes: "Caf\xe9 = 5\r\nsoftbreak\r\ntrailing\r\nend\r\nbytes\x00\xff" };
		},
		// A text/plain part of 'bytes' bytes (lines of 'x') and an HTML part
		big_text: function (bytes) {
			var line = new Array(79).join("x") + "\r\n";
			var chunks = [];
			for (var n = 0; n < bytes; n += line.length)
				chunks.push(line);
			var text = chunks.join("").substr(0, bytes);
			var body = "--big\r\nContent-Type: text/plain; charset=us-ascii\r\n\r\n" + text
				+ "\r\n--big\r\nContent-Type: text/html\r\n\r\n<p>small</p>\r\n--big--";
			return { hdr: mime_hdr("big"), body: body, size: text.length };
		},
		// Malformed messages: a missing closing boundary, 12
		// nested multiparts, a part with no headers
		malformed: function () {
			var text = "--n12\r\nContent-Type: text/plain\r\n\r\ndeep\r\n--n12--";
			for (var i = 11; i >= 1; i--)
				text = "--n" + i + "\r\nContent-Type: multipart/mixed; boundary=n" + (i + 1)
					+ "\r\n\r\n" + text + "\r\n--n" + i + "--";
			return [
				{ label: "missing closing boundary", hdr: mime_hdr("b"),
					body: "--b\r\nContent-Type: text/plain\r\n\r\nhello\r\n--b\r\n"
						+ "Content-Type: text/html\r\n\r\n<b>unterminated" },
				{ label: "12 nested", hdr: mime_hdr("n1"), body: text },
				{ label: "no headers", hdr: mime_hdr("b"), body: "--b\r\n\r\nno headers\r\n--b--" }
			];
		}
	};

	return Kit;
})();
