// The reader pane: one message, read-only.
//
// const reader = createReader({ pane, nextOf, onRead, onEscape, onShow, onItem, inboxDays })
//   pane: the reader <section> (its bar and its empty placeholder are kept);
//   nextOf(id): the list item below the message with this id, or null;
//   onRead(item, read): mark the list row read (true) or unread again
//   (false, when marking failed); returns true when the row changed;
//   onEscape(): Escape in the reader (focus back to the list);
//   onShow(item): the message shown is item (null: none), for the toolbar;
//   onItem(id, fields): the server sent the list fields of a message
//   (read, starred, replied, folder) with marking it read;
//   inboxDays(): the days the Inbox keeps mail (0: no limit).
// reader.open(item, { focus, delay })
//   shows a list item: its header at once, the rest when message.get
//   answers (a "Loading" text after 150 ms).  The user opened it, so it is
//   marked read.  focus: move focus to the message's heading.  delay: ms
//   to wait before asking the server (moving through the list with the
//   arrow keys: a message passed over quickly is not fetched or marked).
// reader.close()       back to the placeholder
// reader.current()     the id shown, or null
// reader.item()        the list item shown, or null
// reader.message()     the message shown (message.get's), or null while
//                      it is loading
// reader.remap(oldId, newId)
//                      a message's id changed (moved between the bases):
//                      the message shown and the ones kept follow it
// reader.patch(id, fields)
//                      fields of the message shown changed (the star: its
//                      note is shown again)
// reader.focus()       focus on the message's heading
// reader.destroy()
//
// Speed: after a message is shown, the one below it in the list is
// fetched in the background (message.get with mark_read false, never the
// sender's avatar: the server computes one only when asked, so each
// sender's is kept for the page's life and asked for on a miss only, with
// the user's own fetch or marking) and kept;
// opening it then shows it at once and marks it read with a normal call
// that returns only its list item (head: true), not the message again.
// At most MAX_KEPT messages are kept, each with the state token of the
// reply it came in; one whose token is not the latest is never used.  A
// reply for a message the user has left is kept, never shown.  Marking a
// message read here changes the token: the messages kept under the token
// before it are carried over to the new one.
//
// Safety: everything from a message goes into the page as text.  An HTML
// body goes only into the srcdoc of a frame sandboxed with
// "allow-popups allow-popups-to-escape-sandbox" (no scripts, no same
// origin, no forms, no top navigation), whose own content policy lets it
// load data: images and inline styles only, and https images too after
// "Show images" (for this message, this session), or at once when the
// sender is on the user's "always show images" list (settings
// images_always; "Always show images from this sender" in the images bar
// adds one) and the message is not in Spam (autoImages: never there,
// whoever the sender claims to be).  Before that the HTML
// is parsed into an inert document and cleaned (body.js prepareHtml: no
// <link> or <base>, every link opens a new tab without an opener).  Mail
// is shown on a light page, as it is written for, unless it says it
// supports a dark color scheme.

import { h, clear } from "./dom.js";
import { icon } from "./icons.js";
import { t } from "./i18n.js";
import { rpc } from "./rpc.js";
import { busy } from "./notice.js";
import { fullDate, zonedDate } from "./format.js";
import { avatarBadge } from "./avatar.js";
import { decode, textView, frameDoc, prepareHtml } from "./body.js";
import { attachmentList, openPreview, closePreview, inlineImages, contentId } from "./attachments.js";
import { prefs } from "./prefs.js";
import { notice } from "./notice.js";

const MAX_KEPT = 20;

