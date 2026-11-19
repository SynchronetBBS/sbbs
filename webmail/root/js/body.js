// Message bodies: decoding, the plain-text view, and the document an HTML
// body is shown in.
//
// decode(part)            a message.get body ({b64, charset}) as text: the
//                         bytes through TextDecoder(charset), or
//                         windows-1252 when the browser does not know the
//                         label (or knows it only as "replacement")
// textView(text, labels)  the plain-text view: an element holding the text
//                         as text nodes, http/https/www addresses as links
//                         (linkify.js), quoted lines (starting with ">")
//                         dimmed, and a long quoted block at the end
//                         hidden behind a "Show quoted text" button.
//                         labels: { show, hide } (the button's texts)
// prepareHtml(html, cids)
//                         the message HTML made ready for its frame: parsed
//                         into an inert document (DOMParser: no script
//                         runs, nothing loads), then every <link> and
//                         <base> element removed (resource hints would let
//                         a browser look up or connect to a host with no
//                         click), every link given target="_blank" and
//                         rel="noopener noreferrer", cid: references in
//                         image attributes (src, background, poster, SVG
//                         image href, srcset entries) and in CSS url()
//                         replaced from cids ({ "<lower-cased id>": data
//                         URL; at most MAX_INLINE characters of them per
//                         message, further references are left), and
//                         serialized again.  Parsing that text again need
//                         not give the same document, and the frame does
//                         parse it again, so the pass is repeated on its
//                         own output until the output stops changing (at
//                         most MAX_PASSES).  Returns { stable (false: it
//                         never settled; html is "" and the message must
//                         not be shown as HTML), html, text (the text of
//                         the last pass's document), remote (images other
//                         than data: URLs are referred to), dark (the
//                         message says it supports a dark color scheme),
//                         refs (the lower-cased Content-IDs it refers to),
//                         used (those replaced) }.  Throws if the HTML
//                         cannot be parsed at all.
// rewriteSrcset(value, fn) a srcset value with each whole URL replaced by
//                         fn(url), in one pass
// remoteCss(css)          true when CSS names an image that is not a data:
//                         URL
// frameDoc(html, opts)    the srcdoc of the sandboxed frame: a constant
//                         prefix (the frame's content policy, DNS prefetch
//                         off, <base target="_blank">, no referrer, a
//                         default style) and the prepared HTML.  opts:
//                         { images (true: https images may load), theme
//                         (null: a light page, white with dark text, as
//                         mail is written for; or { colors: { text, bg,
//                         link } as computed rgb() values, dark }: the
//                         app's colors, for a message that supports dark) }.
//                         The frame cannot run script (its sandbox), and the
//                         policy lets it load nothing but data: images (and
//                         https images once allowed) and inline styles.

import { h } from "./dom.js";
import { appendLinks } from "./linkify.js";

