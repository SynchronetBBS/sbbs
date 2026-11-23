/* pdfunmark.js - remove watermark text from a PDF file.
 *
 *	jsexec pdfunmark.js [-m] input.pdf "watermark text" ["more text" ...] [out=output.pdf]
 *
 * Every piece of text in the book that is part of one of the watermark texts is made invisible (the book's own text and
 * layout are left exactly as they were).  Case and spaces don't matter, and the watermark may be split into any number
 * of pieces in the file.
 *
 * With out=FILE the changed book is written to FILE and the input is not touched.  Without it the input file itself is
 * changed (the changes are added to the end of the file, so the original text of the book is still in there; use out=
 * if you want to keep the original).
 *
 * Run it on the host, not in SixelView.  Use jsexec's -m option for big books, e.g.  jsexec -m536870912 pdfunmark.js ...
 *
 * Only text can be removed; a watermark that is part of a scanned picture can't be.  If the watermark shares one
 * text operator with real words, those words are hidden with it.  Encrypted books are handled if they use RC4 (most do).
 *
 * Tab size is 4.
 */

"use strict";

load("pdf2sixel_lib.js");

function usage()
{
	print("usage: jsexec pdfunmark.js input.pdf \"watermark text\" [\"more text\" ...] [out=output.pdf]");
}

/* ----- finding the text ----- */

function wm_string(font, str)
{
	var out = "", n = str.length, i = 0;
	while (i < n) {
		var code;
		if (font.twobyte) {
			code = ((str.charCodeAt(i) & 255) << 8) | (str.charCodeAt(i + 1) & 255);
			i += 2;
		}
		else
			code = str.charCodeAt(i++) & 255;
		var u = font_unicode(font, code);
		if (typeof u == "string")
			out += u;
	}
	return out.toLowerCase().replace(/\s+/g, "");
}

/* Like parse_content, but each operator also gets the byte range it occupies: [op, args, start, end]. */
function scan_content(doc, bytes)
{
	var lx = new Lexer(bytes, 0), ops = [], args = [], start = -1;
	for (;;) {
		var before = lx.pos;
		var t = lx.token();
		if (t === EOF_TOKEN)
			break;
		if (start < 0)
			start = before;
		var v = parse_value(lx, t, true);
		if (!(v instanceof Cmd)) {
			args.push(v);
			if (args.length > 512)
				args.shift();
			continue;
		}
		if (v.cmd == "BI") {
			for (;;) {	/* inline image: skip to its end */
				var k = lx.token();
				if (k === EOF_TOKEN || ((k instanceof Cmd) && k.cmd == "ID"))
					break;
				if (k instanceof Name)
					parse_value(lx, lx.token(), true);
			}
			var p = lx.pos;
			var e = find_ei(bytes, p);
			lx.pos = Math.min(bytes.length, e + 2);
			ops.push(["BI", [], start, lx.pos]);
			args = [];
			start = -1;
			continue;
		}
		ops.push([v.cmd, args, start, lx.pos]);
		args = [];
		start = -1;
	}
	return ops;
}

/* Returns the new content (Uint8Array) if some of it had to be changed, else null.  Fills "forms" with the form
 * XObjects (PDFStream) found in the resources res. */
function clean_stream(doc, bytes, res, marks, forms)
{
	var ops = scan_content(doc, bytes), font = null, tr = 0, i, k, w;
	var edits = [];		/* { at, text } to insert, { from, to } not used */
	var block = null;
	function flush()
	{
		if (!block || !block.spans.length)
			return;
		var text = block.text;
		for (w = 0; w < marks.length; w++) {
			var at = text.indexOf(marks[w]);
			while (at >= 0) {
				var end = at + marks[w].length;
				for (k = 0; k < block.spans.length; k++)
					if (block.spans[k].from < end && block.spans[k].to > at)
						block.spans[k].hide = true;
				at = text.indexOf(marks[w], end);
			}
		}
		for (k = 0; k < block.spans.length; k++) {
			var sp = block.spans[k];
			if (sp.hide)
				edits.push({ at: sp.start, end: sp.end, tr: sp.tr });
		}
	}
	for (i = 0; i < ops.length; i++) {
		var op = ops[i][0], args = ops[i][1], t = null;
		if (op == "BT") {
			flush();
			block = { text: "", spans: [] };
		}
		else if (op == "ET") {
			flush();
			block = null;
		}
		else if (op == "Tf") {
			var fd = res_get(res, "Font", args[0] instanceof Name ? args[0].name : "");
			try {
				font = fd ? make_font(doc, fd, res) : null;
			}
			catch (e) {
				font = null;
			}
		}
		else if (op == "Tr")
			tr = typeof args[0] == "number" ? args[0] : 0;
		else if (op == "Do") {
			var xo = res_get(res, "XObject", args[0] instanceof Name ? args[0].name : "");
			if (xo instanceof PDFStream && isName(doc.resolve(xo.dict.Subtype), "Form"))
				forms.push(xo);
		}
		else if (block && (op == "Tj" || op == "'" || op == "\"" || op == "TJ") && font) {
			var d = "";
			if (op == "TJ") {
				if (args[0] instanceof Array)
					for (k = 0; k < args[0].length; k++)
						if (typeof args[0][k] == "string")
							d += wm_string(font, args[0][k]);
			}
			else {
				t = op == "\"" ? args[2] : args[0];
				if (typeof t == "string")
					d = wm_string(font, t);
			}
			if (d.length) {
				block.spans.push({ from: block.text.length, to: block.text.length + d.length, start: ops[i][2], end: ops[i][3], tr: tr });
				block.text += d;
			}
		}
	}
	flush();
	if (!edits.length)
		return null;
	/* build the new content: each hidden show operator is wrapped in "3 Tr ... <old mode> Tr", which still moves the text
	 * position but draws nothing */
	var parts = [], pos = 0, total = 0, hdr, ftr;
	edits.sort(function (a, b) { return a.at - b.at; });
	for (i = 0; i < edits.length; i++) {
		var ed = edits[i];
		if (ed.at < pos)
			continue;
		parts.push(bytes_sub(bytes, pos, ed.at));
		parts.push(string_to_bytes(" 3 Tr "));
		parts.push(bytes_sub(bytes, ed.at, ed.end));
		parts.push(string_to_bytes(" " + ed.tr + " Tr "));
		pos = ed.end;
	}
	parts.push(bytes_sub(bytes, pos, bytes.length));
	for (i = 0; i < parts.length; i++)
		total += parts[i].length;
	var out = new Uint8Array(total), o = 0;
	for (i = 0; i < parts.length; i++) {
		out.set(parts[i], o);
		o += parts[i].length;
	}
	return { data: out, count: edits.length };
}

