/* univiewerlibrary.js
 *
 * Library browser for Universal Viewer.  Lists the libraries (directories of PDF, image,
 * ANSI/ASCII art and text files) configured in settings.ini, filtered by each library's
 * ARS, include and exclude settings, and opens the chosen file in the Universal Viewer viewer
 * (PDF and images) or the text/ANSI viewer.  ZIP files in libraries that allow
 * sub-directories are extracted on demand into a directory of the same name.
 * S and P in the lists set the output speed and screen pausing for text/ANSI files.
 *
 *	?univiewerlibrary [force]
 *
 * Configuration: settings.ini (see settings-example.ini) and the theme file it names.
 * Requires univiewer_lib.js.  Tab size is 4.
 */

"use strict";

load("sbbsdefs.js");
load("key_defs.js");
load("smbdefs.js");
load("univiewer_lib.js");
load("http_lib.js");
load("univiewer_music_lib.js");
/* html_lib.js
 *
 * HTML (and EPUB) support for Universal Viewer.  An HTML document is laid out into pages and written as a
 * real PDF file, which the normal viewer then displays.  That gives HTML scrolling, zooming, page
 * navigation, links (Tab / Enter) and a table of contents (C, built from the headings) for free.
 *
 *	var b = new HtmlBuilder({ resolve_image: f, link: g });
 *	b.add_html(source, base);		// can be called again for further chapters
 *	b.write("/path/out.pdf");
 *
 * resolve_image(src, base) returns { w, h, jpeg: "<binary string>" } or { w, h, data: RGB Uint8Array,
 * alpha: Uint8Array|null }, or null.  link(href, base) returns { uri: "..." } for a link that may be
 * followed to another document, { anchor: "key" } for a link inside this document, or null to leave
 * the text unlinked.  anchor_key(base, id) names anchors (the default is just the id).
 *
 * Handled: headings, paragraphs, lists, block quotes, preformatted text, tables (columns sized by
 * their content), rules, bold/italic/monospace, links, images (JPEG passed through, others
 * re-encoded), named character references.  Forms are drawn (and can be filled in and sent) only when
 * opts.forms is on.  Ignored: style sheets and scripts, frames, colors and other CSS layout.
 *
 * Requires pdf2sixel_lib.js (for std_char_width).  Tab size is 4.
 */

/* ---------- Text helpers ---------- */

var HTML_NAMED = { amp: 38, lt: 60, gt: 62, quot: 34, apos: 39, nbsp: 160, hellip: 0x2026, mdash: 0x2014, ndash: 0x2013,
	lsquo: 0x2018, rsquo: 0x2019, ldquo: 0x201c, rdquo: 0x201d, bull: 0x2022, trade: 0x2122, euro: 0x20ac, larr: 0x2190,
	rarr: 0x2192, middot: 183, thinsp: 32, ensp: 32, emsp: 32 };
(function () {
	var l1 = "iexcl cent pound curren yen brvbar sect uml copy ordf laquo not shy reg macr deg plusmn sup2 sup3 acute micro para " +
		"middot cedil sup1 ordm raquo frac14 frac12 frac34 iquest Agrave Aacute Acirc Atilde Auml Aring AElig Ccedil Egrave Eacute " +
		"Ecirc Euml Igrave Iacute Icirc Iuml ETH Ntilde Ograve Oacute Ocirc Otilde Ouml times Oslash Ugrave Uacute Ucirc Uuml " +
		"Yacute THORN szlig agrave aacute acirc atilde auml aring aelig ccedil egrave eacute ecirc euml igrave iacute icirc iuml " +
		"eth ntilde ograve oacute ocirc otilde ouml divide oslash ugrave uacute ucirc uuml yacute thorn yuml";
	var a = l1.split(" ");
	for (var i = 0; i < a.length; i++)
		HTML_NAMED[a[i]] = 0xa1 + i;
})();

function html_decode_entities(s)
{
	if (s.indexOf("&") < 0)
		return s;
	return s.replace(/&(#[xX][0-9a-fA-F]+|#[0-9]+|[a-zA-Z][a-zA-Z0-9]*);/g, function (m, e) {
		var c;
		if (e.charAt(0) == "#")
			c = e.charAt(1) == "x" || e.charAt(1) == "X" ? parseInt(e.substr(2), 16) : parseInt(e.substr(1), 10);
		else
			c = HTML_NAMED[e];
		if (c === undefined || !(c > 0) || c > 0xffff)
			return m;
		return String.fromCharCode(c);
	});
}

/* A binary string holding UTF-8 -> a string of characters (leaves it alone if it isn't valid UTF-8) */
function html_utf8(s)
{
	if (!/[\x80-\xff]/.test(s))
		return s;
	var seq = /[\xc2-\xdf][\x80-\xbf]|[\xe0-\xef][\x80-\xbf][\x80-\xbf]|[\xf0-\xf4][\x80-\xbf][\x80-\xbf][\x80-\xbf]/g;
	if (/[\x80-\xff]/.test(s.replace(seq, "")))
		return s;
	return s.replace(seq, function (m) {
		var c0 = m.charCodeAt(0), cp;
		if (m.length == 2)
			cp = ((c0 & 31) << 6) | (m.charCodeAt(1) & 63);
		else if (m.length == 3)
			cp = ((c0 & 15) << 12) | ((m.charCodeAt(1) & 63) << 6) | (m.charCodeAt(2) & 63);
		else
			cp = 63;		/* beyond the basic plane */
		return String.fromCharCode(cp);
	});
}

var HTML_WINANSI = { 0x20ac: 0x80, 0x2026: 0x85, 0x2018: 0x91, 0x2019: 0x92, 0x201c: 0x93, 0x201d: 0x94, 0x2022: 0x95,
	0x2013: 0x96, 0x2014: 0x97, 0x2122: 0x99, 0x2190: 0x3c, 0x2192: 0x3e, 0x2212: 0x2d, 0x00a0: 0x20, 0x2009: 0x20, 0x2002: 0x20,
	0x2003: 0x20, 0x200b: -1, 0xfeff: -1, 0x2039: 0x8b, 0x203a: 0x9b };

/* A character as a WinAnsi byte value, or -1 to drop it */
function html_winansi(ch)
{
	var c = ch.charCodeAt(0);
	if (c >= 32 && c < 127)
		return c;
	if (HTML_WINANSI[c] !== undefined)
		return HTML_WINANSI[c];
	if (c >= 0xa1 && c <= 0xff)
		return c;
	return 63;
}

/* ---------- HTML parsing ---------- */

var HTML_VOID = { br: 1, hr: 1, img: 1, meta: 1, link: 1, input: 1, area: 1, base: 1, col: 1, wbr: 1, source: 1, param: 1, track: 1 };
var HTML_RAW = { script: 1, style: 1, textarea: 1, title: 1, noscript: 1, template: 1 };
var HTML_DROP = { head: 1, script: 1, style: 1, noscript: 1, svg: 1, iframe: 1, canvas: 1, template: 1, audio: 1, video: 1,
	object: 1, embed: 1, select: 1, textarea: 1, title: 1, map: 1, math: 1 };
var HTML_BLOCK = { p: 1, div: 1, h1: 1, h2: 1, h3: 1, h4: 1, h5: 1, h6: 1, ul: 1, ol: 1, li: 1, blockquote: 1, pre: 1, table: 1,
	hr: 1, center: 1, section: 1, article: 1, header: 1, footer: 1, nav: 1, aside: 1, main: 1, figure: 1, figcaption: 1, form: 1,
	fieldset: 1, address: 1, dl: 1, dt: 1, dd: 1, tr: 1, td: 1, th: 1, thead: 1, tbody: 1, tfoot: 1, caption: 1, body: 1, html: 1,
	details: 1, summary: 1, menu: 1, dir: 1, body_: 0 };
var HTML_CLOSES_P = { p: 1, div: 1, h1: 1, h2: 1, h3: 1, h4: 1, h5: 1, h6: 1, ul: 1, ol: 1, li: 1, blockquote: 1, pre: 1, table: 1,
	hr: 1, section: 1, article: 1, header: 1, footer: 1, nav: 1, aside: 1, figure: 1, form: 1, dl: 1, dt: 1, dd: 1, center: 1 };