// Whether a message's images from other sites load without asking.
// folders: the folder ids known for it (its list item's and its
// message's); none may be "spam" or "trash" (spam moved to Trash loses
// its spam mark, so Trash counts as Spam here), and at least one must be
// known.  addr: its sender's structured ADDRESS (headers.from_addr, never
// the display name), "" when the server marked it malformed; netType:
// headers.net_type, which must be "internet".  The address must be a
// plain address with no control character (C0, DEL, C1 or a
// bidirectional control).  senders: the user's images_always list (ASCII
// lowercased by the server).  A sender's address can be faked: that is
// why Spam and Trash never qualify, whoever the sender claims to be.
// (Written for any JavaScript engine: the client test runs it.)
export function autoImages(folders, addr, netType, senders) {
	var known = false;
	if (!(folders instanceof Array))
		return false;
	for (var i = 0; i < folders.length; i++) {
		if (folders[i] === "spam" || folders[i] === "trash")
			return false;
		if (typeof folders[i] === "string" && folders[i] !== "")
			known = true;
	}
	if (!known || netType !== "internet" || typeof addr !== "string" || !(senders instanceof Array))
		return false;
	if (/[\u0000-\u001f\u007f-\u009f\u061c\u200e\u200f\u202a-\u202e\u2066-\u2069]/.test(addr)
		|| !/^[^@\s"<>()\[\],;:\\]+@[^@\s"<>()\[\],;:\\\/]+\.[^@\s"<>()\[\],;:\\\/]+$/.test(addr))
		return false;
	var a = addr.replace(/[A-Z]+/g, function (m) { return m.toLowerCase(); });
	for (i = 0; i < senders.length; i++) {
		if (senders[i] === a)
			return true;
	}
	return false;
}

export function createReader(o) {
	const pane = o.pane;
	const empty = pane.querySelector(".reader-empty");
	const live = h("div", { class: "vh", role: "status", "aria-live": "polite" });
	const scroll = h("div", { class: "reader-scroll", hidden: true });
	pane.append(scroll, live);

	const kept = new Map();		// id -> { msg, token, cids }
	const fetching = new Map();	// id -> Promise of a background fetch
	const avatars = new Map();	// senderKey() -> the sender's avatar (null: none)
	const MAX_AVATARS = 200;	// senders kept; the oldest is dropped
	const imagesOn = new Set();	// ids whose remote images the user allowed
	const plainChoice = new Map();	// id -> true: show the plain text
	let seq = 0;
	let cur = null;			// { id, item, msg, seq, heading, body, frame, ... }
	let timer = 0;
	let dead = false;

	// ---- The kept messages ----

	function fresh(e) {
		return e && typeof e.token === "string" && e.token !== "" && e.token === rpc.state;
	}

	// token: the state token of the reply msg came in
	function keep(id, msg, token) {
		if (typeof token !== "string" || token === "")
			return;
		const old = kept.get(id);
		kept.delete(id);
		kept.set(id, { msg, token, cids: old && old.msg === msg ? old.cids : null });
		prune();
	}

	// The token moved from before to after by a change made here (marking
	// read): what was kept under before is still good
	function carry(before, after) {
		if (typeof before !== "string" || before === "" || typeof after !== "string" || after === "")
			return;
		for (const e of kept.values()) {
			if (e.token === before)
				e.token = after;
		}
	}

	function prune() {
		for (const [id, e] of kept) {
			if (!fresh(e))
				kept.delete(id);
		}
		while (kept.size > MAX_KEPT)
			kept.delete(kept.keys().next().value);
	}

	function lookup(id) {
		const e = kept.get(id);
		if (!fresh(e)) {
			kept.delete(id);
			return null;
		}
		kept.delete(id);	// most recently used last
		kept.set(id, e);
		return e;
	}

	// ---- Sender avatars ----
	// The server computes a sender's avatar only when asked (avatar: true:
	// costly), so each sender's is kept for the page's life and asked for
	// only on a miss, by the user's own fetch or marking; the background
	// prefetch never asks.

	// The sender as the list item names it
	function senderKey(item) {
		return String(item.from || "") + "\n" + String(item.from_addr || "").toLowerCase();
	}

	function knownAvatar(item, msg) {
		const k = senderKey(item);
		if (msg && avatars.has(k))
			msg.avatar = avatars.get(k);
		return avatars.has(k);
	}

	// A reply that was asked for the avatar (in r.message or beside r.item)
	function learnAvatar(item, msg, value) {
		const a = typeof value === "string" ? value : null;
		const k = senderKey(item);
		avatars.delete(k);	// most recently learned last
		avatars.set(k, a);
		while (avatars.size > MAX_AVATARS)
			avatars.delete(avatars.keys().next().value);
		if (msg)
			msg.avatar = a;
	}

	function prefetch(id) {
		const next = o.nextOf(id);
		if (!next || typeof next.id !== "string" || lookup(next.id) || fetching.has(next.id))
			return;
		const p = rpc.call("message.get", { id: next.id, mark_read: false }, { background: true, retry: false })
			.then((r) => {
				fetching.delete(next.id);
				if (!dead && r.message)
					keep(next.id, r.message, rpc.stateOf(r));
				return r.message || null;
			}, () => {
				fetching.delete(next.id);
				return null;
			});
		fetching.set(next.id, p);
	}

	// ---- Opening ----

	function open(item, opts = {}) {
		if (dead || !item || typeof item.id !== "string")
			return;
		closePreview(false);
		stopFetching();
		clearTimeout(timer);
		timer = 0;
		const my = ++seq;
		const id = item.id;
		cur = { id, item, msg: null, seq: my };
		prune();
		header(item, null);
		if (opts.focus)
			focus();
		const e = lookup(id);
		if (e) {
			show(e.msg);
			settle(my, e.msg, false);
			return;
		}
		const waiting = fetching.get(id);
		if (waiting) {
			loading(waiting.then((m) => {
				if (my !== seq)
					return;
				if (m) {
					show(m);
					settle(my, m, false);
				} else
					fetchOne(my, item);
			}));
			return;
		}
		if (opts.delay > 0) {
			timer = setTimeout(() => {
				timer = 0;
				if (my === seq)
					fetchOne(my, item);
			}, opts.delay);
			return;
		}
		fetchOne(my, item);
	}

	// The user's request: the message, marked read
	function fetchOne(my, item) {
		const changed = item.read !== true && o.onRead(item, true);
		const before = rpc.state;
		const ask = !knownAvatar(item, null);
		const args = { id: item.id, mark_read: true };
		if (ask)
			args.avatar = true;
		const p = rpc.call("message.get", args).then((r) => {
			if (dead)
				return;
			if (ask && r.message && "avatar" in r.message)
				learnAvatar(item, r.message, r.message.avatar);
			else
				knownAvatar(item, r.message);
			if (changed)
				carry(before, rpc.stateOf(r));
			keep(item.id, r.message, rpc.stateOf(r));
			if (my !== seq)
				return;
			show(r.message);
			settle(my, r.message, true);
		}, (err) => {
			if (changed)
				o.onRead(item, false);
			if (!dead && my === seq)
				failed(item, err);
		});
		loading(p);
	}

	// Shown: mark it read when that was not done yet, then fetch the next
	function settle(my, msg, marked) {
		const item = cur.item;
		const ask = !knownAvatar(item, msg);
		if (marked || (msg.read === true && item.read === true)) {
			if (ask)
				avatarOnly(my, item, msg);
			prefetch(item.id);
			return;
		}
		const changed = o.onRead(item, true);
		msg.read = true;
		const before = rpc.state;
		const args = { id: item.id, mark_read: true, head: true };
		if (ask)
			args.avatar = true;
		rpc.call("message.get", args).then((r) => {
			if (dead)
				return;
			if (ask && "avatar" in r)
				gotAvatar(my, item, msg, r.avatar);
			// The message shown is the one kept, under the new state token,
			// with the list fields the server has now
			if (r.item && typeof r.item === "object") {
				const fields = {};
				for (const k of ["read", "starred", "replied"])
					if (typeof r.item[k] === "boolean")
						fields[k] = msg[k] = r.item[k];
				if (typeof r.item.folder === "string")
					fields.folder = msg.folder = r.item.folder;
				if (typeof o.onItem === "function")
					o.onItem(item.id, fields);
			}
			carry(before, rpc.stateOf(r));
			keep(item.id, msg, rpc.stateOf(r));
			if (my === seq)
				prefetch(item.id);
		}, () => {
			if (changed)
				o.onRead(item, false);
			msg.read = false;
		});
	}

	// A kept message shown whose sender's avatar is not known yet (it was
	// prefetched, and is read already): asked for alone (head: only the
	// list item comes back, nothing is marked)
	function avatarOnly(my, item, msg) {
		rpc.call("message.get", { id: item.id, head: true, avatar: true }).then((r) => {
			if (!dead && "avatar" in r)
				gotAvatar(my, item, msg, r.avatar);
		}, () => {});
	}

	// The avatar arrived for msg: kept, and put in place of the badge
	// shown when msg is still the one shown (nothing else is drawn again)
	function gotAvatar(my, item, msg, value) {
		learnAvatar(item, msg, value);
		if (my === seq && cur && cur.msg === msg && cur.badge && cur.badge.parentNode) {
			const b = avatarBadge(msg.avatar, cur.initial, "avatar");
			cur.badge.replaceWith(b);
			cur.badge = b;
		}
	}

	// ---- The header ----

	function line(label, value) {
		return value ? [h("dt", { text: label }), h("dd", { text: value })] : [];
	}

	function writtenTitle(m) {
		const lines = [];
		if (typeof m.date === "number" && m.date > 0) {
			const z = zonedDate(m.date, m.zone);
			lines.push(t("list.label_written", { date: z || fullDate(m.date) }));
		}
		if (typeof m.arrived === "number" && (!(m.date > 0) || Math.abs(m.arrived - m.date) > 60))
			lines.push(t("list.label_received", { date: fullDate(m.arrived) }));
		return lines.join("\n");
	}

	// The header from the list item (msg null) or the whole message
	function header(item, msg) {
		const m = msg || item;
		const hd = msg && msg.headers ? msg.headers : {};
		const name = hd.from || m.from || "";
		const addr = hd.from_addr || m.from_addr || "";
		const who = name !== "" ? name : (addr !== "" ? addr : t("list.label_no_sender"));
		const subject = typeof m.subject === "string" && m.subject !== "" ? m.subject : t("list.label_no_subject");
		const when = (m.date > 0 ? m.date : m.arrived);

		const heading = h("h3", { class: "msg-subject", tabindex: "-1", id: "wm-msg-subject", "data-pane-title": true,
			text: subject });
		const prio = m.priority === "high" || m.priority === "low" ? m.priority : "";
		const top = h("div", { class: "msg-head" }, heading);
		if (prio) {
			top.append(h("span", { class: "tag prio" + (prio === "high" ? " high" : ""),
				text: t(prio === "high" ? "list.label_importance_high" : "list.label_importance_low") }));
		}
		const badge = avatarBadge(msg ? msg.avatar : null, initial(who), "avatar");
		const sender = h("div", { class: "sender" },
			badge,
			h("div", { class: "who" },
				h("div", { class: "nm", text: who }),
				addr !== "" && addr !== who ? h("div", { class: "ad", text: addr }) : null),
			h("div", { class: "when", title: writtenTitle(m), text: fullDate(when) }));

		const expiry = h("p", { class: "msg-expiry", hidden: true });
		const article = h("article", { class: "msg", "aria-labelledby": "wm-msg-subject" }, top, sender, expiry);
		let toggle = null;
		if (msg) {
			const to = hd.to || m.to || "";
			const cc = hd.cc || "";
			const summary = [to ? t("main.label_to") + ": " + to : "", cc ? t("main.label_cc") + ": " + cc : ""]
				.filter((x) => x !== "").join("   ");
			const details = h("dl", { class: "details", id: "wm-msg-details", hidden: true },
				line(t("main.label_from"), addr !== "" && addr !== name && name !== "" ? name + " <" + addr + ">" : who),
				line(t("reader.label_reply_to"), hd.reply_to && hd.reply_to !== addr ? hd.reply_to : ""),
				line(t("main.label_to"), to),
				line(t("main.label_cc"), cc),
				line(t("reader.label_date"), writtenTitle(m)));
			const showText = h("span", { class: "vh", text: ", " + t("reader.label_show_details") });
			toggle = h("button", { class: "tome", type: "button", "aria-expanded": "false",
				"aria-controls": "wm-msg-details" },
				h("span", { class: "ell", text: summary !== "" ? summary : t("reader.label_show_details") }),
				summary !== "" ? showText : null, icon("chev", "sm"));
			toggle.addEventListener("click", () => {
				const opened = details.hidden;
				details.hidden = !opened;
				toggle.setAttribute("aria-expanded", String(opened));
				showText.textContent = ", " + t(opened ? "reader.label_hide_details" : "reader.label_show_details");
			});
			article.append(toggle, details);
			delivery(article, m);
			if (typeof msg.comment === "string" && msg.comment.trim() !== "")
				article.append(h("div", { class: "msg-comment", text: msg.comment.trim() }));
		} else
			delivery(article, m);
		const body = h("div", { class: "msg-main" });
		article.append(body);

		const keepFocus = cur && cur.heading && document.activeElement === cur.heading;
		clear(scroll);
		scroll.append(article);
		empty.hidden = true;
		scroll.hidden = false;
		if (!msg)
			scroll.scrollTop = 0;
		cur.heading = heading;
		cur.badge = badge;
		cur.initial = initial(who);
		cur.body = body;
		cur.article = article;
		cur.expiry = expiry;
		cur.frame = null;
		pane.classList.add("has-msg");
		note();
		if (typeof o.onShow === "function")
			o.onShow(cur.item);
		if (keepFocus)
			heading.focus({ preventScroll: true });
	}

	// A starred Inbox message: Inbox mail is deleted after the system's
	// age limit unless it is moved to a folder
	function note() {
		if (!cur || !cur.expiry)
			return;
		const days = typeof o.inboxDays === "function" ? o.inboxDays() : 0;
		const on = days > 0 && cur.item.folder === "inbox" && cur.item.starred === true;
		cur.expiry.hidden = !on;
		cur.expiry.textContent = on ? t("reader.label_inbox_expires", { days }) : "";
	}

	function remap(from, to) {
		if (from === to)
			return;
		if (cur && cur.id === from) {
			cur.id = to;
			if (cur.item.id !== to)
				cur.item.id = to;
			if (cur.msg && cur.msg.id === from)
				cur.msg.id = to;
		}
		for (const m of [kept, fetching, plainChoice]) {
			if (m.has(from)) {
				m.set(to, m.get(from));
				m.delete(from);
			}
		}
		if (imagesOn.has(from)) {
			imagesOn.delete(from);
			imagesOn.add(to);
		}
	}

	function patch(id, fields) {
		if (!cur || cur.id !== id)
			return;
		if (cur.item !== fields)
			Object.assign(cur.item, fields);
		if (cur.msg)
			Object.assign(cur.msg, fields);
		note();
		if (typeof o.onShow === "function")
			o.onShow(cur.item);
	}

	function delivery(article, m) {
		const st = m.delivery && typeof m.delivery.state === "string" ? m.delivery.state : "";
		const which = { failed: ["alert", "reader.label_failed_help"], queued: ["clock", "reader.label_queued_help"],
			unsent: ["info", "reader.label_unsent_help"] }[st];
		if (!which)
			return;
		article.append(h("div", { class: "failbox" + (st === "failed" ? "" : " queuebox"),
			role: st === "failed" ? "alert" : null }, icon(which[0]), h("p", { text: t(which[1]) })));
	}

	function initial(name) {
		const c = Array.from(String(name || "").trim())[0];
		return c ? c.toUpperCase() : "?";
	}

	// ---- Loading and failure ----

	function loading(p) {
		const body = cur.body;
		busy(body, p, () => (cur && cur.body === body
			? h("div", { class: "busy", role: "status", text: t("reader.label_loading") }) : null));
	}

	function failed(item, err) {
		const body = cur.body;
		clear(body);
		if (err && err.error === "notfound") {
			body.append(h("p", { class: "msg-note", role: "alert", text: t("reader.label_message_gone") }));
			return;
		}
		body.append(h("div", { class: "msg-note", role: "alert" },
			h("p", { text: err && err.message ? err.message : t("errors.failed") }),
			h("button", { class: "btn", type: "button", text: t("list.button_try_again"), onclick: () => {
				if (cur && cur.item === item) {
					cur.heading.focus({ preventScroll: true });
					open(item, {});
				}
			} })));
	}

	// ---- The message ----

	function show(msg) {
		if (!msg || !cur)
			return;
		const item = cur.item;
		if (!msg.delivery && item.delivery)
			msg.delivery = item.delivery;	// the delivery state comes with the list
		cur.msg = msg;
		knownAvatar(item, msg);
		header(item, msg);
		renderBody();
	}

	function hasHtml(m) {
		return !!(m.html && typeof m.html.b64 === "string");
	}

	function hasText(m) {
		return !!(m.text && typeof m.text.b64 === "string");
	}

	function renderBody() {
		const m = cur.msg;
		const body = cur.body;
		clear(body);
		live.textContent = "";
		const both = hasHtml(m) && hasText(m);
		const plain = !hasHtml(m) || (both && plainChoice.get(cur.id) === true);
		if (both) {
			const seg = h("div", { class: "seg viewas", role: "group", "aria-label": t("reader.label_show_as") });
			for (const [p, key] of [[false, "reader.button_formatted"], [true, "reader.button_plain_text"]]) {
				seg.append(h("button", { type: "button", "aria-pressed": String(p === plain), text: t(key),
					onclick: (e) => {
						if (p === plain)
							return;
						plainChoice.set(cur.id, p);
						renderBody();
						const again = cur.body.querySelector(".viewas button[aria-pressed='true']");
						if (again)
							again.focus({ preventScroll: true });
					} }));
			}
			body.append(seg);
		}
		let used = new Set();
		if (!hasHtml(m) && !hasText(m)) {
			body.append(h("p", { class: "msg-note",
				text: t(Array.isArray(m.parts) && m.parts.length > 0 ? "reader.label_body_in_attachments"
					: "reader.label_no_body") }));
		} else if (plain) {
			body.append(textView(decode(m.text), { show: t("reader.button_show_quoted"),
				hide: t("reader.button_hide_quoted") }));
		} else
			used = htmlBody(body);
		const parts = Array.isArray(m.parts) ? m.parts.filter((p) => !used.has(contentId(p))) : [];
		if (parts.length > 0)
			body.append(attachmentList(m.id || cur.id, parts, (p, btn) => openPreview(m.id || cur.id, p, btn)));
	}

	// The HTML body in its frame; returns the Content-IDs shown inline
	// (their parts are not listed again).  The frame is shown at once;
	// the inline images are fetched after, and the frame is filled again
	// when any arrived.
	function htmlBody(body) {
		const id = cur.id;
		const m = cur.msg;
		const slot = h("div", { class: "frame-slot" });
		body.append(slot);
		if (!cur.html || cur.html.msg !== m) {
			const src = decode(m.html);
			const k = kept.get(id);
			const cids = k && k.msg === m && k.cids ? k.cids : null;
			const p = prepare(src, cids);
			if (p.dropped && k)
				k.cids = null;
			cur.html = { msg: m, src, prep: p.prep, fetched: cids !== null, ctl: null };
		}
		const st = cur.html;
		if (!st.prep.stable) {
			// it cannot be shown safely as HTML: its text instead
			slot.append(h("p", { class: "msg-note", text: t("reader.label_html_unshown") }),
				textView(hasText(m) ? decode(m.text) : st.prep.text, { show: t("reader.button_show_quoted"),
					hide: t("reader.button_hide_quoted") }));
			return new Set();
		}
		frame(slot, st.prep);
		const wanted = Array.isArray(m.parts) ? m.parts.filter((p) => st.prep.refs.has(contentId(p))) : [];
		if (!st.fetched && !st.fetching && wanted.length > 0) {
			st.fetching = true;
			st.ctl = typeof AbortController === "function" ? new AbortController() : null;
			const signal = st.ctl ? st.ctl.signal : undefined;
			inlineImages(id, wanted, st.prep.refs, signal).then((map) => {
				st.fetching = false;
				if (signal && signal.aborted)
					return;	// left: fetched again when shown again
				st.fetched = true;
				const k = kept.get(id);
				if (dead || !cur || cur.html !== st || Object.keys(map).length === 0)
					return;
				const p = prepare(st.src, map);
				if (k && k.msg === m)
					k.cids = p.dropped ? null : map;
				st.prep = p.prep;
				renderBody();
			});
		}
		return st.prep.used;
	}

	// prepareHtml(), falling back to no inline images when that fails
	// (dropped: the images were left out), and to a text-only result when
	// even that fails.  Never throws.
	function prepare(src, cids) {
		if (cids && Object.keys(cids).length > 0) {
			try {
				return { prep: prepareHtml(src, cids), dropped: false };
			} catch (e) {
				// below, without them
			}
		}
		try {
			return { prep: prepareHtml(src, {}), dropped: !!cids };
		} catch (e) {
			return { prep: { stable: false, html: "", text: "", remote: false, dark: false,
				refs: new Set(), used: new Set() }, dropped: !!cids };
		}
	}

	// Stop fetching the inline images of the message shown
	function stopFetching() {
		if (cur && cur.html && cur.html.ctl && cur.html.fetching)
			cur.html.ctl.abort();
	}

	function colors() {
		const cs = getComputedStyle(pane);
		const probe = h("span", { hidden: true });
		probe.style.color = "var(--wm-accent)";
		pane.append(probe);
		const link = getComputedStyle(probe).color;
		probe.remove();
		const bg = cs.backgroundColor;
		const n = (bg.match(/[0-9.]+/g) || []).slice(0, 3).map(Number);
		const dark = n.length === 3 && (0.299 * n[0] + 0.587 * n[1] + 0.114 * n[2]) < 128;
		return { colors: { text: cs.color, bg, link }, dark };
	}

	// The sender's address and folders of the message shown
	// The sender's structured address (never the display name), its
	// network type, and the folders of the message shown; the address is
	// "" when the server marked it malformed (from_addr_bad)
	function senderOf() {
		const hd = cur.msg && cur.msg.headers ? cur.msg.headers : null;
		return hd && hd.from_addr_bad !== true && typeof hd.from_addr === "string" ? hd.from_addr : "";
	}

	function netOf() {
		const hd = cur.msg && cur.msg.headers ? cur.msg.headers : null;
		return hd ? hd.net_type : "";
	}

	function foldersOf() {
		return [cur.item.folder, cur.msg ? cur.msg.folder : undefined];
	}

	// The user's list allows the images of the message shown
	function listed() {
		const p = prefs.get();
		return autoImages(foldersOf(), senderOf(), netOf(), p && Array.isArray(p.images_always) ? p.images_always : []);
	}

	// prep: prepareHtml()'s result.  Mail is shown on a light page unless
	// it says it supports a dark one; then the app's colors are used.
	function frame(slot, prep) {
		const id = cur.id;
		const auto = listed();
		const allowed = imagesOn.has(id) || auto;
		const look = prep.dark ? colors() : null;
		const build = (images) => frameDoc(prep.html, { images, theme: look });
		const f = document.createElement("iframe");
		if (prep.remote && !allowed) {
			const show = () => {
				imagesOn.add(id);
				bar.remove();
				live.textContent = "";
				f.srcdoc = build(true);
				// the button is gone: focus stays in the reader
				// (inside the frame, Escape could not reach it)
				cur.heading.focus({ preventScroll: true });
			};
			// offered only where listing the sender would show them (never
			// in Spam or Trash, an Internet sender with a plain address)
			const from = senderOf();
			const always = autoImages(foldersOf(), from, netOf(), [from.replace(/[A-Z]+/g, (m) => m.toLowerCase())])
				? h("button", { class: "btn ghost", type: "button", text: t("reader.button_always_images"),
					title: t("reader.label_always_images_title", { address: from }),
					onclick: () => {
						if (always.getAttribute("aria-disabled") === "true")
							return;
						always.setAttribute("aria-disabled", "true");
						const my = cur;
						rpc.call("settings.set", { images_add: from }).then((r) => {
							prefs.set(r);	// may show the message again, images loaded
							if (bar.isConnected)
								show();
							// the button is gone either way: focus on the message
							if (cur === my && cur.heading)
								cur.heading.focus({ preventScroll: true });
							notice(t("reader.label_always_images_on", { address: from }));
						}, (e) => {
							always.removeAttribute("aria-disabled");
							notice(e && e.message ? e.message : t("errors.failed"));
						});
					} }) : null;
			const bar = h("div", { class: "imgbar" }, icon("image"),
				h("span", { class: "grow", text: t("reader.label_images_hidden") }),
				h("button", { class: "btn ghost", type: "button", text: t("reader.button_show_images"), onclick: show }),
				always);
			slot.before(bar);
			requestAnimationFrame(() => {
				if (bar.isConnected)
					live.textContent = t("reader.label_images_hidden");
			});
		}
		f.className = "msg-frame";
		f.setAttribute("sandbox", "allow-popups allow-popups-to-escape-sandbox");
		f.setAttribute("referrerpolicy", "no-referrer");
		f.title = t("reader.label_frame_title");
		f.srcdoc = build(allowed);
		f.style.height = Math.max(200, scroll.clientHeight) + "px";
		slot.append(f);
		cur.frame = f;
		cur.look = look;
		cur.auto = auto;
	}

	// The settings changed (or arrived after the message was shown): a
	// message whose sender is now listed, or no longer, is shown again
	const offPrefs = prefs.on(() => {
		if (!dead && cur && cur.frame && cur.html && cur.html.prep.remote && !imagesOn.has(cur.id)
			&& listed() !== cur.auto)
			renderBody();
	});

	// The frame is as tall as the reader's scroll area (it scrolls inside;
	// its content cannot be measured from here)
	const ro = new ResizeObserver(() => {
		if (cur && cur.frame)
			cur.frame.style.height = Math.max(200, scroll.clientHeight) + "px";
	});
	ro.observe(scroll);

	// A theme change: the frame of a message that supports a dark scheme
	// follows it (other mail stays on its light page)
	function retheme() {
		if (!cur || !cur.frame || !cur.look)
			return;
		if (JSON.stringify(colors()) !== JSON.stringify(cur.look))
			renderBody();
	}
	const mo = new MutationObserver(retheme);
	mo.observe(document.documentElement, { attributes: true, attributeFilter: ["data-theme"] });
	const mq = window.matchMedia("(prefers-color-scheme: dark)");
	if (mq.addEventListener)
		mq.addEventListener("change", retheme);

	pane.addEventListener("keydown", (e) => {
		if (e.key === "Escape" && !e.defaultPrevented && cur && typeof o.onEscape === "function") {
			e.preventDefault();
			o.onEscape();
		}
	});

	// ---- The rest ----

	function focus() {
		if (cur && cur.heading)
			cur.heading.focus({ preventScroll: true });
	}

	function close() {
		closePreview(false);
		stopFetching();
		clearTimeout(timer);
		timer = 0;
		seq++;
		cur = null;
		clear(scroll);
		live.textContent = "";
		scroll.hidden = true;
		empty.hidden = false;
		pane.classList.remove("has-msg");
		if (typeof o.onShow === "function" && !dead)
			o.onShow(null);
	}

	function destroy() {
		dead = true;
		close();
		offPrefs();
		ro.disconnect();
		mo.disconnect();
		if (mq.removeEventListener)
			mq.removeEventListener("change", retheme);
		kept.clear();
	}

	return { open, close, focus, destroy, patch, remap, current: () => (cur ? cur.id : null), item: () => (cur ? cur.item : null),
		message: () => (cur ? cur.msg : null) };
}