/* ----- writing the changes ----- */

function ser(doc, v)
{
	var i, s;
	if (v === null || v === undefined)
		return "null";
	if (v === true)
		return "true";
	if (v === false)
		return "false";
	if (typeof v == "number")
		return String(v);
	if (typeof v == "string") {
		s = "<";
		for (i = 0; i < v.length; i++)
			s += (v.charCodeAt(i) < 16 ? "0" : "") + (v.charCodeAt(i) & 255).toString(16);
		return s + ">";
	}
	if (v instanceof Name)
		return "/" + v.name.replace(/[^A-Za-z0-9_.+-]/g, function (c) { return "#" + (c.charCodeAt(0) < 16 ? "0" : "") + c.charCodeAt(0).toString(16); });
	if (v instanceof Ref)
		return v.num + " " + v.gen + " R";
	if (v instanceof Array) {
		s = "[";
		for (i = 0; i < v.length; i++)
			s += (i ? " " : "") + ser(doc, v[i]);
		return s + "]";
	}
	if (isDict(v)) {
		s = "<<";
		for (var k in v)
			s += "/" + k + " " + ser(doc, v[k]) + " ";
		return s + ">>";
	}
	return "null";
}

function put_bytes(f, b)
{
	for (var i = 0; i < b.length; i += 65536)
		f.write(bytes_to_string(b, i, Math.min(b.length, i + 65536)));
}

function pad10(n)
{
	var s = String(n);
	while (s.length < 10)
		s = "0" + s;
	return s;
}