function html_attrs(s)
{
	var a = {};
	var re = /([^\s=\/"'>]+)(?:\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s"'>]+)))?/g, m;
	while ((m = re.exec(s)) !== null) {
		var v = m[2] !== undefined ? m[2] : m[3] !== undefined ? m[3] : m[4] !== undefined ? m[4] : "";
		a[m[1].toLowerCase()] = html_decode_entities(v);
	}
	return a;
}

/* Parse HTML into { t: tag, a: attributes, c: children } and { t: "#text", s } nodes; returns { root, title, charset } */
function html_parse(src)
{
	src = src.replace(/^\xef\xbb\xbf/, "");
	var cs = /<meta[^>]*charset\s*=\s*["']?\s*([a-zA-Z0-9_-]+)/i.exec(src.substr(0, 4096));
	var charset = cs ? cs[1].toLowerCase() : "";
	var latin = /^(iso-8859-1|latin1|windows-1252|us-ascii|cp1252)$/.test(charset);
	if (!latin)
		src = html_utf8(src);
	var root = { t: "root", a: {}, c: [] };
	var stack = [root];
	var title = "";
	var lower = null;
	var re = /<!--[\s\S]*?-->|<!\[CDATA\[([\s\S]*?)\]\]>|<[!?][^>]*>|<(\/?)([a-zA-Z][a-zA-Z0-9:_-]*)((?:"[^"]*"|'[^']*'|[^>"'])*)>|([^<]+|<)/g;
	var m, count = 0;

	function top() { return stack[stack.length - 1]; }
	function has_open(name, limit)
	{
		for (var i = stack.length - 1; i >= 1; i--) {
			if (stack[i].t == name)
				return i;
			if (limit && limit[stack[i].t])
				return 0;
		}
		return 0;
	}
	function close_to(i)
	{
		stack.length = i;
	}
	while ((m = re.exec(src)) !== null) {
		if (++count > 400000)
			break;
		if (m[5] !== undefined) {
			if (m[5] != "<")
				top().c.push({ t: "#text", s: html_decode_entities(m[5]) });
			continue;
		}
		if (m[1] !== undefined) {
			top().c.push({ t: "#text", s: m[1] });
			continue;
		}
		if (m[3] === undefined)
			continue;
		var name = m[3].toLowerCase().replace(/^.*:/, "");
		if (m[2]) {		/* closing tag */
			var oi = has_open(name, name == "li" ? { ul: 1, ol: 1 } : (name == "td" || name == "th" ? { table: 1 } : null));
			if (oi)
				close_to(oi);
			continue;
		}
		var attrs = html_attrs(m[4]);
		var selfclose = /\/\s*$/.test(m[4]);
		/* implied end tags */
		if (HTML_CLOSES_P[name]) {
			var pi = has_open("p", { table: 1, td: 1, th: 1, li: 1, div: 1, blockquote: 1 });
			if (pi)
				close_to(pi);
		}
		if (name == "li") {
			var li = has_open("li", { ul: 1, ol: 1 });
			if (li)
				close_to(li);
		}
		else if (name == "td" || name == "th") {
			var ti = has_open("td", { tr: 1, table: 1 }) || has_open("th", { tr: 1, table: 1 });
			if (ti)
				close_to(ti);
		}
		else if (name == "tr") {
			var ri = has_open("tr", { table: 1 });
			if (ri)
				close_to(ri);
		}
		else if (name == "dt" || name == "dd") {
			var di = has_open("dt", { dl: 1 }) || has_open("dd", { dl: 1 });
			if (di)
				close_to(di);
		}
		if (HTML_RAW[name] && !selfclose) {
			if (lower === null)
				lower = src.toLowerCase();
			var end = lower.indexOf("</" + name, re.lastIndex);
			if (end < 0)
				end = src.length;
			var body = src.substring(re.lastIndex, end);
			var gt = src.indexOf(">", end);
			re.lastIndex = gt < 0 ? src.length : gt + 1;
			if (name == "title")
				title = html_decode_entities(body.replace(/\s+/g, " ")).replace(/^\s+|\s+$/g, "");
			else if (name == "textarea")		/* kept, for forms: its text is its starting value */
				top().c.push({ t: "textarea", a: attrs, c: [{ t: "#text", s: html_decode_entities(body) }] });
			continue;
		}
		var node = { t: name, a: attrs, c: [] };
		top().c.push(node);
		if (!HTML_VOID[name] && !selfclose)
			stack.push(node);
	}
	return { root: root, title: title, charset: charset };
}

/* ---------- Layout ---------- */

var HTML_FONTS = [
	["Times-Roman", 1, 0, 0], ["Times-Bold", 1, 1, 0], ["Times-Italic", 1, 0, 1], ["Times-BoldItalic", 1, 1, 1],
	["Helvetica", 0, 0, 0], ["Helvetica-Bold", 0, 1, 0], ["Helvetica-Oblique", 0, 0, 1], ["Helvetica-BoldOblique", 0, 1, 1],
	["Courier", 2, 0, 0], ["Courier-Bold", 2, 1, 0], ["Courier-Oblique", 2, 0, 1], ["Courier-BoldOblique", 2, 1, 1]
];

/* Index into HTML_FONTS: family 0 sans, 1 serif, 2 mono */
function html_font_index(fam, bold, italic)
{
	return (fam == 1 ? 0 : fam == 0 ? 4 : 8) + (bold ? 1 : 0) + (italic ? 2 : 0);
}

function html_text_width(fam, bold, sz, s)
{
	var w = 0;
	for (var i = 0; i < s.length; i++) {
		if (html_winansi(s.charAt(i)) < 0)
			continue;
		w += std_char_width(fam, bold ? 1 : 0, s.charAt(i));
	}
	return w * sz / 1000;
}

function html_has_block(node)
{
	if (node.hb !== undefined)
		return node.hb;
	var r = false;
	if (node.c) {
		for (var i = 0; i < node.c.length && !r; i++) {
			var c = node.c[i];
			if (c.t != "#text" && (HTML_BLOCK[c.t] || html_has_block(c)))
				r = true;
		}
	}
	node.hb = r;
	return r;
}

/* Elements that are left out of the page.  Select and textarea are kept when forms are on. */
function html_dropped(flow, n)
{
	return HTML_DROP[n.t] && !(flow.b.opts.forms && (n.t == "select" || n.t == "textarea"));
}

function html_node_text(node)
{
	if (node.t == "#text")
		return node.s;
	var s = "";
	if (node.c) {
		for (var i = 0; i < node.c.length; i++) {
			if (!HTML_DROP[node.c[i].t])
				s += html_node_text(node.c[i]) + (node.c[i].t == "br" ? " " : "");
		}
	}
	return s;
}

/* One page being laid out: lists of drawing operations */
function HtmlPage()
{
	this.texts = [];	/* { x, y (baseline, from the top), f, sz, s, link, w } */
	this.rects = [];	/* { x, y, w, h, g } filled gray rectangles */
	this.imgs = [];		/* { x, y, w, h, img } */
	this.links = [];	/* { x0, y0, x1, y1, link } */
}

/* The flow of content down a page; a scratch Flow with a huge page height is used to measure table cells */
function HtmlFlow(builder, pagew, pageh, margin, scratch)
{
	this.b = builder;
	this.pagew = pagew;
	this.pageh = pageh;
	this.margin = margin;
	this.scratch = !!scratch;
	this.pages = [new HtmlPage()];
	this.y = margin;
	this.marker = null;
	this.pending_anchor = [];
}

HtmlFlow.prototype.page = function ()
{
	return this.pages[this.pages.length - 1];
};

HtmlFlow.prototype.new_page = function ()
{
	if (this.scratch)
		return;
	this.pages.push(new HtmlPage());
	this.y = this.margin;
};

HtmlFlow.prototype.room = function ()
{
	return this.pageh - this.margin - this.y;
};

/* Make sure h points of height fit, starting a new page if not (and if something is already on this page) */
HtmlFlow.prototype.need = function (h)
{
	if (this.scratch)
		return;
	if (this.y + h > this.pageh - this.margin && this.y > this.margin + 0.5)
		this.new_page();
};

HtmlFlow.prototype.skip = function (h)
{
	if (this.y > this.margin + 0.5)
		this.y += h;
};

/* Record anchors (ids) at the current position */
HtmlFlow.prototype.anchor = function (key)
{
	if (key && this.b.anchors[key] === undefined)
		this.b.anchors[key] = { flow: this, page: this.pages.length - 1, y: this.y };
};

/* Collect inline items from nodes.  st: { fam, bold, italic, sz, link, ul, pre } */
HtmlFlow.prototype.collect = function (nodes, st, items, base)
{
	for (var i = 0; i < nodes.length; i++) {
		var n = nodes[i];
		if (n.t == "#text") {
			var s = n.s;
			if (st.pre) {
				var lines = s.replace(/\r\n?/g, "\n").replace(/\t/g, "    ").split("\n");
				for (var li = 0; li < lines.length; li++) {
					if (li > 0)
						items.push({ k: "br", sz: st.sz });
					if (lines[li].length)
						items.push({ k: "w", s: lines[li], st: st, sp: false, pre: true });
				}
				continue;
			}
			s = s.replace(/[\s ]+/g, " ");
			if (!s.length)
				continue;
			var startsp = s.charAt(0) == " ", endsp = s.charAt(s.length - 1) == " ";
			var words = s.replace(/^ | $/g, "").split(" ");
			if (words.length == 1 && words[0] == "") {
				if (items.length)
					items[items.length - 1].sp = true;
				continue;
			}
			if (startsp && items.length)
				items[items.length - 1].sp = true;
			for (var wi = 0; wi < words.length; wi++)
				items.push({ k: "w", s: words[wi], st: st, sp: wi < words.length - 1 || endsp });
			continue;
		}
		if (this.b.opts.forms && st.form && (n.t == "input" || n.t == "textarea" || n.t == "select" || n.t == "button")) {
			var fit = this.b.form_item(n, st, base);
			if (fit)
				items.push(fit);
			continue;
		}
		if (html_dropped(this, n))
			continue;
		var ns = st;
		var t = n.t;
		if (n.a && (n.a.id || (t == "a" && n.a.name)))
			items.push({ k: "anchor", id: n.a.id || n.a.name, base: base });
		if (t == "br") {
			items.push({ k: "br", sz: st.sz });
			continue;
		}
		if (t == "img") {
			items.push({ k: "img", a: n.a, st: st, base: base });
			continue;
		}
		if (t == "b" || t == "strong" || t == "th") {
			ns = this.derive(st, { bold: true });
		}
		else if (t == "i" || t == "em" || t == "cite" || t == "var" || t == "dfn" || t == "address")
			ns = this.derive(st, { italic: true });
		else if (t == "code" || t == "tt" || t == "kbd" || t == "samp")
			ns = this.derive(st, { fam: 2 });
		else if (t == "u" || t == "ins")
			ns = this.derive(st, { ul: true });
		else if (t == "sub" || t == "sup" || t == "small")
			ns = this.derive(st, { sz: st.sz * 0.8 });
		else if (t == "big")
			ns = this.derive(st, { sz: st.sz * 1.2 });
		else if (t == "a" && n.a && n.a.href) {
			var lk = this.b.opts.link ? this.b.opts.link(n.a.href, base) : null;
			if (lk)
				ns = this.derive(st, { link: lk, ul: true });
		}
		else if (t == "form" && this.b.opts.forms)
			ns = this.derive(st, { form: this.b.new_form(n, base) });
		this.collect(n.c || [], ns, items, base);
		if (t == "td" || t == "th")
			items.push({ k: "w", s: "", st: ns, sp: true });
	}
};

HtmlFlow.prototype.derive = function (st, over)
{
	var o = {};
	for (var k in st)
		o[k] = st[k];
	for (k in over)
		o[k] = over[k];
	return o;
};

/* Lay out inline items as lines of text inside a box of the given x and width */
HtmlFlow.prototype.flush = function (items, x0, width, align, st0)
{
	var b = this.b;
	var lines = [], cur = null;
	function newline() { cur = { frags: [], w: 0, h: 0, asc: 0 }; lines.push(cur); }
	newline();
	var i, it, spw;
	function trim_end()
	{
		while (cur.frags.length && cur.frags[cur.frags.length - 1].k == "gap") {
			cur.w -= cur.frags[cur.frags.length - 1].w;
			cur.frags.pop();
		}
	}
	for (i = 0; i < items.length; i++) {
		it = items[i];
		if (it.k == "anchor") {
			cur.frags.push({ k: "anchor", id: it.id, base: it.base, w: 0 });
			continue;
		}
		if (it.k == "br") {
			cur.h = Math.max(cur.h, it.sz * 1.25);
			cur.asc = Math.max(cur.asc, it.sz);
			trim_end();
			newline();
			continue;
		}
		if (it.k == "img") {
			var im = this.b.get_image(it.a, it.base);
			if (!im)
				continue;
			var w = im.dw, h = im.dh;
			var maxw = width, maxh = this.pageh - 2 * this.margin - 4;
			if (w > maxw) { h = h * maxw / w; w = maxw; }
			if (h > maxh) { w = w * maxh / h; h = maxh; }
			if (cur.w + w > width && cur.frags.length) {
				trim_end();
				newline();
			}
			cur.frags.push({ k: "img", img: im, w: w, h: h, link: it.st.link, x: cur.w });
			cur.w += w;
			cur.h = Math.max(cur.h, h + 2);
			cur.asc = Math.max(cur.asc, h);
			if (it.sp !== false) {
				var gw0 = it.st.sz * 0.25;
				cur.frags.push({ k: "gap", w: gw0 });
				cur.w += gw0;
			}
			continue;
		}
		/* a word */
		var s = it.s, stt = it.st;
		var sz = stt.sz;
		var ww = html_text_width(stt.fam, stt.bold, sz, s);
		spw = html_text_width(stt.fam, stt.bold, sz, " ");
		if (!s.length) {
			if (it.sp) {
				cur.frags.push({ k: "gap", w: spw });
				cur.w += spw;
			}
			continue;
		}
		if (cur.w + ww > width + 0.01 && cur.frags.length && !it.pre) {
			trim_end();
			newline();
		}
		/* a word longer than a whole line is broken up */
		while (ww > width && s.length > 1) {
			var cut = s.length;
			while (cut > 1 && html_text_width(stt.fam, stt.bold, sz, s.substr(0, cut)) > width - (cur.w > 0 ? cur.w : 0))
				cut--;
			if (cut < 1)
				cut = 1;
			var piece = s.substr(0, cut);
			cur.frags.push({ k: "w", s: piece, st: stt, w: html_text_width(stt.fam, stt.bold, sz, piece), x: cur.w });
			cur.w += cur.frags[cur.frags.length - 1].w;
			cur.h = Math.max(cur.h, sz * 1.25);
			cur.asc = Math.max(cur.asc, sz);
			newline();
			s = s.substr(cut);
			ww = html_text_width(stt.fam, stt.bold, sz, s);
		}
		cur.frags.push({ k: "w", s: s, st: stt, w: ww, x: cur.w });
		cur.w += ww;
		cur.h = Math.max(cur.h, sz * 1.25);
		cur.asc = Math.max(cur.asc, sz);
		if (it.sp && !it.pre) {
			cur.frags.push({ k: "gap", w: spw, link: stt.link, ul: stt.ul });
			cur.w += spw;
		}
	}
	trim_end();
	/* emit the lines */
	var pageLinks = [];
	for (var li = 0; li < lines.length; li++) {
		var ln = lines[li];
		if (!ln.frags.length && li == lines.length - 1)
			break;
		var lh = ln.h || (st0 ? st0.sz * 1.25 : 15);
		var asc = ln.asc || (st0 ? st0.sz : 12);
		this.need(lh);
		var pg = this.page();
		var ox = x0;
		if (align == "center")
			ox = x0 + Math.max(0, (width - ln.w) / 2);
		else if (align == "right")
			ox = x0 + Math.max(0, width - ln.w);
		if (this.marker) {
			var mk = this.marker;
			this.marker = null;
			pg.texts.push({ x: x0 - mk.w - 4, y: this.y + asc, f: html_font_index(mk.fam, false, false), sz: mk.sz, s: mk.s, w: mk.w });
		}
		for (var fi = 0; fi < ln.frags.length; fi++) {
			var f = ln.frags[fi];
			if (f.k == "anchor") {
				this.b.set_anchor(this, f.id, f.base, this.y);
				continue;
			}
			if (f.k == "gap") {
				continue;
			}
			if (f.k == "img") {
				pg.imgs.push({ x: ox + f.x, y: this.y + asc - f.h, w: f.w, h: f.h, img: f.img });
				if (f.link)
					pg.links.push({ x0: ox + f.x, y0: this.y + asc - f.h, x1: ox + f.x + f.w, y1: this.y + asc, link: f.link });
				continue;
			}
			var t = { x: ox + f.x, y: this.y + asc, f: html_font_index(f.st.fam, f.st.bold, f.st.italic), sz: f.st.sz, s: f.s, w: f.w,
				link: f.st.link ? true : false, ul: f.st.ul, color: f.st.color };
			pg.texts.push(t);
			if (f.st.link && f.st.link.field && !this.scratch)
				f.st.link.field.page = this.pages.length - 1;
			if (f.st.link) {
				/* join neighboring words of the same link into one clickable box */
				var lastl = pg.links.length ? pg.links[pg.links.length - 1] : null;
				if (lastl && lastl.link === f.st.link && Math.abs(lastl.y1 - (this.y + lh)) < 0.5 && Math.abs(lastl.x1 + 0.5 - (ox + f.x)) < f.st.sz)
					lastl.x1 = ox + f.x + f.w;
				else
					pg.links.push({ x0: ox + f.x, y0: this.y, x1: ox + f.x + f.w, y1: this.y + lh, link: f.st.link });
			}
		}
		this.y += lh;
	}
};

HtmlFlow.prototype.heading_entry = function (level, title, base)
{
	if (this.scratch || !title)
		return;
	this.b.outline.push({ level: level, title: title, flow: this, page: this.pages.length - 1, y: this.y });
};

/* Lay out a list of sibling nodes as block content in a box (x, w) */
HtmlFlow.prototype.block = function (nodes, st, x, w, base)
{
	var run = [];
	var self = this;
	function flush_run()
	{
		if (!run.length)
			return;
		var has = false;
		for (var i = 0; i < run.length; i++) {
			if (run[i].k != "w" || run[i].s.length)
				has = true;
		}
		if (has)
			self.flush(run, x, w, st.align, st);
		run = [];
	}
	for (var i = 0; i < nodes.length; i++) {
		var n = nodes[i];
		if (n.t == "#text") {
			this.collect([n], st, run, base);
			continue;
		}
		if (html_dropped(this, n))
			continue;
		if (!HTML_BLOCK[n.t] && !html_has_block(n)) {
			this.collect([n], st, run, base);
			continue;
		}
		flush_run();
		this.block_element(n, st, x, w, base);
	}
	flush_run();
};

HtmlFlow.prototype.block_element = function (n, st, x, w, base)
{
	var t = n.t;
	var sz = st.sz;
	var a = n.a || {};
	var key = a.id || (t == "a" ? a.name : null);
	if (key)
		this.b.set_anchor(this, key, base, this.y);
	var i;
	if (t == "h1" || t == "h2" || t == "h3" || t == "h4" || t == "h5" || t == "h6") {
		var lv = parseInt(t.charAt(1), 10);
		var hs = [0, 1.9, 1.55, 1.3, 1.15, 1.0, 0.9][lv] * this.b.opts.fontsize;
		var hst = this.derive(st, { fam: 0, bold: true, sz: hs, align: a.align || st.align });
		this.skip(hs * 0.7);
		this.need(hs * 2.5);
		this.heading_entry(lv, html_node_text(n).replace(/\s+/g, " ").replace(/^ | $/g, ""), base);
		var items = [];
		this.collect(n.c, hst, items, base);
		this.flush(items, x, w, hst.align, hst);
		this.y += hs * 0.3;
		return;
	}
	if (t == "p" || t == "figcaption" || t == "summary" || t == "caption") {
		this.skip(sz * 0.55);
		var pst = a.align ? this.derive(st, { align: a.align }) : st;
		if (t == "figcaption" || t == "caption")
			pst = this.derive(pst, { italic: true, align: "center" });
		if (html_has_block(n))
			this.block(n.c, pst, x, w, base);
		else {
			var pit = [];
			this.collect(n.c, pst, pit, base);
			this.flush(pit, x, w, pst.align, pst);
		}
		this.y += sz * 0.35;
		return;
	}
	if (t == "br") {
		this.y += sz * 1.25;
		return;
	}
	if (t == "hr") {
		this.skip(sz * 0.5);
		this.need(sz);
		this.page().rects.push({ x: x, y: this.y, w: w, h: 1, g: 0.55 });
		this.y += sz * 0.7;
		return;
	}
	if (t == "pre") {
		this.skip(sz * 0.6);
		var prst = this.derive(st, { fam: 2, pre: true, sz: sz * 0.85, align: "left" });
		var pri = [];
		this.collect(n.c, prst, pri, base);
		this.flush(pri, x, w, "left", prst);
		this.y += sz * 0.4;
		return;
	}
	if (t == "blockquote") {
		this.skip(sz * 0.5);
		this.block(n.c, this.derive(st, { italic: st.italic }), x + 28, w - 36, base);
		this.y += sz * 0.4;
		return;
	}
	if (t == "ul" || t == "ol" || t == "menu" || t == "dir") {
		this.skip(sz * 0.4);
		var num = parseInt(a.start, 10) || 1;
		for (i = 0; i < n.c.length; i++) {
			var li = n.c[i];
			if (li.t == "#text" || HTML_DROP[li.t])
				continue;
			if (li.t != "li") {
				this.block([li], st, x + 22, w - 22, base);
				continue;
			}
			var mk = t == "ol" ? (num++) + "." : String.fromCharCode(0x2022);
			var mw = html_text_width(st.fam, false, sz, mk);
			this.need(sz * 2);
			this.marker = { s: mk, w: mw, fam: st.fam, sz: sz };
			if (li.a && li.a.id)
				this.b.set_anchor(this, li.a.id, base, this.y);
			this.block(li.c, st, x + 22, w - 22, base);
			this.marker = null;
			this.y += sz * 0.2;
		}
		this.y += sz * 0.3;
		return;
	}
	if (t == "li") {
		this.marker = { s: String.fromCharCode(0x2022), w: html_text_width(st.fam, false, sz, String.fromCharCode(0x2022)), fam: st.fam, sz: sz };
		this.block(n.c, st, x + 22, w - 22, base);
		this.marker = null;
		return;
	}
	if (t == "dl") {
		this.skip(sz * 0.4);
		this.block(n.c, st, x, w, base);
		return;
	}
	if (t == "dt") {
		this.skip(sz * 0.3);
		this.block(n.c, this.derive(st, { bold: true }), x, w, base);
		return;
	}
	if (t == "dd") {
		this.block(n.c, st, x + 28, w - 28, base);
		return;
	}
	if (t == "center") {
		this.block(n.c, this.derive(st, { align: "center" }), x, w, base);
		return;
	}
	if (t == "form" && this.b.opts.forms) {
		this.skip(sz * 0.3);
		this.block(n.c, this.derive(st, { form: this.b.new_form(n, base) }), x, w, base);
		return;
	}
	if (t == "table") {
		this.table(n, st, x, w, base);
		return;
	}
	if (t == "tr" || t == "td" || t == "th" || t == "thead" || t == "tbody" || t == "tfoot") {
		this.block(n.c, st, x, w, base);
		return;
	}
	/* div, section and other containers (and inline elements that contain blocks) */
	var cst = a.align ? this.derive(st, { align: a.align }) : st;
	if (t == "a" && a.href && this.b.opts.link) {
		var lk = this.b.opts.link(a.href, base);
		if (lk)
			cst = this.derive(cst, { link: lk, ul: true });
	}
	else if (t == "b" || t == "strong")
		cst = this.derive(cst, { bold: true });
	else if (t == "i" || t == "em")
		cst = this.derive(cst, { italic: true });
	this.block(n.c, cst, x, w, base);
};

/* Tables: columns sized by their content; each cell is laid out separately and placed in its row */
HtmlFlow.prototype.table = function (n, st, x, w, base)
{
	var rows = [];
	function find_rows(node)
	{
		for (var i = 0; i < node.c.length; i++) {
			var c = node.c[i];
			if (c.t == "tr") {
				var cells = [];
				for (var j = 0; j < c.c.length; j++) {
					if (c.c[j].t == "td" || c.c[j].t == "th")
						cells.push(c.c[j]);
				}
				if (cells.length)
					rows.push(cells);
			}
			else if (c.t == "thead" || c.t == "tbody" || c.t == "tfoot")
				find_rows(c);
		}
	}
	find_rows(n);
	if (!rows.length) {
		this.block(n.c, st, x, w, base);
		return;
	}
	var ncol = 0, r, c;
	for (r = 0; r < rows.length; r++) {
		var span = 0;
		for (c = 0; c < rows[r].length; c++)
			span += Math.max(1, parseInt(rows[r][c].a.colspan, 10) || 1);
		ncol = Math.max(ncol, span);
	}
	if (ncol > 12)
		ncol = 12;
	var weight = [];
	for (c = 0; c < ncol; c++)
		weight[c] = 4;
	for (r = 0; r < rows.length; r++) {
		var col = 0;
		for (c = 0; c < rows[r].length && col < ncol; c++) {
			var cs = Math.max(1, parseInt(rows[r][c].a.colspan, 10) || 1);
			var len = html_node_text(rows[r][c]).replace(/\s+/g, " ").length;
			if (cs == 1)
				weight[col] = Math.max(weight[col], Math.min(len, 40));
			col += cs;
		}
	}
	var pad = 3, total = 0;
	for (c = 0; c < ncol; c++)
		total += weight[c];
	var avail = w - ncol * pad * 2;
	var cw = [];
	for (c = 0; c < ncol; c++)
		cw[c] = Math.max(18, avail * weight[c] / total);
	var border = n.a && n.a.border && n.a.border != "0";
	this.skip(st.sz * 0.5);
	for (r = 0; r < rows.length; r++) {
		var scratch = [], rowh = 0;
		var colx = x, ci = 0;
		for (c = 0; c < rows[r].length && ci < ncol; c++) {
			var cell = rows[r][c];
			var span2 = Math.max(1, parseInt(cell.a.colspan, 10) || 1);
			var width = 0;
			for (var k = 0; k < span2 && ci + k < ncol; k++)
				width += cw[ci + k] + (k ? pad * 2 : 0);
			var sf = new HtmlFlow(this.b, this.pagew, 1e6, 0, true);
			var cst = this.derive(st, { bold: cell.t == "th" ? true : st.bold, align: cell.a.align || (cell.t == "th" ? "center" : "left") });
			sf.block(cell.c, cst, 0, width, base);
			var ch = sf.y;
			scratch.push({ flow: sf, x: colx + pad, w: width, h: ch });
			rowh = Math.max(rowh, ch);
			colx += width + pad * 2;
			ci += span2;
		}
		this.need(Math.min(rowh + pad * 2, this.pageh - 2 * this.margin));
		var pg = this.page();
		var ytop = this.y;
		for (var si = 0; si < scratch.length; si++) {
			var sc = scratch[si];
			var sp = sc.flow.pages[0];
			var dx = sc.x, dy = ytop + pad;
			var k2;
			for (k2 = 0; k2 < sp.texts.length; k2++) {
				var tt = sp.texts[k2];
				tt.x += dx;
				tt.y += dy;
				pg.texts.push(tt);
			}
			for (k2 = 0; k2 < sp.imgs.length; k2++) {
				var ii = sp.imgs[k2];
				ii.x += dx;
				ii.y += dy;
				pg.imgs.push(ii);
			}
			for (k2 = 0; k2 < sp.rects.length; k2++) {
				var rr = sp.rects[k2];
				rr.x += dx;
				rr.y += dy;
				pg.rects.push(rr);
			}
			for (k2 = 0; k2 < sp.links.length; k2++) {
				var ll = sp.links[k2];
				ll.x0 += dx; ll.x1 += dx; ll.y0 += dy; ll.y1 += dy;
				pg.links.push(ll);
			}
			/* anchors recorded while measuring the cell now live on the real page */
			this.b.rehome_anchors(sc.flow, this, this.pages.length - 1, dy);
			if (border)
				pg.rects.push({ x: sc.x - pad, y: ytop, w: sc.w + pad * 2, h: 0.6, g: 0.5 }),
				pg.rects.push({ x: sc.x - pad, y: ytop + rowh + pad * 2, w: sc.w + pad * 2, h: 0.6, g: 0.5 });
		}
		this.y = ytop + rowh + pad * 2;
	}
	this.y += st.sz * 0.4;
};

/* ---------- The builder: pages, images, outline and PDF output ---------- */

function HtmlBuilder(opts)
{
	this.opts = opts || {};
	var o = this.opts;
	o.pagew = o.pagew || 480;
	o.pageh = o.pageh || 660;
	o.margin = o.margin || 30;
	o.fontsize = o.fontsize || 12;
	this.flow = new HtmlFlow(this, o.pagew, o.pageh, o.margin, false);
	this.anchors = {};
	this.outline = [];
	this.forms = [];		/* forms on the page (only when opts.forms is on), and their fields */
	this.fields = [];
	this.images = [];
	this.imgbytes = 0;
	this.imgcache = {};
	this.titles = [];
	this.first = true;
}

HtmlBuilder.prototype.set_anchor = function (flow, id, base, y)
{
	var key = this.opts.anchor_key ? this.opts.anchor_key(base, id) : id;
	if (this.anchors[key] === undefined)
		this.anchors[key] = { flow: flow, page: flow.pages.length - 1, y: y };
};

HtmlBuilder.prototype.rehome_anchors = function (from, to, page, dy)
{
	for (var k in this.anchors) {
		var a = this.anchors[k];
		if (a.flow === from) {
			a.flow = to;
			a.page = page;
			a.y += dy;
		}
	}
	for (var i = 0; i < this.outline.length; i++) {
		var o = this.outline[i];
		if (o.flow === from) {
			o.flow = to;
			o.page = page;
			o.y += dy;
		}
	}
};

/* ----- forms -----
 * With opts.forms on, <form> controls are drawn as text in brackets that can be selected like links.  A control's
 * address is "sixelform:<number>"; the number indexes this.fields.  opts.form_state holds what the user has entered
 * (by field key), so that the page can be laid out again showing it. */
var FORM_PLAIN_TYPES = { hidden: 1, checkbox: 1, radio: 1, submit: 1, image: 1, reset: 1, file: 1, button: 1, select: 1, textarea: 1 };

function form_is_text(f)
{
	return f.type == "textarea" || !FORM_PLAIN_TYPES[f.type];
}

HtmlBuilder.prototype.new_form = function (n, base)
{
	var a = n.a || {};
	var form = { id: this.forms.length, action: String(a.action || ""), method: String(a.method || "get").toLowerCase() == "post" ? "POST" : "GET",
		base: base, fields: [] };
	this.forms.push(form);
	return form;
};

/* The words drawn for a control */
HtmlBuilder.prototype.field_text = function (f)
{
	var w = Math.max(6, Math.min(30, parseInt(f.size, 10) || 20)), v, i;
	switch (f.type) {
		case "checkbox": return f.checked ? "[x]" : "[ ]";
		case "radio": return f.checked ? "(*)" : "( )";
		case "submit": case "button": case "image": return "[ " + f.label + " ]";
		case "file": return "[file upload not supported]";
		case "select": return "[" + String(f.options.length ? f.options[f.sel].label : "").substr(0, 24) + " v]";
	}
	v = String(f.value).replace(/[\r\n]+/g, " ");
	if (!v.length && f.placeholder.length && f.type != "password")
		v = f.placeholder;		/* an empty box shows its hint */
	if (f.type == "password")
		v = new Array(v.length + 1).join("*");
	if (v.length > w)
		v = v.substr(0, w - 1) + ">";
	while (v.length < w)
		v += "_";
	return "[" + v + "]";
};

/* A control inside a form: remembers it and returns the item to draw (null for hidden ones) */
HtmlBuilder.prototype.form_item = function (n, st, base)
{
	var a = n.a || {}, form = st.form, t = n.t, type, f, i;
	if (t == "input")
		type = String(a.type || "text").toLowerCase();
	else if (t == "textarea" || t == "select")
		type = t;
	else
		type = String(a.type || "submit").toLowerCase();
	if (type == "reset" || (t == "button" && type == "button") || form.fields.length >= 100 || this.fields.length >= 400)
		return null;
	f = { id: this.fields.length, form: form, type: type, name: String(a.name || "").substr(0, 100), size: a.size,
		maxlength: Math.min(FORM_MAX_VALUE, parseInt(a.maxlength, 10) > 0 ? parseInt(a.maxlength, 10) : FORM_MAX_VALUE),
		disabled: a.disabled !== undefined, readonly: a.readonly !== undefined, value: "", checked: false, options: [], sel: 0, label: "" };
	f.key = form.id + "." + form.fields.length;
	f.placeholder = String(a.placeholder || "").replace(/[\x00-\x1f]/g, " ").substr(0, 40);
	f.prompt = String(a.placeholder || a.title || a["aria-label"] || a.name || "Text").replace(/[\x00-\x1f]/g, " ").substr(0, 30);
	var sv = this.opts.form_state ? this.opts.form_state[f.key] : undefined;
	if (type == "select") {
		var stack = [n.c || []], opt;
		while (stack.length && f.options.length < 200) {
			var list = stack.shift();
			for (i = 0; i < list.length; i++) {
				if (list[i].t == "option") {
					var txt = "";
					for (var ci = 0; ci < (list[i].c || []).length; ci++)
						if (list[i].c[ci].t == "#text")
							txt += list[i].c[ci].s;
					txt = txt.replace(/\s+/g, " ").replace(/^ | $/g, "");
					opt = { label: txt, value: list[i].a && list[i].a.value !== undefined ? String(list[i].a.value) : txt };
					if (list[i].a && list[i].a.selected !== undefined && !f.sel_set) {
						f.sel = f.options.length;
						f.sel_set = true;
					}
					f.options.push(opt);
				}
				else if (list[i].c)
					stack.push(list[i].c);
			}
		}
		if (sv !== undefined && sv >= 0 && sv < f.options.length)
			f.sel = sv;
	}
	else if (type == "textarea") {
		var tx = "";
		for (i = 0; i < (n.c || []).length; i++)
			if (n.c[i].t == "#text")
				tx += n.c[i].s;
		f.value = sv !== undefined ? sv : tx.replace(/^\r?\n/, "");
	}
	else if (type == "checkbox" || type == "radio") {
		f.value = a.value !== undefined ? String(a.value) : "on";
		f.checked = sv !== undefined ? !!sv : a.checked !== undefined;
	}
	else if (type == "submit" || type == "button" || type == "image") {
		f.value = a.value !== undefined ? String(a.value) : "";
		var bt = t == "button" ? html_node_text(n).replace(/\s+/g, " ").replace(/^ | $/g, "") : "";
		f.label = (type == "image" ? a.alt || a.value : (a.value || bt)) || (type == "image" ? "Go" : "Submit");
		f.label = String(f.label).substr(0, 30);
	}
	else
		f.value = sv !== undefined ? sv : (a.value !== undefined ? String(a.value) : "");
	form.fields.push(f);
	this.fields.push(f);
	if (type == "hidden")
		return null;
	var text = this.field_text(f);
	var lk = (f.disabled || type == "file" || (f.readonly && form_is_text(f))) ? null : { uri: "sixelform:" + f.id, field: f };
	return { k: "w", s: text, st: this.flow.derive(st, { link: lk, ul: false, form: null }), sp: true };
};

/* Find (or load) an image for an <img> element; returns { dw, dh, id } or null */
HtmlBuilder.prototype.get_image = function (attrs, base)
{
	if (this.opts.images === false || !attrs.src || !this.opts.resolve_image)
		return null;
	var key = (base || "") + "\n" + attrs.src;
	var rec = this.imgcache[key];
	if (rec === undefined) {
		rec = null;
		if (this.images.length < 4000 && this.imgbytes < 128 * 1048576) {
			try {
				var im = this.opts.resolve_image(attrs.src, base);
				if (im && im.w > 0 && im.h > 0) {
					rec = { im: im, id: this.images.length };
					this.images.push(rec);
					this.imgbytes += im.jpeg ? im.jpeg.length : (im.png ? im.png.data.length : (im.indexed ? im.indexed.data.length : im.w * im.h * 3));
				}
			}
			catch (e) {
				rec = null;
			}
		}
		this.imgcache[key] = rec;
	}
	if (!rec)
		return null;
	/* displayed size: the width/height attributes if present, else the natural size at a sensible scale */
	var w = parseInt(attrs.width, 10), h = parseInt(attrs.height, 10);
	var nw = rec.im.w, nh = rec.im.h;
	var dw, dh;
	if (w > 0 && h > 0) { dw = w; dh = h; }
	else if (w > 0) { dw = w; dh = nh * w / nw; }
	else if (h > 0) { dh = h; dw = nw * h / nh; }
	else { dw = nw; dh = nh; }
	/* pictures are drawn at 0.75 pt per pixel so that a 640 pixel picture fills most of a page */
	dw *= 0.75;
	dh *= 0.75;
	return { dw: Math.max(1, dw), dh: Math.max(1, dh), id: rec.id };
};

/* Lay out an HTML document; base is passed to the resolve callbacks.  Returns the document's title. */
/* The element with the given id, or null */
function html_find_id(node, id)
{
	if (node.a && node.a.id === id)
		return node;
	for (var i = 0; node.c && i < node.c.length; i++) {
		var r = html_find_id(node.c[i], id);
		if (r)
			return r;
	}
	return null;
}

HtmlBuilder.prototype.add_html = function (source, base, newpage, only)
{
	var doc = html_parse(source);
	var fl = this.flow;
	if (!this.first && newpage !== false) {
		if (fl.y > fl.margin + 0.5)
			fl.new_page();
	}
	this.first = false;
	var st = { fam: 1, bold: false, italic: false, sz: this.opts.fontsize, link: null, ul: false, pre: false, align: "left" };
	if (this.opts.anchor_key)
		this.set_anchor(fl, "", base, fl.y);		/* the start of this document */
	var body = doc.root;
	/* only: ids of the elements to show instead of the whole page (a site's menus and side bars are left out); if none
	 * of them is on the page, the whole page is shown */
	var parts = [];
	for (var oi = 0; only && oi < only.length; oi++) {
		var on = html_find_id(body, only[oi]);
		if (on)
			parts.push(on);
	}
	fl.block(parts.length ? parts : body.c, st, this.opts.margin, this.opts.pagew - 2 * this.opts.margin, base);
	if (doc.title)
		this.titles.push(doc.title);
	return doc.title;
};

function html_pdf_str(s)
{
	var out = "";
	for (var i = 0; i < s.length; i++) {
		var b = html_winansi(s.charAt(i));
		if (b < 0)
			continue;
		if (b == 40 || b == 41 || b == 92)
			out += "\\" + String.fromCharCode(b);
		else if (b < 32 || b > 126)
			out += "\\" + ("00" + b.toString(8)).slice(-3);
		else
			out += String.fromCharCode(b);
	}
	return out;
}

function html_n(v)
{
	return (Math.round(v * 100) / 100).toString();
}

function html_bytes_string(a)
{
	var parts = [];
	for (var i = 0; i < a.length; i += 4096)
		parts.push(String.fromCharCode.apply(null, Array.prototype.slice.call(a.subarray ? a.subarray(i, i + 4096) : a.slice(i, i + 4096))));
	return parts.join("");
}

/* Write the document as a PDF.  toc: optional [{ level, title, key }] replacing the headings as the outline.
 * Returns the number of pages. */
HtmlBuilder.prototype.write = function (path, toc)
{
	var o = this.opts;
	var H = o.pageh;
	var flow = this.flow;
	var pages = flow.pages;
	/* drop a trailing empty page */
	while (pages.length > 1 && !pages[pages.length - 1].texts.length && !pages[pages.length - 1].imgs.length &&
		!pages[pages.length - 1].rects.length)
		pages.pop();
	var objs = [];		/* index = object number - 1 */
	function reserve() { objs.push(null); return objs.length; }
	var cat = reserve(), pagesroot = reserve(), outroot = reserve();
	var fontobj = [];
	var i, k;
	for (i = 0; i < HTML_FONTS.length; i++) {
		var fo = reserve();
		fontobj.push(fo);
		objs[fo - 1] = "<< /Type /Font /Subtype /Type1 /BaseFont /" + HTML_FONTS[i][0] + " /Encoding /WinAnsiEncoding >>";
	}
	var imgobj = [];
	for (i = 0; i < this.images.length; i++) {
		var io = reserve();
		imgobj.push(io);
		var im = this.images[i].im;
		if (im.png) {
			var pn = im.png, pc = pn.ct == 2 ? 3 : 1, pcs = "/DeviceGray";
			if (pn.ct == 2)
				pcs = "/DeviceRGB";
			else if (pn.ct == 3) {
				var ph = "";
				for (k = 0; k < pn.plte.length; k++)
					ph += ("0" + pn.plte.charCodeAt(k).toString(16)).slice(-2);
				pcs = "[/Indexed /DeviceRGB " + (pn.plte.length / 3 - 1) + " <" + ph + ">]";
			}
			objs[io - 1] = { dict: "<< /Type /XObject /Subtype /Image /Width " + im.w + " /Height " + im.h + " /ColorSpace " + pcs +
				" /BitsPerComponent " + pn.depth + " /Filter /FlateDecode /DecodeParms << /Predictor 15 /Colors " + pc + " /BitsPerComponent " +
				pn.depth + " /Columns " + im.w + " >>", data: pn.data };
		}
		else if (im.indexed) {
			var xh = "";
			for (k = 0; k < im.indexed.pal.length; k++)
				xh += ("0" + im.indexed.pal.charCodeAt(k).toString(16)).slice(-2);
			objs[io - 1] = { dict: "<< /Type /XObject /Subtype /Image /Width " + im.w + " /Height " + im.h + " /ColorSpace [/Indexed /DeviceRGB " +
				(im.indexed.pal.length / 3 - 1) + " <" + xh + ">] /BitsPerComponent 8 /Filter /RunLengthDecode", data: im.indexed.data };
		}
		else if (im.jpeg) {
			objs[io - 1] = { dict: "<< /Type /XObject /Subtype /Image /Width " + im.w + " /Height " + im.h +
				" /ColorSpace /" + (im.comps == 1 ? "DeviceGray" : "DeviceRGB") + " /BitsPerComponent 8 /Filter /DCTDecode", data: im.jpeg };
		}
		else {
			var rgb = new Uint8Array(im.w * im.h * 3);
			var d = im.data, al = im.alpha;
			for (k = 0; k < im.w * im.h; k++) {
				var a = al ? al[k] / 255 : 1;
				for (var c = 0; c < 3; c++) {
					var v = d[k * 3 + c];
					rgb[k * 3 + c] = a >= 1 ? v : Math.round(v * a + 255 * (1 - a));
				}
			}
			objs[io - 1] = { dict: "<< /Type /XObject /Subtype /Image /Width " + im.w + " /Height " + im.h +
				" /ColorSpace /DeviceRGB /BitsPerComponent 8", data: html_bytes_string(rgb) };
		}
	}
	var pageobj = [], contentobj = [];
	for (i = 0; i < pages.length; i++) {
		pageobj.push(reserve());
		contentobj.push(reserve());
	}
	/* anchors -> destinations */
	var self = this;
	function dest_of(key)
	{
		var a = self.anchors[key];
		if (!a)
			return null;
		var pg = a.flow === flow ? a.page : 0;
		if (pg >= pages.length)
			pg = pages.length - 1;
		return "[" + pageobj[pg] + " 0 R /XYZ 0 " + html_n(H - Math.max(0, a.y - 4)) + " 0]";
	}
	var annotobj = [];
	for (i = 0; i < pages.length; i++) {
		var pg = pages[i], an = [];
		for (k = 0; k < pg.links.length; k++) {
			var L = pg.links[k], lk = L.link, dict = null;
			var rect = "[" + html_n(L.x0) + " " + html_n(H - L.y1) + " " + html_n(L.x1) + " " + html_n(H - L.y0) + "]";
			if (lk.uri)
				dict = "/A << /S /URI /URI (" + html_pdf_str(lk.uri) + ") >>";
			else if (lk.anchor !== undefined) {
				var ds = dest_of(lk.anchor);
				if (ds)
					dict = "/Dest " + ds;
			}
			if (!dict)
				continue;
			var ao = reserve();
			objs[ao - 1] = "<< /Type /Annot /Subtype /Link /Rect " + rect + " /Border [0 0 0] " + dict + " >>";
			an.push(ao);
		}
		annotobj.push(an);
	}
	/* outline */
	var ol = [];
	if (toc) {
		for (i = 0; i < toc.length; i++) {
			var ds2 = dest_of(toc[i].key);
			if (ds2)
				ol.push({ level: toc[i].level, title: toc[i].title, dest: ds2 });
		}
	}
	else {
		for (i = 0; i < this.outline.length; i++) {
			var oe = this.outline[i];
			if (oe.level > 4)
				continue;
			var pgi = oe.flow === flow ? oe.page : 0;
			ol.push({ level: oe.level, title: oe.title, dest: "[" + pageobj[Math.min(pgi, pages.length - 1)] + " 0 R /XYZ 0 " + html_n(H - Math.max(0, oe.y - 6)) + " 0]" });
		}
	}
	/* build the outline tree (levels are normalised so that no level is skipped) */
	var tree = { kids: [] }, stack2 = [{ level: 0, node: tree }];
	for (i = 0; i < ol.length; i++) {
		var e = ol[i];
		var lvl = Math.max(1, e.level);
		while (stack2.length > 1 && stack2[stack2.length - 1].level >= lvl)
			stack2.pop();
		var parent = stack2[stack2.length - 1].node;
		var node = { obj: reserve(), title: e.title, dest: e.dest, kids: [], parent: parent };
		parent.kids.push(node);
		stack2.push({ level: lvl, node: node });
	}
	function emit_outline(node, selfobj)
	{
		for (var j = 0; j < node.kids.length; j++) {
			var kid = node.kids[j];
			var s = "<< /Title (" + html_pdf_str(kid.title) + ") /Parent " + selfobj + " 0 R /Dest " + kid.dest;
			if (j > 0)
				s += " /Prev " + node.kids[j - 1].obj + " 0 R";
			if (j < node.kids.length - 1)
				s += " /Next " + node.kids[j + 1].obj + " 0 R";
			if (kid.kids.length)
				s += " /First " + kid.kids[0].obj + " 0 R /Last " + kid.kids[kid.kids.length - 1].obj + " 0 R /Count " + kid.kids.length;
			objs[kid.obj - 1] = s + " >>";
			emit_outline(kid, kid.obj);
		}
	}
	emit_outline(tree, outroot);
	objs[outroot - 1] = tree.kids.length ? "<< /Type /Outlines /First " + tree.kids[0].obj + " 0 R /Last " + tree.kids[tree.kids.length - 1].obj +
		" 0 R /Count " + tree.kids.length + " >>" : "<< /Type /Outlines /Count 0 >>";
	objs[cat - 1] = "<< /Type /Catalog /Pages " + pagesroot + " 0 R" + (tree.kids.length ? " /Outlines " + outroot + " 0 R /PageMode /UseOutlines" : "") + " >>";
	var kids = [];
	for (i = 0; i < pages.length; i++)
		kids.push(pageobj[i] + " 0 R");
	objs[pagesroot - 1] = "<< /Type /Pages /Kids [" + kids.join(" ") + "] /Count " + pages.length + " >>";
	var fontres = "";
	for (i = 0; i < fontobj.length; i++)
		fontres += "/F" + i + " " + fontobj[i] + " 0 R ";
	/* page content */
	for (i = 0; i < pages.length; i++) {
		var p = pages[i], cs = [];
		for (k = 0; k < p.rects.length; k++) {
			var r = p.rects[k];
			cs.push(html_n(r.g) + " g " + html_n(r.x) + " " + html_n(H - r.y - r.h) + " " + html_n(r.w) + " " + html_n(r.h) + " re f");
		}
		var used = {};
		for (k = 0; k < p.imgs.length; k++) {
			var m = p.imgs[k];
			used[m.img.id] = true;
			cs.push("q " + html_n(m.w) + " 0 0 " + html_n(m.h) + " " + html_n(m.x) + " " + html_n(H - m.y - m.h) + " cm /Im" + m.img.id + " Do Q");
		}
		var curcolor = "";
		for (k = 0; k < p.texts.length; k++) {
			var t = p.texts[k];
			var col = t.link ? "0 0 0.75 rg 0 0 0.75 RG" : "0 g 0 G";
			if (col != curcolor) {
				cs.push(col);
				curcolor = col;
			}
			cs.push("BT /F" + t.f + " " + html_n(t.sz) + " Tf " + html_n(t.x) + " " + html_n(H - t.y) + " Td (" + html_pdf_str(t.s) + ") Tj ET");
			if (t.ul)
				cs.push("0.5 w " + html_n(t.x) + " " + html_n(H - t.y - t.sz * 0.12) + " m " + html_n(t.x + t.w) + " " + html_n(H - t.y - t.sz * 0.12) + " l S");
		}
		var xobj = "";
		for (var uid in used)
			xobj += "/Im" + uid + " " + imgobj[uid] + " 0 R ";
		var stream = cs.join("\n");
		objs[contentobj[i] - 1] = { dict: "<<", data: stream, plain: true };
		var annots = annotobj[i].length ? " /Annots [" + annotobj[i].map(function (q) { return q + " 0 R"; }).join(" ") + "]" : "";
		objs[pageobj[i] - 1] = "<< /Type /Page /Parent " + pagesroot + " 0 R /MediaBox [0 0 " + o.pagew + " " + H + "] /Contents " + contentobj[i] +
			" 0 R /Resources << /Font << " + fontres + ">> /XObject << " + xobj + ">> >>" + annots + " >>";
	}
	/* serialise */
	var out = ["%PDF-1.4\n"], offs = [], pos = out[0].length;
	for (i = 0; i < objs.length; i++) {
		var ob = objs[i];
		if (ob === null)
			ob = "null";
		offs.push(pos);
		var txt;
		if (typeof ob == "object") {
			if (ob.plain)
				txt = (i + 1) + " 0 obj\n<< /Length " + ob.data.length + " >>\nstream\n" + ob.data + "\nendstream\nendobj\n";
			else
				txt = (i + 1) + " 0 obj\n" + ob.dict + " /Length " + ob.data.length + " >>\nstream\n" + ob.data + "\nendstream\nendobj\n";
		}
		else
			txt = (i + 1) + " 0 obj\n" + ob + "\nendobj\n";
		out.push(txt);
		pos += txt.length;
	}
	var xref = "xref\n0 " + (objs.length + 1) + "\n0000000000 65535 f \n";
	for (i = 0; i < offs.length; i++)
		xref += ("0000000000" + offs[i]).slice(-10) + " 00000 n \n";
	out.push(xref + "trailer\n<< /Size " + (objs.length + 1) + " /Root " + cat + " 0 R >>\nstartxref\n" + pos + "\n%%EOF\n");
	var f = new File(path);
	if (!f.open("wb"))
		throw "Unable to write " + path;
	f.write(out.join(""));
	f.close();
	return pages.length;
};

/* ---------- URLs ---------- */

var IMAGE_TYPES = ["pdf", "png", "jpg", "jpeg", "gif", "bmp", "ppm", "pgm", "six", "svg", "pcx", "tga", "ico", "tif", "tiff",
	"iff", "lbm", "ilbm", "xb", "xbin", "bin"];
var TEXT_TYPES = ["ans", "ansi", "asc", "txt", "msg", "diz", "nfo", "bbs", "md", "log", "cfg", "ini", "conf", "1st"];
var ARCHIVE_TYPES = ["zip", "cbz", "rar", "cbr"];		/* extracted on demand into a directory of the same name */
var HTML_TYPES = ["html", "htm", "xhtml", "epub"];	/* laid out as pages and shown like a PDF */
var SUPPORTED_TYPES = IMAGE_TYPES.concat(TEXT_TYPES, ARCHIVE_TYPES, HTML_TYPES, MUSIC_TYPES);	/* MUSIC_TYPES: see univiewer_music_lib.js */

/* Output speeds for text/ANSI files; index 0 = unlimited (the index is also what SyncTERM is sent) */
var SPEEDS = [0, 300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 76800, 115200];
var opt = { speed: 0, pause: false, zip_max_bytes: 50 * 1048576, zip_max_files: 2000,
	image_max_bytes: 3 * 1048576, cache_max_days: 14, cache_max_bytes: 100 * 1048576, error_mail: "" };
var syncterm;

var BASE_DIR = (function () {
	/* absolute, with forward slashes: the HTML/EPUB code compares and joins paths */
	var d = String(js.exec_dir);
	try {
		d = fullpath(d);
	}
	catch (e) {}
	d = d.replace(/\\/g, "/");
	return d.charAt(d.length - 1) == "/" ? d : d + "/";
})();

/* Fallback colors/text, used for anything the theme file does not set */
var DEFAULT_THEME = {
	headerTextColor: "n7r",
	headerNameColor: "b",
	colHeaderColor: "nhw",
	listNumColor: "nhy",
	listNameColor: "nc",
	listDescColor: "nc",
	listSizeColor: "nc",
	listDateColor: "hb",
	listDirColor: "nhy",
	listHighlightBkgColor: "4",
	listNumHighlightColor: "hy",
	listNameHighlightColor: "hc",
	listDescHighlightColor: "hc",
	listSizeHighlightColor: "hw",
	listDateHighlightColor: "hw",
	listDirHighlightColor: "hy",
	helpLineBkgColor: "7",
	helpLineGeneralColor: "b",
	helpLineHotkeyColor: "r",
	helpLineParenColor: "m",
	textBodyColor: "nw",
	scrollbarBGColor: "nhk",
	scrollbarBGChar: "\\xb0",
	scrollbarScrollBlockColor: "nhw",
	scrollbarScrollBlockChar: "\\xdb",
	titleText: "\\x01n\\x01h\\x01wUniversal Viewer Library",
	noLibrariesText: "\\x01n\\x01h\\x01yThere are no libraries available.",
	emptyLibraryText: "\\x01n\\x01h\\x01yThere are no files to display here.",
	errorText: "\\x01n\\x01h\\x01rUnable to display file: \\x01w%s",
	loadingText: "\\x01n\\x01h\\x01cLoading...",
	hookErrorText: "\\x01n\\x01h\\x01rUnable to get the list from the server."
};

var theme = {};

/* ---------- Helpers ---------- */

function is_absolute(path)
{
	return /^([a-zA-Z]:)?[\\\/]/.test(path);
}

function resolve_path(path)
{
	return is_absolute(path) ? path : BASE_DIR + path;
}

/* Comma-separated wildcard patterns (* and ?, case-insensitive) -> array of RegExp.
 * A bare extension such as "pdf" or ".pdf" is accepted as *.pdf */
/* Host-name patterns (* and ?) for the sites a web library may use; the default is the host of its url */
function host_patterns(str, url)
{
	var list = str ? String(str).split(/[\s,;]+/) : [];
	if (!list.length && url) {
		var h = host_of(String(url));
		if (h)
			list = [h];
	}
	var out = [];
	for (var i = 0; i < list.length; i++) {
		if (!list[i].length)
			continue;
		var re = list[i].replace(/[\\^$.+()\[\]{}|\/]/g, "\\$&").replace(/\*/g, ".*").replace(/\?/g, ".");
		out.push(new RegExp("^" + re + "$", "i"));
	}
	return out;
}

function host_of(url)
{
	var m = /^[a-zA-Z][a-zA-Z0-9+.-]*:\/\/(?:[^@\/]*@)?([^:\/?#]+)/.exec(url);
	return m ? m[1].toLowerCase() : "";
}

function split_list(str)
{
	var out = [];
	if (!str)
		return out;
	var parts = String(str).split(/\s*[,;]\s*|\s+/);
	for (var i = 0; i < parts.length; i++) {
		var p = parts[i];
		if (!p.length)
			continue;
		/* file names are raw bytes (CP437 on DOS-era archives), so UTF-8 characters typed in settings.ini
		 * (a box-drawing line, say) are converted to CP437, and \xHH writes any byte */
		if (/[\x80-\xff]/.test(p)) {
			var cp = utf8_decode(p);
			if (cp)
				p = cp;
		}
		p = p.replace(/\\x([0-9a-fA-F]{2})/g, function (m, h) { return String.fromCharCode(parseInt(h, 16)); });
		/* a pattern ending in a slash (such as old/) matches directories only; the others match files only */
		var isdir = /\/$/.test(p);
		if (isdir)
			p = p.replace(/\/+$/, "");
		else if (!/[*?]/.test(p))
			p = "*." + p.replace(/^\./, "");
		if (!p.length)
			continue;
		var re = p.replace(/[\\^$.+()\[\]{}|\/]/g, "\\$&").replace(/\*/g, ".*").replace(/\?/g, ".");
		var rx = new RegExp("^" + re + "$", "i");
		rx.isdir = isdir;
		out.push(rx);
	}
	return out;
}

/* Does name match a pattern in the list?  Patterns ending in / are for directories (isdir) and the others for files */
function matches_any(list, name, isdir)
{
	for (var i = 0; i < list.length; i++) {
		if (!!list[i].isdir == !!isdir && list[i].test(name))
			return true;
	}
	return false;
}

/* Does the list have any patterns for directories (isdir) or for files? */
function has_patterns(list, isdir)
{
	for (var i = 0; i < list.length; i++) {
		if (!!list[i].isdir == !!isdir)
			return true;
	}
	return false;
}

/* Turn color codes ("nhy", "n7r") into Ctrl-A attribute characters */
function attr(codes)
{
	var s = "";
	codes = String(codes || "");
	for (var i = 0; i < codes.length; i++)
		s += "\x01" + codes.charAt(i);
	return s;
}

/* Expand \xNN escapes in theme values */
function unescape_text(str)
{
	return String(str).replace(/\\x([0-9a-fA-F]{2})/g, function (m, h) {
		return ascii(parseInt(h, 16));
	});
}

function T(name)
{
	return theme[name];
}

function C(name)
{
	return attr(theme[name]);
}

/* Highlighted color: the highlight foreground with the highlight background */
function CH(name)
{
	return attr(theme[name]) + "\x01" + theme.listHighlightBkgColor.charAt(0);
}

function fit(str, width)
{
	str = String(str);
	if (str.length > width)
		return str.substr(0, width);
	while (str.length < width)
		str += " ";
	return str;
}

function rjust(str, width)
{
	str = String(str);
	if (str.length > width)
		return str.substr(0, width);
	while (str.length < width)
		str = " " + str;
	return str;
}

function format_size(n)
{
	if (n >= 1048576)
		return (n / 1048576).toFixed(1) + "M";
	if (n >= 1024)
		return Math.round(n / 1024) + "K";
	return String(n);
}

function format_date(t)
{
	return strftime("%Y-%m-%d", t);
}

function file_ext(name)
{
	var i = name.lastIndexOf(".");
	return i < 0 ? "" : name.substr(i + 1).toLowerCase();
}

function pause_error(msg)
{
	console.print("\r\n" + msg + "\x01n\r\n");
	console.pause();
}

/* ---------- Configuration ---------- */

function read_theme(filename)
{
	var key;
	for (key in DEFAULT_THEME)
		theme[key] = DEFAULT_THEME[key];
	if (!filename)
		return;
	var f = new File(resolve_path(filename));
	if (!f.open("r")) {
		log(LOG_WARNING, "univiewerlibrary: unable to open theme " + f.name);
		return;
	}
	var obj = f.iniGetObject();
	f.close();
	for (key in obj) {
		if (DEFAULT_THEME.hasOwnProperty(key))
			theme[key] = String(obj[key]);
	}
}

/* The settings file is settings.ini unless the first command-line argument names another .ini
   file (absolute, or relative to this directory), so each menu entry can use its own set of
   libraries: ?univiewerlibrary.js music.ini */
function settings_path()
{
	var arg = (typeof argv !== "undefined" && argv.length) ? String(argv[0]) : "";
	if (!arg)
		return BASE_DIR + "settings.ini";
	if (!/\.ini$/i.test(arg))
		arg += ".ini";
	if (!/^([\/\\]|[A-Za-z]:)/.test(arg))
		arg = BASE_DIR + arg;
	return arg;
}

function read_settings()
{
	var f = new File(settings_path());
	if (!f.open("r")) {
		log(LOG_ERR, "univiewerlibrary: unable to open " + f.name);
		return null;
	}
	var root = f.iniGetObject() || {};
	var libraries = [];
	var sections = f.iniGetSections("library:");
	for (var i = 0; i < sections.length; i++) {
		var o = f.iniGetObject(sections[i]);
		if (!o || !(o.dir || o.module || o.url))
			continue;
		var sort = String(o.sort || "name").toLowerCase();
		var lib = {
			name: sections[i].substr("library:".length),
			desc: o.desc || "",
			dir: o.dir ? backslash(resolve_path(String(o.dir))) : "",
			module: o.module ? String(o.module) : "",
			web: !o.dir && !o.module && !!o.url,
			url: o.url ? String(o.url) : "",
			allow: host_patterns(o.allow, o.url),
			images: o.images === undefined ? true : (o.images === true || /^(true|yes|1)$/i.test(String(o.images))),
			content: o.content ? String(o.content).split(/[\s,;]+/).filter(function (x) { return /^[A-Za-z0-9_:.-]+$/.test(x); }) : [],	/* ids of the page elements to show */
			forms: o.forms === true || /^(true|yes|1)$/i.test(String(o.forms)),		/* forms on web pages can be filled in and sent */
			max_page: (Number(o.max_page_kb) > 0 ? Number(o.max_page_kb) : 1024) * 1024,
			max_image: (Number(o.max_image_kb) > 0 ? Number(o.max_image_kb) : 3072) * 1024,
			options: o,
			ars: o.ars ? String(o.ars) : "",
			include: split_list(o.include),
			exclude: split_list(o.exclude).concat(split_list(o.hide)),	/* hide= is ansiview's name for exclude= */
			subdirs: !o.module && (o.subdirs === true || /^(true|yes|1)$/i.test(String(o.subdirs))),
			reverse: sort.charAt(0) == "-",
			sort: sort.replace(/^-/, "")
		};
		if (["name", "date", "size"].indexOf(lib.sort) < 0)
			lib.sort = "name";
		libraries.push(lib);
	}
	f.close();
	if (root.pause === true || /^(true|yes|1)$/i.test(String(root.pause)))
		opt.pause = true;
	if (SPEEDS.indexOf(Number(root.speed)) > 0)
		opt.speed = SPEEDS.indexOf(Number(root.speed));
	if (Number(root.zip_max_mb) > 0)
		opt.zip_max_bytes = Number(root.zip_max_mb) * 1048576;
	if (Number(root.zip_max_files) > 0)
		opt.zip_max_files = Number(root.zip_max_files);
	if (root.error_mail !== undefined)
		opt.error_mail = String(root.error_mail).replace(/^\s+|\s+$/g, "");
	/* extra file types to treat as plain text, e.g. text_types=conf,sh,readme */
	var extra = String(root.text_types || "").toLowerCase().split(/[\s,;]+/);
	for (var ti = 0; ti < extra.length; ti++) {
		var te = extra[ti].replace(/^[.*]+/, "");
		if (te.length && /^[a-z0-9_+-]+$/.test(te) && SUPPORTED_TYPES.indexOf(te) < 0) {
			TEXT_TYPES.push(te);
			SUPPORTED_TYPES.push(te);
		}
	}
	if (Number(root.image_max_mb) > 0)
		opt.image_max_bytes = Number(root.image_max_mb) * 1048576;
	if (root.cache_max_days !== undefined && Number(root.cache_max_days) >= 0)
		opt.cache_max_days = Number(root.cache_max_days);
	if (root.cache_max_mb !== undefined && Number(root.cache_max_mb) >= 0)
		opt.cache_max_bytes = Number(root.cache_max_mb) * 1048576;
	/* music_kbps=128 sends songs bigger than that as a smaller Opus copy made with ffmpeg (0 = as they are) */
	if (Number(root.music_kbps) > 0)
		opt.music_kbps = Number(root.music_kbps);
	if (root.ffmpeg)		/* full path of ffmpeg; otherwise the exec directory, then the path */
		opt.ffmpeg = String(root.ffmpeg).replace(/^\s+|\s+$/g, "");
	return { theme: root.theme, libraries: libraries };
}

/* Is a file of this type shown in this library? */
function type_allowed(lib, name)
{
	var ext = file_ext(name);
	/* any other extension (or none, or .012) is treated as a text file; use exclude= to hide things */
	if (ARCHIVE_TYPES.indexOf(ext) >= 0 && !lib.subdirs)	/* extracted archives are browsed as sub-directories */
		return false;
	if (has_patterns(lib.include, false) && !matches_any(lib.include, name, false))
		return false;
	if (matches_any(lib.exclude, name, false))
		return false;
	return true;
}

/* Is a directory shown in this library?  (include and exclude patterns that end in /) */
function dir_allowed(lib, name)
{
	if (has_patterns(lib.include, true) && !matches_any(lib.include, name, true))
		return false;
	return !matches_any(lib.exclude, name, true);
}

function library_allowed(lib)
{
	return !lib.ars.length || bbs.compare_ars(lib.ars);
}

/* ---------- Hooks (online archives) ---------- */

/*
 * A library with module=<file> instead of dir= gets its listing from a hook in the hooks
 * directory.  See hooks/sixteencolors.js for the interface.  Hook files and downloads are
 * cached under .cache/<module>/ (cache_ttl seconds for listings, default 86400).
 */
function open_hook(lib)
{
	if (lib.hook !== undefined)
		return lib.hook;
	lib.hook = null;
	var modname = lib.module.replace(/[^A-Za-z0-9_.\-]/g, "");
	var cache_dir = BASE_DIR + ".cache/" + modname.replace(/\.js$/, "") + "/";
	var ttl = Number(lib.options.cachettl) >= 0 && lib.options.cachettl !== undefined ? Number(lib.options.cachettl) : 86400;

	function safe_name(n)
	{
		return String(n).replace(/[^A-Za-z0-9._\-\/]/g, "_").replace(/\.\.+/g, "_").replace(/^\/+/, "");
	}
	function write_file(path, data)
	{
		mkpath(path.replace(/[^\/]*$/, ""));
		var f = new File(path);
		if (!f.open("wb"))
			return false;
		f.write(data);
		f.close();
		return true;
	}
	function http_get(url)
	{
		try {
			load("http.js");
			return new HTTPRequest().Get(url);
		}
		catch (e) {
			log(LOG_WARNING, "univiewerlibrary: " + url + ": " + e);
			return null;
		}
	}
	var ctx = {
		options: lib.options,
		cache_dir: cache_dir,
		get_json: function (url, cache_name) {
			var cf = cache_dir + safe_name(cache_name);
			var cached = file_exists(cf) ? file_date(cf) : 0;
			if (!cached || time() - cached > ttl) {
				var data = http_get(url);
				if (data) {
					try {
						var obj = JSON.parse(data);
						write_file(cf, data);
						return obj;
					}
					catch (e) {}
				}
				if (!cached)
					return null;
			}
			var f = new File(cf);
			if (!f.open("r"))
				return null;
			var txt = f.read();
			f.close();
			try { return JSON.parse(txt); } catch (e2) { return null; }
		},
		get_file: function (url, local_name) {
			var path = cache_dir + "files/" + safe_name(local_name);
			if (file_exists(path)) {
				file_touch(path);	/* recently used files are kept longest by clean_cache() */
				return path;
			}
			var data = http_get(url);
			if (data === null || data === undefined || !write_file(path, data))
				return null;
			return path;
		}
	};
	try {
		var scope = load({}, BASE_DIR + "hooks/" + modname);
		lib.hook = scope.create(ctx);
	}
	catch (e) {
		log(LOG_ERR, "univiewerlibrary: hook " + modname + ": " + e);
	}
	return lib.hook;
}

/* Delete cached files older than cache_max_days (0 = no age limit); then, if the cache is still
 * larger than cache_max_mb (0 = no size limit), delete the least recently used files until it fits. */
function clean_cache()
{
	var root = BASE_DIR + ".cache/";
	if (!file_isdir(root))
		return;
	var files = [];
	var dirs = [];
	function walk(dir)
	{
		var paths = directory(dir + "*");
		for (var i = 0; i < paths.length; i++) {
			var p = paths[i].replace(/[\\\/]+$/, "");
			if (file_isdir(p)) {
				walk(p + "/");
				dirs.push(p);
			}
			else
				files.push({ path: p, date: file_date(p), size: file_size(p) });
		}
	}
	walk(root);
	var now = time();
	var recent = 600;	/* never touch files from the last 10 minutes (another user may be using them) */
	var total = 0;
	var keep = [];
	for (var i = 0; i < files.length; i++) {
		var f = files[i];
		if (opt.cache_max_days > 0 && now - f.date > opt.cache_max_days * 86400 && now - f.date > recent)
			file_remove(f.path);
		else {
			keep.push(f);
			total += f.size;
		}
	}
	if (opt.cache_max_bytes > 0 && total > opt.cache_max_bytes) {
		keep.sort(function (a, b) { return a.date - b.date; });
		for (i = 0; i < keep.length && total > opt.cache_max_bytes; i++) {
			if (now - keep[i].date <= recent)
				break;
			if (file_remove(keep[i].path))
				total -= keep[i].size;
		}
	}
	for (i = 0; i < dirs.length; i++)	/* remove directories left empty (fails harmlessly if not empty) */
		rmdir(dirs[i]);
}

function list_hook_entries(lib, subpath)
{
	var hook = open_hook(lib);
	var items = hook ? hook.list(subpath) : null;
	if (!items)
		return null;
	var out = [{ name: "..", isdir: true, up: true, size: 0, date: 0 }];	/* at the top it leaves the library */
	for (var i = 0; i < items.length; i++) {
		var it = items[i];
		if (it.isdir ? !dir_allowed(lib, it.name) : !type_allowed(lib, it.name))
			continue;
		out.push(it);
	}
	return out;
}

/* Case-insensitive comparison that orders numbers by value (page2 before page10) */
function natural_compare(a, b)
{
	var pa = String(a).toLowerCase().match(/\d+|\D+/g) || [];
	var pb = String(b).toLowerCase().match(/\d+|\D+/g) || [];
	for (var i = 0; i < pa.length && i < pb.length; i++) {
		if (pa[i] == pb[i])
			continue;
		var da = /^\d/.test(pa[i]), db = /^\d/.test(pb[i]);
		if (da && db) {
			var d = parseInt(pa[i], 10) - parseInt(pb[i], 10);
			if (d)
				return d < 0 ? -1 : 1;
		}
		return pa[i] < pb[i] ? -1 : 1;
	}
	return pa.length - pb.length;
}

/* ---------- Directory listing ---------- */

/* subpath: array of sub-directory names below the library directory */
/*
 * Descriptions: a hidden file named .desc in a directory holds one "filename<TAB>description" line per
 * file or sub-directory (blank lines and lines starting with ; or # are ignored).  Returns an object
 * keyed by name, or null.
 */
function read_descriptions(dir)
{
	var f = new File(dir + ".desc");
	if (!f.exists || !f.open("rb"))
		return null;
	var txt = f.read();
	f.close();
	if (!txt)
		return null;
	var map = {}, lines = txt.split(/\r?\n/);
	for (var i = 0; i < lines.length; i++) {
		var t = lines[i].indexOf("\t");
		if (t < 1 || /^[;#]/.test(lines[i]))
			continue;
		var d = lines[i].substr(t + 1).replace(/[\x00-\x1f\x7f]/g, " ").replace(/^\s+|\s+$/g, "");
		if (d.length)
			map[lines[i].substr(0, t)] = d;
	}
	return map;
}

function list_entries(lib, subpath)
{
	var dir = lib.dir + (subpath.length ? subpath.join("/") + "/" : "");
	var descs = read_descriptions(dir);
	var paths = directory(dir + "*");
	var dirs = [];
	var files = [];
	/* comic books that are listed: the directory they were extracted into is not */
	var comics = {};
	for (var ci = 0; ci < paths.length; ci++) {
		var cn = paths[ci].replace(/[\\\/]+$/, "");
		cn = cn.substr(Math.max(cn.lastIndexOf("/"), cn.lastIndexOf("\\")) + 1);
		if ((file_ext(cn) == "cbz" || file_ext(cn) == "cbr") && type_allowed(lib, cn))
			comics[cn.replace(/\.[^.]*$/, "").toLowerCase()] = true;
	}
	for (var i = 0; i < paths.length; i++) {
		var p = paths[i].replace(/[\\\/]+$/, "");
		var name = p.substr(Math.max(p.lastIndexOf("/"), p.lastIndexOf("\\")) + 1);
		if (!name.length || name.charAt(0) == ".")
			continue;
		if (file_isdir(p)) {
			if (comics[name.toLowerCase()])
				continue;
			if (lib.subdirs && dir_allowed(lib, name))
				dirs.push({ name: name, isdir: true, path: p, size: 0, date: file_date(p), desc: descs && descs[name] || "" });
		}
		else if (type_allowed(lib, name)) {
			/* a ZIP or RAR that has already been extracted is hidden (its directory is listed instead); a comic book
			 * (.cbz, .cbr) stays listed and its extracted directory is the one that is hidden */
			if ((file_ext(name) == "zip" || file_ext(name) == "rar") && file_isdir(zip_dest(p)))
				continue;
			files.push({ name: name, isdir: false, path: p, size: file_size(p), date: file_date(p),
				desc: descs && descs[name] || "", type: file_ext(name).toUpperCase(), iszip: ARCHIVE_TYPES.indexOf(file_ext(name)) >= 0 });
		}
	}
	function cmp(a, b)
	{
		var r;
		if (lib.sort == "date")
			r = a.date - b.date;
		else if (lib.sort == "size")
			r = a.size - b.size;
		else
			r = 0;
		if (r == 0)
			return natural_compare(a.name, b.name);
		return lib.reverse ? -r : r;
	}
	dirs.sort(function (a, b) { return natural_compare(a.name, b.name); });
	files.sort(cmp);
	var out = [{ name: "..", isdir: true, up: true, size: 0, date: 0 }];	/* at the top it leaves the library */
	return out.concat(dirs, files);
}

function count_files(lib)
{
	var n = 0;
	var paths = directory(lib.dir + "*");
	for (var i = 0; i < paths.length; i++) {
		var p = paths[i].replace(/[\\\/]+$/, "");
		if (!file_isdir(p) && type_allowed(lib, file_getname(p)))
			n++;
	}
	return n;
}

/* ---------- Generic lightbar list ---------- */

/*
 * title:   header text (plain), subtitle: name shown after it
 * cols:    array of {head, width, align, color, hicolor, get: function(item)}; one column
 *          (the one with flex: true) takes the remaining width
 *          Columns with scroll: true form a single horizontally scrolling area (Left/Right keys) on the
 *          left of the row, each one as wide as its longest entry (maxw limits that, except for the last
 *          one, which just runs on); the other columns stay put on the right.
 * items:   array of items
 * help:    array of [hotkey, text], or a function returning one
 * initial: index of the item to start on (default 0)
 * Returns { item: <selected item>, index: <its index> } or null on quit.
 * onkey(key, item) may return true to end the list with that key.
 */
function lightbar(title, subtitle, cols, items, help, onkey, initial)
{
	var W = console.screen_columns;
	var H = console.screen_rows;
	var listTop = 3;
	var listRows = H - listTop - 1;
	var sbX = W;
	var width = W - 1;		/* leave the last column for the scrollbar */
	var sel = 0;
	var top = 0;
	var i;
	if (initial > 0 && initial < items.length) {
		sel = initial;
		if (sel >= listRows)
			top = Math.max(0, Math.min(items.length - listRows, sel - Math.floor(listRows / 2)));
	}

	/* Column widths */
	var fixed = 0;
	var flex = null;
	for (i = 0; i < cols.length; i++) {
		if (cols[i].flex)
			flex = cols[i];
		else
			fixed += cols[i].width + 1;
	}
	if (flex)
		flex.width = Math.max(8, width - fixed - 2);

	/* Horizontal scrolling area */
	var hs = null;			/* { cols, fixed cols, region width, max offset } */
	var hoff = 0;
	for (i = 0; i < cols.length; i++) {
		if (cols[i].scroll) {
			hs = { sc: [], fx: [], w: 0, max: 0 };
			break;
		}
	}
	if (hs) {
		var fxw = 0;
		for (i = 0; i < cols.length; i++) {
			if (cols[i].scroll)
				hs.sc.push(cols[i]);
			else {
				hs.fx.push(cols[i]);
				fxw += cols[i].width + 1;
			}
		}
		hs.w = Math.max(8, width - 2 - fxw);
		for (i = 0; i < hs.sc.length; i++)
			hs.sc[i].nat = String(hs.sc[i].head).length;
		for (var ri = 0; ri < items.length; ri++) {
			for (i = 0; i < hs.sc.length - 1; i++) {
				var tl = String(hs.sc[i].get(items[ri])).length;
				if (tl > hs.sc[i].nat && tl <= (hs.sc[i].maxw || 60))
					hs.sc[i].nat = tl;
			}
		}
		/* longest row: padded earlier columns (or the whole name when it is longer) plus the last column */
		var maxlen = 0;
		for (ri = 0; ri < items.length; ri++) {
			var len = 0;
			for (i = 0; i < hs.sc.length; i++) {
				var tx = String(hs.sc[i].get(items[ri]));
				if (i < hs.sc.length - 1)
					len += Math.max(tx.length, hs.sc[i].nat) + 1;
				else
					len += tx.length;
			}
			if (len > maxlen)
				maxlen = len;
		}
		hs.max = Math.max(0, maxlen - hs.w);
	}

	/* The visible part of a row's scrolling area: segments is [[color, text], ...] */
	function hs_window(segments, pad_attr)
	{
		var out = "", pos = 0, shown = 0;
		for (var g = 0; g < segments.length; g++) {
			var t = segments[g][1];
			var a = Math.max(pos, hoff), b = Math.min(pos + t.length, hoff + hs.w);
			if (b > a) {
				out += segments[g][0] + t.substring(a - pos, b - pos);
				shown += b - a;
			}
			pos += t.length;
		}
		return out + pad_attr + new Array(hs.w - shown + 1).join(" ");
	}

	function draw_header()
	{
		console.gotoxy(1, 1);
		console.print(C("headerTextColor") + title + " ");
		console.print(C("headerNameColor") + subtitle);
		console.cleartoeol();
		console.gotoxy(1, 2);
		console.print("\x01n" + C("colHeaderColor"));
		var s = " ";
		if (hs) {
			var hsegs = [];
			for (var hc = 0; hc < hs.sc.length; hc++)
				hsegs.push(["", hc < hs.sc.length - 1 ? fit(hs.sc[hc].head, hs.sc[hc].nat) + " " : String(hs.sc[hc].head)]);
			s = (hoff > 0 ? "<" : " ") + hs_window(hsegs, "") + (hoff < hs.max ? ">" : " ");
			for (hc = 0; hc < hs.fx.length; hc++)
				s += (hs.fx[hc].align == "right" ? rjust(hs.fx[hc].head, hs.fx[hc].width) : fit(hs.fx[hc].head, hs.fx[hc].width)) + " ";
			console.print(s);
			console.cleartoeol();
			return;
		}
		for (var c = 0; c < cols.length; c++)
			s += (cols[c].align == "right" ? rjust(cols[c].head, cols[c].width) : fit(cols[c].head, cols[c].width)) + " ";
		console.print(s.substr(0, width));
		console.cleartoeol();
	}

	function draw_help()
	{
		console.gotoxy(1, H);
		var bkg = "\x01" + T("helpLineBkgColor").charAt(0);
		var s = "\x01n" + bkg;
		var hl = typeof help == "function" ? help() : help;
		var hx = 1;
		BAR_SPOTS = [];
		for (var h = 0; h < hl.length; h++) {
			s += C("helpLineHotkeyColor") + bkg + hl[h][0] + C("helpLineParenColor") + bkg + ")" +
				C("helpLineGeneralColor") + bkg + hl[h][1] + " ";
			var hw = String(hl[h][0]).length + 1 + String(hl[h][1]).length;
			bar_add(hx, hx + hw - 1, String(hl[h][0]));
			hx += hw + 1;
		}
		console.print(s);
		console.cleartoeol();
		console.print("\x01n");
	}

	function draw_row(idx, selected)
	{
		var y = listTop + (idx - top);
		console.gotoxy(1, y);
		if (idx >= items.length) {
			console.print("\x01n");
			console.cleartoeol();
			return;
		}
		var it = items[idx];
		var s = "\x01n" + (selected ? "\x01" + T("listHighlightBkgColor").charAt(0) : "") + " ";
		if (hs) {
			var segs = [];
			for (var sc = 0; sc < hs.sc.length; sc++) {
				var scol = hs.sc[sc];
				var sname = it.isdir && scol.dircolor ? scol.dircolor : scol.color;
				var shi = it.isdir && scol.dircolor ? scol.dirhicolor : scol.hicolor;
				var stext = String(scol.get(it));
				if (sc < hs.sc.length - 1)
					stext = (stext.length < scol.nat ? fit(stext, scol.nat) : stext) + " ";
				segs.push([selected ? CH(shi) : C(sname), stext]);
			}
			s += hs_window(segs, "\x01n" + (selected ? "\x01" + T("listHighlightBkgColor").charAt(0) : "")) + " ";
			for (var fc = 0; fc < hs.fx.length; fc++) {
				var fcol = hs.fx[fc];
				var ftext = fcol.get(it);
				ftext = fcol.align == "right" ? rjust(ftext, fcol.width) : fit(ftext, fcol.width);
				s += (selected ? CH(fcol.hicolor) : C(fcol.color)) + ftext + " ";
			}
			console.print(s + "\x01n");
			return;
		}
		for (var c = 0; c < cols.length; c++) {
			var col = cols[c];
			var name = it.isdir && col.dircolor ? col.dircolor : col.color;
			var hiname = it.isdir && col.dircolor ? col.dirhicolor : col.hicolor;
			var text = col.get(it);
			text = col.align == "right" ? rjust(text, col.width) : fit(text, col.width);
			s += (selected ? CH(hiname) : C(name)) + text + " ";
		}
		/* pad the row to the list width (the last column belongs to the scrollbar) */
		var used = 1;
		for (c = 0; c < cols.length; c++)
			used += cols[c].width + 1;
		for (; used < width; used++)
			s += " ";
		console.print(s + "\x01n");
	}

	function draw_scrollbar()
	{
		if (items.length <= listRows)
			return;
		var thumb = Math.max(1, Math.round(listRows * listRows / items.length));
		var span = listRows - thumb;
		var pos = items.length > listRows ? Math.round(top * span / (items.length - listRows)) : 0;
		for (var r = 0; r < listRows; r++) {
			console.gotoxy(sbX, listTop + r);
			if (r >= pos && r < pos + thumb)
				console.print(C("scrollbarScrollBlockColor") + unescape_text(T("scrollbarScrollBlockChar")));
			else
				console.print(C("scrollbarBGColor") + unescape_text(T("scrollbarBGChar")));
		}
		console.print("\x01n");
	}

	function draw_list()
	{
		for (var r = 0; r < listRows; r++)
			draw_row(top + r, top + r == sel);
		draw_scrollbar();
	}

	function move_to(n)
	{
		n = Math.max(0, Math.min(items.length - 1, n));
		if (n == sel)
			return;
		var old = sel;
		sel = n;
		var oldtop = top;
		if (sel < top)
			top = sel;
		else if (sel >= top + listRows)
			top = sel - listRows + 1;
		if (top != oldtop)
			draw_list();
		else {
			draw_row(old, false);
			draw_row(sel, true);
		}
	}

	/* A click on a row selects it, a click on the selected row opens it, a click in the scrollbar column
	 * jumps there.  (Wheel and right button are turned into keys by Synchronet.) */
	function mouse_click(m)
	{
		if ((m.button & ~28) !== 0)	/* left button only, ignoring shift/ctrl/alt; no drags */
			return null;
		if (m.x == sbX && m.y >= listTop && m.y < listTop + listRows && items.length > listRows) {
			var nt = Math.round((m.y - listTop) * (items.length - listRows) / (listRows - 1));
			nt = Math.max(0, Math.min(items.length - listRows, nt));
			if (nt != top) {
				top = nt;
				sel = Math.max(top, Math.min(top + listRows - 1, sel));
				draw_list();
			}
			return null;
		}
		var idx = top + m.y - listTop;
		if (m.y < listTop || m.y >= listTop + listRows || idx >= items.length)
			return null;
		if (idx == sel)
			return { item: items[sel], index: sel };
		move_to(idx);
		return null;
	}

	console.clear();
	mouse_refresh();
	draw_header();
	draw_help();
	mouse_clicks(true);
	try {
		return lightbar_loop();
	}
	finally {
		mouse_clicks(false);
	}

	function lightbar_loop()
	{
	if (!items.length) {
		console.gotoxy(2, listTop);
		console.print(unescape_text(T("emptyLibraryText")) + "\x01n");
	}
	else
		draw_list();

	while (bbs.online) {
		var key = console.getkey(K_NONE);
		if (key === "" || key === undefined)
			continue;
		if (key == KEY_ESC) {
			var mk = read_mouse();
			if (mk !== false) {
				var bk = mk ? bar_key(mk) : null;
				if (bk)
					key = bk;
				else {
					var chosen = mk && bk === null ? mouse_click(mk) : null;
					if (chosen)
						return chosen;
					continue;
				}
			}
		}
		switch (key) {
			case KEY_UP:
				move_to(sel - 1);
				continue;
			case KEY_DOWN:
				move_to(sel + 1);
				continue;
			case KEY_PAGEUP:
				move_to(sel - listRows);
				continue;
			case KEY_PAGEDN:
				move_to(sel + listRows);
				continue;
			case KEY_LEFT:
			case KEY_RIGHT:
				if (hs) {
					var nh = Math.max(0, Math.min(hs.max, hoff + (key == KEY_LEFT ? -1 : 1) * Math.max(10, Math.floor(hs.w / 2))));
					if (nh != hoff) {
						hoff = nh;
						draw_header();
						draw_list();
					}
					continue;
				}
				break;
			case KEY_HOME:
				move_to(0);
				continue;
			case KEY_END:
				move_to(items.length - 1);
				continue;
			case "\r":
			case "\n":
				if (items.length)
					return { item: items[sel], index: sel };
				continue;
			case KEY_ESC:
			case "\x03":
			case "q":
			case "Q":
				return null;
		}
		if (onkey) {
			var handled = onkey(key, items[sel]);
			if (handled === "exit")
				return { item: items[sel], index: sel, action: key };
			if (handled) {
				draw_help();
				continue;
			}
		}
		/* digits jump to the numbered item; letters jump to the next item starting with that letter */
		if (items.length && /^[0-9]$/.test(key) && items[0].num !== undefined) {
			for (i = 0; i < items.length; i++) {
				if (items[i].num == parseInt(key, 10)) {
					move_to(i);
					break;
				}
			}
		}
		else if (items.length && /^[a-zA-Z]$/.test(key)) {
			var lk = key.toLowerCase();
			for (i = 1; i <= items.length; i++) {
				var j = (sel + i) % items.length;
				if (items[j].name.toLowerCase().charAt(0) == lk) {
					move_to(j);
					break;
				}
			}
		}
	}
	return null;
	}
}

/* ---------- Screens ---------- */

/* S and P: cycle the text/ANSI output speed, toggle screen pausing.  Returns true if changed. */
function option_key(key)
{
	switch (key) {
		case "s":
		case "S":
			opt.speed = (opt.speed + 1) % SPEEDS.length;
			return true;
		case "p":
		case "P":
			opt.pause = !opt.pause;
			return true;
	}
	return false;
}

function option_help()
{
	return [["S", "Speed:" + (opt.speed ? SPEEDS[opt.speed] : "Full")], ["P", "Pause:" + (opt.pause ? "On" : "Off")]];
}

/* ---------- Text / ANSI viewing ---------- */

var Sauce, Graphic, Ansi, CTermLib;

function load_text_libs()
{
	if (Sauce)
		return;
	Sauce = load({}, "sauce_lib.js");
	Graphic = load({}, "graphic.js");
	Ansi = load({}, "ansiterm_lib.js");
	CTermLib = load({}, "cterm_lib.js");
	syncterm = CTermLib.query_da() !== false;
}

/* True if the user pressed Ctrl-C, Esc or Q (or Synchronet saw an abort) */
function user_interrupted()
{
	var k;
	while ((k = console.inkey(K_NONE, 0)) !== "" && k !== undefined) {
		if (k == "\x03" || k == KEY_ESC || k == "q" || k == "Q")
			return true;
	}
	if (bbs.sys_status & SS_ABORT) {
		bbs.sys_status &= ~SS_ABORT;
		return true;
	}
	return false;
}

/*
 * Send ANSI/ASCII art.  If opt.pause is set it pauses after every screenful; if cps > 0 the output is
 * paced to that many characters per second (the speed setting).  Ctrl-C, Esc or Q stops it.
 *
 * Synchronet's own pause can't be used: art that moves the cursor up (ESC[A) after each line cancels
 * the line count, so it never pauses.  The lines are counted here instead.  The speed is done here
 * as well, rather than by SyncTERM's speed emulation, because output already sent to the terminal
 * can't be interrupted.
 * Returns false if the user stopped the output.
 */
function send_art(contents, cps)
{
	var pagerows = Math.max(2, console.screen_rows - 1);
	var pagelines = 0;
	var sgr = [];		/* the color/attribute codes in effect, so they can be restored after a prompt */
	var sent = 0;
	var t0 = system.timer;

	function track(text)
	{
		var re = /\x1b\[([0-9;]*)m/g, m;
		while ((m = re.exec(text)) !== null) {
			var codes = m[1].length ? m[1].split(";") : ["0"];
			for (var i = 0; i < codes.length; i++) {
				if (parseInt(codes[i] || "0", 10) === 0)
					sgr = [];
				else
					sgr.push(codes[i]);
			}
		}
	}

	function more()
	{
		console.write("\x1b[s");
		console.gotoxy(1, console.screen_rows);
		console.print("\x01n\x01h\x01w\x014 -- More --  any key continues, Q stops \x01n");
		console.write("\x1b[K");
		var k = console.getkey(K_NONE);
		bbs.sys_status &= ~SS_ABORT;
		console.gotoxy(1, console.screen_rows);
		console.write("\x1b[0m\x1b[K\x1b[u\x1b[0" + (sgr.length ? ";" + sgr.join(";") : "") + "m");
		t0 = system.timer - sent / cps;		/* the time spent at the prompt doesn't count */
		return !(k == "q" || k == "Q" || k == KEY_ESC || k == "\x03");
	}

	var lines = contents.split("\n");
	for (var i = 0; i < lines.length && bbs.online; i++) {
		var text = lines[i];
		var last = i == lines.length - 1;
		if (!last)
			text += "\n";
		if (!last && text.length > 1 && text.charAt(text.length - 2) != "\r")
			text = text.substr(0, text.length - 1) + "\r\n";
		if (cps > 0) {
			for (var pos = 0; pos < text.length && bbs.online;) {
				var allowed = Math.floor((system.timer - t0) * cps) - sent;
				if (allowed < 1) {
					if (user_interrupted())
						return false;
					mswait(2);
					continue;
				}
				var n = Math.min(allowed, text.length - pos);
				console.print(text.substr(pos, n), P_NOATCODES | P_NOPAUSE);
				pos += n;
				sent += n;
				if (user_interrupted())
					return false;
			}
		}
		else {
			console.print(text, P_NOATCODES | P_NOPAUSE);
			if (user_interrupted())
				return false;
		}
		track(text);
		if (!last && opt.pause && ++pagelines >= pagerows) {
			pagelines = 0;
			if (!more())
				return false;
		}
	}
	return true;
}

/* Characters per second for the current speed setting (0 = full speed) */
function art_cps()
{
	return opt.speed ? SPEEDS[opt.speed] / 10 : 0;
}

function print_art(file)
{
	if (!opt.pause && !opt.speed)
		return console.printfile(file, P_NOPAUSE | P_CPM_EOF);
	var f = new File(file);
	if (!f.open("rb"))
		throw "Failed to open " + file_getname(file);
	var contents = f.read().split("\x1a")[0];
	f.close();
	return send_art(contents, art_cps());
}

function draw_text(file, cols, rows)
{
	if (opt.speed || !cols || !rows || (rows > console.screen_rows && cols <= console.screen_columns))
		return print_art(file);
	/* the whole image fits on the screen: use Graphic */
	var graphic = new Graphic(cols, rows);
	try {
		if (graphic.load(file))
			graphic.draw();
		else
			return print_art(file);
	}
	catch (e) {
		return print_art(file);
	}
}

/* ---------- Scrollable text reader ---------- */

var READER_MAX_BYTES = 2 * 1048576;

/* Strip a UTF-8 byte-order mark and, if the text is valid UTF-8 (not CP437 art), convert it to
 * CP437; Synchronet translates CP437 for UTF-8 terminals on output. */
function normalize_text(text)
{
	text = text.replace(/^\xef\xbb\xbf/, "");
	var seq = /[\xc2-\xdf][\x80-\xbf]|[\xe0-\xef][\x80-\xbf][\x80-\xbf]|[\xf0-\xf4][\x80-\xbf][\x80-\xbf][\x80-\xbf]/g;
	if (seq.test(text) && !/[\x80-\xff]/.test(text.replace(seq, ""))) {
		load("utf8_cp437.js");
		text = utf8_cp437(text);
	}
	return text;
}

/* Split text into display lines of at most width characters */
function wrap_lines(text, width)
{
	var raw = text.split(/\r\n|\r|\n/);
	var out = [];
	for (var i = 0; i < raw.length; i++) {
		var line = raw[i].replace(/\t/g, "        ").replace(/[\x00-\x1f]/g, " ").replace(/\s+$/, "");
		while (line.length > width) {
			var brk = line.lastIndexOf(" ", width);
			if (brk < width / 2)
				brk = width;
			out.push(line.substr(0, brk).replace(/\s+$/, ""));
			line = line.substr(brk).replace(/^ /, "");
		}
		out.push(line);
	}
	return out;
}

function view_reader(file, text)
{
	var clicks_were_on = MOUSE_STARTED && (console.status & CON_MOUSE_CLK_PASSTHRU) != 0;
	mouse_clicks(true);		/* the list this was opened from has stopped asking for clicks by now */
	try {
		return view_reader_inner(file, text);
	}
	finally {
		mouse_clicks(clicks_were_on);
	}
}

function view_reader_inner(file, text)
{
	var W = console.screen_columns;
	var H = console.screen_rows;
	var rows = H - 2;
	var lines = wrap_lines(normalize_text(text), W - 1);
	var maxtop = Math.max(0, lines.length - rows);
	var top = 0;
	var name = file_getname(file);

	function draw()
	{
		var pct = maxtop ? Math.round(100 * top / maxtop) : 100;
		var pos = " Line " + Math.min(lines.length, top + 1) + "-" + Math.min(lines.length, top + rows) +
			"/" + lines.length + "  " + pct + "% ";
		console.gotoxy(1, 1);
		console.print("\x01n" + C("headerTextColor") + " " + fit(name, Math.max(1, W - 1 - pos.length - 1)) +
			rjust(pos, pos.length));
		console.cleartoeol();
		for (var r = 0; r < rows; r++) {
			console.gotoxy(1, 2 + r);
			console.print("\x01n" + C("textBodyColor"));
			if (top + r < lines.length)
				console.write(lines[top + r]);
			console.cleartoeol();
		}
		if (maxtop) {
			var thumb = Math.max(1, Math.round(rows * rows / lines.length));
			var tpos = Math.round(top * (rows - thumb) / maxtop);
			for (r = 0; r < rows; r++) {
				console.gotoxy(W, 2 + r);
				if (r >= tpos && r < tpos + thumb)
					console.print(C("scrollbarScrollBlockColor") + unescape_text(T("scrollbarScrollBlockChar")));
				else
					console.print(C("scrollbarBGColor") + unescape_text(T("scrollbarBGChar")));
			}
		}
		console.gotoxy(1, H);
		var bkg = "\x01" + T("helpLineBkgColor").charAt(0);
		var help = [["Up/Dn", "Scroll"], ["PgUp/PgDn", "Page"], ["Home/End", "Top/Bottom"], ["N/P", "Next/Prev File"], ["Q", "Back"]];
		var hs = "\x01n" + bkg;
		var hx = 1;
		BAR_SPOTS = [];
		for (var h = 0; h < help.length; h++) {
			hs += C("helpLineHotkeyColor") + bkg + help[h][0] + C("helpLineParenColor") + bkg + ")" +
				C("helpLineGeneralColor") + bkg + help[h][1] + " ";
			var hw = help[h][0].length + 1 + help[h][1].length;
			bar_add(hx, hx + hw - 1, help[h][0]);
			hx += hw + 1;
		}
		console.print(hs);
		console.cleartoeol();
		console.print("\x01n");
	}

	console.clear();
	draw();
	while (bbs.online) {
		var key = console.getkey(K_NONE);
		var old = top;
		if (key == KEY_ESC) {
			var rmk = read_mouse();
			if (rmk !== false) {
				var rbk = rmk ? bar_key(rmk) : null;
				if (!rbk)
					continue;
				key = rbk;
			}
		}
		if (bbs.sys_status & SS_ABORT) {		/* right mouse button */
			bbs.sys_status &= ~SS_ABORT;
			key = "\x03";
		}
		switch (key) {
			case KEY_UP:
				top--;
				break;
			case KEY_DOWN:
			case "\r":
				top++;
				break;
			case KEY_PAGEUP:
			case "b":
			case "B":
				top -= rows - 1;
				break;
			case KEY_PAGEDN:
			case " ":
				top += rows - 1;
				break;
			case KEY_HOME:
				top = 0;
				break;
			case KEY_END:
				top = maxtop;
				break;
			case "n":
			case "N":
				return "next";
			case "p":
			case "P":
				return "prev";
			case "\x03":
				RIGHT_CLICKED = true;
				return "";
			case KEY_ESC:
			case "q":
			case "Q":
				return "";
		}
		top = Math.max(0, Math.min(maxtop, top));
		if (top != old)
			draw();
	}
}

/* Returns "prev" or "next" if the user pressed P or N, otherwise "" */
function view_text(file)
{
	load_text_libs();
	/* Plain text (no ANSI sequences) goes into the scrollable reader */
	var ext = file_ext(file_getname(file));
	if (ext != "ans" && ext != "ansi" && file_size(file) <= READER_MAX_BYTES) {
		var rf = new File(file);
		if (rf.open("rb")) {
			var text = rf.read().split("\x1a")[0];
			rf.close();
			if (!/\x1b/.test(text)) {
				var nav = view_reader(file, text);
				console.clear(BG_BLACK | LIGHTGRAY);
				return nav;
			}
		}
	}
	var sauce = Sauce.read(file) || {};
	var saved_status = bbs.sys_status;
	/* Synchronet's own pause is always off here: send_art() does the pausing, and Synchronet's line
	 * counter gets it wrong (it counts a full 80-column line followed by CR/LF as two lines, and
	 * cursor-up codes as minus one), which made the display stop every few lines in some files. */
	bbs.sys_status &= ~SS_PAUSEON;
	bbs.sys_status |= SS_PAUSEOFF;
	console.clear(BG_BLACK | LIGHTGRAY);
	var nav = "";
	function wait_key()
	{
		var k = console.getkey();
		nav = /^[nN]$/.test(k) ? "next" : /^[pP]$/.test(k) ? "prev" : "";
	}
	try {
		/* (the speed is paced by send_art(), not SyncTERM's own emulation, so that Ctrl-C can stop it) */
		if (syncterm) {
			Ansi.send("ext_mode", "clear", "cursor");
			if (sauce.ice_color)
				Ansi.send("ext_mode", "set", "bg_bright_intensity");
			mswait(500);
		}
		var finished = draw_text(file, sauce.cols, sauce.rows);
		if (finished !== false)		/* if the user interrupted it, go straight back to the list */
			wait_key();
		if (syncterm) {
			Ansi.send("ext_mode", "clear", "bg_bright_intensity");
			Ansi.send("ext_mode", "set", "cursor");
		}
	}
	finally {
		bbs.sys_status = saved_status;
		console.clear(BG_BLACK | LIGHTGRAY);
	}
	return nav;
}

/* ---------- ZIP extraction ---------- */

function zip_dest(zipfile)
{
	return zipfile.replace(/\.[^.\/\\]*$/, "");
}

/* Delete a directory and everything in it (used to clean up after a failed extraction) */
function remove_tree(dir)
{
	var paths = directory(backslash(dir) + "*");
	for (var i = 0; i < paths.length; i++) {
		var p = paths[i].replace(/[\\\/]+$/, "");
		if (file_isdir(p))
			remove_tree(p);
		else
			file_remove(p);
	}
	rmdir(dir);
}

/* Netmail a problem report: to error_mail= (a user's alias or number, or an Internet address), else the
 * sysop (user 1); error_mail=none turns it off.  Each file is reported once a day, however many users
 * run into it.  Never throws. */
function report_problem(path, problem)
{
	try {
		var to = opt.error_mail;
		if (to.toLowerCase() == "none")
			return;
		var mark = BASE_DIR + ".cache/reported/";
		mkpath(mark);
		mark += md5_calc(path + "|" + problem.substr(0, 40), true);
		if (file_exists(mark) && time() - file_date(mark) < 86400)
			return;
		var hdr = { from: "Universal Viewer", from_ext: "0", subject: "Universal Viewer: problem with " + file_getname(path) };
		if (to.indexOf("@") > 0) {
			hdr.to = to;
			hdr.to_net_type = NET_INTERNET;
			hdr.to_net_addr = to;
		} else {
			var un = to === "" ? 1 : (/^\d+$/.test(to) ? Number(to) : system.matchuser(to));
			if (!un || !system.username(un)) {
				log(LOG_WARNING, "univiewerlibrary: error_mail user '" + to + "' not found");
				return;
			}
			hdr.to = system.username(un);
			hdr.to_ext = String(un);
		}
		var mb = new MsgBase("mail");
		if (!mb.open()) {
			log(LOG_WARNING, "univiewerlibrary: " + mb.error + " opening the mail base");
			return;
		}
		var ok = mb.save_msg(hdr, "Universal Viewer could not open a file.\r\n\r\n" +
			"File:    " + path + "\r\n" +
			"Problem: " + problem + "\r\n" +
			"User:    " + user.alias + " (#" + user.number + ") on node " + bbs.node_num + "\r\n" +
			"Time:    " + system.timestr() + "\r\n");
		mb.close();
		if (ok) {
			var f = new File(mark);
			if (f.open("w")) { f.write(""); f.close(); }
		}
		else
			log(LOG_WARNING, "univiewerlibrary: unable to send problem report: " + mb.error);
	}
	catch (e) {
		log(LOG_WARNING, "univiewerlibrary: problem report: " + e);
	}
}

/* If an archive holds more files or data than the limits allow, report it (log + netmail) and return the
 * message for the user; otherwise null. */
function archive_too_big(path, name, count, total)
{
	if (count <= opt.zip_max_files && total <= opt.zip_max_bytes)
		return null;
	var why = [];
	if (count > opt.zip_max_files)
		why.push("it has " + count + " files and the limit is " + opt.zip_max_files);
	if (total > opt.zip_max_bytes)
		why.push("it is " + Math.ceil(total / 1048576) + " MB unpacked and the limit is " + Math.floor(opt.zip_max_bytes / 1048576) + " MB");
	log(LOG_WARNING, "univiewerlibrary: " + user.alias + " could not open " + path + ": " + why.join(", and "));
	report_problem(path, "archive too large to extract: " + why.join(", and "));
	return "Can't open " + name + ": " + why.join(", and ") +
		". The sysop has been told so the limit can be raised";
}

/* RAR files libarchive can't read (old formats, odd headers) or extract: use the unrar program, if there is one */
function extract_rar(path, dest, name)
{
	if (/["$`\\!]/.test(path + dest))
		return "Unable to extract " + name;
	var unrar = file_exists(system.exec_dir + "unrar") ? system.exec_dir + "unrar" : "unrar";
	var tmp = BASE_DIR + ".cache/unrar-" + system.timer.toString().replace(/\D/g, "") + ".txt";
	mkpath(BASE_DIR + ".cache/");
	/* the last line of a listing is "  <total size> ... <file count>" */
	system.exec(unrar + ' l -p- "' + path + '" > "' + tmp + '" 2>&1');
	var f = new File(tmp), txt = "";
	if (f.open("r")) { txt = f.read() || ""; f.close(); }
	file_remove(tmp);
	var m = /(\d+)[^\n\d]*(\d+)\s*\r?\n?\s*$/.exec(txt.replace(/\s+$/, ""));
	if (!m || txt.indexOf("-----") < 0) {
		log(LOG_WARNING, "univiewerlibrary: unable to list " + path + " with unrar");
		return "Unable to extract " + name;
	}
	var big = archive_too_big(path, name, Number(m[2]), Number(m[1]));
	if (big)
		return big;
	mkpath(dest);
	if (!file_isdir(dest))
		return "Unable to create " + file_getname(dest);
	var rc = system.exec(unrar + ' x -o+ -y -p- -inul "' + path + '" "' + backslash(dest) + '"');
	if (rc !== 0 || directory(backslash(dest) + "*").length < 1) {
		log(LOG_WARNING, "univiewerlibrary: unrar of " + path + " failed, code " + rc);
		remove_tree(dest);
		return "Unable to extract " + name;
	}
	return null;
}

/* Extract an archive into dest (which must not exist).  Returns an error string or null. */
function extract_archive(path, dest, name)
{
	if (file_exists(dest))
		return "Cannot extract: " + file_getname(dest) + " already exists";
	try {
		var ext = file_ext(path).toLowerCase();
		try {
			var a = new Archive(path);
			var list = a.list();
		}
		catch (e) {
			if (ext == "rar") {
				log(LOG_INFO, "univiewerlibrary: " + path + ": " + e + " (trying unrar)");
				return extract_rar(path, dest, name);
			}
			throw e;
		}
		var total = 0;
		var count = 0;
		for (var i = 0; i < list.length; i++) {
			var n = String(list[i].name);
			if (/^([a-zA-Z]:)?[\\\/]/.test(n) || /(^|[\\\/])\.\.([\\\/]|$)/.test(n))
				return "Archive contains unsafe file names";
			if (list[i].type == "file") {
				count++;
				total += list[i].size;
			}
		}
		var big = archive_too_big(path, name, count, total);
		if (big)
			return big;
		mkpath(dest);
		if (!file_isdir(dest))
			return "Unable to create " + file_getname(dest);
		/* some archives (many EPUBs) have no entries for their folders, and libarchive then can't create the files in them */
		var made = {};
		for (i = 0; i < list.length; i++) {
			var dn = String(list[i].name).replace(/\\/g, "/");
			dn = list[i].type == "file" ? dn.replace(/[^\/]*$/, "") : dn;
			if (dn.length && !made[dn]) {
				made[dn] = true;
				mkpath(backslash(dest) + dn);
			}
		}
		try {
			a.extract(dest, true, true, opt.zip_max_files);
			return null;
		}
		catch (e) {
			/* e.g. old "imploded" ZIPs that libarchive can't read: start again with unzip, if there is one */
			log(LOG_INFO, "univiewerlibrary: " + path + ": " + e + " (trying " + (ext == "rar" ? "unrar" : "unzip") + ")");
		}
		remove_tree(dest);
		if (ext == "rar")
			return extract_rar(path, dest, name);
		if (/["$`\\!]/.test(path + dest))
			return "Unable to extract " + name;
		var unzip = file_exists(system.exec_dir + "unzip") ? system.exec_dir + "unzip" : "unzip";
		mkpath(dest);
		if (!file_isdir(dest))
			return "Unable to create " + file_getname(dest);
		var rc = system.exec(unzip + ' -o -qq -d "' + dest + '" "' + path + '"');
		if ((rc !== 0 && rc !== 1) || directory(backslash(dest) + "*").length < 1) {
			log(LOG_WARNING, "univiewerlibrary: unzip of " + path + " failed, code " + rc);
			remove_tree(dest);
			return "Unable to extract " + name;
		}
	}
	catch (e) {
		log(LOG_WARNING, "univiewerlibrary: extracting " + path + ": " + e);
		remove_tree(dest);
		return "Unable to extract " + name;
	}
	return null;
}

/* Extract a ZIP next to itself, into a directory of the same name (once).  Returns an error string or null. */
function extract_zip(entry)
{
	var dest = zip_dest(entry.path);
	if (file_isdir(dest))
		return null;
	return extract_archive(entry.path, dest, entry.name);
}

/* ---------- HTML pages, web sites and EPUB books ---------- */

var CUR_LIB = null;		/* the library whose files are being viewed (for the local-file limits) */

function web_dir()
{
	return BASE_DIR + ".cache/web/";
}

function web_allowed(lib, url)
{
	if (!/^https?:\/\//i.test(url))
		return false;
	var h = host_of(url);
	return h.length > 0 && matches_any(lib.allow, h);
}

/* Fetch a URL (only from hosts the library allows, following redirects only to such hosts), with size and
 * time limits and a cache.  Returns { body, url } or null.  (The request itself is in http_lib.js.) */
function web_fetch(lib, url, maxbytes, ttl)
{
	url = url.replace(/#.*$/, "");
	var cf = web_dir() + md5_calc(url, true) + ".dat";
	var cached = http_cache_read(cf, ttl);
	if (cached)
		return cached;
	var r = http_request({ url: url, maxbytes: maxbytes, allowed: function (u) { return web_allowed(lib, u); } });
	if (r.error)
		return null;
	mkpath(web_dir());
	http_cache_write(cf, r.url, r.body);
	return { url: r.url, body: r.body };
}

/* ---------- Forms on web pages: entering data and sending it ----------
 *
 * What the user types is only ever used as data in a request that http_lib.js builds: it is never run, never put in a
 * file name, never given to a program.  On the way it is limited in length, stripped of control characters, and
 * percent-encoded, so it can't add a header or a second request.  A form can only be sent to an address the library's
 * allow list permits, and a password is never sent without https.  Nothing that is typed is written to the log.
 */
var FORM_MAX_VALUE = 512;		/* longest single value, in characters */
var FORM_MAX_BODY = 8192;		/* longest set of form data, once encoded */
var FORM_MAX_PER_MINUTE = 10;		/* forms one user may send per minute */
var FORM_STATE = {};		/* what has been entered: by page address, then by field key */
var FORM_SENT = [];			/* when forms were sent, for the rate limit */

/* A word for the status line when a control is selected */
function form_link_label(fields, uri)
{
	var m = /^sixelform:([0-9]+)$/.exec(uri);
	var f = m ? fields[parseInt(m[1], 10)] : null;
	if (!f)
		return null;
	switch (f.type) {
		case "checkbox": return "checkbox: Enter ticks or clears it";
		case "radio": return "choice: Enter selects it";
		case "select": return "list: Enter shows the next entry";
		case "submit": case "button": case "image": return "button: Enter sends the form";
		case "password": return "password box: Enter to type";
	}
	return "text box (" + f.prompt + "): Enter to type";
}

/* The Unicode code point for a CP437 character (what the console hands back for what the user typed) */
function form_cp437_unicode(ch)
{
	var u = utf8_encode(ch), b0 = u.charCodeAt(0);
	if (b0 < 0x80)
		return b0;
	if (b0 < 0xe0)
		return ((b0 & 0x1f) << 6) | (u.charCodeAt(1) & 0x3f);
	return ((b0 & 0x0f) << 12) | ((u.charCodeAt(1) & 0x3f) << 6) | (u.charCodeAt(2) & 0x3f);
}

/* Remove control characters (and the BBS's own Ctrl-A color codes) and cut to length */
function form_clean(s, maxlen)
{
	return String(s).replace(/[\x00-\x1f\x7f-\x9f  ]/g, "").substr(0, maxlen);
}

/* A string of Unicode characters as UTF-8 bytes */
function form_utf8(s)
{
	var out = [];
	for (var i = 0; i < s.length; i++) {
		var c = s.charCodeAt(i);
		if (c < 0x80)
			out.push(String.fromCharCode(c));
		else if (c < 0x800)
			out.push(String.fromCharCode(0xc0 | (c >> 6), 0x80 | (c & 0x3f)));
		else
			out.push(String.fromCharCode(0xe0 | (c >> 12), 0x80 | ((c >> 6) & 0x3f), 0x80 | (c & 0x3f)));
	}
	return out.join("");
}

/* Ask for a line of text on the bottom line.  Only printable characters get in; Enter accepts, Esc cancels (returns
 * null).  secret = show * instead of the characters. */
function form_line_edit(prompt, initial, maxlen, secret)
{
	var val = form_clean(initial, maxlen), key, clicks_on = (console.status & CON_MOUSE_CLK_PASSTHRU) != 0;
	mouse_clicks(false);		/* mouse reports must not end up in the text */
	try {
		for (;;) {
			var shown = "", i;
			for (i = 0; i < val.length; i++) {
				var cp = unicode_to_cp437(val.charCodeAt(i));
				shown += secret ? "*" : (cp >= 0 ? String.fromCharCode(cp) : "?");
			}
			var head = " " + prompt + ": ", room = console.screen_columns - 2 - head.length;
			if (shown.length > room)
				shown = shown.substr(shown.length - room);
			status_line(head + shown);
			console.gotoxy(head.length + shown.length + 1, console.screen_rows);
			key = console.getkey(K_NONE);
			if (!bbs.online || !key)
				return null;
			if (key == "\r" || key == "\n")
				return val;
			if (key == "\x1b") {
				if (read_mouse() === false)
					return null;		/* the Esc key */
				continue;
			}
			if (key == "\b" || key == "\x7f")
				val = val.substr(0, val.length - 1);
			else if (key == "\x15")
				val = "";
			else if (key.length == 1 && key.charCodeAt(0) >= 0x20 && key.charCodeAt(0) != 0x7f && val.length < maxlen)
				val += String.fromCharCode(form_cp437_unicode(key));
		}
	}
	finally {
		mouse_clicks(clicks_on);
	}
}

/* The value of a control now: what the user entered, else what the page said */
function form_field_value(f, st)
{
	var v = st[f.key];
	if (f.type == "checkbox" || f.type == "radio")
		return v !== undefined ? !!v : f.checked;
	if (f.type == "select")
		return v !== undefined && v >= 0 && v < f.options.length ? v : f.sel;
	return v !== undefined ? v : f.value;
}

/* Send a form: returns { nav: address of the page to show next } or { error: "why not" } */
function form_submit(lib, form, clicked, st, base)
{
	var now = time(), i, f;
	while (FORM_SENT.length && now - FORM_SENT[0] > 60)
		FORM_SENT.shift();
	if (FORM_SENT.length >= FORM_MAX_PER_MINUTE)
		return { error: "Too many forms sent just now; please wait a moment" };
	var action = http_clean_url(html_url_resolve(form.base || base, form.action || ""));
	if (!action || !web_allowed(lib, action))
		return { error: "This form sends to a site that this library doesn't allow" };
	var pairs = [], v;
	for (i = 0; i < form.fields.length; i++) {
		f = form.fields[i];
		var name = form_clean(f.name, 100);
		if (f.disabled || !name.length || f.type == "file" || f.type == "reset")
			continue;
		if (f.type == "submit" || f.type == "button" || f.type == "image") {
			if (f !== clicked)
				continue;
			v = f.value;
		}
		else if (f.type == "checkbox" || f.type == "radio") {
			if (!form_field_value(f, st))
				continue;
			v = f.value;
		}
		else if (f.type == "select") {
			if (!f.options.length)
				continue;
			v = f.options[form_field_value(f, st)].value;
		}
		else {
			v = form_field_value(f, st);
			if (f.type == "password" && String(v).length && !/^https:/i.test(action))
				return { error: "Not sending a password over an unencrypted connection" };
		}
		pairs.push([form_utf8(name), form_utf8(form_clean(v, FORM_MAX_VALUE))]);
	}
	var body = http_form_body(pairs);
	if (body.length > FORM_MAX_BODY)
		return { error: "There is too much data to send" };
	FORM_SENT.push(now);
	/* the log says where a form went, never what was in it */
	log(LOG_INFO, "univiewerlibrary: user " + user.number + " sent a " + form.method + " form to " + action.replace(/[?#].*$/, ""));
	if (form.method == "GET") {
		var target = http_clean_url(http_get_url(action, body));
		return target ? { nav: target } : { error: "The address would be too long" };
	}
	var r = http_request({ method: "POST", url: action, body: body, maxbytes: lib.max_page, types: /^(text\/html|application\/xhtml|text\/plain)/i,
		allowed: function (u) { return web_allowed(lib, u); } });
	if (r.error)
		return { error: r.error };
	var id = md5_calc(r.url + "|" + system.timer + "|" + Math.random() + "|" + user.number, true);
	mkpath(web_dir());
	http_cache_write(web_dir() + "post-" + id + ".dat", r.url, r.body);
	return { nav: "sixelpost:" + id };
}

/* The user chose control number idx on the page url.  Returns { page } to show the same page again (zero based, the
 * page the control is on), { nav } to go to another page, or { error }. */
function form_activate(lib, d, url, idx)
{
	var f = d.fields ? d.fields[idx] : null;
	if (!f)
		return {};
	var st = FORM_STATE[url.replace(/#.*$/, "")] = FORM_STATE[url.replace(/#.*$/, "")] || {};
	var again = { page: f.page };
	var i, g;
	switch (f.type) {
		case "checkbox":
			st[f.key] = !form_field_value(f, st);
			return again;
		case "radio":
			for (i = 0; i < f.form.fields.length; i++) {
				g = f.form.fields[i];
				if (g.type == "radio" && g.name == f.name)
					st[g.key] = (g === f);
			}
			return again;
		case "select":
			if (f.options.length)
				st[f.key] = (form_field_value(f, st) + 1) % f.options.length;
			return again;
		case "submit": case "button": case "image":
			return form_submit(lib, f.form, f, st, url);
	}
	/* a box to type in */
	var v = form_line_edit(f.prompt, String(form_field_value(f, st)), f.maxlength, f.type == "password");
	if (v === null)
		return again;
	st[f.key] = form_clean(v, f.maxlength);
	var texts = 0;
	for (i = 0; i < f.form.fields.length; i++)
		if (form_is_text(f.form.fields[i]) && f.form.fields[i].type != "textarea" && !f.form.fields[i].disabled)
			texts++;
	if (texts == 1 && f.type != "textarea" && String(st[f.key]).length)
		return form_submit(lib, f.form, null, st, url);		/* Enter in the only text box sends the form */
	return again;
}

/* Number of colour components of a JPEG, or 0 */
function jpeg_components(s)
{
	var p = 2;
	while (p + 10 < s.length) {
		if (s.charCodeAt(p) != 0xff) {
			p++;
			continue;
		}
		var m = s.charCodeAt(p + 1);
		if (m == 0xff || m == 0 || m == 1 || (m >= 0xd0 && m <= 0xd8)) {
			p += m == 0xff ? 1 : 2;
			continue;
		}
		if (m == 0xc0 || m == 0xc1 || m == 0xc2)
			return s.charCodeAt(p + 9);
		p += 2 + ((s.charCodeAt(p + 2) << 8) | s.charCodeAt(p + 3));
	}
	return 0;
}

/* Shrink an RGB picture by a whole factor so that it is at most maxw wide */
function image_shrink(im, maxw)
{
	var f = Math.ceil(im.w / maxw);
	if (f < 2)
		return im;
	var w = Math.floor(im.w / f), h = Math.floor(im.h / f);
	var out = new Uint8Array(w * h * 3), al = im.alpha ? new Uint8Array(w * h) : null;
	var n = f * f;
	for (var y = 0; y < h; y++) {
		for (var x = 0; x < w; x++) {
			var r = 0, g = 0, b = 0, a = 0;
			for (var yy = 0; yy < f; yy++) {
				for (var xx = 0; xx < f; xx++) {
					var q = ((y * f + yy) * im.w + x * f + xx);
					r += im.data[q * 3]; g += im.data[q * 3 + 1]; b += im.data[q * 3 + 2];
					if (al)
						a += im.alpha[q];
				}
			}
			var o = (y * w + x) * 3;
			out[o] = r / n; out[o + 1] = g / n; out[o + 2] = b / n;
			if (al)
				al[y * w + x] = a / n;
		}
	}
	return { w: w, h: h, data: out, alpha: al };
}

/* Picture bytes -> what HtmlBuilder wants, or null */
/* A PNG that can go into the book as it is (8 bits or less, not interlaced, no transparency, and gray, RGB or palette):
 * its compressed data is kept and the viewer decodes it only when the picture is shown.  Returns null otherwise. */
function png_passthrough(bytes)
{
	if (bytes.substr(0, 8) != "\x89PNG\r\n\x1a\n")
		return null;
	var pos = 8, hdr = null, plte = null, idat = [], bad = false;
	while (pos + 12 <= bytes.length) {
		var len = (bytes.charCodeAt(pos) << 24 | bytes.charCodeAt(pos + 1) << 16 | bytes.charCodeAt(pos + 2) << 8 | bytes.charCodeAt(pos + 3)) >>> 0;
		var type = bytes.substr(pos + 4, 4);
		if (pos + 12 + len > bytes.length)
			return null;
		var body = bytes.substr(pos + 8, len);
		if (type == "IHDR")
			hdr = body;
		else if (type == "PLTE")
			plte = body;
		else if (type == "IDAT")
			idat.push(body);
		else if (type == "tRNS" || type == "acTL")
			bad = true;
		else if (type == "IEND")
			break;
		pos += 12 + len;
	}
	if (bad || !hdr || hdr.length < 13 || !idat.length)
		return null;
	var w = (hdr.charCodeAt(0) << 24 | hdr.charCodeAt(1) << 16 | hdr.charCodeAt(2) << 8 | hdr.charCodeAt(3)) >>> 0;
	var h = (hdr.charCodeAt(4) << 24 | hdr.charCodeAt(5) << 16 | hdr.charCodeAt(6) << 8 | hdr.charCodeAt(7)) >>> 0;
	var depth = hdr.charCodeAt(8), ct = hdr.charCodeAt(9), il = hdr.charCodeAt(12);
	if (il || depth > 8 || !w || !h || w > 32768 || h > 32768 || (ct != 0 && ct != 2 && ct != 3))
		return null;
	if (ct == 2 && depth != 8)
		return null;
	if (ct == 3 && (!plte || plte.length < 3 || plte.length % 3))
		return null;
	return { w: w, h: h, png: { data: idat.join(""), depth: depth, ct: ct, plte: plte } };
}

/* RunLengthDecode encoding of a byte array */
function run_length_encode(a)
{
	var out = [], n = a.length, i = 0;
	while (i < n) {
		var run = 1;
		while (i + run < n && run < 128 && a[i + run] == a[i])
			run++;
		if (run > 1) {
			out.push(String.fromCharCode(257 - run, a[i]));
			i += run;
			continue;
		}
		var j = i + 1;
		while (j < n && j - i < 128 && !(j + 1 < n && a[j] == a[j + 1]))
			j++;
		var chunk = [String.fromCharCode(j - i - 1)];
		for (var k = i; k < j; k++)
			chunk.push(String.fromCharCode(a[k]));
		out.push(chunk.join(""));
		i = j;
	}
	out.push("\x80");
	return out.join("");
}

/* A decoded picture with at most 256 colors (a GIF, say) as palette indexes, run-length coded, which is far smaller than RGB;
 * null if it has more colors than that. */
function picture_indexed(im)
{
	var n = im.w * im.h, d = im.data, al = im.alpha, map = {}, pal = [], idx = new Uint8Array(n);
	for (var i = 0; i < n; i++) {
		var r = d[i * 3], g = d[i * 3 + 1], b = d[i * 3 + 2];
		if (al && al[i] < 255) {
			var a = al[i] / 255;
			r = Math.round(r * a + 255 * (1 - a));
			g = Math.round(g * a + 255 * (1 - a));
			b = Math.round(b * a + 255 * (1 - a));
		}
		var key = (r << 16) | (g << 8) | b, ci = map[key];
		if (ci === undefined) {
			if (pal.length >= 256)
				return null;
			ci = pal.length;
			map[key] = ci;
			pal.push(String.fromCharCode(r, g, b));
		}
		idx[i] = ci;
	}
	return { w: im.w, h: im.h, indexed: { pal: pal.join(""), data: run_length_encode(idx) } };
}

/* A one-frame GIF as palette indexes, without going through RGB: { w, h, indexed } or null (animated, partial frame, no palette).
 * A transparent color becomes white, which is what the page is. */
function gif_passthrough(bytes)
{
	if (bytes.substr(0, 3) != "GIF" || bytes.length < 30)
		return null;
	var b = string_to_bytes(bytes), p = 6;
	var sw = b[p] | (b[p + 1] << 8), sh = b[p + 2] | (b[p + 3] << 8), flags = b[p + 4];
	p += 7;
	if (!sw || !sh || sw * sh > 16 * 1024 * 1024)
		return null;
	var gpal = null, trans = -1;
	if (flags & 0x80) {
		gpal = bytes_sub(b, p, p + 3 * (2 << (flags & 7)));
		p += 3 * (2 << (flags & 7));
	}
	while (p < b.length) {
		var t = b[p++];
		if (t == 0x21) {
			var label = b[p++];
			if (label == 0xf9 && b[p] >= 4)
				trans = (b[p + 1] & 1) ? b[p + 4] : -1;
			while (p < b.length && b[p])
				p += b[p] + 1;
			p++;
		}
		else if (t == 0x2c) {
			var ix = b[p] | (b[p + 1] << 8), iy = b[p + 2] | (b[p + 3] << 8);
			var w = b[p + 4] | (b[p + 5] << 8), h = b[p + 6] | (b[p + 7] << 8), lf = b[p + 8];
			p += 9;
			var pal = gpal;
			if (lf & 0x80) {
				pal = bytes_sub(b, p, p + 3 * (2 << (lf & 7)));
				p += 3 * (2 << (lf & 7));
			}
			if (!pal || ix || iy || w != sw || h != sh)
				return null;
			var fr = gif_lzw(b, p + 1, b[p], w * h);
			/* anything after this frame means an animation: leave that to the full decoder */
			var q = fr.end;
			while (q < b.length && b[q] == 0x21) {
				q++;
				q++;
				while (q < b.length && b[q])
					q += b[q] + 1;
				q++;
			}
			if (q < b.length && b[q] == 0x2c)
				return null;
			var idx = fr.idx;
			if (lf & 0x40) {
				var out = new Uint8Array(w * h), src = 0, passes = [[0, 8], [4, 8], [2, 4], [1, 2]];
				for (var ps = 0; ps < 4; ps++)
					for (var y = passes[ps][0]; y < h; y += passes[ps][1]) {
						for (var x = 0; x < w; x++)
							out[y * w + x] = idx[src * w + x];
						src++;
					}
				idx = out;
			}
			var ps2 = [];
			for (var i = 0; i < pal.length; i++)
				ps2.push(String.fromCharCode(trans >= 0 && (i / 3 | 0) == trans ? 255 : pal[i]));
			return { w: w, h: h, indexed: { pal: ps2.join(""), data: run_length_encode(idx) } };
		}
		else
			return null;
	}
	return null;
}

function picture_for_page(bytes)
{
	if (!bytes || bytes.length < 16)
		return null;
	if (bytes.charCodeAt(0) == 0xff && bytes.charCodeAt(1) == 0xd8) {
		var comps = jpeg_components(bytes);
		var d = jpeg_dims(string_to_bytes(bytes.substr(0, 65536)));
		if ((comps == 1 || comps == 3) && d)
			return { w: d.w, h: d.h, jpeg: bytes, comps: comps };
	}
	if (/^\s*(<\?xml|<svg)/i.test(bytes.substr(0, 200)))
		return null;
	var pt = png_passthrough(bytes) || gif_passthrough(bytes);
	if (pt)
		return pt;
	var im = null;
	try {
		im = image_from_string(bytes, "picture");
		if (im.jpeg)
			im = jpeg_image(im.jpeg, 8, null);
	}
	catch (e) {
		im = null;
	}
	if (!im || !im.data || im.n != 3)
		return null;
	im = image_shrink({ w: im.w, h: im.h, data: im.data, alpha: im.alpha }, 700);
	return picture_indexed(im) || im;
}

function read_whole(path)
{
	var f = new File(path);
	if (!f.open("rb"))
		return null;
	var s = f.read();
	f.close();
	return s;
}

/* A path inside dir (no .. escapes)?  Both are normalised, forward-slash paths. */
function path_inside(dir, path)
{
	dir = backslash(dir).replace(/\\/g, "/");
	path = path.replace(/\\/g, "/");
	return path.substr(0, dir.length) == dir && path.indexOf("/../") < 0;
}

function url_unescape(s)
{
	try {
		return decodeURIComponent(s);
	}
	catch (e) {
		return s;
	}
}

/* Build a viewer document from an HTML source.  url is an http(s) URL (web libraries) or file:///path (local
 * files, which must be inside the library directory).  Returns { doc, page } (page = 1-based start page). */
function html_document(lib, url, opts)
{
	opts = opts || {};
	var post = /^sixelpost:/i.test(url);		/* the answer to a form that was sent, kept in the cache */
	var local = /^file:/i.test(url);
	var frag = url.indexOf("#") >= 0 ? url.substr(url.indexOf("#") + 1) : "";
	var main = url.replace(/#.*$/, "");
	var statekey = main;
	var base, src, path = "";
	if (post) {
		var pm = /^sixelpost:([0-9a-f]{32})$/.exec(main);
		var pc = pm && lib.web ? http_cache_read(web_dir() + "post-" + pm[1] + ".dat", 3600) : null;
		if (!pc)
			throw "That page is no longer available";
		src = pc.body;
		base = pc.url;
		main = pc.url;
	}
	else if (local) {
		path = url_unescape(main.replace(/^file:\/\//i, ""));
		if (lib.web || !path_inside(lib.dir, path))
			throw "That file isn't available";
		src = read_whole(path);
		if (src === null)
			throw "Unable to read the file";
		base = path;
	}
	else {
		var r = web_fetch(lib, main, lib.max_page, (lib.options && lib.options.cachettl !== undefined && Number(lib.options.cachettl) >= 0 ? Number(lib.options.cachettl) : 600));
		if (!r)
			throw "Unable to load the page";
		src = r.body;
		base = r.url;
		main = r.url;
	}
	var forms_on = !local && lib.web && lib.forms;
	var builder = new HtmlBuilder({
		forms: forms_on,
		form_state: forms_on ? (FORM_STATE[statekey] = FORM_STATE[statekey] || {}) : null,
		resolve_image: function (isrc, ibase) {
			if (!lib.images || /^data:/i.test(isrc))
				return null;
			var abs = html_url_resolve(ibase, isrc);
			var bytes;
			if (local) {
				abs = url_unescape(abs.replace(/[?#].*$/, ""));
				if (!path_inside(lib.dir, abs))
					return null;
				bytes = read_whole(abs);
			}
			else {
				var ir = web_fetch(lib, abs, lib.max_image, 7 * 86400);
				bytes = ir ? ir.body : null;
			}
			return picture_for_page(bytes);
		},
		link: function (href, lbase) {
			if (href.charAt(0) == "#")
				return href.length > 1 ? { anchor: href.substr(1) } : null;
			if (/^(javascript|mailto|tel|data):/i.test(href))
				return null;
			var abs = html_url_resolve(lbase, href);
			var nf = abs.indexOf("#") >= 0 ? abs.substr(abs.indexOf("#") + 1) : "";
			var nopart = abs.replace(/#.*$/, "");
			if (local) {
				var lp = url_unescape(nopart.replace(/[?].*$/, ""));
				if (lp == path)
					return nf ? { anchor: nf } : null;
				if (!path_inside(lib.dir, lp) || !/\.(html?|xhtml)$/i.test(lp) || !file_exists(lp))
					return null;
				return { uri: "file://" + lp + (nf ? "#" + nf : "") };
			}
			if (nopart == main)
				return nf ? { anchor: nf } : null;
			return web_allowed(lib, nopart) ? { uri: abs } : null;
		}
	});
	builder.add_html(src, base, undefined, !local && lib.content.length ? lib.content : undefined);
	mkpath(web_dir());
	var pdf = web_dir() + md5_calc(url + "|" + (local ? file_date(path) : time()), true) + ".pdf";
	builder.write(pdf);
	var doc = load_pdf(pdf);
	doc.web = true;
	doc.allow_uri = true;
	doc.link_label = function (uri) { return form_link_label(builder.fields, uri); };
	var page = 1;
	if (frag && builder.anchors[frag])
		page = builder.anchors[frag].page + 1;
	if (opts.page > 0)
		page = opts.page;
	return { doc: doc, page: page, fields: builder.fields };
}

/* View an HTML document and the documents its links lead to.  Returns "next"/"prev" if the user asked for the
 * neighboring file, else "". */
function view_html_chain(lib, first, browse)
{
	var stack = [], url = first, startpage = 0;
	while (url && bbs.online) {
		var d;
		try {
			console.clear();
			console.print(unescape_text(T("loadingText")) + "\x01n");
			d = html_document(lib, url, startpage ? { page: startpage } : undefined);
			startpage = 0;
		}
		catch (e) {
			console.clear();
			pause_error(unescape_text(T("errorText")).replace("%s", String(e).replace(/\x01/g, "")));
			if (!stack.length)
				return "";
			url = stack.pop();
			continue;
		}
		var res = viewer(d.doc, d.page, browse && !stack.length) || "";
		if (res.substr(0, 5) == "link:") {
			if (/^sixelform:[0-9]+$/.test(res.substr(5))) {
				/* a form control: change it (and show the page again), or send the form and show the answer */
				var fr = form_activate(lib, d, url, parseInt(res.substr(5 + 10), 10));
				if (fr.error) {
					console.clear();
					pause_error(unescape_text(T("errorText")).replace("%s", String(fr.error).replace(/\x01/g, "")));
				}
				if (fr.nav) {
					stack.push(url);
					url = fr.nav;
				}
				else
					startpage = (fr.page !== undefined ? fr.page : d.page - 1) + 1;
				continue;
			}
			stack.push(url);
			url = res.substr(5);
			continue;
		}
		if (res == "back") {
			url = stack.pop();
			continue;
		}
		FORM_STATE = {};
		return res == "next" || res == "prev" ? res : "";
	}
	FORM_STATE = {};
	return "";
}

/* --- EPUB --- */

function xml_find(el, name, out)
{
	out = out || [];
	if (el.name == name)
		out.push(el);
	for (var i = 0; i < el.children.length; i++)
		xml_find(el.children[i], name, out);
	return out;
}

function xml_text(el)
{
	var s = el.text || "";
	for (var i = 0; i < el.children.length; i++)
		s += xml_text(el.children[i]);
	return s;
}

function epub_document(lib, path)
{
	var dest = BASE_DIR + ".cache/epub/" + md5_calc(path + "|" + file_date(path) + "|" + file_size(path), true) + "/";
	if (!file_isdir(dest)) {
		var err = extract_archive(path, dest, file_getname(path));
		if (err)
			throw err;
	}
	var container = read_whole(dest + "META-INF/container.xml");
	if (!container)
		throw "Not a valid EPUB file";
	var rf = xml_find(svg_parse_xml(container), "rootfile");
	if (!rf.length || !rf[0].attrs["full-path"])
		throw "Not a valid EPUB file";
	var opfpath = dest + rf[0].attrs["full-path"];
	var opfdir = opfpath.replace(/[^\/]*$/, "");
	var opfsrc = read_whole(opfpath);
	if (!opfsrc)
		throw "Not a valid EPUB file";
	var opf = svg_parse_xml(opfsrc);
	var manifest = {}, items = xml_find(opf, "item"), i;
	for (i = 0; i < items.length; i++)
		manifest[items[i].attrs.id] = items[i].attrs;
	var spine = xml_find(opf, "itemref");
	var spineel = xml_find(opf, "spine");
	var navfile = null, ncx = null;
	for (i = 0; i < items.length; i++) {
		if (/\bnav\b/.test(items[i].attrs.properties || ""))
			navfile = items[i].attrs.href;
	}
	if (spineel.length && spineel[0].attrs.toc && manifest[spineel[0].attrs.toc])
		ncx = manifest[spineel[0].attrs.toc].href;
	if (!ncx) {
		for (i = 0; i < items.length; i++) {
			if (items[i].attrs["media-type"] == "application/x-dtbncx+xml")
				ncx = items[i].attrs.href;
		}
	}
	var norm = function (p) { return html_url_resolve("/", "/" + p.replace(/^\/+/, "")).replace(/^\//, ""); };
	var key_of = function (base, id) { return base.replace(/\\/g, "/") + "#" + id; };
	var builder = new HtmlBuilder({
		anchor_key: key_of,
		resolve_image: function (isrc, ibase) {
			if (/^data:/i.test(isrc))
				return null;
			var abs = url_unescape(html_url_resolve(ibase, isrc).replace(/[?#].*$/, ""));
			if (!path_inside(dest, abs))
				return null;
			return picture_for_page(read_whole(abs));
		},
		link: function (href, lbase) {
			if (/^[a-zA-Z][a-zA-Z0-9+.-]*:/.test(href))
				return null;
			var abs = url_unescape(html_url_resolve(lbase, href));
			var f = abs.indexOf("#") >= 0 ? abs.substr(abs.indexOf("#") + 1) : "";
			var fp = abs.replace(/#.*$/, "");
			if (!fp.length)
				fp = lbase;
			return { anchor: key_of(fp, f) };
		}
	});
	var count = 0;
	for (i = 0; i < spine.length && count < 600; i++) {
		var it = manifest[spine[i].attrs.idref];
		if (!it || !/x?html/.test(it["media-type"] || "") || spine[i].attrs.linear == "no")
			continue;
		var file = url_unescape(opfdir + it.href.replace(/[?#].*$/, ""));
		file = url_unescape(html_url_resolve("/", file.replace(/^\//, "/"))).replace(/\\/g, "/");
		var csrc = read_whole(file);
		if (csrc === null)
			continue;
		builder.add_html(csrc, file);
		count++;
	}
	if (!count)
		throw "The book has no readable pages";
	/* table of contents */
	var toc = [];
	function toc_key(tocbase, href)
	{
		var abs = url_unescape(html_url_resolve(tocbase, href));
		var f = abs.indexOf("#") >= 0 ? abs.substr(abs.indexOf("#") + 1) : "";
		return key_of(abs.replace(/#.*$/, ""), f);
	}
	if (navfile) {
		var navpath = url_unescape(opfdir + navfile);
		var navsrc = read_whole(navpath);
		if (navsrc) {
			var navs = xml_find(svg_parse_xml(navsrc), "nav");
			var tocnav = null;
			for (i = 0; i < navs.length; i++) {
				if (navs[i].attrs.type == "toc" || !tocnav)
					tocnav = navs[i];
			}
			var walk = function (el, level) {
				for (var j = 0; j < el.children.length; j++) {
					var c = el.children[j];
					if (c.name == "a" && c.attrs.href) {
						var tt = xml_text(c).replace(/\s+/g, " ").replace(/^ | $/g, "");
						if (tt)
							toc.push({ level: level, title: tt, key: toc_key(navpath, c.attrs.href) });
					}
					else if (c.name == "ol" || c.name == "ul")
						walk(c, level + 1);
					else
						walk(c, level);
				}
			};
			if (tocnav)
				walk(tocnav, 0);
		}
	}
	if (!toc.length && ncx) {
		var ncxpath = url_unescape(opfdir + ncx);
		var ncxsrc = read_whole(ncxpath);
		if (ncxsrc) {
			var walkn = function (el, level) {
				for (var j = 0; j < el.children.length; j++) {
					var c = el.children[j];
					if (c.name == "navPoint") {
						var lab = xml_find(c, "text"), con = xml_find(c, "content");
						if (lab.length && con.length && con[0].attrs.src)
							toc.push({ level: level, title: xml_text(lab[0]).replace(/\s+/g, " ").replace(/^ | $/g, ""), key: toc_key(ncxpath, con[0].attrs.src) });
						walkn(c, level + 1);
					}
					else
						walkn(c, level);
				}
			};
			walkn(svg_parse_xml(ncxsrc), 1);
		}
	}
	mkpath(web_dir());
	var pdf = web_dir() + md5_calc("epub|" + path + "|" + file_date(path), true) + ".pdf";
	builder.write(pdf, toc.length ? toc : undefined);
	var doc = load_pdf(pdf);
	doc.allow_uri = false;
	return doc;
}

function view_epub(entry)
{
	console.clear();
	console.print(unescape_text(T("loadingText")) + "\x01n");
	var doc = epub_document(CUR_LIB, entry.path);
	return viewer(doc, 0, true) || "";
}

/* For files with an unknown extension: PDF, picture or other binary data rather than text? */
function looks_binary(path)
{
	var f = new File(path);
	if (!f.open("rb"))
		return false;
	var h = f.read(1024);
	f.close();
	if (/^(%PDF-|\x89PNG|GIF8|\xff\xd8\xff|BM|P[1-6]\s|II\*\x00|MM\x00\*|\x00\x00\x01\x00|FORM|PK\x03\x04)/.test(h))
		return true;
	if (/<svg[\s>]/i.test(h))
		return true;
	return h.indexOf("\x00") >= 0;
}

function view_file(entry)
{
	var nav = view_file_inner(entry);
	swallow_right_click();
	return nav;
}

function view_file_inner(entry)
{
	var doc;
	try {
		var xt = file_ext(entry.name);
		if (MUSIC_TYPES.indexOf(xt) >= 0) {
			/* the music files beside this one make the playlist; the player handles next and previous itself */
			var mpl = music_playlist_for(entry.path);
			/* music_kbps in the library's own section overrides the one in the root (0 turns it off) */
			var lo = CUR_LIB && CUR_LIB.options ? CUR_LIB.options : {};
			var mk = lo.music_kbps !== undefined && Number(lo.music_kbps) >= 0 ? Number(lo.music_kbps) : opt.music_kbps;
			music_player(mpl.list, mpl.index, mk, opt.ffmpeg);
			return "";
		}
		if (TEXT_TYPES.indexOf(xt) >= 0)
			return view_text(entry.path);
		if (SUPPORTED_TYPES.indexOf(xt) < 0 && !looks_binary(entry.path))
			return view_text(entry.path);	/* unknown extension: text unless it is clearly something else */
		if (file_ext(entry.name) == "epub")
			return view_epub(entry);
		if (/^(html?|xhtml)$/.test(file_ext(entry.name)) && CUR_LIB)
			return view_html_chain(CUR_LIB, "file://" + entry.path.replace(/\\/g, "/"), true);
		var f = new File(entry.path);
		var head = "";
		if (f.exists && f.open("rb")) {
			head = f.read(1024);
			f.close();
		}
		status_line(" Loading ...");		/* big files take a while to read; "Rendering" follows once the file is in */
		if (head.indexOf("%PDF-") >= 0)
			doc = load_pdf(entry.path);
		else if (file_ext(entry.name) == "svg" || /<svg[\s>]/i.test(head))
			doc = load_svg(entry.path);
		else
			doc = new ImageDoc(load_image_file(entry.path));
		return viewer(doc, 0, true) || "";
	}
	catch (e) {
		console.clear();
		pause_error(unescape_text(T("errorText")).replace("%s", String(e).replace(/\x01/g, "")));
	}
	return "";
}

/* Fetch (for online libraries) and view a file.  Returns "prev"/"next" from the text viewer, else "" */
function open_entry(lib, hook, subpath, e, image)
{
	CUR_LIB = lib;
	if (lib.module) {
		console.gotoxy(1, console.screen_rows);
		console.print(unescape_text(T("loadingText")) + "\x01n");
		console.cleartoeol();
		var path = hook && hook.fetch ? hook.fetch(subpath, e, image ? "image" : undefined) : null;
		if (path && image && file_size(path) > opt.image_max_bytes) {
			console.clear();
			pause_error(unescape_text(T("errorText")).replace("%s", "image is too large"));
			return "";
		}
		if (!path) {
			console.clear();
			pause_error(unescape_text(T("errorText")).replace("%s", "download failed"));
			return "";
		}
		e = { name: image ? e.name + ".png" : e.name, path: path, isdir: false };
	}
	return view_file(e);
}

/* Index of the next/previous viewable file from index i, or -1 */
function neighbor(entries, i, dir)
{
	for (i += dir; i >= 0 && i < entries.length; i += dir) {
		if (!entries[i].isdir && !entries[i].iszip)
			return i;
	}
	return -1;
}

function browse_library(lib)
{
	if (lib.web) {
		CUR_LIB = lib;
		view_html_chain(lib, lib.url, false);
		return;
	}
	var subpath = [];
	var sel = 0;		/* item to put the cursor on */
	var parents = [];	/* selected index in each parent directory */
	while (bbs.online) {
		var entries;
		if (lib.module) {
			console.clear();
			console.print(unescape_text(T("loadingText")) + "\x01n");
			entries = list_hook_entries(lib, subpath);
			if (!entries) {
				console.clear();
				pause_error(unescape_text(T("hookErrorText")));
				if (!subpath.length)
					return;
				subpath.pop();
				sel = parents.pop() || 0;
				continue;
			}
		}
		else
			entries = list_entries(lib, subpath);
		var cols = [
			{ head: "Name", scroll: true, maxw: 30, color: "listNameColor", hicolor: "listNameHighlightColor",
				dircolor: "listDirColor", dirhicolor: "listDirHighlightColor",
				get: function (e) { return e.label ? e.label : e.isdir && !e.up ? e.name + "/" : e.name; } },
			{ head: "Description", scroll: true, color: "listDescColor", hicolor: "listDescHighlightColor",
				dircolor: "listDescColor", dirhicolor: "listDescHighlightColor",
				get: function (e) { return e.desc || ""; } },
			{ head: "Size", width: 6, align: "right", color: "listSizeColor", hicolor: "listSizeHighlightColor",
				get: function (e) { return e.isdir || !e.size ? "" : format_size(e.size); } },
			{ head: "Date", width: 10, color: "listDateColor", hicolor: "listDateHighlightColor",
				get: function (e) { return e.up || !e.date ? "" : format_date(e.date); } }
		];
		var where = lib.name + (subpath.length ? "/" + subpath.join("/") : "");
		var hook = lib.module ? open_hook(lib) : null;
		var has_images = hook && hook.images;
		var scrollable = false;
		for (var si = 0; si < entries.length && !scrollable; si++)
			scrollable = !!entries[si].desc || String(entries[si].name).length > 30;
		var help = function () {
			return [["Enter", "View"]].concat(has_images ? [["I", "Image"]] : [], [["Up/Dn", "Select"]],
				scrollable ? [["Left/Right", "Scroll"]] : [],
				option_help(), [["Q", "Back"]]);
		};
		var onkey = function (key, item) {
			if (has_images && (key == "i" || key == "I"))
				return item && !item.isdir ? "exit" : false;
			return option_key(key);
		};
		var res = lightbar(unescape_text(T("titleText")).replace(/\x01./g, ""), where, cols, entries, help, onkey, sel);
		if (!res) {
			/* Q / Esc goes up one level, like the .. entry, and leaves the library only from its top level */
			if (subpath.length) {
				subpath.pop();
				sel = parents.pop() || 0;
				continue;
			}
			return;
		}
		var e = res.item;
		sel = res.index;
		if (e.up) {
			if (!subpath.length)
				return;
			subpath.pop();
			sel = parents.pop() || 0;
		}
		else if (e.isdir) {
			subpath.push(e.name);
			parents.push(sel);
			sel = 0;
		}
		else if (e.iszip) {
			var err = extract_zip(e);
			if (err) {
				console.clear();
				pause_error(unescape_text(T("errorText")).replace("%s", err));
			}
			else {
				subpath.push(file_getname(zip_dest(e.path)));
				parents.push(sel);
				sel = 0;
			}
		}
		else {
			var image = res.action == "i" || res.action == "I";
			var nav = open_entry(lib, hook, subpath, e, image);
			/* N and P in the viewers move on to the next/previous file */
			while (nav == "prev" || nav == "next") {
				var n = neighbor(entries, sel, nav == "next" ? 1 : -1);
				if (n < 0)
					break;
				sel = n;
				nav = open_entry(lib, hook, subpath, entries[n], false);
			}
		}
	}
}

function main()
{
	var settings = read_settings();
	if (!settings) {
		console.print("\x01n\x01h\x01rUniversal Viewer Library is not configured (" + file_getname(settings_path()) + " not found).\x01n\r\n");
		console.pause();
		return;
	}
	read_theme(settings.theme);

	var libs = [];
	var have_hook = false;
	for (var i = 0; i < settings.libraries.length; i++) {
		if (library_allowed(settings.libraries[i])) {
			libs.push(settings.libraries[i]);
			if (settings.libraries[i].module)
				have_hook = true;
		}
	}
	clean_cache();
	if (!libs.length) {
		console.print(unescape_text(T("noLibrariesText")) + "\x01n\r\n");
		console.pause();
		return;
	}

	/* Only one library available: go straight into it */
	if (libs.length == 1) {
		browse_library(libs[0]);
		return;
	}

	var items = [];
	for (i = 0; i < libs.length; i++) {
		items.push({ lib: libs[i], num: i + 1, name: libs[i].name, desc: libs[i].desc,
			files: libs[i].subdirs || libs[i].module || libs[i].web ? "" : String(count_files(libs[i])) });
	}
	var cols = [
		{ head: "  #", scroll: true, color: "listNumColor", hicolor: "listNumHighlightColor",
			get: function (it) { return ("   " + it.num).slice(-3); } },
		{ head: "Library", scroll: true, maxw: 24, color: "listNameColor", hicolor: "listNameHighlightColor",
			get: function (it) { return it.name; } },
		{ head: "Description", scroll: true, color: "listDescColor", hicolor: "listDescHighlightColor",
			get: function (it) { return it.desc; } },
		{ head: "Files", width: 5, align: "right", color: "listSizeColor", hicolor: "listSizeHighlightColor",
			get: function (it) { return it.files; } }
	];
	var longest = 0;
	for (i = 0; i < items.length; i++)
		longest = Math.max(longest, Math.min(24, String(items[i].name).length) + String(items[i].desc || "").length);
	var scrollable = longest + 10 > console.screen_columns - 6;
	var help = function () {
		return [["Enter", "Open"], ["Up/Dn", "Select"]].concat(scrollable ? [["Left/Right", "Scroll"]] : [], option_help(), [["Q", "Quit"]]);
	};
	var sel = 0;
	while (bbs.online) {
		var res = lightbar(unescape_text(T("titleText")).replace(/\x01./g, ""), "", cols, items, help, option_key, sel);
		if (!res)
			break;
		sel = res.index;
		browse_library(res.item.lib);
	}
}

try {
	mouse_start();
	try {
		main();
	}
	finally {
		mouse_stop();
	}
	console.attributes = 7;
	console.clear();
}
catch (e) {
	log(LOG_ERR, "univiewerlibrary error: " + e + (e && e.stack ? "\n" + e.stack : ""));
	console.print("\x01n\x01h\x01runiviewerlibrary error: \x01w" + String(e).replace(/\x01/g, "") + "\x01n\r\n");
}
