// Writing a message: new, reply, reply all, forward, or a draft opened
// again.  A docked panel at the bottom right on wide screens, the whole
// screen on a phone.
//
// const compose = createCompose({ host, layout, limits, user, onSent, onDrafts, onClosed })
//   host(): the element the panel goes in (the mail frame);
//   limits: session.get's limits (max_attachment_bytes, max_draft_bytes,
//   max_attachments, max_message_bytes, max_body_bytes);
//   user: session.get's user (for the From name);
//   onSent({ mode, sourceId }): a message was sent (the folders, the list
//   and the source's replied mark are refreshed by the caller);
//   onDrafts(): the drafts changed (created, saved, deleted, sent);
//   onClosed(): the panel closed and focus had nowhere to go back to.
//   openDrafts(): show the Drafts folder (its draft.list, not in the
//   background, completes or gives back a stalled send); a Promise of
//   its load.
// compose.open({ mode, item, msg, opener, to })
//   a new draft: mode "new", or "reply", "replyall", "forward" of a
//   message (item: its list item, msg: its message.get message, from which
//   the quote is made here, never by the server); to: for "new", the
//   entries of the To field (a contact's "Write to")
// compose.openDraft(id, { opener, maybeSent })
//   a draft from the Drafts folder
// compose.isOpen()
// compose.destroy()
//
// Drafts: draft.create when the panel opens (with the mode and the source
// message), then draft.save SAVE_DELAY after the last change, before Send
// and when the panel closes, always with the revision last seen ("rev").
// The footer shows "Saving draft ..." and "Draft saved at ...".  A save
// refused as stale (another window saved the draft since) shows a bar
// that lets the user load the other version or keep this text (saved
// again over it).  A save whose reply was lost may have been done: a
// stale answer whose stored fields equal what was sent is taken as done.
// Closing the panel never asks: the text stays a draft and a notice says
// so, with Open to come back (a draft with nothing in it is deleted
// instead).  When the page is hidden or closed (visibilitychange,
// pagehide) unsaved changes are sent at once with fetch's keepalive (only
// for a request of up to KEEPALIVE_MAX bytes, the browsers' limit); a
// longer one is sent as usual and may not arrive: the last autosave stays.
//
// Recipients: To, Cc and Bcc (Cc and Bcc shown on demand), each a list of
// entries ("Name <address>" or an address) shown as chips, with an input
// that is a combobox (WAI-ARIA combobox with a listbox popup): typing
// asks contact.suggest (SUGGEST_DELAY after the last key); Down and Up
// move through the suggestions, Enter or Tab accepts one, Escape closes
// the list; a comma, semicolon, Enter or leaving the field makes the text
// typed an entry; Backspace in an empty input takes the last entry back
// into the input.  The From address is chosen from settings.get's
// from_addresses (hidden with only one); importance (Normal, High, Low) is
// in the options menu; the body is a plain-text field, with the
// signature (settings.get, when signature_enabled) put in once when a
// draft is made.
//
// Attachments: the file picker (multiple files) or files dropped on the
// panel; each checked against the limits first, then uploaded with
// draft.attach in pieces of PIECE bytes (each slice of the file read on
// its own), one piece per request, alone, one file at a time.  A piece
// that fails for want of a connection is sent again after a pause
// (UPLOAD_BACKOFF), from the server's count (an offset conflict carries
// it; received equal to the size means the file arrived).  Cancel or
// Remove takes it out (draft.detach).  Send waits for the uploads.
//
// Sending: at least one recipient and at most limits.max_recipients
// (each address once, as the server counts them); a key (32 hex characters from
// crypto.getRandomValues) made once per attempt and kept for every retry
// of that attempt; the last changes saved, then draft.send.  The panel
// closes at once and notices say "Sending ..." then "Message sent".  A
// failure opens the panel again with what was typed and the error (an
// error about a recipient marks that entry).  conflict "sending", "busy"
// or "taken", and a lost connection, retry with the SAME key after a
// pause (SEND_RETRY), then leave it to the Drafts folder; "unknown" and
// "maybe_sent" ask "may already have been sent: send again?" and only
// then send with confirm_resend and a NEW key; "partial" says the rest
// will be tried again (the server's recovery, when a folder is next opened
// after Send.LIVE); "key" gets a new key once.
//
// Everything typed goes into the page as text (value, textContent).

import { h, clear } from "./dom.js";
import { icon } from "./icons.js";
import { t } from "./i18n.js";
import { rpc } from "./rpc.js";
import { notice, dismissNotice } from "./notice.js";
import { confirmDialog, openDialog, openMenu } from "./dialog.js";
import { size, fullDate, senderText, fromChoices, fromIndex, fromChoicesWith } from "./format.js";
import { decode } from "./body.js";
import { avatarBadge } from "./avatar.js";
import { prefs } from "./prefs.js";

const SAVE_DELAY = 2000;
const SAVE_RETRY = 10000;	// a failed autosave is tried again after this long
const SUGGEST_DELAY = 200;
const SUGGEST_LIMIT = 8;
const PIECE = 1000000;		// bytes of a file per draft.attach
const UPLOAD_BACKOFF = [1000, 2000, 4000, 8000, 15000];
const SEND_RETRY = [2000, 4000, 8000, 15000, 30000];
const MAX_QUOTE = 100000;	// characters of a source quoted at most
const SHARED_MAX = 250000;	// a save request longer than this goes alone
const KEEPALIVE_MAX = 60000;
const PHONE = "(max-width: 899.98px)";
const PRIORITIES = ["", "high", "low"];
const FIELDS = ["to", "cc", "bcc"];

// ---- Addresses ----

// The entries of an address list, as the server splits them: splitList
// is Address.split_entries() of webmail/lib/address.js ported line for
// line (the client test, client.js, runs both over a table); entries are
// trimmed and empty ones left out.  A line break (a pasted list) also
// separates entries.
export function splitEntries(text) {
	return splitList(String(text).replace(/\r\n|\r|\n/g, ",")).map((x) => x.trim()).filter((x) => x !== "");
}

// (Written for any JavaScript engine: the test runs it on the server's)
function splitList(s) {
	var list = [];
	var cur = "";
	var quote = false, angle = false, paren = 0;
	for (var i = 0; i < s.length; i++) {
		var ch = s.charAt(i);
		if ((quote || paren) && ch == "\\" && i + 1 < s.length) {
			cur += ch + s.charAt(++i);
			continue;
		}
		if (quote) {
			if (ch == '"')
				quote = false;
		} else if (paren) {
			if (ch == "(")
				paren++;
			else if (ch == ")")
				paren--;
		} else if (ch == '"')
			quote = true;
		else if (ch == "(")
			paren++;
		else if (ch == "<")
			angle = true;
		else if (ch == ">")
			angle = false;
		else if (!angle && (ch == "," || ch == ";")) {
			list.push(cur);
			cur = "";
			continue;
		}
		cur += ch;
	}
	list.push(cur);
	return list;
}

// The size of the message the server will build, as MimeWrite.estimate()
// of webmail/lib/mimewrite.js computes it (ported; the client test runs
// both over a table).  t: the text as UTF-8 bytes (one character a byte);
// sizes: the files' sizes.  The text's line breaks become CRLF; it is sent
// quoted-printable (counted as 3 bytes a byte plus 3 for every 75) when a
// line is over 998 bytes, ends in a space or tab, starts with "From " or
// holds a control character, else as it is; plus 1024 for its part.  Each
// file is c = ceil(n / 3) * 4 base64 characters, plus 2 for every 76 of
// them (line breaks), plus 1024 for its part.
function estimateBytes(t, sizes) {
	var s = String(t).replace(/\r\n|\r|\n/g, "\r\n");
	var lines = s.split("\r\n");
	var qp = false;
	for (var i = 0; i < lines.length && !qp; i++) {
		var l = lines[i];
		if (l.length > 998 || /[ \t]$/.test(l) || l.substr(0, 5) == "From " || /[\x00-\x08\x0A-\x1F\x7F]/.test(l))
			qp = true;
	}
	var size = (qp ? s.length * 3 + Math.ceil(s.length / 75) * 3 : s.length) + 1024;
	for (var j = 0; j < sizes.length; j++) {
		var c = Math.ceil(Math.max(0, sizes[j] || 0) / 3) * 4;
		size += c + Math.ceil(c / 76) * 2 + 1024;
	}
	return size;
}

// estimateBytes() of a body as typed
function messageBytes(body, sizes) {
	const bytes = new TextEncoder().encode(body);
	let bin = "";
	for (let i = 0; i < bytes.length; i += 8192)
		bin += String.fromCharCode.apply(null, bytes.subarray(i, i + 8192));
	return estimateBytes(bin, sizes);
}

