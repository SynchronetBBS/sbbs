/* univiewer_syntax_lib.js - formatting and syntax highlighting for the text reader
 *
 * Pure JavaScript, no external libraries.
 *
 *   kind_of(ext)          "ini", "json", "xml" or "" for a file extension
 *   format(kind, text)    JSON and XML are re-indented; returns null if the text is not
 *                         valid (the caller then shows the file exactly as it is).
 *                         INI text is returned unchanged.
 *   view(kind, text)     both of the above, and Markdown rendering, in one call: { text, kinds }, or
 *                         null to show the file as it is
 *   highlight(kind, text) a string with one character per character of text giving its
 *                         kind: p plain, c comment, k key / XML tag, t section / XML
 *                         attribute name, s string or value, n number / literal / entity.
 *                         Returns null if the text is not valid JSON or XML.
 *
 * Text must use \n line endings.  A file longer than its limit (limit_for) is left alone (re-indenting can
 * make the text several times longer, so highlight() accepts three times as much).
 */
"use strict";
(function () {
	/* Files larger than this are shown as they are.  JSON takes much more memory and time to lay out (a 4 MB file
	 * needs about 400 MB and 13 seconds) than the others (a 2.8 MB XML file needs 190 MB and 2.5 seconds). */
	var MAX_CHARS = 4194304;
	var LIMITS = { json: 1048576, md: 1048576 };
	function limit_for(kind)
	{
		return LIMITS[kind] || MAX_CHARS;
	}
	var MAX_DEPTH = 100;		/* deeper than this is treated as not valid */
	var NAME = "[A-Za-z_:][A-Za-z0-9_:.\\-]*";
	var TAG_RE = new RegExp("^<(" + NAME + ")((?:\\s+" + NAME + "\\s*=\\s*(?:\"[^\"<]*\"|'[^'<]*'))*)\\s*(\\/?)>$");
	var ATTR_RE = new RegExp("\\s+(" + NAME + ")\\s*=\\s*(\"[^\"<]*\"|'[^'<]*')", "g");
	var CLOSE_RE = new RegExp("^<\\/(" + NAME + ")\\s*>$");

	function kind_of(ext)
	{
		ext = String(ext || "").toLowerCase();
		if (ext == "ini" || ext == "cfg" || ext == "conf" || ext == "inf")
			return "ini";
		if (ext == "json")
			return "json";
		if (/^(xml|xsd|xsl|xslt|rss|atom|plist)$/.test(ext))
			return "xml";
		if (ext == "yaml" || ext == "yml")
			return "yaml";
		if (ext == "toml")
			return "toml";
		if (ext == "md" || ext == "markdown")
			return "md";
		return "";
	}

	function repeat(ch, count)
	{
		return count > 0 ? new Array(count + 1).join(ch) : "";
	}

	function pad(level)
	{
		return repeat(" ", level * 2);
	}

	function bad()
	{
		throw new Error("not valid");
	}

	/* ---------- JSON ---------- */

	/* Parse into a tree that keeps every literal exactly as written, so numbers, escapes and the
	 * order of keys come out unchanged.  Throws if the text is not valid JSON. */
	function parse_json(text)
	{
		var pos = 0;
		var n = text.length;
		var num_re = /-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?/g;

		function ws()
		{
			var c;
			while (pos < n) {
				c = text.charCodeAt(pos);
				if (c == 32 || c == 9 || c == 10 || c == 13)
					pos++;
				else
					break;
			}
		}

		function string()
		{
			var start = pos;
			var c, e;
			pos++;
			for (;;) {
				if (pos >= n)
					bad();
				c = text.charAt(pos);
				if (c == "\"") {
					pos++;
					break;
				}
				if (c == "\\") {
					pos++;
					e = text.charAt(pos);
					if (e !== "" && "\"\\/bfnrt".indexOf(e) >= 0)
						pos++;
					else if (e == "u" && /^[0-9a-fA-F]{4}$/.test(text.substr(pos + 1, 4)))
						pos += 5;
					else
						bad();
				}
				else if (text.charCodeAt(pos) < 32)
					bad();
				else
					pos++;
			}
			return text.substring(start, pos);
		}

		function value(depth)
		{
			var c, m, node, key, lit;
			if (depth > MAX_DEPTH)
				bad();
			ws();
			c = text.charAt(pos);
			if (c == "{") {
				node = { t: "o", kv: [] };
				pos++;
				ws();
				if (text.charAt(pos) == "}") {
					pos++;
					return node;
				}
				for (;;) {
					ws();
					if (text.charAt(pos) != "\"")
						bad();
					key = string();
					ws();
					if (text.charAt(pos) != ":")
						bad();
					pos++;
					node.kv.push([key, value(depth + 1)]);
					ws();
					c = text.charAt(pos++);
					if (c == "}")
						return node;
					if (c != ",")
						bad();
				}
			}
			if (c == "[") {
				node = { t: "a", items: [] };
				pos++;
				ws();
				if (text.charAt(pos) == "]") {
					pos++;
					return node;
				}
				for (;;) {
					node.items.push(value(depth + 1));
					ws();
					c = text.charAt(pos++);
					if (c == "]")
						return node;
					if (c != ",")
						bad();
				}
			}
			if (c == "\"")
				return { t: "v", raw: string() };
			lit = text.substr(pos, 5);
			if (lit == "false") {
				pos += 5;
				return { t: "v", raw: lit };
			}
			lit = text.substr(pos, 4);
			if (lit == "true" || lit == "null") {
				pos += 4;
				return { t: "v", raw: lit };
			}
			num_re.lastIndex = pos;
			m = num_re.exec(text);
			if (!m || m.index != pos)
				bad();
			pos += m[0].length;
			return { t: "v", raw: m[0] };
		}

		var root = value(0);
		ws();
		if (pos != n)
			bad();
		return root;
	}

	function json_inline(node)
	{
		/* an array of plain values that fits on a short line stays on one line */
		var parts = [];
		var total = 2;
		for (var i = 0; i < node.items.length; i++) {
			if (node.items[i].t != "v")
				return null;
			parts.push(node.items[i].raw);
			total += node.items[i].raw.length + 2;
			if (total > 60)
				return null;
		}
		return "[" + parts.join(", ") + "]";
	}

	function print_json(node, level, out)
	{
		var i, inl;
		if (node.t == "v") {
			out.push(node.raw);
		}
		else if (node.t == "o") {
			if (!node.kv.length) {
				out.push("{}");
				return;
			}
			out.push("{\n");
			for (i = 0; i < node.kv.length; i++) {
				out.push(pad(level + 1), node.kv[i][0], ": ");
				print_json(node.kv[i][1], level + 1, out);
				out.push(i < node.kv.length - 1 ? ",\n" : "\n");
			}
			out.push(pad(level), "}");
		}
		else {
			if (!node.items.length) {
				out.push("[]");
				return;
			}
			inl = json_inline(node);
			if (inl !== null) {
				out.push(inl);
				return;
			}
			out.push("[\n");
			for (i = 0; i < node.items.length; i++) {
				out.push(pad(level + 1));
				print_json(node.items[i], level + 1, out);
				out.push(i < node.items.length - 1 ? ",\n" : "\n");
			}
			out.push(pad(level), "]");
		}
	}

	function format_json(text)
	{
		var out = [];
		print_json(parse_json(text), 0, out);
		return out.join("") + "\n";
	}

	function highlight_json(text)
	{
		var n = text.length;
		var out = [];
		var i = 0;
		var j, k, c, cls;
		while (i < n) {
			c = text.charAt(i);
			if (c == "\"") {
				j = i + 1;
				while (j < n && text.charAt(j) != "\"") {
					if (text.charAt(j) == "\\")
						j++;
					j++;
				}
				j++;
				k = j;
				while (k < n && /[ \t\r\n]/.test(text.charAt(k)))
					k++;
				cls = text.charAt(k) == ":" ? "k" : "s";
				out.push(repeat(cls, j - i));
				i = j;
			}
			else if (/[-0-9tfn]/.test(c)) {
				j = i + 1;
				while (j < n && /[-+.A-Za-z0-9]/.test(text.charAt(j)))
					j++;
				out.push(repeat("n", j - i));
				i = j;
			}
			else {
				out.push("p");
				i++;
			}
		}
		return out.join("");
	}

	/* ---------- XML ---------- */

	/* Split the text into tokens: {k: kind, s: start, e: end (exclusive)}.  Throws if the markup
	 * is not well formed enough to trust (unbalanced tags, unquoted attributes, ...). */
	function xml_tokens(text)
	{
		var n = text.length;
		var toks = [];
		var stack = [];
		var i = 0;
		var roots = 0;
		var j, q, ch, m, depth, kind, name;

		while (i < n) {
			if (text.charAt(i) != "<") {
				j = text.indexOf("<", i);
				if (j < 0)
					j = n;
				if (!stack.length && /[^ \t\r\n]/.test(text.substring(i, j)))
					bad();		/* text outside the root element */
				toks.push({ k: "text", s: i, e: j });
				i = j;
				continue;
			}
			if (text.substr(i, 4) == "<!--") {
				j = text.indexOf("-->", i + 4);
				if (j < 0)
					bad();
				toks.push({ k: "comment", s: i, e: j + 3 });
				i = j + 3;
			}
			else if (text.substr(i, 9) == "<![CDATA[") {
				j = text.indexOf("]]>", i + 9);
				if (j < 0 || !stack.length)
					bad();
				toks.push({ k: "cdata", s: i, e: j + 3 });
				i = j + 3;
			}
			else if (text.substr(i, 2) == "<?") {
				j = text.indexOf("?>", i + 2);
				if (j < 0)
					bad();
				toks.push({ k: "pi", s: i, e: j + 2 });
				i = j + 2;
			}
			else if (text.substr(i, 2) == "<!") {
				/* <!DOCTYPE ...>, which may hold an [internal subset] and quoted strings */
				j = i + 2;
				depth = 0;
				q = "";
				for (; j < n; j++) {
					ch = text.charAt(j);
					if (q) {
						if (ch == q)
							q = "";
					}
					else if (ch == "\"" || ch == "'")
						q = ch;
					else if (ch == "[")
						depth++;
					else if (ch == "]")
						depth--;
					else if (ch == ">" && depth <= 0)
						break;
				}
				if (j >= n || stack.length)
					bad();
				toks.push({ k: "doctype", s: i, e: j + 1 });
				i = j + 1;
			}
			else {
				j = i + 1;
				q = "";
				for (; j < n; j++) {
					ch = text.charAt(j);
					if (q) {
						if (ch == q)
							q = "";
					}
					else if (ch == "\"" || ch == "'")
						q = ch;
					else if (ch == ">")
						break;
				}
				if (j >= n)
					bad();
				j++;
				if (text.charAt(i + 1) == "/") {
					m = CLOSE_RE.exec(text.substring(i, j));
					if (!m || !stack.length || stack.pop() != m[1])
						bad();
					toks.push({ k: "close", s: i, e: j });
				}
				else {
					m = TAG_RE.exec(text.substring(i, j));
					if (!m)
						bad();
					name = m[1];
					kind = m[3] ? "empty" : "open";
					if (!stack.length && ++roots > 1)
						bad();		/* more than one root element */
					if (kind == "open") {
						stack.push(name);
						if (stack.length > MAX_DEPTH)
							bad();
					}
					toks.push({ k: kind, s: i, e: j, name: name });
				}
				i = j;
			}
		}
		if (stack.length || !roots)
			bad();
		return toks;
	}

	/* An opening tag with one space between the attributes */
	function tidy_tag(raw, name, empty)
	{
		var s = "<" + name;
		var m;
		ATTR_RE.lastIndex = 0;
		while ((m = ATTR_RE.exec(raw)) !== null)
			s += " " + m[1] + "=" + m[2];
		return s + (empty ? "/>" : ">");
	}

	function indent_block(raw, level)
	{
		/* keeps the lines of a comment or verbatim element as they are, indenting only the first */
		return pad(level) + raw;
	}

	function format_xml(text)
	{
		var toks = xml_tokens(text);
		var out = [];
		var pos = 0;

		/* Print the tokens of one element (its open token is toks[pos]) and leave pos after its close */
		function element(level)
		{
			var open = toks[pos];
			var start = open.s;
			var i, t, has_elem = false;

			if (open.k == "empty") {
				out.push(pad(level) + tidy_tag(text.substring(open.s, open.e), open.name, true) + "\n");
				pos++;
				return;
			}
			/* look ahead over the content to decide how to lay it out */
			for (i = pos + 1; ; i++) {
				t = toks[i];
				if (t.k == "close")
					break;
				if (t.k == "open" || t.k == "empty" || t.k == "comment" || t.k == "pi") {
					has_elem = true;
					break;
				}
			}
			var tag = tidy_tag(text.substring(open.s, open.e), open.name, false);
			if (!has_elem) {
				/* nothing but text (or nothing at all) inside */
				var inner = text.substring(open.e, toks[i].s);
				var close = "</" + open.name + ">";
				var trimmed = inner.replace(/^\s+|\s+$/g, "");
				if (trimmed === "") {
					out.push(pad(level) + tag + close + "\n");
				}
				else if (trimmed.indexOf("\n") < 0) {
					out.push(pad(level) + tag + trimmed + close + "\n");
				}
				else {
					out.push(pad(level) + tag + "\n");
					var lines = trimmed.split("\n");
					for (var l = 0; l < lines.length; l++)
						out.push(pad(level + 1) + lines[l].replace(/^\s+|\s+$/g, "") + "\n");
					out.push(pad(level) + close + "\n");
				}
				pos = i + 1;
				return;
			}
			/* children: find where this element really ends, and whether text is mixed in */
			var d = 0, mixed = false;
			for (i = pos + 1; i < toks.length; i++) {
				t = toks[i];
				if (t.k == "open")
					d++;
				else if (t.k == "close") {
					if (d == 0)
						break;
					d--;
				}
				else if (d == 0 && (t.k == "cdata" || (t.k == "text" && /[^ \t\r\n]/.test(text.substring(t.s, t.e)))))
					mixed = true;
			}
			if (mixed) {
				/* text mixed with elements (as in XHTML): whitespace matters, so leave it as written */
				out.push(indent_block(text.substring(start, toks[i].e), level) + "\n");
				pos = i + 1;
				return;
			}
			out.push(pad(level) + tag + "\n");
			pos++;
			while (toks[pos].k != "close") {
				t = toks[pos];
				if (t.k == "open" || t.k == "empty")
					element(level + 1);
				else {
					if (t.k != "text")
						out.push(indent_block(text.substring(t.s, t.e), level + 1) + "\n");
					pos++;
				}
			}
			out.push(pad(level) + "</" + open.name + ">\n");
			pos++;
		}

		while (pos < toks.length) {
			var tk = toks[pos];
			if (tk.k == "open" || tk.k == "empty")
				element(0);
			else {
				if (tk.k != "text")
					out.push(text.substring(tk.s, tk.e) + "\n");
				pos++;
			}
		}
		return out.join("");
	}

	function highlight_xml(text)
	{
		var toks = xml_tokens(text);
		var out = [];
		var i, t, raw, k, c, e;

		function entities(from, to)
		{
			var seg = text.substring(from, to);
			var p = 0;
			var a, b;
			while ((a = seg.indexOf("&", p)) >= 0) {
				b = seg.indexOf(";", a);
				if (b < 0 || b - a > 12 || /[\s<]/.test(seg.substring(a, b)))
					b = -1;
				if (b < 0) {
					out.push(repeat("p", a + 1 - p));
					p = a + 1;
					continue;
				}
				out.push(repeat("p", a - p), repeat("n", b + 1 - a));
				p = b + 1;
			}
			out.push(repeat("p", seg.length - p));
		}

		for (i = 0; i < toks.length; i++) {
			t = toks[i];
			if (t.k == "text") {
				entities(t.s, t.e);
			}
			else if (t.k == "comment" || t.k == "pi" || t.k == "doctype") {
				out.push(repeat("c", t.e - t.s));
			}
			else if (t.k == "cdata") {
				out.push(repeat("k", 9), repeat("s", t.e - t.s - 12), repeat("k", 3));
			}
			else if (t.k == "close") {
				out.push(repeat("k", t.e - t.s));
			}
			else {
				raw = text.substring(t.s, t.e);
				out.push(repeat("k", 1 + t.name.length));
				k = 1 + t.name.length;
				while (k < raw.length) {
					c = raw.charAt(k);
					if (c == "\"" || c == "'") {
						e = raw.indexOf(c, k + 1) + 1;
						out.push(repeat("s", e - k));
						k = e;
					}
					else if (c == "/" || c == ">") {
						out.push("k");
						k++;
					}
					else if (c == "=" || /\s/.test(c)) {
						out.push("p");
						k++;
					}
					else {
						e = k;
						while (e < raw.length && !/[\s=\/>]/.test(raw.charAt(e)))
							e++;
						out.push(repeat("t", e - k));
						k = e;
					}
				}
			}
		}
		return out.join("");
	}

	/* ---------- INI ---------- */

	function highlight_ini(text)
	{
		var lines = text.split("\n");
		var out = [];
		var i, line, m, eq, key, val, ws, tail;
		for (i = 0; i < lines.length; i++) {
			line = lines[i];
			if (/^\s*[;#]/.test(line))
				out.push(repeat("c", line.length));
			else if ((m = /^(\s*)(\[[^\]]*\])(.*)$/.exec(line)) !== null)
				out.push(repeat("p", m[1].length), repeat("t", m[2].length), repeat("p", m[3].length));
			else if ((eq = line.indexOf("=")) > 0 && /[^\s=]/.test(line.substring(0, eq))) {
				key = /^(\s*)(.*?)(\s*)$/.exec(line.substring(0, eq));
				out.push(repeat("p", key[1].length), repeat("k", key[2].length), repeat("p", key[3].length + 1));
				val = line.substring(eq + 1);
				m = /^(\s*)(.*?)(\s*)$/.exec(val);
				ws = m[1].length;
				tail = m[3].length;
				if (/^(-?[0-9]+(\.[0-9]+)?|0[xX][0-9a-fA-F]+|true|false|yes|no|on|off)$/i.test(m[2]))
					out.push(repeat("p", ws), repeat("n", m[2].length), repeat("p", tail));
				else
					out.push(repeat("p", ws), repeat("s", m[2].length), repeat("p", tail));
			}
			else
				out.push(repeat("p", line.length));
			if (i < lines.length - 1)
				out.push("p");
		}
		return out.join("");
	}

	/* ---------- YAML and TOML ---------- */

	var LITERAL_RE = /^(?:[-+]?(?:[0-9][0-9_]*(?:\.[0-9_]*)?(?:[eE][-+]?[0-9]+)?|\.[0-9]+)|0[xXoObB][0-9a-fA-F_]+|true|false|null|yes|no|on|off|~|nan|[-+]?inf|\.inf|\.nan|[0-9]{4}-[0-9]{2}-[0-9]{2}(?:[Tt ][0-9:.]+(?:Z|[-+][0-9:]+)?)?|[0-9]{2}:[0-9]{2}:[0-9]{2}(?:\.[0-9]+)?)$/i;

	/* Kinds for a value on one line: "quoted" strings, a trailing # comment, literals (numbers, true, null,
	 * dates) and other words as strings.  delims, if given, are characters that separate words and are
	 * plain (the punctuation of a [ ] or { } list). */
	function scan_value(v, delims)
	{
		var dre = delims ? new RegExp("[" + delims + "]") : null;
		var out = [];
		var i = 0;
		var n = v.length;
		var j, c;
		while (i < n) {
			c = v.charAt(i);
			if (c == "\"" || c == "'") {
				j = i + 1;
				while (j < n && v.charAt(j) != c) {
					if (c == "\"" && v.charAt(j) == "\\")
						j++;
					j++;
				}
				j = Math.min(n, j + 1);
				out.push(repeat("s", j - i));
				i = j;
			}
			else if (c == "#" && (i == 0 || /\s/.test(v.charAt(i - 1)))) {
				out.push(repeat("c", n - i));
				i = n;
			}
			else if (/\s/.test(c) || (dre && dre.test(c))) {
				out.push("p");
				i++;
			}
			else {
				j = i;
				while (j < n && !/\s/.test(v.charAt(j)) && !(dre && dre.test(v.charAt(j))))
					j++;
				out.push(repeat(LITERAL_RE.test(v.substring(i, j)) ? "n" : "s", j - i));
				i = j;
			}
		}
		return out.join("");
	}

	/* A YAML value to the end of the line: one string unless it is a literal, with any # comment after it */
	function yaml_value(v)
	{
		var lead = /^\s*/.exec(v)[0].length;
		var rest = v.substring(lead);
		var m, k, gap, x, tail = "";
		if (rest === "")
			return repeat("p", v.length);
		if (/^["'\[{]/.test(rest))
			return repeat("p", lead) + scan_value(rest, /^["']/.test(rest) ? null : "\\[\\]{},:");
		if (rest.charAt(0) == "#")
			return repeat("p", lead) + repeat("c", rest.length);
		m = /^(.*?)(\s+#.*)?$/.exec(rest);
		if (LITERAL_RE.test(m[1]))
			k = "n";
		else if (/^[|>][-+0-9]*$/.test(m[1]))
			k = "t";
		else if ((x = /^([&*!][^\s]*)(\s+(.*))?$/.exec(m[1])) !== null) {
			/* &anchor, *alias or !tag, then whatever follows it */
			return repeat("p", lead) + repeat("t", x[1].length) + (x[2] ? yaml_value(x[2] + (m[2] || "")) : (m[2] ? repeat("p", /^\s*/.exec(m[2])[0].length) + repeat("c", m[2].replace(/^\s+/, "").length) : ""));
		}
		else
			k = "s";
		if (m[2]) {
			gap = /^\s*/.exec(m[2])[0].length;
			tail = repeat("p", gap) + repeat("c", m[2].length - gap);
		}
		return repeat("p", lead) + repeat(k, m[1].length) + tail;
	}

	/* If a value starts a quoted string that doesn't end on this line, the quote character, else "" */
	function open_quote(v)
	{
		var t = v.replace(/^\s+/, "");
		var q = t.charAt(0);
		if (q != "\"" && q != "'")
			return "";
		return close_quote(t, 1, q) < 0 ? q : "";
	}

	/* The index of the quote that closes a string (q is its opening quote) in line, looking from i, or -1 */
	function close_quote(line, i, q)
	{
		for (; i < line.length; i++) {
			var c = line.charAt(i);
			if (q == "\"" && c == "\\")
				i++;
			else if (c == q) {
				if (q == "'" && line.charAt(i + 1) == "'")		/* '' is a quote inside the string */
					i++;
				else
					return i;
			}
		}
		return -1;
	}

	function highlight_yaml(text)
	{
		var lines = text.split("\n");
		var out = [];
		var block = -1;		/* indent of the line that started a | or > block, while inside one */
		var quote = "";		/* the quote character of a string that carries on over more lines */
		var i, line, m, indent, cls, dash, ws, ce;
		for (i = 0; i < lines.length; i++) {
			line = lines[i];
			indent = /^ */.exec(line)[0].length;
			if (quote) {
				ce = close_quote(line, 0, quote);
				if (ce < 0)
					out.push(repeat("s", line.length));
				else {
					out.push(repeat("s", ce + 1) + yaml_value(line.substring(ce + 1)));
					quote = "";
				}
				if (i < lines.length - 1)
					out.push("p");
				continue;
			}
			if (block >= 0) {
				if (line.replace(/\s+/g, "") === "" || indent > block) {
					out.push(repeat("s", line.length));
					if (i < lines.length - 1)
						out.push("p");
					continue;
				}
				block = -1;
			}
			if (/^\s*#/.test(line))
				cls = repeat("c", line.length);
			else if (/^(---|\.\.\.)(\s|$)/.test(line))
				cls = repeat("t", 3) + yaml_value(line.substring(3));
			else {
				m = /^(\s*(?:-\s+)*)((?:"[^"]*"|'[^']*'|[^\s#:\[\]{},&*!|>'"%@`-][^#:]*?|-[^\s#:][^#:]*?)\s*:)(?=\s|$)(.*)$/.exec(line);
				if (m) {
					cls = repeat("p", m[1].length) + repeat("k", m[2].length - 1) + "p" + yaml_value(m[3]);
					quote = open_quote(m[3]);
					if (/^\s*[|>][-+0-9]*\s*(#.*)?$/.test(m[3]))
						block = indent;
				}
				else if ((m = /^(\s*-)(\s+)(.*)$/.exec(line)) !== null) {
					dash = m[1].length;
					ws = m[2].length;
					cls = repeat("n", dash) + repeat("p", ws) + yaml_value(m[3]);
					quote = open_quote(m[3]);
					if (/^[|>][-+0-9]*\s*$/.test(m[3]))
						block = indent;
				}
				else
					cls = repeat("p", line.length);
			}
			out.push(cls);
			if (i < lines.length - 1)
				out.push("p");
		}
		return out.join("");
	}

	var TOML_DELIMS = "\\[\\]{},=";
	var TOML_KEY_RE = /^\s*(?:[A-Za-z0-9_-]+|"[^"]*"|'[^']*')(?:\s*\.\s*(?:[A-Za-z0-9_-]+|"[^"]*"|'[^']*'))*\s*$/;

	function highlight_toml(text)
	{
		var lines = text.split("\n");
		var out = [];
		var multi = "";		/* the closing triple quote, while inside a multi-line string */
		var i, line, m, cls, idx, eq, key, v, q, head;
		for (i = 0; i < lines.length; i++) {
			line = lines[i];
			if (multi) {
				idx = line.indexOf(multi);
				if (idx < 0)
					cls = repeat("s", line.length);
				else {
					cls = repeat("s", idx + 3) + scan_value(line.substring(idx + 3), TOML_DELIMS);
					multi = "";
				}
			}
			else if (/^\s*#/.test(line))
				cls = repeat("c", line.length);
			else if ((m = /^(\s*)(\[\[?[^\]]*\]\]?)(.*)$/.exec(line)) !== null)
				cls = repeat("p", m[1].length) + repeat("t", m[2].length) + scan_value(m[3], TOML_DELIMS);
			else if ((eq = line.indexOf("=")) > 0 && TOML_KEY_RE.test(line.substring(0, eq))) {
				key = /^(\s*)(.*?)(\s*)$/.exec(line.substring(0, eq));
				v = line.substring(eq + 1);
				head = repeat("p", key[1].length) + repeat("k", key[2].length) + repeat("p", key[3].length + 1);
				q = /^\s*("""|''')/.exec(v);
				if (q && v.indexOf(q[1], v.indexOf(q[1]) + 3) < 0) {
					idx = v.indexOf(q[1]);
					cls = head + repeat("p", idx) + repeat("s", v.length - idx);
					multi = q[1];
				}
				else
					cls = head + scan_value(v, TOML_DELIMS);
			}
			else
				cls = repeat("p", line.length);
			out.push(cls);
			if (i < lines.length - 1)
				out.push("p");
		}
		return out.join("");
	}

	/* ---------- Markdown ---------- */

	/* Show Markdown the way it is meant to be read: the marks are removed and the text is colored instead.
	 * Headings are t, **bold** n, *italic* and link text k, `code` and code blocks s, quotes, rules and
	 * link addresses c.  Returns { text, kinds }. */
	function render_md(text)
	{
		var lines = text.split("\n");
		var outt = [];
		var outk = [];
		var fence = "";
		var i, line, m, next, r;
		var INLINE_RE = /(`+)([^`]|[^`][\s\S]*?[^`])\1(?!`)|(!?)\[([^\]\n]*)\]\(([^)\s]*)(?:\s+"[^"]*")?\)|\*\*([^*\n]+?)\*\*|__([^_\n]+?)__|\*([^*\s][^*\n]*?)\*|_([^_\s][^_\n]*?)_|\\([\\`*_{}\[\]()#+\-.!>|])/g;

		function emit(t, k)
		{
			outt.push(t);
			outk.push(k);
		}

		/* inline marks in a piece of text; base is the kind of the plain text around them */
		function inline(src, base)
		{
			var t = "";
			var k = "";
			var last = 0;
			var x, before, after;
			function put(str, kind)
			{
				t += str;
				k += repeat(kind, str.length);
			}
			INLINE_RE.lastIndex = 0;
			while ((x = INLINE_RE.exec(src)) !== null) {
				before = x.index > 0 ? src.charAt(x.index - 1) : " ";
				after = src.charAt(x.index + x[0].length);
				/* snake_case_names are not italic: underscores must not touch a letter or digit */
				if ((x[9] !== undefined && (/\w/.test(before) || /\w/.test(after))) ||
				    (x[7] !== undefined && /\w/.test(before))) {
					INLINE_RE.lastIndex = x.index + 1;
					continue;
				}
				put(src.substring(last, x.index), base);
				if (x[2] !== undefined)
					put(x[2].replace(/^ (.*) $/, "$1"), "s");
				else if (x[4] !== undefined) {
					put(x[3] ? "[" + x[4] + "]" : x[4], "k");
					if (!x[3] && x[5])
						put(" (" + x[5] + ")", "c");
				}
				else if (x[6] !== undefined)
					put(x[6], "n");
				else if (x[7] !== undefined)
					put(x[7], "n");
				else if (x[8] !== undefined)
					put(x[8], "k");
				else if (x[9] !== undefined)
					put(x[9], "k");
				else
					put(x[10], base);
				last = x.index + x[0].length;
			}
			put(src.substring(last), base);
			return { t: t, k: k };
		}

		for (i = 0; i < lines.length; i++) {
			line = lines[i];
			if (fence) {
				if (new RegExp("^\\s{0,3}" + (fence.charAt(0) == "`" ? "`" : "~") + "{" + fence.length + ",}\\s*$").test(line))
					fence = "";
				else
					emit(line, repeat("s", line.length));
				continue;
			}
			m = /^\s{0,3}(`{3,}|~{3,})/.exec(line);
			if (m) {
				fence = m[1];
				continue;
			}
			if ((m = /^\s{0,3}#{1,6}\s+(.*?)(?:\s+#+)?\s*$/.exec(line)) !== null) {
				r = inline(m[1], "t");
				emit(r.t, r.k);
				continue;
			}
			next = i + 1 < lines.length ? lines[i + 1] : "";
			if (/^\s{0,3}\S/.test(line) && !/^\s*([-*+]|\d+[.)])\s/.test(line) && !/^\s*>/.test(line) &&
			    !/^\s{0,3}([-*_])(\s*\1){2,}\s*$/.test(line) && (/^=+\s*$/.test(next) || /^-{2,}\s*$/.test(next))) {
				r = inline(line.replace(/^\s+|\s+$/g, ""), "t");
				emit(r.t, r.k);
				i++;
				continue;
			}
			if (/^\s{0,3}([-*_])(\s*\1){2,}\s*$/.test(line)) {
				emit(repeat("\xc4", 40), repeat("c", 40));
				continue;
			}
			if ((m = /^(\s*)>\s?(.*)$/.exec(line)) !== null) {
				r = inline(m[2], "c");
				emit(m[1] + "\xb3 " + r.t, repeat("p", m[1].length) + "cc" + r.k);
				continue;
			}
			if ((m = /^(\s*)([-*+]|\d+[.)])\s+(.*)$/.exec(line)) !== null) {
				r = inline(m[3], "p");
				var mark = /^\d/.test(m[2]) ? m[2] : "\xf9";
				emit(m[1] + mark + " " + r.t, repeat("p", m[1].length) + repeat("n", mark.length) + "p" + r.k);
				continue;
			}
			r = inline(line, "p");
			emit(r.t, r.k);
		}
		/* the kinds string has a "p" for each newline, as the other kinds do */
		return { text: outt.join("\n"), kinds: outk.join("p") };
	}

	/* ---------- Public ---------- */

	function format(kind, text)
	{
		if (text.length > limit_for(kind))
			return null;
		try {
			if (kind == "json")
				return format_json(text);
			if (kind == "xml")
				return format_xml(text);
		}
		catch (e) {
			return null;
		}
		return text;
	}

	function highlight(kind, text)
	{
		if (text.length > limit_for(kind) * 3)
			return null;
		try {
			if (kind == "json") {
				parse_json(text);
				return highlight_json(text);
			}
			if (kind == "xml")
				return highlight_xml(text);
			if (kind == "ini")
				return highlight_ini(text);
			if (kind == "yaml")
				return highlight_yaml(text);
			if (kind == "toml")
				return highlight_toml(text);
		}
		catch (e) {
		}
		return null;
	}

	/* Everything the reader needs in one call: { text, kinds } to show in place of the file (laid out again
	 * for JSON, XML and Markdown), or null if the file should be shown as it is.  text uses \n line ends and
	 * kinds has one character per character of text, newlines included. */
	function view(kind, text)
	{
		var out, kinds;
		if (!kind || text.length > limit_for(kind))
			return null;
		try {
			if (kind == "md") {
				out = render_md(text);
				return out.kinds.length == out.text.length ? out : null;
			}
			out = format(kind, text);
			if (out === null)
				return null;
			kinds = highlight(kind, out);
			if (kinds === null || kinds.length != out.length)
				return null;
			return { text: out, kinds: kinds };
		}
		catch (e) {
			return null;
		}
	}

	return { kind_of: kind_of, format: format, highlight: highlight, view: view };
})();