function main()
{
	var file = null, outfile = null, marks = [], i;
	for (i = 0; i < argv.length; i++) {
		if (/^out=/.test(argv[i]))
			outfile = argv[i].substr(4);
		else if (!file)
			file = argv[i];
		else {
			var m = argv[i].toLowerCase().replace(/\s+/g, "");
			if (m.length)
				marks.push(m);
		}
	}
	if (!file || !marks.length) {
		usage();
		return 1;
	}
	var doc = load_pdf(file);
	if (doc.rebuilt)
		throw "The file's cross reference table is damaged; repair it first (for example with gs -sDEVICE=pdfwrite)";
	if (doc.crypt && (doc.crypt.stm == "aes" || doc.crypt.stm == "none"))
		throw "This book uses AES encryption, which can't be changed by this program";
	RS = { doc: doc };

	/* every content stream of every page, and every form XObject they use */
	var todo = [], seen = {}, pages = doc.get_pages(), p, j;
	var changed = [];
	function add(st, res)
	{
		if (!(st instanceof PDFStream) || !st.num || seen[st.num])
			return;
		seen[st.num] = true;
		todo.push({ st: st, res: res });
	}
	for (p = 0; p < pages.length; p++) {
		var c = doc.resolve(pages[p].dict.Contents);
		var list = (c instanceof Array) ? c : [c];
		for (j = 0; j < list.length; j++)
			add(doc.resolve(list[j]), pages[p].res);
	}
	var found = 0;
	for (i = 0; i < todo.length; i++) {
		var item = todo[i], forms = [];
		var r = null;
		try {
			r = clean_stream(doc, item.st.bytes(), item.res, marks, forms);
		}
		catch (e) {
			log("skipping object " + item.st.num + ": " + e);
		}
		if (r) {
			changed.push({ st: item.st, data: r.data });
			found += r.count;
		}
		for (j = 0; j < forms.length; j++) {
			var fres = doc.resolve(forms[j].dict.Resources);
			add(forms[j], isDict(fres) ? fres : item.res);
		}
		if (item.st.dict.Subtype === undefined && i % 50 == 49)
			print("  " + (i + 1) + " of " + todo.length + " content streams checked, " + found + " pieces of text found");
	}
	if (!found) {
		print("The watermark text wasn't found as text in " + todo.length + " content streams; nothing changed.");
		print("(If you can see it on the page it may be part of a picture.)");
		return 2;
	}

	/* where the old cross reference starts, and what kind it is */
	var buf = doc.buf, sx = -1, q;
	for (q = buf.length - 9; q >= 0 && q > buf.length - 2048; q--) {
		if (find_bytes(buf, "startxref", q, q + 9) == q) {
			sx = q;
			break;
		}
	}
	if (sx < 0)
		throw "startxref not found";
	var lx = new Lexer(buf, sx + 9);
	var prev = lx.token();
	if (typeof prev != "number")
		throw "bad startxref";
	var xstream = find_bytes(buf, "xref", prev, prev + 4) != prev;

	var out = new File(outfile || file);
	if (outfile) {
		if (!out.open("wb"))
			throw "Unable to create " + outfile;
		put_bytes(out, buf);
	}
	else {
		/* in place: add to the end of the file */
		if (out.length != buf.length)
			throw "This file has extra data before the %PDF header; use out=FILE";
		if (!out.open("ab"))
			throw "Unable to open " + file + " for writing";
	}
	var pos = buf.length, entries = [], maxnum = 0, k2;
	function emit(str)
	{
		out.write(str);
		pos += str.length;
	}
	emit("\n");
	for (var key in doc.xref)
		maxnum = Math.max(maxnum, Number(key));
	for (i = 0; i < changed.length; i++) {
		var st = changed[i].st, data = changed[i].data, gen = st.crypt ? st.crypt.gen : 0;
		if (st.crypt && doc.crypt)
			data = rc4_crypt(doc.object_key(st.num, gen, false), data);
		var nd = {};
		for (var dk in st.dict)
			if (dk != "Filter" && dk != "DecodeParms" && dk != "DP" && dk != "F" && dk != "Length")
				nd[dk] = st.dict[dk];
		nd.Length = data.length;
		entries.push({ num: st.num, gen: gen, off: pos - 0 });
		emit(st.num + " " + gen + " obj\n" + ser(doc, nd) + "\nstream\n");
		put_bytes(out, data);
		pos += data.length;
		emit("\nendstream\nendobj\n");
	}
	entries.sort(function (a, b) { return a.num - b.num; });
	var tr = doc.trailer, size = Math.max(maxnum + 1, Number(doc.resolve(tr.Size)) || 0);
	var xoff = pos;
	if (!xstream) {
		emit("xref\n");
		for (i = 0; i < entries.length; ) {
			var run = 1;
			while (i + run < entries.length && entries[i + run].num == entries[i].num + run)
				run++;
			emit(entries[i].num + " " + run + "\n");
			for (k2 = 0; k2 < run; k2++)
				emit(pad10(entries[i + k2].off) + " " + (entries[i + k2].gen < 10 ? "0000" : "000") + entries[i + k2].gen + " n \n");
			i += run;
		}
		var t = { Size: size, Root: tr.Root, Prev: prev };
		if (tr.Info !== undefined) t.Info = tr.Info;
		if (tr.ID !== undefined) t.ID = tr.ID;
		if (tr.Encrypt !== undefined) t.Encrypt = tr.Encrypt;
		emit("trailer\n" + ser(doc, t) + "\nstartxref\n" + xoff + "\n%%EOF\n");
	}
	else {
		/* a cross reference stream with W [1 4 2], covering the changed objects and itself */
		var xnum = size, rows = "", index = "";
		var all = entries.concat([{ num: xnum, gen: 0, off: xoff }]);
		for (i = 0; i < all.length; i++) {
			var e = all[i];
			rows += String.fromCharCode(1, (e.off >> 24) & 255, (e.off >> 16) & 255, (e.off >> 8) & 255, e.off & 255, (e.gen >> 8) & 255, e.gen & 255);
			index += e.num + " 1 ";
		}
		var xd = { Type: new Name("XRef"), Size: xnum + 1, Root: tr.Root, Prev: prev, W: [1, 4, 2], Index: index.replace(/ $/, "").split(" ").map(Number), Length: rows.length };
		if (tr.Info !== undefined) xd.Info = tr.Info;
		if (tr.ID !== undefined) xd.ID = tr.ID;
		if (tr.Encrypt !== undefined) xd.Encrypt = tr.Encrypt;
		emit(xnum + " 0 obj\n" + ser(doc, xd) + "\nstream\n");
		emit(rows);
		emit("\nendstream\nendobj\nstartxref\n" + xoff + "\n%%EOF\n");
	}
	out.close();
	print(found + " pieces of text hidden, in " + changed.length + " content streams (of " + todo.length + ")." );
	print((outfile ? "Written to " + outfile : "Changed " + file));
	return 0;
}

try {
	exit(main());
}
catch (e) {
	print("Error: " + e);
	exit(1);
}