// "Name <address>" as an entry; the name quoted when it needs to be
export function entryOf(name, addr) {
	if (!name || name === addr)
		return addr;
	if (/[",;<>()@\\:[\]]/.test(name))
		return "\"" + name.replace(/(["\\])/g, "\\$1") + "\" <" + addr + ">";
	return name + " <" + addr + ">";
}

// What a chip shows: the name of "Name <address>" (encoded words, as the
// server writes a name that is not ASCII, decoded), else the entry; as a
// list row shows a sender (format.js senderText): a name that could pass
// for an address ('@', '<' or '>' in it) shows the real address instead
// (the whole entry is the chip's title)
function chipText(entry) {
	const m = /^\s*(?:"((?:[^"\\]|\\.)*)"|([^<]*?))\s*<([^>]*)>\s*$/.exec(entry);
	if (!m)
		return entry;
	const name = m[1] !== undefined ? m[1].replace(/\\(.)/g, "$1") : (m[2] || "");
	return senderText(decodeWords(name.trim()), m[3].trim()).text;
}

// RFC 2047 encoded words (=?charset?B|Q?text?=) as text; a word that
// cannot be decoded is left as it is
function decodeWords(s) {
	return s.replace(/=\?([^?\s]+)\?([BbQq])\?([^?\s]*)\?=(\s+(?==\?))?/g, (all, cs, enc, text) => {
		try {
			let bin;
			if (enc === "B" || enc === "b")
				bin = atob(text);
			else
				bin = text.replace(/_/g, " ").replace(/=([0-9A-Fa-f]{2})/g, (x, hex) => String.fromCharCode(parseInt(hex, 16)));
			const bytes = Uint8Array.from(bin, (c) => c.charCodeAt(0));
			return new TextDecoder(cs.replace(/\*.*$/, ""), { fatal: true }).decode(bytes);
		} catch (e) {
			return all;
		}
	});
}

// ---- Quoting (from the decoded text the reader has) ----

const BLOCK = new Set(["ADDRESS", "ARTICLE", "ASIDE", "BLOCKQUOTE", "DD", "DIV", "DL", "DT", "FIELDSET",
	"FIGCAPTION", "FIGURE", "FOOTER", "FORM", "H1", "H2", "H3", "H4", "H5", "H6", "HEADER", "HR", "LI", "MAIN",
	"NAV", "OL", "P", "PRE", "SECTION", "TABLE", "TR", "UL"]);

// The text of an HTML body: parsed into an inert document (nothing in it
// runs or loads, and it is never put in the page), its text read with
// line breaks for blocks and <br>
const SKIP = new Set(["SELECT", "OPTION", "TEXTAREA", "INPUT", "DATALIST", "OBJECT", "EMBED", "IFRAME"]);

export function htmlText(html) {
	const doc = new DOMParser().parseFromString("<!DOCTYPE html>" + String(html || ""), "text/html");
	for (const el of doc.querySelectorAll("script, style, template, noscript, head, title"))
		el.remove();
	let out = "";
	const walk = (node, pre) => {
		for (let n = node.firstChild; n; n = n.nextSibling) {
			if (n.nodeType === 3) {
				out += pre ? n.data : n.data.replace(/[ \t\r\n\f]+/g, " ");
				continue;
			}
			if (n.nodeType !== 1)
				continue;
			const tag = n.tagName;
			// what a reader does not see is not quoted: hidden elements and
			// the values of form fields
			if (n.hidden || n.getAttribute("aria-hidden") === "true" || SKIP.has(tag))
				continue;
			const css = n.style;
			if (css && (css.display === "none" || css.visibility === "hidden"))
				continue;
			if (tag === "BR") {
				out += "\n";
				continue;
			}
			const block = BLOCK.has(tag);
			if (block && out !== "" && !out.endsWith("\n"))
				out += "\n";
			if (tag === "TD" || tag === "TH") {
				if (out !== "" && !out.endsWith("\n"))
					out += "  ";
			}
			walk(n, pre || tag === "PRE");
			if (block && !out.endsWith("\n"))
				out += "\n";
			if (tag === "P" || /^H[1-6]$/.test(tag))
				out += "\n";
		}
	};
	if (doc.body)
		walk(doc.body, false);
	return out.split("\n").map((l) => l.replace(/^ +| +$/g, "")).join("\n")
		.replace(/\n{3,}/g, "\n\n").replace(/^\n+|\n+$/g, "");
}

// The source's text: its plain text, else the text of its HTML
function sourceText(msg) {
	if (!msg)
		return "";
	if (msg.text && typeof msg.text.b64 === "string")
		return decode(msg.text).replace(/\r\n?/g, "\n");
	if (msg.html && typeof msg.html.b64 === "string") {
		try {
			return htmlText(decode(msg.html));
		} catch (e) {
			return "";
		}
	}
	return "";
}

// text cut to MAX_QUOTE characters (at a line end) with a note
function limited(text) {
	if (text.length <= MAX_QUOTE)
		return text;
	let cut = text.lastIndexOf("\n", MAX_QUOTE);
	if (cut < MAX_QUOTE / 2) {
		cut = MAX_QUOTE;
		// never between the two halves of a surrogate pair
		const c = text.charCodeAt(cut - 1);
		if (c >= 0xd800 && c <= 0xdbff)
			cut--;
	}
	return text.slice(0, cut) + "\n" + t("compose.label_quote_cut");
}

function senderOf(item, msg) {
	const hd = msg && msg.headers ? msg.headers : {};
	const name = hd.from || (msg && msg.from) || item.from || "";
	const addr = hd.from_addr || (msg && msg.from_addr) || item.from_addr || "";
	return { name, addr, both: name && addr && name !== addr ? name + " <" + addr + ">" : (name || addr) };
}

function whenOf(item, msg) {
	const m = msg || item;
	return m.date > 0 ? m.date : m.arrived;
}

// The body of a reply: room to write, then the quote under "On <date>,
// <name> wrote:"
function replyBody(item, msg) {
	const text = limited(sourceText(msg).replace(/\s+$/, ""));
	const who = senderOf(item, msg);
	const head = t("reader.label_wrote", { date: fullDate(whenOf(item, msg)), name: who.name || who.addr });
	return head + "\n" + text.split("\n").map((l) => "> " + l).join("\n") + "\n";
}

// The body of a forward: the source's From, Date, Subject and To, then
// its text
function forwardBody(item, msg) {
	const hd = msg && msg.headers ? msg.headers : {};
	const m = msg || item;
	const lines = ["---------- " + t("reader.label_forwarded") + " ----------",
		t("main.label_from") + ": " + senderOf(item, msg).both,
		t("reader.label_date") + ": " + fullDate(whenOf(item, msg)),
		t("main.label_subject") + ": " + (m.subject || "")];
	const to = hd.to || m.to || "";
	if (to)
		lines.push(t("main.label_to") + ": " + to);
	if (hd.cc)
		lines.push(t("main.label_cc") + ": " + hd.cc);
	return lines.join("\n") + "\n\n" + limited(sourceText(msg).replace(/\s+$/, "")) + "\n";
}

// The subject with "Re: " or "Fwd: " in front, once
function prefixed(subject, mode) {
	const s = String(subject || "");
	if (mode === "forward") {
		const fwd = t("compose.label_fwd");
		return new RegExp("^\\s*(fwd?|fw|" + escapeRe(fwd) + ")\\s*:", "i").test(s) ? s : fwd + ": " + s;
	}
	const re = t("compose.label_re");
	return new RegExp("^\\s*(re|" + escapeRe(re) + ")\\s*:", "i").test(s) ? s : re + ": " + s;
}

function escapeRe(s) {
	return String(s).replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

// ---- Small helpers ----

function newKey() {
	const b = new Uint8Array(16);
	crypto.getRandomValues(b);
	return Array.from(b, (x) => x.toString(16).padStart(2, "0")).join("");
}

function uploadKey() {
	return "u" + newKey().slice(0, 23);
}

function utf8Length(s) {
	return new TextEncoder().encode(s).length;
}

// A slice of a file as base64 (the data: URL's payload)
function readB64(blob) {
	return new Promise((resolve, reject) => {
		const r = new FileReader();
		r.onload = () => {
			const s = String(r.result || "");
			resolve(s.slice(s.indexOf(",") + 1));
		};
		r.onerror = () => reject(r.error);
		r.readAsDataURL(blob);
	});
}

function sleep(ms) {
	return new Promise((r) => setTimeout(r, ms));
}

function timeNow() {
	try {
		return new Intl.DateTimeFormat(document.documentElement.lang || undefined,
			{ hour: "numeric", minute: "2-digit" }).format(new Date());
	} catch (e) {
		return new Date().toLocaleTimeString();
	}
}

function errText(e) {
	return e && typeof e.message === "string" && e.message !== "" ? e.message : t("errors.failed");
}

// ---- The panel ----

export function createCompose(o) {
	const limits = o.limits && typeof o.limits === "object" ? o.limits : {};
	const mq = window.matchMedia(PHONE);
	const live = new Map();		// draft id -> state, while uploads run after the panel closed
	let st = null;			// the state of the draft shown
	let v = null;			// the panel's elements
	let settings = null;		// the last settings.get result
	let dead = false;
	let seq = 0;			// suggestion requests

	// ---- Settings ----

	// settings.get; the newest settings (also when the settings view or
	// the reader changed them) through prefs
	function loadSettings() {
		return prefs.load({ retry: false }).then((r) => r, () => settings);
	}
	const offPrefs = prefs.on((r) => {
		settings = r;
	});
	settings = prefs.get();

	// ---- State ----

	function blank(mode) {
		return { id: null, rev: 0, mode, sourceId: null, entries: { to: [], cc: [], bcc: [] },
			subject: "", body: "", from_domain: "", from_local: "", priority: "", atts: [], saved: null,
			maybeSent: false,
			chain: Promise.resolve(), created: null, stale: false, uploading: null, closed: false,
			key: newKey(), usage: null, added: 0, fresh: false, touched: false };
	}

	function fromDraft(s, d) {
		for (const f of FIELDS)
			s.entries[f] = splitEntries(typeof d[f] === "string" ? d[f] : "");
		s.subject = typeof d.subject === "string" ? d.subject : "";
		s.body = typeof d.body === "string" ? d.body : "";
		s.from_domain = typeof d.from_domain === "string" ? d.from_domain : "";
		s.from_local = typeof d.from_local === "string" ? d.from_local : "";
		s.priority = PRIORITIES.includes(d.priority) ? d.priority : "";
		if (typeof d.rev === "number")
			s.rev = d.rev;
		if (typeof d.mode === "string")
			s.mode = d.mode;
		if (typeof d.in_reply_to === "string" && d.in_reply_to !== "")
			s.sourceId = d.in_reply_to;
	}

	// The attachments as the server lists them (kept: the ones still
	// uploading here)
	function attsFromDraft(s, d) {
		const list = Array.isArray(d.attachments) ? d.attachments : [];
		const mine = s.atts.filter((a) => !a.complete && !a.failed && a.file && !a.id);
		const byId = new Map(s.atts.filter((a) => a.id).map((a) => [a.id, a]));
		const out = [];
		for (const x of list) {
			if (!x || typeof x.id !== "string")
				continue;
			const had = byId.get(x.id);
			if (had && !had.complete && had.file && !had.cancelled) {
				out.push(had);
				continue;
			}
			out.push({ key: x.id, id: x.id, name: String(x.name || ""), type: String(x.type || ""),
				total: typeof x.total === "number" ? x.total : (x.size || 0),
				received: x.complete ? x.total : 0, complete: x.complete === true, file: null,
				incomplete: x.complete !== true });
		}
		s.atts = out.concat(mine);
	}

	// The text of each field: the entries plus what is typed and not yet
	// an entry
	function fieldText(s, f) {
		const list = s.entries[f].slice();
		if (v && s === st && v.fields[f]) {
			const typed = v.fields[f].input.value.trim();
			if (typed !== "")
				list.push(...splitEntries(typed));
		}
		return list.join(", ");
	}

	function fields(s) {
		if (v && s === st) {
			s.subject = v.subject.value;
			s.body = v.body.value;
		}
		return { to: fieldText(s, "to"), cc: fieldText(s, "cc"), bcc: fieldText(s, "bcc"), subject: s.subject,
			body: s.body, from_domain: s.from_domain, from_local: s.from_local, priority: s.priority };
	}

	function hasContent(s) {
		// a message started here that the user changed nothing in: what
		// it holds (a reply's recipient, subject and quote, a forward's
		// files, the signature) was all put there for them
		if (s.fresh && !s.touched)
			return false;
		const f = fields(s);
		const sig = signatureFor("new").trim();
		const body = f.body.trim();
		return f.to !== "" || f.cc !== "" || f.bcc !== "" || f.subject.trim() !== ""
			|| (body !== "" && body !== sig) || s.atts.length > 0;
	}

	// The signature (settings.get, when signature_enabled), put in once
	// when a draft is made.  A new message: under the conventional "-- "
	// line (unless the signature starts with one already), so mail
	// readers know it.  Above the quote of a reply or forward: without
	// that line (a reader would take everything after it, the quote too,
	// for the signature), a "--" line the signature starts with removed.
	// mode "new": "\n\n" + delimiter + signature + "\n"; else signature +
	// "\n\n" (or "" without a signature).
	function signatureFor(mode) {
		if (!settings || settings.signature_enabled !== true || typeof settings.signature !== "string")
			return "";
		let sig = settings.signature.replace(/\r\n?/g, "\n").replace(/\s+$/, "");
		const delimited = /^-- ?\n/.test(sig + "\n");
		if (mode !== "new")
			sig = delimited ? sig.replace(/^-- ?(\n|$)/, "") : sig;
		if (sig.trim() === "")
			return "";
		if (mode !== "new")
			return sig + "\n\n";
		return "\n\n" + (delimited ? sig : "-- \n" + sig) + "\n";
	}

	// ---- Saving ----

	// Make the draft when it has no id yet (the first attempt failed)
	function ensureId(s) {
		if (s.id)
			return Promise.resolve(true);
		if (!s.created)
			s.created = create(s);
		return s.created;
	}

	function create(s) {
		const args = { mode: s.mode };
		if (s.mode !== "new")
			args.source_id = s.sourceId;
		return rpc.call("draft.create", args, { retry: false }).then((r) => {
			const d = r.draft || {};
			s.id = d.id;
			s.rev = d.rev;
			if (r.usage && typeof r.usage.bytes === "number") {
				s.usage = r.usage.bytes;
				s.added = 0;
			}
			// recipients the server chose for a reply, unless the user has
			// already put some in
			for (const f of FIELDS) {
				const typed = s === st && v ? v.fields[f].input.value.trim() : "";
				if (s.entries[f].length === 0 && typed === "" && typeof d[f] === "string" && d[f] !== "")
					s.entries[f] = splitEntries(d[f]);
			}
			if (s.mode !== "new" && typeof d.from_domain === "string") {
				// a reply is from the address the message was sent to
				s.from_domain = d.from_domain;
				s.from_local = typeof d.from_local === "string" ? d.from_local : "";
			}
			attsFromDraft(s, d);
			if (typeof r.dropped_recipients === "number" && r.dropped_recipients > 0 && s === st)
				banner(t("compose.label_dropped_recipients", { count: r.dropped_recipients }), "info");
			if (s === st)
				render();
			if (typeof o.onDrafts === "function")
				o.onDrafts();
			s.created = null;
			return true;
		}, (e) => {
			s.created = null;
			if (s === st) {
				banner(errText(e), "error");
				status(t("compose.label_draft_failed"));
			}
			return false;
		});
	}

	// Saves the draft when it changed since the last save (force: even
	// when not); one save at a time, except one made as the page goes away
	// (unload: sent at once with keepalive; if a save is in flight the
	// revision decides, see stale()).  keepalive: the request may outlive
	// the page when it fits the browser's keepalive limit.  A Promise of
	// true when saved.
	function save(s, opts) {
		opts = opts || {};
		if (opts.unload)
			return doSave(s, Object.assign({ keepalive: true }, opts));
		const run = s.chain.then(() => doSave(s, opts), () => doSave(s, opts));
		s.chain = run.catch(() => false);
		return run;
	}

	async function doSave(s, opts) {
		if (s === st)
			clearTimeout(v.timer);
		if (!(await ensureId(s)))
			return false;
		const f = fields(s);
		const snap = JSON.stringify(f);
		if (snap === s.saved && !opts.force)
			return true;
		if ((s.stale && !opts.force) || s.gone)
			return false;
		if (limits.max_body_bytes > 0 && utf8Length(f.body) > limits.max_body_bytes) {
			if (s === st) {
				status(t("compose.label_draft_failed"));
				banner(t("errors.toolarge_body"), "error");
			}
			return false;
		}
		if (s === st)
			status(t("compose.label_saving"));
		const args = Object.assign({ id: s.id, rev: s.rev }, f);
		const length = utf8Length(JSON.stringify(args)) + 200;	// bytes on the wire
		const callOpts = { retry: false, alone: length > SHARED_MAX };
		if (opts.keepalive && length < KEEPALIVE_MAX)
			callOpts.keepalive = true;
		try {
			const r = await rpc.call("draft.save", args, callOpts);
			s.rev = r.draft.rev;
			s.saved = snap;
			s.stale = false;
			s.lastError = null;
			// the kept copy goes only when what was saved is what is there now
			if (JSON.stringify(fields(s)) === snap)
				unstash(s);
			if (s === st)
				saved();
			return true;
		} catch (e) {
			if (e.error === "conflict" && e.reason === "stale")
				return stale(s, f, snap, e);
			s.lastError = e;
			if (e.error === "notfound") {
				// sent (reason "sent") or deleted elsewhere: never saved
				// again by itself; the user chooses (goneBar)
				s.gone = goneKind(e);
				if (s === st) {
					status(t("compose.label_draft_failed"));
					goneBar();
				}
				return false;
			}
			if (s === st) {
				status(t("compose.label_draft_failed"));
				if (e.error !== "failed")
					banner(errText(e), "error");
				else if (!s.closed)
					later(SAVE_RETRY);
			}
			return false;
		}
	}

	// Saved by another window, or by this one when a reply was lost: the
	// stored fields decide
	async function stale(s, f, snap, e) {
		let d = null;
		try {
			d = (await rpc.call("draft.get", { id: s.id }, { retry: false })).draft;
		} catch (err) {
			d = null;
		}
		if (d && FIELDS.concat(["subject", "body", "from_domain", "from_local", "priority"]).every((k) => d[k] === f[k])) {
			s.rev = d.rev;
			s.saved = snap;
			if (s === st)
				saved();
			return true;
		}
		s.stale = true;
		if (typeof e.rev === "number")
			s.staleRev = e.rev;
		if (d)
			s.other = d;
		if (s === st) {
			status(t("compose.label_draft_failed"));
			staleBar();
		}
		return false;
	}

	function staleBar() {
		banner(t("errors.conflict_stale"), "error", [
			{ label: t("compose.button_load_other"), onclick: () => {
				const s = st;
				const d = s.other;
				if (!d)
					return;
				fromDraft(s, d);
				attsFromDraft(s, d);
				s.stale = false;
				s.saved = JSON.stringify({ to: d.to, cc: d.cc, bcc: d.bcc, subject: d.subject, body: d.body,
					from_domain: d.from_domain, from_local: d.from_local, priority: d.priority });
				v.subject.value = s.subject;
				v.body.value = s.body;
				banner(null);
				render();
				saved();
				v.body.focus();
			} },
			{ label: t("compose.button_keep_mine"), primary: true, onclick: () => {
				const s = st;
				if (typeof s.staleRev === "number")
					s.rev = s.staleRev;
				else if (s.other)
					s.rev = s.other.rev;
				s.stale = false;
				banner(null);
				save(s, { force: true });
				v.body.focus();
			} }
		]);
	}

	// The draft was sent or deleted elsewhere: the text stays on screen;
	// the user keeps it as a new draft (or, when it was sent, writes a new
	// message with it) or discards it.  Nothing is saved or sent until then.
	function goneKind(e) {
		return ["sent", "sending", "stalled"].includes(e.reason) ? e.reason : "deleted";
	}

	// The fields can be read but not changed (a send of this draft is
	// under way in another window, or stalled)
	function readOnly(on) {
		v.subject.readOnly = on;
		v.body.readOnly = on;
		for (const f of FIELDS)
			v.fields[f].input.readOnly = on;
		v.el.classList.toggle("readonly", on);
		for (const b of v.el.querySelectorAll(".chip .ib, .ccbcc button, .cf select, .catt .ib"))
			b.disabled = on;
		v.attach.disabled = on;
		v.options.disabled = on;
		v.discard.disabled = on;
		sendState();
		if (on)
			v.send.disabled = true;
	}

	function readonlyNow() {
		return !!(v && v.el.classList.contains("readonly"));
	}

	// Being sent elsewhere ("sending"): no choice to keep it as a new
	// draft (sending it again would send it twice).  "stalled": the send's
	// claim has had no heartbeat for the server's live window, so it did
	// not finish: the user is told to look in Sent and offered Open Drafts,
	// whose draft.list (not in the background) completes the send or gives
	// the draft back; the draft is read again after it.  Either way it is
	// checked again while the panel shows it, in the background (the
	// session is not kept alive by it), after RECHECK[n] ms (the last one
	// repeated), at most RECHECK_MAX times, until it is sent ("already
	// sent") or back as a draft (editable again, the stale bar if the text
	// differs).  Closing the panel or the page stops the checks.
	const RECHECK = [5000, 30000, 60000];
	const RECHECK_MAX = 30;

	function stopRecheck(s) {
		if (s && s.recheck) {
			clearTimeout(s.recheck);
			s.recheck = 0;
		}
	}

	function scheduleRecheck(s) {
		stopRecheck(s);
		const n = s.rechecks || 0;
		if (n >= RECHECK_MAX)
			return;
		s.rechecks = n + 1;
		s.recheck = setTimeout(() => recheck(s), RECHECK[Math.min(n, RECHECK.length - 1)]);
	}

	function sendingBar(s) {
		if (s.gone === "stalled") {
			banner(t("errors.notfound_stalled"), "error", [
				{ label: t("compose.button_open_drafts"), primary: true, onclick: () => openDrafts(s) }
			]);
		} else
			banner(t("errors.notfound_sending"), "info");
		readOnly(true);
		scheduleRecheck(s);
	}

	function openDrafts(s) {
		const p = typeof o.openDrafts === "function" ? o.openDrafts() : null;
		Promise.resolve(p).then(() => {
			if (dead || s !== st || !v || v.el.hidden)
				return;
			stopRecheck(s);
			s.rechecks = 0;
			recheck(s, true);
		}, () => {});
	}

	// now: the user asked (Open Drafts): not a background request
	function recheck(s, now) {
		s.recheck = 0;
		if (dead || (s.gone !== "sending" && s.gone !== "stalled") || s !== st || !v || v.el.hidden)
			return;
		rpc.call("draft.get", { id: s.id }, { retry: false, background: now !== true }).then((r) => {
			if (dead || s !== st)
				return;
			const d = r.draft || {};
			s.gone = null;
			const f = fields(s);
			if (FIELDS.concat(["subject", "body", "from_domain", "from_local", "priority"]).every((k) => d[k] === f[k])) {
				s.rev = d.rev;
				s.saved = JSON.stringify(f);
			} else {
				s.stale = true;
				s.staleRev = d.rev;
				s.other = d;
			}
			attsFromDraft(s, d);
			readOnly(false);
			banner(null);
			render();
			if (s.stale)
				staleBar();
		}, (e) => {
			if (dead || s !== st)
				return;
			if (e && e.error === "notfound") {
				s.gone = goneKind(e);
				if (s.gone === "sending" || s.gone === "stalled")
					sendingBar(s);
				else {
					readOnly(false);
					goneBar();
				}
				return;
			}
			scheduleRecheck(s);	// no answer: again later
		});
	}

	function goneBar() {
		if (st.gone === "sending" || st.gone === "stalled") {
			sendingBar(st);
			return;
		}
		const sent = st.gone === "sent";
		banner(t(sent ? "errors.notfound_sent" : "compose.label_gone"), "error", [
			{ label: t(sent ? "compose.button_new_from_text" : "compose.button_keep_new"), primary: true, onclick: () => {
				const s = st;
				s.gone = null;
				s.id = null;
				s.rev = 0;
				s.saved = null;
				s.mode = "new";
				s.sourceId = null;
				s.maybeSent = false;
				for (const a of s.atts) {
					if (!a.file)
						a.incomplete = true;
				}
				banner(null);
				render();
				save(s);
				v.body.focus();
			} },
			{ label: t("compose.button_discard"), onclick: () => {
				const s = st;
				close({ discard: true });
				live.delete(s.key);
				unstash(s);
				notice(t("compose.label_discarded"));
			} }
		]);
	}

	function later(ms) {
		if (!v)
			return;
		clearTimeout(v.timer);
		v.timer = setTimeout(() => {
			if (st && v)
				save(st);
		}, ms);
	}

	function changed() {
		if (!st || !v)
			return;
		st.touched = true;
		status(st.stale ? t("compose.label_draft_failed") : t("compose.label_unsaved"));
		if (!st.stale)
			later(SAVE_DELAY);
	}

	function saved() {
		status(t("compose.label_draft_saved", { time: timeNow() }));
		if (typeof o.onDrafts === "function")
			o.onDrafts();
	}

	function status(text) {
		if (v)
			v.status.textContent = text;
	}

	// ---- The view ----

	function banner(text, kind, buttons) {
		if (!v)
			return;
		clear(v.banner);
		v.banner.hidden = !text;
		if (!text)
			return;
		v.banner.className = "cbanner " + (kind === "info" ? "info" : "error");
		v.banner.setAttribute("role", kind === "info" ? "status" : "alert");
		v.banner.append(icon(kind === "info" ? "info" : "alert"), h("p", { text }));
		if (buttons && buttons.length) {
			const row = h("div", { class: "cbanner-btns" });
			for (const b of buttons)
				row.append(h("button", { class: "btn" + (b.primary ? " primary" : ""), type: "button", text: b.label,
					onclick: b.onclick }));
			v.banner.append(row);
		}
	}

	function title(s) {
		return t({ reply: "reader.button_reply", replyall: "reader.button_reply_all",
			forward: "reader.button_forward" }[s.mode] || "compose.label_new_message");
	}

	function build() {
		const titleEl = h("h2", { id: "wm-compose-title", class: "ell", tabindex: "-1" });
		const prio = h("span", { class: "tag prio", hidden: true });
		const closeLabel = t("compose.button_close_keep");
		const head = h("div", { class: "chead" },
			h("button", { class: "ib ph-only", type: "button", "aria-label": closeLabel, title: closeLabel,
				onclick: () => close() }, icon("back")),
			titleEl, prio,
			h("button", { class: "ib dt-only", type: "button", "aria-label": closeLabel, title: closeLabel,
				onclick: () => close() }, icon("x")));
		const bannerEl = h("div", { class: "cbanner", hidden: true });

		const fromSel = h("select", { id: "wm-c-from", onchange: () => {
			const c = fromList[fromSel.selectedIndex];
			if (!c)
				return;
			st.from_domain = c.domain;
			st.from_local = c.local;
			changed();
		} });
		const fromRow = h("div", { class: "cf", hidden: true },
			h("label", { for: "wm-c-from", text: t("main.label_from") }), fromSel);

		const fieldsEl = {};
		const ccBtn = h("button", { class: "linkbtn", type: "button", text: t("main.label_cc"),
			"aria-label": t("compose.label_show_cc"), onclick: () => reveal("cc") });
		const bccBtn = h("button", { class: "linkbtn", type: "button", text: t("compose.label_bcc"),
			"aria-label": t("compose.label_show_bcc"), onclick: () => reveal("bcc") });
		const rows = {};
		for (const f of FIELDS) {
			fieldsEl[f] = recipientField(f);
			rows[f] = fieldsEl[f].row;
		}
		rows.to.append(h("span", { class: "ccbcc" }, ccBtn, bccBtn));

		const subject = h("input", { id: "wm-c-subject", type: "text", autocomplete: "off",
			placeholder: t("main.label_subject"), oninput: changed, onfocus: () => closeLists() });
		const subjRow = h("div", { class: "cf" }, h("label", { for: "wm-c-subject", class: "vh",
			text: t("main.label_subject") }), subject);
		const body = h("textarea", { id: "wm-c-body", spellcheck: "true", oninput: changed,
			onfocus: () => closeLists() });
		const bodyLabel = h("label", { for: "wm-c-body", class: "vh", text: t("compose.label_body") });
		const atts = h("ul", { class: "catts", "aria-label": t("main.label_attachments"), hidden: true });
		const picker = h("input", { type: "file", multiple: true, class: "vh", tabindex: "-1", "aria-hidden": "true",
			onchange: () => {
				addFiles(Array.from(picker.files || []));
				picker.value = "";
			} });
		const cbody = h("div", { class: "cbody" }, bannerEl, fromRow, rows.to, rows.cc, rows.bcc, subjRow, bodyLabel,
			body, atts);

		const sendHint = h("span", { id: "wm-c-send-hint", class: "vh" });
		const send = h("button", { class: "btn primary", type: "button", "aria-describedby": "wm-c-send-hint",
			onclick: () => sendNow() }, icon("send"), h("span", { text: t("compose.button_send") }));
		const attachLabel = t("compose.button_attach");
		const attach = h("button", { class: "ib", type: "button", "aria-label": attachLabel, title: attachLabel,
			onclick: () => picker.click() }, icon("clip"));
		const optLabel = t("compose.button_options");
		const options = h("button", { class: "ib", type: "button", "aria-label": optLabel, title: optLabel,
			"aria-haspopup": "dialog", "aria-expanded": "false", onclick: (e) => optionsMenu(e.currentTarget) },
		icon("more"));
		const statusEl = h("span", { class: "status ell" });
		const discardLabel = t("compose.button_discard");
		const discard = h("button", { class: "ib", type: "button", "aria-label": discardLabel, title: discardLabel,
			onclick: () => discardDraft() }, icon("trash"));
		const foot = h("div", { class: "cfoot" }, send, attach, options, statusEl, sendHint, discard, picker);
		const drop = h("div", { class: "cdrop", "aria-hidden": "true" }, icon("clip"), h("span", { text: t("compose.label_drop_files") }));

		const el = h("section", { class: "compose", role: "dialog", "aria-labelledby": "wm-compose-title",
			hidden: true }, head, cbody, foot, drop);
		el.addEventListener("keydown", onKey);
		el.addEventListener("dragenter", onDrag);
		el.addEventListener("dragover", onDrag);
		el.addEventListener("dragleave", (e) => {
			if (!e.relatedTarget || !el.contains(e.relatedTarget))
				el.classList.remove("dropping");
		});
		el.addEventListener("drop", (e) => {
			el.classList.remove("dropping");
			if (!e.dataTransfer || !e.dataTransfer.files || e.dataTransfer.files.length === 0)
				return;
			e.preventDefault();
			if (readonlyNow())
				return;
			addFiles(Array.from(e.dataTransfer.files));
		});
		return { el, title: titleEl, prio, banner: bannerEl, fromRow, fromSel, fields: fieldsEl, ccBtn, bccBtn,
			subject, body, atts, send, sendHint, attach, options, status: statusEl, discard, timer: 0, opener: null };
	}

	function onDrag(e) {
		const types = e.dataTransfer ? Array.from(e.dataTransfer.types || []) : [];
		if (!types.includes("Files"))
			return;
		e.preventDefault();
		e.dataTransfer.dropEffect = "copy";
		v.el.classList.add("dropping");
	}

	function onKey(e) {
		if (e.defaultPrevented)
			return;
		if (e.key === "Escape") {
			e.preventDefault();
			close();
		} else if (e.key === "Enter" && (e.ctrlKey || e.metaKey)) {
			e.preventDefault();
			sendNow();
		}
	}

	// ---- Recipient fields (combobox with a listbox of suggestions) ----

	function recipientField(f) {
		const id = "wm-c-" + f;
		const listId = id + "-list";
		const label = f === "to" ? t("main.label_to") : (f === "cc" ? t("main.label_cc") : t("compose.label_bcc"));
		const chips = h("ul", { class: "chips", "aria-label": label });
		const input = h("input", { id, type: "text", class: "rcpt", autocomplete: "off", autocapitalize: "none",
			spellcheck: "false", inputmode: "email", role: "combobox", "aria-autocomplete": "list",
			"aria-expanded": "false", "aria-controls": listId });
		const list = h("ul", { class: "sugg", id: listId, role: "listbox", "aria-label": t("compose.label_suggestions"),
			hidden: true });
		const row = h("div", { class: "cf", hidden: f !== "to", onclick: (e) => {
			if (e.target === row || e.target === chips)
				input.focus();
		} }, h("label", { for: id, text: label }), chips, input, list);
		const fld = { f, row, chips, input, list, items: [], active: -1, timer: 0 };
		input.addEventListener("input", () => {
			if (readonlyNow())
				return;
			const val = input.value;
			// a comma or semicolon typed (or pasted) ends entries
			if (/[,;]/.test(val)) {
				const parts = splitEntries(val);
				const ends = /[,;]\s*$/.test(val);
				const keep = ends ? "" : (parts.length > 0 ? parts.pop() : "");
				if (parts.length > 0) {
					st.entries[f].push(...parts);
					renderChips(f);
				}
				input.value = keep;
			}
			changed();
			suggestSoon(fld);
		});
		// a pasted list (lines, commas or semicolons) becomes entries; a
		// one-line field would otherwise turn its line breaks into spaces
		input.addEventListener("paste", (e) => {
			if (readonlyNow()) {
				e.preventDefault();
				return;
			}
			const text = e.clipboardData ? e.clipboardData.getData("text") : "";
			if (!/[\r\n,;]/.test(text))
				return;
			e.preventDefault();
			const all = input.value.slice(0, input.selectionStart) + text + input.value.slice(input.selectionEnd);
			const parts = splitEntries(all);
			const keep = /[\r\n,;]\s*$/.test(all) || parts.length === 0 ? "" : parts.pop();
			if (parts.length > 0) {
				st.entries[f].push(...parts);
				renderChips(f);
			}
			input.value = keep;
			changed();
		});
		input.addEventListener("keydown", (e) => {
			if (!readonlyNow())
				fieldKey(fld, e);
		});
		input.addEventListener("blur", () => setTimeout(() => {
			if (!v || document.activeElement === input || (fld.list.contains(document.activeElement)))
				return;
			closeList(fld);
			commit(f);
		}, 150));
		return fld;
	}

	function fieldKey(fld, e) {
		const open = !fld.list.hidden && fld.items.length > 0;
		switch (e.key) {
			case "ArrowDown":
			case "ArrowUp":
				if (!open) {
					if (fld.input.value.trim() !== "") {
						e.preventDefault();
						suggest(fld);
					}
					return;
				}
				e.preventDefault();
				setActive(fld, fld.active < 0 ? (e.key === "ArrowDown" ? 0 : fld.items.length - 1)
					: (fld.active + (e.key === "ArrowDown" ? 1 : fld.items.length - 1)) % fld.items.length);
				return;
			case "Enter":
				if (e.ctrlKey || e.metaKey)
					return;
				e.preventDefault();
				if (open && fld.active >= 0)
					accept(fld, fld.active);
				else
					commit(fld.f);
				return;
			case "Tab":
				if (open && fld.active >= 0 && !e.shiftKey) {
					e.preventDefault();
					accept(fld, fld.active);
				} else {
					closeList(fld);
					commit(fld.f);
				}
				return;
			case "Escape":
				if (open) {
					e.preventDefault();
					e.stopPropagation();
					closeList(fld);
				}
				return;
			case "Backspace":
				if (fld.input.value === "" && st.entries[fld.f].length > 0) {
					e.preventDefault();
					fld.input.value = st.entries[fld.f].pop();
					renderChips(fld.f);
					changed();
				}
				return;
		}
	}

	// What is typed becomes entries
	function commit(f) {
		if (!v || !st || readonlyNow())
			return;
		const input = v.fields[f].input;
		const parts = splitEntries(input.value);
		if (parts.length === 0)
			return;
		st.entries[f].push(...parts);
		input.value = "";
		renderChips(f);
		changed();
	}

	function suggestSoon(fld) {
		clearTimeout(fld.timer);
		fld.timer = setTimeout(() => suggest(fld), SUGGEST_DELAY);
	}

	function suggest(fld) {
		const q = fld.input.value.trim();
		const my = ++seq;
		if (q === "") {
			closeList(fld);
			return;
		}
		rpc.call("contact.suggest", { q, limit: SUGGEST_LIMIT }, { retry: false }).then((r) => {
			if (my !== seq || !v || dead || fld.input.value.trim() !== q)
				return;
			const have = new Set(st.entries[fld.f].map((x) => x.toLowerCase()));
			const items = (Array.isArray(r.suggestions) ? r.suggestions : []).filter((x) => x && typeof x.addr === "string"
				&& !have.has(entryOf(x.name, x.addr).toLowerCase()));
			showList(fld, items);
		}, () => {});
	}

	function showList(fld, items) {
		fld.items = items;
		fld.active = -1;
		clear(fld.list);
		items.forEach((x, i) => {
			const name = typeof x.name === "string" && x.name !== "" ? x.name : x.addr;
			fld.list.append(h("li", { id: fld.list.id + "-" + i, role: "option", "aria-selected": "false",
				onmousedown: (e) => e.preventDefault(), onclick: () => accept(fld, i) },
			avatarBadge(null, (Array.from(name)[0] || "?").toUpperCase(), "avatar"),
			h("span", { class: "ell", text: name }),
			h("small", { class: "ell", text: x.addr })));
		});
		const open = items.length > 0;
		fld.list.hidden = !open;
		fld.input.setAttribute("aria-expanded", String(open));
		fld.input.removeAttribute("aria-activedescendant");
		if (open)
			setActive(fld, 0);
	}

	function setActive(fld, i) {
		fld.active = i;
		const opts = Array.from(fld.list.children);
		opts.forEach((li, j) => li.setAttribute("aria-selected", String(j === i)));
		if (i >= 0 && opts[i]) {
			fld.input.setAttribute("aria-activedescendant", opts[i].id);
			opts[i].scrollIntoView({ block: "nearest" });
		} else
			fld.input.removeAttribute("aria-activedescendant");
	}

	function closeList(fld) {
		clearTimeout(fld.timer);
		fld.list.hidden = true;
		fld.items = [];
		fld.active = -1;
		clear(fld.list);
		fld.input.setAttribute("aria-expanded", "false");
		fld.input.removeAttribute("aria-activedescendant");
	}

	function closeLists() {
		if (v)
			for (const f of FIELDS)
				closeList(v.fields[f]);
	}

	function accept(fld, i) {
		const x = fld.items[i];
		if (!x || readonlyNow())
			return;
		st.entries[fld.f].push(entryOf(x.name, x.addr));
		fld.input.value = "";
		closeList(fld);
		renderChips(fld.f);
		changed();
		fld.input.focus();
	}

	function renderChips(f) {
		const fld = v.fields[f];
		clear(fld.chips);
		st.entries[f].forEach((entry, i) => {
			const text = chipText(entry);
			const bad = st.bad && st.bad.f === f && st.bad.i === i;
			fld.chips.append(h("li", { class: "chip" + (bad ? " bad" : ""), title: decodeWords(entry) },
				h("span", { text }),
				bad ? h("span", { class: "vh", text: ", " + t("compose.label_bad_entry") }) : null,
				h("button", { class: "ib", type: "button", "aria-label": t("compose.button_remove_name", { name: decodeWords(entry) }),
					disabled: readonlyNow(), onclick: () => {
						if (readonlyNow())
							return;
						st.entries[f].splice(i, 1);
						if (st.bad && st.bad.f === f)
							st.bad = null;
						renderChips(f);
						changed();
						fld.input.focus();
					} }, icon("x", "sm"))));
		});
		fld.input.setAttribute("aria-invalid", String(!!(st.bad && st.bad.f === f)));
		fld.chips.hidden = st.entries[f].length === 0;
	}

	function reveal(f, focus = true) {
		v.fields[f].row.hidden = false;
		(f === "cc" ? v.ccBtn : v.bccBtn).hidden = true;
		if (focus)
			v.fields[f].input.focus();
	}

	// ---- From and options ----

	let fromList = [];	// the choices in the From menu, in its order

	function renderFrom() {
		const s = st;
		// the addresses offered, and the draft's own when it is another
		// (a reply from the address its message was sent to)
		const choices = fromChoicesWith(fromChoices(settings), s.from_local, s.from_domain);
		fromList = choices;
		clear(v.fromSel);
		v.fromRow.hidden = choices.length < 2;
		if (choices.length < 2)
			return;
		const name = settings.display_name || (o.user && (o.user.name || o.user.alias)) || "";
		choices.forEach((c, i) => {
			v.fromSel.append(h("option", { value: String(i), text: name ? name + " <" + c.addr + ">" : c.addr }));
		});
		v.fromSel.selectedIndex = fromIndex(choices, s.from_local, s.from_domain);
	}

	function optionsMenu(anchor) {
		const pick = (p) => {
			st.priority = p;
			renderPrio();
			changed();
		};
		openMenu(anchor, { label: t("compose.button_options"), title: t("compose.label_importance"), items: [
			{ label: t("compose.label_importance_normal"), icon: st.priority === "" ? "check" : "blank",
				pressed: st.priority === "", onclick: () => pick("") },
			{ label: t("list.label_importance_high"), icon: st.priority === "high" ? "check" : "blank",
				pressed: st.priority === "high", onclick: () => pick("high") },
			{ label: t("list.label_importance_low"), icon: st.priority === "low" ? "check" : "blank",
				pressed: st.priority === "low", onclick: () => pick("low") }
		] });
	}

	function renderPrio() {
		const p = st.priority;
		v.prio.hidden = p === "";
		v.prio.classList.toggle("high", p === "high");
		v.prio.textContent = p === "high" ? t("list.label_importance_high") : (p === "low" ? t("list.label_importance_low") : "");
	}

	// ---- Attachments ----

	function totalOf(s) {
		return s.atts.filter((a) => !a.failed && !a.cancelled).reduce((n, a) => n + (a.total || 0), 0);
	}

	// Checked against the limits before anything is sent: the number of
	// files, the attachment bytes a message may carry (max_attach_total,
	// from the server), the message's size as the server will estimate it
	// (estimateBytes, text included), and the room left for drafts (the
	// server's figure for all the user's drafts, plus what was added here
	// since)
	function addFiles(files) {
		const s = st;
		if (!s || files.length === 0 || readonlyNow())
			return;
		const max = limits.max_attachments > 0 ? limits.max_attachments : 0;
		const maxTotal = limits.max_attach_total > 0 ? limits.max_attach_total : 0;
		const maxDraft = limits.max_draft_bytes > 0 ? limits.max_draft_bytes : 0;
		const refused = [];
		for (const file of files) {
			const count = s.atts.filter((a) => !a.failed && !a.cancelled).length;
			const total = totalOf(s);
			if (max && count >= max) {
				refused.push(t("compose.label_too_many_files", { count: max }));
				break;
			}
			if ((maxTotal && total + file.size > maxTotal) || tooLarge(s, [file.size])) {
				refused.push(t("compose.label_attachments_over", Object.assign({ name: file.name }, sizeText())));
				continue;
			}
			const used = (typeof s.usage === "number" ? s.usage : 0) + s.added;
			if (maxDraft && used + file.size > maxDraft) {
				refused.push(t("compose.label_drafts_full", { name: file.name, max: limitSize(maxDraft), used: size(used) }));
				continue;
			}
			s.added += file.size;
			s.touched = true;
			s.atts.push({ key: uploadKey(), id: null, name: file.name || "file", type: file.type || "",
				total: file.size, received: 0, complete: false, file, waiting: false });
		}
		if (refused.length > 0)
			banner(refused.join(" "), "error");
		renderAtts();
		pump(s);
	}

	// Uploads the files waiting, one after another
	function pump(s) {
		if (s.uploading)
			return;
		const next = s.atts.find((a) => a.file && !a.complete && !a.failed && !a.cancelled);
		if (!next) {
			if (s.closed && !uploading(s) && !unsaved(s))
				live.delete(s.key);
			if (s === st)
				sendState();
			return;
		}
		s.uploading = upload(s, next).finally(() => {
			s.uploading = null;
			pump(s);
		});
		if (s === st)
			sendState();
	}

	async function upload(s, a) {
		if (!(await ensureId(s))) {
			a.failed = true;
			a.error = t("compose.label_draft_failed");
			paint(s, a);
			return;
		}
		live.set(s.key, s);
		let offset = a.received || 0;
		let fails = 0;
		let tried = false;
		// not past the page's life: signed out or gone, the loop ends
		while (!a.cancelled && !a.complete && !dead) {
			let b64;
			try {
				b64 = await readB64(a.file.slice(offset, Math.min(a.total, offset + PIECE)));
			} catch (e) {
				a.failed = true;
				a.error = t("compose.label_file_unreadable");
				break;
			}
			if (a.cancelled)
				break;
			try {
				tried = true;
				const r = await rpc.call("draft.attach", { id: s.id, upload: a.key, name: a.name, type: a.type, offset,
					total: a.total, b64 }, { alone: true, retry: false });
				const x = r.attachment || {};
				if (typeof x.id === "string")
					a.id = x.id;
				offset = a.received = typeof x.received === "number" ? x.received : offset;
				a.waiting = false;
				fails = 0;
				if (x.complete === true || offset >= a.total)
					a.complete = true;
			} catch (e) {
				if (e.error === "conflict" && e.reason === "offset" && typeof e.received === "number") {
					// the server has this many bytes (a reply was lost, or
					// the piece came twice): go on from there
					offset = a.received = e.received;
					a.waiting = false;
					if (offset >= a.total) {
						a.complete = true;
						await adopt(s, a);
					}
					continue;
				}
				if (e.error === "failed" || e.error === "internal" || e.unsure
					|| (e.error === "conflict" && e.reason === "busy")) {
					a.waiting = true;
					paint(s, a);
					await sleep(UPLOAD_BACKOFF[Math.min(fails, UPLOAD_BACKOFF.length - 1)]);
					fails++;
					continue;
				}
				a.failed = true;
				a.error = errText(e);
			}
			paint(s, a);
		}
		if (dead && !a.complete && !a.cancelled) {
			a.failed = true;
			a.error = t("errors.failed");
		}
		if (a.cancelled && a.id)
			detach(s, a);
		else if (a.cancelled && tried)
			refreshAtts(s);	// what arrived of it, if any, shows and can be removed
		if (a.complete)
			a.file = null;
		paint(s, a);
	}

	// A file that arrived although its last reply was lost: its id from
	// the draft
	async function adopt(s, a) {
		if (a.id)
			return;
		try {
			const d = (await rpc.call("draft.get", { id: s.id }, { retry: false })).draft;
			const known = new Set(s.atts.map((x) => x.id).filter((x) => x));
			const m = (d.attachments || []).find((x) => !known.has(x.id) && x.name === a.name && x.total === a.total);
			if (m)
				a.id = m.id;
		} catch (e) {
			// the id stays unknown: Remove then refreshes from the server
		}
	}

	function detach(s, a) {
		if (!a.id || !s.id)
			return Promise.resolve();
		return rpc.call("draft.detach", { id: s.id, attachment: a.id }, { retry: false }).then(() => {
			if (typeof o.onDrafts === "function")
				o.onDrafts();
		}, (e) => {
			if (e.unsure)
				setTimeout(() => refreshAtts(s), 2000);	// it may have gone: show what is there
			else if (e.error !== "notfound")
				notice(errText(e));
		});
	}

	function removeAtt(a) {
		const s = st;
		const i = s.atts.indexOf(a);
		const running = a.file && !a.complete && !a.failed;
		if (a.file && !a.complete)
			s.added = Math.max(0, s.added - a.total);
		if (running)
			a.cancelled = true;	// the upload stops and detaches what arrived
		else if (a.id)
			detach(s, a);
		if (i >= 0)
			s.atts.splice(i, 1);
		s.touched = true;
		renderAtts();
		sendState();
		const rows = Array.from(v.atts.querySelectorAll(".catt .ib"));
		(rows[Math.min(i, rows.length - 1)] || v.attach).focus();
	}

	function renderAtts() {
		if (!v || !st)
			return;
		clear(v.atts);
		for (const a of st.atts)
			v.atts.append(attRow(a));
		v.atts.hidden = st.atts.length === 0;
	}

	function paint(s, a) {
		if (s !== st || !v)
			return;
		const old = v.atts.querySelector('[data-key="' + CSS.escape(a.key) + '"]');
		if (old) {
			const had = old.contains(document.activeElement);
			const row = attRow(a);
			old.replaceWith(row);
			if (had)
				row.querySelector(".ib").focus();
		}
		sendState();
	}

	function attRow(a) {
		const up = a.file && !a.complete && !a.failed;
		const img = /^image\//i.test(a.type);
		const meta = h("span", { class: "meta" }, h("span", { class: "nm ell", text: a.name }));
		let note;
		if (a.failed)
			note = t("compose.label_upload_failed", { reason: a.error || t("errors.failed") });
		else if (a.incomplete)
			note = t("compose.label_upload_incomplete");
		else if (up && a.waiting)
			note = t("compose.label_upload_waiting");
		else if (up)
			note = t("compose.label_uploading", { done: size(a.received), total: size(a.total) });
		else
			note = size(a.total);
		meta.append(h("small", { class: a.failed || a.incomplete ? "bad" : null, text: note }));
		if (up) {
			const pct = a.total > 0 ? Math.floor(a.received * 100 / a.total) : 0;
			const fill = h("span");
			fill.style.width = pct + "%";
			meta.append(h("span", { class: "prog", role: "progressbar", "aria-label": t("compose.label_upload_progress",
				{ name: a.name }), "aria-valuemin": "0", "aria-valuemax": "100", "aria-valuenow": String(pct) }, fill));
		}
		const label = up ? t("compose.button_cancel_upload", { name: a.name }) : t("compose.button_remove_name", { name: a.name });
		return h("li", { class: "catt" + (a.failed || a.incomplete ? " failed" : ""), "data-key": a.key },
			icon(img ? "image" : "file"), meta,
			h("button", { class: "ib", type: "button", "aria-label": label, title: label, onclick: () => removeAtt(a) },
				icon("x")));
	}

	// Send is off while files upload, with the reason
	function sendState() {
		if (!v || !st)
			return;
		const busy = st.atts.some((a) => a.file && !a.complete && !a.failed && !a.cancelled);
		v.send.disabled = busy || v.el.classList.contains("readonly");
		v.sendHint.textContent = busy ? t("compose.label_wait_uploads") : "";
		v.el.classList.toggle("uploading", busy);
		if (busy)
			status(t("compose.label_wait_uploads"));
		else if (v.status.textContent === t("compose.label_wait_uploads"))
			status(st.saved !== null ? t("compose.label_draft_saved", { time: timeNow() }) : "");
	}

	// ---- Opening and closing ----

	function host() {
		const x = typeof o.host === "function" ? o.host() : o.host;
		return x || document.body;
	}

	function show(s, opener, focusWhere) {
		if (!v) {
			v = build();
			host().append(v.el);
		}
		st = s;
		s.closed = false;
		v.opener = opener || document.activeElement;
		banner(null);
		v.subject.value = s.subject;
		v.body.value = s.body;
		v.el.hidden = false;
		v.el.classList.remove("dropping");
		for (const f of ["cc", "bcc"]) {
			v.fields[f].row.hidden = true;
			(f === "cc" ? v.ccBtn : v.bccBtn).hidden = false;
		}
		render();
		status(s.saved !== null ? t("compose.label_draft_saved", { time: timeNow() }) : "");
		phone(true);
		docked(true);
		fit();
		if (focusWhere === "body") {
			v.body.focus();
			v.body.setSelectionRange(0, 0);
			v.body.scrollTop = 0;
		} else if (focusWhere === "subject")
			v.subject.focus();
		else if (focusWhere !== "none")
			v.fields.to.input.focus();
		if (s.maybeSent)
			banner(t("errors.conflict_maybe_sent"), "error");
		readOnly(false);
		if (s.stale)
			staleBar();
		if (s.gone)
			goneBar();
	}

	function render() {
		if (!v || !st)
			return;
		v.title.textContent = title(st);
		for (const f of FIELDS)
			renderChips(f);
		for (const f of ["cc", "bcc"]) {
			if (st.entries[f].length > 0)
				reveal(f, false);
		}
		renderFrom();
		renderPrio();
		renderAtts();
		sendState();
	}

	// The state of a draft, as shown: what the user typed is never
	// replaced by the server's copy while there is text not yet saved
	function unsaved(s) {
		return !s.id || s.stale || JSON.stringify(fields(s)) !== s.saved;
	}

	function uploading(s) {
		return s.atts.some((a) => a.file && !a.complete && !a.failed && !a.cancelled);
	}

	function findKept(id) {
		for (const s of live.values()) {
			if (s.id === id)
				return s;
		}
		return null;
	}

	// One open at a time: a second Compose or Reply while one is opening
	// is ignored (it would make a second draft)
	let opening = false;

	function open(opts) {
		const mode = ["reply", "replyall", "forward"].includes(opts && opts.mode) ? opts.mode : "new";
		const s = blank(mode);
		const item = opts && opts.item;
		const msg = opts && opts.msg;
		if (mode !== "new") {
			if (!item || typeof item.id !== "string" || !msg)
				return;
			s.sourceId = item.id;
		}
		if (opening)
			return;
		opening = true;
		replace(() => {
			const ready = settings ? Promise.resolve(settings) : loadSettings();
			if (settings)
				loadSettings();
			ready.then(() => {
				opening = false;
				if (dead)
					return;
				if (mode === "new") {
					if (Array.isArray(opts.to))
						s.entries.to = opts.to.filter((x) => typeof x === "string" && x.trim() !== "");
					s.body = signatureFor("new");
					if (settings && typeof settings.from_domain === "string") {
						s.from_domain = settings.from_domain;
						s.from_local = typeof settings.from_local === "string" ? settings.from_local : "";
					}
				} else {
					s.subject = prefixed(msg.subject || item.subject, mode);
					const quote = mode === "forward" ? forwardBody(item, msg) : replyBody(item, msg);
					s.body = "\n\n" + signatureFor(mode) + quote;
				}
				// addressed by the caller (Write to, a contact): theirs to keep
				s.fresh = !(mode === "new" && s.entries.to.length > 0);
				show(s, opts.opener, mode === "new" ? (s.entries.to.length > 0 ? "subject" : "to")
					: (mode === "forward" ? "to" : "body"));
				ensureId(s).then((ok) => {
					if (ok)
						save(s);
				});
			});
		});
	}

	function openDraft(id, opts) {
		opts = opts || {};
		// already in the panel: just go there
		if (st && st.id === id && v && !v.el.hidden) {
			v.body.focus();
			return;
		}
		// kept here (its close-save failed, is still running, or files
		// upload): after its saves, the text as typed
		const kept = findKept(id);
		if (kept) {
			kept.chain.then(() => resume(kept, opts.opener));
			return;
		}
		if (opening)
			return;
		opening = true;
		replace(() => {
			Promise.all([loadSettings(), rpc.call("draft.get", { id }, { retry: false })]).then(([, r]) => {
				opening = false;
				if (dead)
					return;
				const d = r.draft || {};
				const s = blank(typeof d.mode === "string" ? d.mode : "new");
				s.id = d.id;
				fromDraft(s, d);
				attsFromDraft(s, d);
				s.usage = r.usage && typeof r.usage.bytes === "number" ? r.usage.bytes : null;
				s.saved = snapshotOf(d);
				s.maybeSent = !!(opts.maybeSent || d.maybe_sent);
				show(s, opts.opener, "none");
				focusFirst();
			}, (e) => {
				opening = false;
				notice(e && e.error === "notfound" && sending.has(id) ? t("compose.label_still_sending") : errText(e));
			});
		});
	}

	function snapshotOf(d) {
		return JSON.stringify({ to: d.to, cc: d.cc, bcc: d.bcc, subject: d.subject, body: d.body,
			from_domain: d.from_domain, from_local: d.from_local, priority: d.priority });
	}

	function focusFirst() {
		const first = [v.fields.to.input, v.subject].find((x) => x.value === "") || v.body;
		first.focus();
	}

	// A kept state in the panel again, with its own text; only its
	// attachments are read from the server again (its rev stays, so a
	// change made in another window meanwhile is still noticed)
	function resume(s, opener) {
		if (dead)
			return;
		if (st === s && v && !v.el.hidden) {
			v.body.focus();
			return;
		}
		if (!unsaved(s) && !uploading(s) && s.id) {
			live.delete(s.key);
			openDraft(s.id, { opener, maybeSent: s.maybeSent });
			return;
		}
		replace(() => {
			show(s, opener, "none");
			focusFirst();
			pump(s);
			refreshAtts(s);
			if (s.lastError && !s.gone && !s.stale && s.lastError.error !== "failed" && !s.lastError.unsure)
				banner(errText(s.lastError), "error");
			if (unsaved(s) && !s.stale && !s.gone) {
				status(t("compose.label_unsaved"));
				later(SAVE_DELAY);
			}
		});
	}

	// The attachments as the server has them (an unfinished one shows,
	// and can be removed)
	function refreshAtts(s) {
		if (!s.id)
			return Promise.resolve();
		return rpc.call("draft.get", { id: s.id }, { retry: false }).then((r) => {
			attsFromDraft(s, r.draft || {});
			if (r.usage && typeof r.usage.bytes === "number") {
				s.usage = r.usage.bytes;
				s.added = 0;
			}
			if (s === st)
				renderAtts();
			if (s === st)
				sendState();
		}, () => {});
	}

	// Another draft in the panel: the one shown is closed first (kept)
	function replace(then) {
		if (st && v && !v.el.hidden)
			close({ quiet: false, then });
		else
			then();
	}

	// After a failed save of a closed draft: tried again, with growing
	// pauses, while the page is open and the text is not shown
	const KEEP_RETRY = [5000, 10000, 20000, 40000, 60000];
	const RETRYABLE = ["failed", "internal", "busy"];
	function keepTrying(s, n) {
		if (n === 0) {
			if (s.retrying)
				return;	// one loop per draft
			s.retrying = true;
		}
		setTimeout(() => {
			if (dead || s === st || !live.has(s.key) || s.stale || s.gone) {
				s.retrying = false;
				return;
			}
			save(s).then((ok) => {
				if (ok) {
					s.retrying = false;
					settled(s);
					return;
				}
				const e = s.lastError;
				const again = !e || e.unsure || RETRYABLE.includes(e.error) || (e.error === "conflict" && e.reason === "busy");
				if (again && !s.stale && !s.gone) {
					keepTrying(s, n + 1);
					return;
				}
				// it cannot succeed by trying again: kept here, the user told
				s.retrying = false;
				stash(s);
				if (!dead)
					notice(t(s.gone ? "compose.label_gone_kept" : "compose.label_save_gave_up"),
						{ action_label: t("compose.button_open_draft"), on_action: () => resume(s, null), ms: 20000 });
			});
		}, KEEP_RETRY[Math.min(n, KEEP_RETRY.length - 1)]);
	}

	// Saved: nothing to keep here unless files still upload
	function settled(s) {
		if (!uploading(s) && s !== st)
			live.delete(s.key);
		unstash(s);
	}

	// Closes the panel.  how.sent: no notice (the send says what
	// happens); how.discard: deleted already
	function close(how) {
		how = how || {};
		if (!st || !v || v.el.hidden) {
			if (how.then)
				how.then();
			return;
		}
		const s = st;
		stopRecheck(s);
		for (const f of FIELDS)
			commit(f);
		fields(s);	// the text in the fields into the state
		closeLists();
		clearTimeout(v.timer);
		s.closed = true;
		const opener = v.opener;
		v.el.hidden = true;
		docked(false);
		phone(false, !!how.then);
		st = null;
		if (uploading(s))
			live.set(s.key, s);
		if (!how.sent && !how.discard) {
			if (!hasContent(s)) {
				// nothing to keep
				s.atts.forEach((a) => {
					a.cancelled = true;
				});
				live.delete(s.key);
				unstash(s);
				if (s.id || s.created)
					ensureId(s).then((ok) => ok && rpc.call("draft.delete", { id: s.id }, { retry: false })
						.then(() => o.onDrafts && o.onDrafts(), () => {}));
			} else {
				// kept here until the server has it; "saved" only then
				live.set(s.key, s);
				const reopenIt = () => resume(s, opener);
				save(s).then((ok) => {
					if (dead)
						return;
					if (ok) {
						settled(s);
						notice(t("compose.label_kept_draft"), { action_label: t("compose.button_open_draft"),
							on_action: reopenIt, ms: 8000 });
						return;
					}
					stash(s);
					const e = s.lastError;
					const again = !s.stale && !s.gone && (!e || e.unsure || RETRYABLE.includes(e.error)
						|| (e.error === "conflict" && e.reason === "busy"));
					notice(t(s.gone ? "compose.label_gone_kept" : (again ? "compose.label_not_saved" : "compose.label_save_gave_up")),
						{ action_label: t("compose.button_open_draft"), on_action: reopenIt, ms: 20000 });
					if (again)
						keepTrying(s, 0);
				});
			}
		}
		if (opener && opener.isConnected && !opener.closest("[hidden]"))
			opener.focus({ preventScroll: true });
		else if (typeof o.onClosed === "function")
			o.onClosed();
		if (how.then)
			how.then();
	}

	async function discardDraft() {
		const s = st;
		if (hasContent(s)) {
			const yes = await confirmDialog({ title: t("compose.button_discard"), text: t("compose.label_discard_confirm"),
				ok: t("compose.button_discard"), danger: true });
			if (!yes || st !== s)
				return;
		}
		s.atts.forEach((a) => {
			a.cancelled = true;
		});
		close({ discard: true });
		live.delete(s.key);
		unstash(s);
		if (await ensureId(s)) {
			rpc.call("draft.delete", { id: s.id }, { retry: false }).then(() => {
				notice(t("compose.label_discarded"));
				if (typeof o.onDrafts === "function")
					o.onDrafts();
			}, (e) => notice(errText(e)));
		}
	}

	// ---- Sending ----

	const sending = new Set();	// draft ids with a send under way here

	function allEntries(s) {
		const out = [];
		for (const f of FIELDS)
			s.entries[f].forEach((e, i) => out.push({ f, i }));
		return out;
	}

	// The number of recipients as the server counts them: each address
	// once, ASCII case ignored (the server also counts a local user named
	// twice by different names once, which this cannot see)
	function recipientCount(s) {
		const seen = new Set();
		for (const f of FIELDS) {
			for (const e of s.entries[f]) {
				const m = /<([^>]*)>\s*$/.exec(e);
				seen.add((m ? m[1] : e).trim().replace(/[A-Z]+/g, (c) => c.toLowerCase()));
			}
		}
		return seen.size;
	}

	// The message's size as the server will estimate it (estimateBytes)
	function tooLarge(s, extra) {
		const max = limits.max_message_bytes > 0 ? limits.max_message_bytes : 0;
		if (!max)
			return false;
		const sizes = s.atts.filter((a) => !a.failed && !a.cancelled).map((a) => a.total || 0).concat(extra || []);
		return messageBytes(fields(s).body, sizes) > max;
	}

	function sizeText() {
		return { total: limitSize(limits.max_attach_total > 0 ? limits.max_attach_total : 0),
			max: limitSize(limits.max_message_bytes > 0 ? limits.max_message_bytes : 0) };
	}

	// A limit in MB with one decimal (14.6 MB, not 15 MB: a limit must not
	// read larger than it is); below 1 MB as size() writes it
	function limitSize(n) {
		if (n < 1048576)
			return size(n);
		let v;
		try {
			v = new Intl.NumberFormat(document.documentElement.lang || undefined, { maximumFractionDigits: 1 })
				.format(Math.floor(n / 104857.6) / 10);
		} catch (e) {
			v = String(Math.floor(n / 104857.6) / 10);
		}
		return t("main.label_size_mb", { n: v });
	}

	async function sendNow() {
		const s = st;
		if (!s || v.send.disabled)
			return;
		if (s.gone) {
			goneBar();
			return;
		}
		if (s.id && sending.has(s.id)) {
			notice(t("compose.label_still_sending"));
			return;
		}
		for (const f of FIELDS)
			commit(f);
		s.bad = null;
		if (allEntries(s).length === 0) {
			banner(t("compose.label_no_recipient"), "error");
			v.fields.to.input.focus();
			return;
		}
		const maxRecipients = limits.max_recipients > 0 ? limits.max_recipients : 0;
		if (maxRecipients && recipientCount(s) > maxRecipients) {
			banner(t("errors.invalid_toomany", { count: maxRecipients }), "error");
			return;
		}
		if (s.atts.some((a) => a.incomplete || a.failed)) {
			banner(t("errors.conflict_incomplete"), "error");
			return;
		}
		if (tooLarge(s)) {
			banner(t("compose.label_message_over", sizeText()), "error");
			return;
		}
		if (v.subject.value.trim() === "") {
			const yes = await confirmDialog({ title: t("compose.button_send"), text: t("compose.label_no_subject_confirm"),
				ok: t("compose.button_send") });
			if (!yes || st !== s) {
				if (st === s)
					v.subject.focus();
				return;
			}
		}
		go(s, { s, key: newKey(), confirm: false, tries: 0, rekeyed: false });
	}

	// Close, save the last changes (keepalive when it fits: the tab may be
	// closed right after Send), then send
	function go(s, job) {
		fields(s);
		close({ sent: true });
		live.set(s.key, s);
		notice(t("compose.label_sending"), { ms: 120000 });
		save(s, { keepalive: true }).then((ok) => {
			if (!ok) {
				reopen(s, s.stale ? null : t("compose.label_draft_failed"));
				return;
			}
			sending.add(s.id);
			attempt(job);
		});
	}

	// The end of a send attempt (sent, given up, or back to the user)
	function done(s) {
		sending.delete(s.id);
	}

	// One draft.send of a job (the same key on every retry of it)
	function attempt(job) {
		const s = job.s;
		const args = { id: s.id, key: job.key };
		if (job.confirm)
			args.confirm_resend = true;
		rpc.call("draft.send", args, { retry: false, keepalive: true }).then((r) => {
			done(s);
			live.delete(s.key);
			unstash(s);
			const w = { nocopy: "compose.label_sent_nocopy", longlist: "compose.label_sent_longlist",
				recovered: "compose.label_sent_recovered" }[r.warning];
			notice(t(w || "list.label_message_sent"));
			if (typeof o.onSent === "function")
				o.onSent({ mode: s.mode, sourceId: s.sourceId });
		}, (e) => failed(job, e));
	}

	function retryLater(job, text, gaveUp) {
		if (job.tries >= SEND_RETRY.length) {
			done(job.s);
			live.delete(job.s.key);
			notice(gaveUp, { ms: 15000 });
			if (typeof o.onDrafts === "function")
				o.onDrafts();
			return;
		}
		notice(text, { ms: SEND_RETRY[job.tries] + 5000 });
		setTimeout(() => {
			if (!dead)
				attempt(job);
		}, SEND_RETRY[job.tries]);
		job.tries++;
	}

	function failed(job, e) {
		const s = job.s;
		const reason = typeof e.reason === "string" ? e.reason : "";
		if (e.unsure || e.error === "internal") {
			// it may have gone: only the same key again
			retryLater(job, t("compose.label_still_sending"), t("compose.label_send_unsure"));
			return;
		}
		if (e.error === "conflict") {
			switch (reason) {
				case "sending":
				case "busy":
				case "taken":
					retryLater(job, t("compose.label_still_sending"), t("compose.label_send_in_drafts"));
					return;
				case "key":
					if (!job.rekeyed) {
						job.rekeyed = true;
						job.key = newKey();
						attempt(job);
						return;
					}
					break;
				case "partial":
					done(s);
					live.delete(s.key);
					notice(t("errors.conflict_partial"), { ms: 15000 });
					if (typeof o.onSent === "function")
						o.onSent({ mode: s.mode, sourceId: s.sourceId, partial: true });
					return;
				case "unknown":
				case "maybe_sent":
					s.maybeSent = true;
					reopen(s, null);
					askResend(s);
					return;
				case "stale":
					reopen(s, errText(e));
					return;
				case "incomplete":
					// an unfinished attachment on the server: shown, removable
					refreshAtts(s).then(() => reopen(s, errText(e)));
					return;
			}
		}
		if (typeof e.index === "number")
			markBad(s, e.index);
		reopen(s, errText(e) + (typeof e.entry === "string" && e.entry !== "" ? " (" + e.entry + ")" : ""));
	}

	// index counts the entries of To, Cc and Bcc in that order
	function markBad(s, index) {
		const all = allEntries(s);
		s.bad = index >= 0 && index < all.length ? all[index] : null;
	}

	// The panel again, with the error; what was typed is all there
	function reopen(s, text) {
		done(s);
		dismissNotice();
		live.set(s.key, s);
		const back = () => {
			show(s, null, "none");
			if (text)
				banner(text, "error");
			if (s.bad) {
				reveal(s.bad.f, false);
				renderChips(s.bad.f);
				v.fields[s.bad.f].input.focus();
			} else
				v.send.focus();
			if (typeof o.onDrafts === "function")
				o.onDrafts();
		};
		if (st && st !== s && v && !v.el.hidden) {
			// another message is being written: offer this one
			notice(text || t("errors.conflict_maybe_sent"), { action_label: t("compose.button_open_draft"),
				on_action: () => resume(s, null), ms: 15000 });
			return;
		}
		back();
	}

	function askResend(s) {
		dismissNotice();
		const d = openDialog({ title: t("compose.label_resend_title"), returnTo: v.send,
			body: [h("p", { class: "dlg-text", text: t("compose.label_resend_text") })],
			buttons: [
				{ label: t("compose.button_not_now"), onclick: () => d.close() },
				{ label: t("compose.button_send_again"), primary: true, onclick: () => {
					d.close("keep-focus");
					if (st !== s)
						return;
					go(s, { s, key: newKey(), confirm: true, tries: 0, rekeyed: false });
				} }
			] });
	}

	// A draft flagged maybe_sent, sent from the panel: the server answers
	// maybe_sent and the question above follows

	// ---- Phone ----

	// The panel covers the screen: the rest is inert, Back closes it
	function phone(on, keepHistory) {
		const shell = v.el.parentNode;
		const others = shell ? Array.from(shell.children).filter((x) => x !== v.el && !x.classList.contains("toast-region")) : [];
		const ph = mq.matches;
		v.el.setAttribute("aria-modal", String(on && ph));
		for (const x of others) {
			if (on && ph)
				x.setAttribute("inert", "");
			else
				x.removeAttribute("inert");
		}
		// one history entry for the panel, also when it shows another draft
		if (on && ph && !(history.state && history.state.compose)) {
			try {
				const base = history.state && typeof history.state === "object" ? history.state : {};
				history.pushState(Object.assign({}, base, { webmail: true, compose: true }), "");
			} catch (e) {
				// Back then leaves by the button only
			}
		} else if (!on && !keepHistory && history.state && history.state.compose) {
			try {
				history.back();
			} catch (e) {
				// nothing to go back to
			}
		}
	}

	function onPop(e) {
		if (st && v && !v.el.hidden && !(e.state && e.state.compose))
			close();
	}

	// The keyboard on a phone: the panel fits the part of the screen above it
	function fit() {
		if (!v)
			return;
		const vv = window.visualViewport;
		if (!vv || !mq.matches || v.el.hidden) {
			v.el.style.top = "";
			v.el.style.height = "";
			return;
		}
		v.el.style.top = vv.offsetTop + "px";
		v.el.style.height = vv.height + "px";
	}

	// The docked panel: notices move to the left so they do not cover Send
	function docked(on) {
		document.documentElement.classList.toggle("wm-docked", on && !mq.matches);
	}

	// A file dropped outside the panel while it is open: never opened by
	// the browser in place of the app
	function onDocDrag(e) {
		if (!st || !v || v.el.hidden || v.el.contains(e.target))
			return;
		const types = e.dataTransfer ? Array.from(e.dataTransfer.types || []) : [];
		if (types.includes("Files")) {
			e.preventDefault();
			if (e.dataTransfer)
				e.dataTransfer.dropEffect = "none";
		}
	}

	function onMedia() {
		docked(!!(v && !v.el.hidden));
		if (v && !v.el.hidden) {
			v.el.setAttribute("aria-modal", String(mq.matches));
			const shell = v.el.parentNode;
			for (const x of shell ? Array.from(shell.children) : []) {
				if (x !== v.el && !x.classList.contains("toast-region")) {
					if (mq.matches)
						x.setAttribute("inert", "");
					else
						x.removeAttribute("inert");
				}
			}
		}
		fit();
	}

	// ---- Text not saved when the page goes away ----
	//
	// The page is closed, reloaded (another user signed in in this
	// browser), or the session ends while text is not saved: the fields
	// (not the attachments) go into sessionStorage under the user's number
	// and the draft's id (or the state's key), and the next time compose
	// starts for the SAME user it offers them back.  Removed when the text
	// is saved.  Storage that is missing or refuses: nothing is kept.

	const uid = o.user && typeof o.user.number === "number" ? o.user.number : 0;
	const STASH = "webmail.unsaved." + uid + ".";

	function stash(s) {
		if (!uid || !unsaved(s) || !hasContent(s))
			return;
		try {
			const k = STASH + (s.id || s.key);
			sessionStorage.setItem(k, JSON.stringify({ id: s.id, rev: s.rev, mode: s.mode, fields: fields(s),
				at: Date.now() }));
			if (s.stashKey && s.stashKey !== k)
				sessionStorage.removeItem(s.stashKey);
			s.stashKey = k;
		} catch (e) {
			// no storage: nothing kept
		}
	}

	function unstash(s) {
		try {
			for (const k of [s.stashKey, STASH + s.key, s.id ? STASH + s.id : null]) {
				if (k)
					sessionStorage.removeItem(k);
			}
		} catch (e) {
			// no storage
		}
		s.stashKey = null;
	}

	function stashAll() {
		if (st && v && !v.el.hidden)
			stash(st);
		for (const s of live.values())
			stash(s);
	}

	// The kept texts of this user: [{key, id, subject, to, at}], newest
	// first, leaving out those this page still holds (the Drafts folder
	// lists them).  Kept texts of OTHER users in
	// this tab (a shared computer) are removed.
	function stashes() {
		const out = [];
		try {
			const other = [];
			for (let i = 0; i < sessionStorage.length; i++) {
				const k = sessionStorage.key(i);
				if (!k || k.indexOf("webmail.unsaved.") !== 0)
					continue;
				if (k.indexOf(STASH) !== 0) {
					other.push(k);
					continue;
				}
				const x = JSON.parse(sessionStorage.getItem(k) || "null");
				// a text this page still holds is reached through its draft
				if (x && typeof x.id === "string" && x.id !== "" && ((st && st.id === x.id) || findKept(x.id)))
					continue;
				if (live.has(k.substr(STASH.length)) || (st && STASH + st.key === k))
					continue;
				if (x && x.fields && typeof x.fields === "object")
					out.push({ key: k, id: typeof x.id === "string" ? x.id : "", at: x.at || 0,
						subject: String(x.fields.subject || ""), to: String(x.fields.to || "") });
			}
			for (const k of other)
				sessionStorage.removeItem(k);
		} catch (e) {
			return [];
		}
		return out.sort((a, b) => b.at - a.at);
	}

	// Offered once per page load, the newest; after that only from the
	// Drafts folder's list
	function offerStash() {
		const list = uid ? stashes() : [];
		if (list.length === 0)
			return;
		notice(t("compose.label_restored"), { action_label: t("compose.button_open_draft"),
			on_action: () => restoreKept(list[0].key), ms: 30000 });
	}

	// The user declines a kept text (its Delete in the Drafts folder)
	function dropKept(key) {
		try {
			if (key.indexOf(STASH) === 0)
				sessionStorage.removeItem(key);
		} catch (e) {
			// no storage
		}
	}

	function restoreKept(key) {
		let x = null;
		try {
			x = key.indexOf(STASH) === 0 ? JSON.parse(sessionStorage.getItem(key) || "null") : null;
		} catch (e) {
			x = null;
		}
		if (x && x.fields)
			restore(key, x);
	}

	// A kept text in compose.  Its draft is saved with the revision the
	// text was kept at, so a change saved since (another window) raises the
	// stale bar instead of being overwritten; a draft that is gone (sent
	// or deleted) gets the choice of goneBar.
	function restore(k, x) {
		const s = blank(typeof x.mode === "string" ? x.mode : "new");
		const f = x.fields;
		for (const n of FIELDS)
			s.entries[n] = splitEntries(typeof f[n] === "string" ? f[n] : "");
		s.subject = typeof f.subject === "string" ? f.subject : "";
		s.body = typeof f.body === "string" ? f.body : "";
		s.from_domain = typeof f.from_domain === "string" ? f.from_domain : "";
		s.from_local = typeof f.from_local === "string" ? f.from_local : "";
		s.priority = PRIORITIES.includes(f.priority) ? f.priority : "";
		s.stashKey = k;
		const shown = () => replace(() => {
			loadSettings().then(() => {
				show(s, null, "none");
				if (!s.gone)
					banner(t("compose.label_restored"), "info");
				focusFirst();
				if (!s.gone)
					save(s);
			});
		});
		if (typeof x.id !== "string" || x.id === "") {
			s.mode = "new";
			shown();
			return;
		}
		rpc.call("draft.get", { id: x.id }, { retry: false }).then((r) => {
			const d = r.draft || {};
			s.id = d.id;
			s.rev = typeof x.rev === "number" ? x.rev : d.rev;
			attsFromDraft(s, d);
			s.saved = null;
			shown();
		}, (e) => {
			if (e && e.error === "notfound") {
				s.id = x.id;
				s.gone = goneKind(e);
			} else
				s.mode = "new";
			shown();
		});
	}

	// A draft deleted from the Drafts folder: compose forgets it (the
	// panel closes if it shows it; its kept state and kept text go)
	function forget(id) {
		if (st && st.id === id && v && !v.el.hidden) {
			const s = st;
			s.gone = "deleted";
			s.atts.forEach((a) => {
				a.cancelled = true;
			});
			close({ discard: true });
		}
		for (const s of Array.from(live.values())) {
			if (s.id === id) {
				s.gone = "deleted";
				live.delete(s.key);
				unstash(s);
			}
		}
		dropKept(STASH + id);
	}

	// Before signing out: what is not saved is saved (a Promise, resolved
	// either way); what could not be saved is kept in sessionStorage
	function flush() {
		const all = [];
		if (st && v && !v.el.hidden) {
			for (const f of FIELDS)
				commit(f);
			all.push(st);
		}
		for (const s of live.values()) {
			if (!all.includes(s))
				all.push(s);
		}
		return Promise.all(all.map((s) => save(s).then((ok) => {
			if (!ok)
				stash(s);
		}, () => stash(s))));
	}

	// The tab or window goes away: what is not saved yet goes now (keepalive),
	// and is kept in sessionStorage until a save is acknowledged
	function onHide() {
		stashAll();
		if (st && v && !v.el.hidden && unsaved(st))
			save(st, { unload: true });
	}
	function onVisibility() {
		if (document.visibilityState === "hidden")
			onHide();
	}

	window.addEventListener("popstate", onPop);
	document.addEventListener("dragover", onDocDrag);
	document.addEventListener("drop", onDocDrag);
	window.addEventListener("pagehide", onHide);
	document.addEventListener("visibilitychange", onVisibility);
	if (window.visualViewport) {
		window.visualViewport.addEventListener("resize", fit);
		window.visualViewport.addEventListener("scroll", fit);
	}
	if (mq.addEventListener)
		mq.addEventListener("change", onMedia);

	function destroy() {
		stashAll();
		if (st && v && !v.el.hidden && unsaved(st))
			save(st, { unload: true });
		dead = true;
		stopRecheck(st);
		for (const k of live.values())
			stopRecheck(k);
		offPrefs();
		docked(false);
		document.removeEventListener("dragover", onDocDrag);
		document.removeEventListener("drop", onDocDrag);
		window.removeEventListener("popstate", onPop);
		window.removeEventListener("pagehide", onHide);
		document.removeEventListener("visibilitychange", onVisibility);
		if (window.visualViewport) {
			window.visualViewport.removeEventListener("resize", fit);
			window.visualViewport.removeEventListener("scroll", fit);
		}
		if (mq.removeEventListener)
			mq.removeEventListener("change", onMedia);
		if (v) {
			clearTimeout(v.timer);
			v.el.remove();
		}
		v = null;
		st = null;
	}

	setTimeout(offerStash, 0);

	return { open, openDraft, flush, forget, stashes, restoreKept, dropKept,
		isOpen: () => !!(st && v && !v.el.hidden), destroy };
}