const QUOTE = /^[ \t]*>/;
const LONG_QUOTE = 7;		// quoted lines at the end hidden behind a button
const BOX = /[\u2500-\u259f]/;	// line drawing and block characters: a fixed-width font
// A terminal's control sequences and other control characters: never text
const CONTROL = /\u001b\[[0-9;?]*[ -\/]*[@-~]|[\u0000-\u0008\u000b\u000e-\u001f\u007f]/g;

export function decode(part) {
	if (!part || typeof part.b64 !== "string")
		return "";
	let bin;
	try {
		bin = atob(part.b64);
	} catch (e) {
		return "";
	}
	const bytes = new Uint8Array(bin.length);
	for (let i = 0; i < bin.length; i++)
		bytes[i] = bin.charCodeAt(i);
	// A UTF-8 byte order mark: UTF-8, whatever the label says
	if (bytes.length >= 3 && bytes[0] === 0xef && bytes[1] === 0xbb && bytes[2] === 0xbf)
		return new TextDecoder("utf-8").decode(bytes.subarray(3));
	let dec = null;
	try {
		dec = new TextDecoder(typeof part.charset === "string" ? part.charset : "");
	} catch (e) {
		dec = null;	// RangeError: a label this browser does not know
	}
	if (!dec || dec.encoding === "replacement")
		dec = new TextDecoder("windows-1252");
	return dec.decode(bytes);
}

// ---- Plain text ----

function quoted(line) {
	return QUOTE.test(line);
}

// The index of the first line of a long quoted block at the end (with the
// "... wrote:" line before it), or -1
function tailQuote(lines) {
	let end = lines.length;
	while (end > 0 && lines[end - 1].trim() === "")
		end--;
	let start = end;
	let n = 0;
	while (start > 0 && (quoted(lines[start - 1]) || lines[start - 1].trim() === "")) {
		start--;
		if (quoted(lines[start]))
			n++;
	}
	while (start < end && !quoted(lines[start]))
		start++;	// blank lines before the block stay in view
	if (n < LONG_QUOTE)
		return -1;
	let s = start;
	while (s > 0 && lines[s - 1].trim() === "")
		s--;
	if (s > 0 && /:\s*$/.test(lines[s - 1]))
		start = s - 1;	// "On <date>, <name> wrote:"
	return start > 0 ? start : -1;
}

// Lines into el: runs of quoted lines in a dimmed span
function appendLines(el, lines) {
	let i = 0;
	while (i < lines.length) {
		const q = quoted(lines[i]);
		let j = i;
		while (j < lines.length && quoted(lines[j]) === q)
			j++;
		let text = lines.slice(i, j).join("\n");
		if (j < lines.length)
			text += "\n";
		if (q) {
			const span = h("span", { class: "q" });
			appendLinks(span, text);
			el.append(span);
		} else
			appendLinks(el, text);
		i = j;
	}
}

let nextId = 1;

export function textView(text, labels) {
	text = text.replace(/\r\n?/g, "\n").replace(CONTROL, "");
	const el = h("div", { class: "body plain" });
	if (BOX.test(text))
		el.classList.add("mono");
	const lines = text.split("\n");
	while (lines.length > 1 && lines[lines.length - 1] === "")
		lines.pop();
	const cut = tailQuote(lines);
	if (cut < 0) {
		appendLines(el, lines);
		return el;
	}
	appendLines(el, lines.slice(0, cut));
	const rest = h("div", { class: "quoted-tail", id: "wm-quoted-" + (nextId++), hidden: true });
	appendLines(rest, lines.slice(cut));
	const btn = h("button", { class: "linkbtn quote-btn", type: "button", "aria-expanded": "false",
		"aria-controls": rest.id, text: labels.show });
	btn.addEventListener("click", () => {
		const open = rest.hidden;
		rest.hidden = !open;
		btn.setAttribute("aria-expanded", String(open));
		btn.textContent = open ? labels.hide : labels.show;
	});
	el.append(h("div", { class: "quote-toggle" }, btn), rest);
	return el;
}

// ---- HTML ----

const POLICY_BLOCKED = "default-src 'none'; img-src data:; style-src 'unsafe-inline'";
const POLICY_IMAGES = "default-src 'none'; img-src data: https:; style-src 'unsafe-inline'";
const RGB = /^rgba?\(\s*[0-9.]+%?\s*,\s*[0-9.]+%?\s*,\s*[0-9.]+%?\s*(,\s*[0-9.]+%?\s*)?\)$/;
const LIGHT = { text: "rgb(26, 26, 26)", bg: "rgb(255, 255, 255)", link: "rgb(11, 87, 208)" };

// A computed color as constant text for the frame's style, or ""
function color(c) {
	return typeof c === "string" && RGB.test(c) ? c : "";
}

export function frameDoc(html, opts) {
	const theme = opts.theme || null;
	const c = theme ? theme.colors || {} : LIGHT;
	const text = color(c.text);
	const bg = color(c.bg);
	const link = color(c.link);
	// The defaults go on <html>, so a message's own colors win (a bgcolor
	// or text attribute on <body> as well as its styles)
	let style = "html{" + (text ? "color:" + text + ";" : "") + (bg ? "background:" + bg + ";" : "")
		+ "font:15px/1.5 system-ui,-apple-system,\"Segoe UI\",Roboto,\"Helvetica Neue\",Arial,sans-serif;"
		+ "color-scheme:" + (theme && theme.dark ? "dark" : "light") + ";height:100%}"
		+ "body{margin:0;padding:12px 16px;min-height:100%;box-sizing:border-box;overflow-wrap:break-word}"
		+ "img{max-width:100%}pre{white-space:pre-wrap}";
	if (link)
		style += "a{color:" + link + "}";
	return "<meta http-equiv=\"Content-Security-Policy\" content=\""
		+ (opts.images === true ? POLICY_IMAGES : POLICY_BLOCKED) + "\">"
		+ "<meta http-equiv=\"x-dns-prefetch-control\" content=\"off\">"
		+ "<base target=\"_blank\">"
		+ "<meta name=\"referrer\" content=\"no-referrer\">"
		+ "<style>" + style + "</style>"
		+ html;
}

function remoteUrl(u) {
	const s = String(u || "").trim().toLowerCase();
	return s !== "" && !s.startsWith("data:") && !s.startsWith("cid:") && !s.startsWith("#")
		&& s !== "about:blank";
}

const CSS_URL = /url\(\s*(['"]?)([^'")]*)\1\s*\)/gi;

// true when CSS refers to an image that is not a data: URL (url(),
// @import, or a plain string in image-set())
export function remoteCss(css) {
	css = String(css);
	if (/@import/i.test(css))
		return true;
	let m;
	CSS_URL.lastIndex = 0;
	while ((m = CSS_URL.exec(css)) !== null) {
		if (remoteUrl(m[2]))
			return true;
	}
	// image-set() may name its images as plain strings; each part of the
	// text is looked at once
	const sets = /image-set\s*\(/gi;
	while ((m = sets.exec(css)) !== null) {
		const from = m.index + m[0].length;
		const rest = css.slice(from, from + 4096);
		let depth = 1;
		let end = rest.length;
		for (let i = 0; i < rest.length; i++) {
			if (rest[i] === "(")
				depth++;
			else if (rest[i] === ")" && --depth === 0) {
				end = i;
				break;
			}
		}
		const str = /(["'])([^"']*)\1/g;
		const inner = rest.slice(0, end);
		let q;
		while ((q = str.exec(inner)) !== null) {
			if (remoteUrl(q[2]))
				return true;
		}
		sets.lastIndex = Math.max(sets.lastIndex, from + end);
	}
	return false;
}

function space(c) {
	return c === " " || c === "\t" || c === "\n" || c === "\r" || c === "\f";
}

// A srcset value as its entries: [{ url, desc }] (a data: URL may hold
// commas; the descriptors run to the next comma)
function srcsetEntries(v) {
	const out = [];
	const s = String(v);
	let i = 0;
	while (i < s.length) {
		while (i < s.length && (space(s[i]) || s[i] === ","))
			i++;
		let j = i;
		while (j < s.length && !space(s[j]))
			j++;
		let k = j;
		while (k > i && s[k - 1] === ",")
			k--;
		const url = s.slice(i, k);
		if (k < j) {	// the URL ended with commas: no descriptors
			if (url !== "")
				out.push({ url, desc: "" });
			i = j;
			continue;
		}
		let d = j;
		while (d < s.length && s[d] !== ",")
			d++;
		if (url !== "")
			out.push({ url, desc: s.slice(j, d).trim() });
		i = d + 1;
	}
	return out;
}

// srcset value v rebuilt in one pass, each whole URL through fn(url)
// (which returns the URL to use); the value itself when nothing changed
export function rewriteSrcset(v, fn) {
	const list = srcsetEntries(v);
	let changed = false;
	const parts = [];
	for (const e of list) {
		const u = fn(e.url);
		if (u !== e.url)
			changed = true;
		parts.push(e.desc !== "" ? u + " " + e.desc : u);
	}
	return changed ? parts.join(", ") : v;
}

function cidKey(raw) {
	let id = raw;
	try {
		id = decodeURIComponent(raw);
	} catch (e) {
		// not URL-encoded
	}
	return id.trim().replace(/^<|>$/g, "").toLowerCase();
}

function cidOf(v) {
	const s = String(v || "").trim();
	return /^cid:/i.test(s) ? cidKey(s.slice(4)) : null;
}

// The attributes that can name an image, for an element
function imageAttr(tag, name) {
	if (name === "background" || name === "poster")
		return true;
	if (name === "src")
		return tag === "img" || tag === "input" || tag === "image";
	return name === "href" && (tag === "image" || tag === "feimage" || tag === "use");
}

function parse(html) {
	try {
		return new DOMParser().parseFromString(html, "text/html");
	} catch (e) {
		return null;
	}
}

const CSS_CID = /url\(\s*(['"]?)\s*cid:([^'")\s]+)\s*\1\s*\)/gi;

function cssRefs(css, refs) {
	let m;
	CSS_CID.lastIndex = 0;
	while ((m = CSS_CID.exec(css)) !== null)
		refs.add(cidKey(m[2]));
}

const MAX_INLINE = 40 * 1048576;	// characters of data: URLs put in per message

// A cid substitution: the data: URL for key, or null (not given, or the
// message's cap reached)
function substitute(cids, key, st) {
	if (!Object.prototype.hasOwnProperty.call(cids, key))
		return null;
	const url = cids[key];
	if (st.bytes + url.length > MAX_INLINE)
		return null;
	st.bytes += url.length;
	st.used.add(key);
	return url;
}

function cssReplace(css, cids, st) {
	return css.replace(CSS_CID, (all, q, id) => {
		const url = substitute(cids, cidKey(id), st);
		return url === null ? all : "url(\"" + url + "\")";
	});
}

// One cleaning pass over html: { html (serialized), remote, dark, text }
function cleanPass(html, cids, st) {
	const out = { html: "", remote: false, dark: false, text: "" };
	const doc = parse(html);
	if (!doc || !doc.documentElement)
		throw new Error("not parsed");
	for (const el of Array.from(doc.querySelectorAll("link, base")))
		el.remove();
	for (const el of doc.querySelectorAll("*")) {
		const tag = el.localName.toLowerCase();
		if (tag === "a" || tag === "area") {
			el.setAttribute("target", "_blank");
			el.setAttribute("rel", "noopener noreferrer");
		}
		if (tag === "meta") {
			const n = String(el.getAttribute("name") || "").toLowerCase();
			if ((n === "color-scheme" || n === "supported-color-schemes")
				&& /\bdark\b/i.test(el.getAttribute("content") || ""))
				out.dark = true;
		}
		if (tag === "style") {
			const css = el.textContent;
			if (/prefers-color-scheme\s*:\s*dark/i.test(css))
				out.dark = true;
			cssRefs(css, st.refs);
			const css2 = cssReplace(css, cids, st);
			if (css2 !== css)
				el.textContent = css2;
			if (remoteCss(css2))
				out.remote = true;
		}
		for (const a of Array.from(el.attributes)) {
			const n = a.localName.toLowerCase();
			if (n === "srcset") {
				const v = rewriteSrcset(a.value, (u) => {
					const k = cidOf(u);
					if (k === null)
						return u;
					st.refs.add(k);
					const url = substitute(cids, k, st);
					return url === null ? u : url;
				});
				if (v !== a.value)
					el.setAttributeNS(a.namespaceURI, a.name, v);
				if (srcsetEntries(v).some((e) => remoteUrl(e.url)))
					out.remote = true;
			} else if (n === "style") {
				cssRefs(a.value, st.refs);
				const v = cssReplace(a.value, cids, st);
				if (v !== a.value)
					el.setAttribute("style", v);
				if (remoteCss(v))
					out.remote = true;
			} else if (imageAttr(tag, n)) {
				const k = cidOf(a.value);
				if (k !== null) {
					st.refs.add(k);
					const url = substitute(cids, k, st);
					if (url !== null) {
						el.setAttributeNS(a.namespaceURI, a.name, url);
						continue;
					}
				}
				if (remoteUrl(a.value))
					out.remote = true;
			}
		}
	}
	// The serialized markup of the inert document (read, never written
	// into this page)
	out.html = doc.documentElement.outerHTML;
	out.text = doc.body ? doc.body.textContent : doc.documentElement.textContent;
	return out;
}

const MAX_PASSES = 4;

export function prepareHtml(html, cids) {
	cids = cids || {};
	const st = { refs: new Set(), used: new Set(), bytes: 0 };
	const out = { html: "", remote: false, dark: false, refs: st.refs, used: st.used, stable: false, text: "" };
	// Parsing the serialized text again can give a different document
	// (the frame parses it again): the pass is repeated on its own output
	// until that output no longer changes
	let input = html;
	let last = null;
	for (let i = 0; i < MAX_PASSES; i++) {
		last = cleanPass(input, i === 0 ? cids : {}, st);
		if (i > 0 && last.html === input) {
			out.stable = true;
			break;
		}
		input = last.html;
	}
	out.html = out.stable ? last.html : "";
	out.remote = last.remote;
	out.dark = last.dark;
	out.text = last.text;
	return out;
}
