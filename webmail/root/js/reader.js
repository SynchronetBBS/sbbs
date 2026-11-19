// The reader pane: one message, read-only.
//
// const reader = createReader({ pane, nextOf, onRead, onEscape })
//   pane: the reader <section> (its bar and its empty placeholder are kept);
//   nextOf(id): the list item below the message with this id, or null;
//   onRead(item, read): mark the list row read (true) or unread again
//   (false, when marking failed); returns true when the row changed;
//   onEscape(): Escape in the reader (focus back to the list).
// reader.open(item, { focus, delay })
//   shows a list item: its header at once, the rest when message.get
//   answers (a "Loading" text after 150 ms).  The user opened it, so it is
//   marked read.  focus: move focus to the message's heading.  delay: ms
//   to wait before asking the server (moving through the list with the
//   arrow keys: a message passed over quickly is not fetched or marked).
// reader.close()       back to the placeholder
// reader.current()     the id shown, or null
// reader.focus()       focus on the message's heading
// reader.destroy()
//
// Speed: after a message is shown, the one below it in the list is
// fetched in the background (message.get with mark_read false) and kept;
// opening it then shows it at once and marks it read with a normal call
// that returns only its list item (head: true), not the message again.
// At most MAX_KEPT messages are kept, each with the state token of the
// reply it came in; one whose token is not the latest is never used.  A
// reply for a message the user has left is kept, never shown.
//
// Safety: everything from a message goes into the page as text.  An HTML
// body goes only into the srcdoc of a frame sandboxed with
// "allow-popups allow-popups-to-escape-sandbox" (no scripts, no same
// origin, no forms, no top navigation), whose own content policy lets it
// load data: images and inline styles only, and https images too after
// "Show images" (for this message, this session).  Before that the HTML
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

const MAX_KEPT = 20;

export function createReader(o) {
	const pane = o.pane;
	const empty = pane.querySelector(".reader-empty");
	const live = h("div", { class: "vh", role: "status", "aria-live": "polite" });
	const scroll = h("div", { class: "reader-scroll", hidden: true });
	pane.append(scroll, live);

	const kept = new Map();		// id -> { msg, token, cids }
	const fetching = new Map();	// id -> Promise of a background fetch
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
		const p = rpc.call("message.get", { id: item.id, mark_read: true }).then((r) => {
			if (dead)
				return;
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
		if (marked || (msg.read === true && item.read === true)) {
			prefetch(item.id);
			return;
		}
		const changed = o.onRead(item, true);
		msg.read = true;
		rpc.call("message.get", { id: item.id, mark_read: true, head: true }).then((r) => {
			if (dead)
				return;
			// The message shown is the one kept, under the new state token
			if (r.item && typeof r.item.read === "boolean")
				msg.read = r.item.read;
			keep(item.id, msg, rpc.stateOf(r));
			if (my === seq)
				prefetch(item.id);
		}, () => {
			if (changed)
				o.onRead(item, false);
			msg.read = false;
		});
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

		const heading = h("h3", { class: "msg-subject", tabindex: "-1", id: "wm-msg-subject", text: subject });
		const prio = m.priority === "high" || m.priority === "low" ? m.priority : "";
		const top = h("div", { class: "msg-head" }, heading);
		if (prio) {
			top.append(h("span", { class: "tag prio" + (prio === "high" ? " high" : ""),
				text: t(prio === "high" ? "list.label_importance_high" : "list.label_importance_low") }));
		}
		const sender = h("div", { class: "sender" },
			avatarBadge(msg ? msg.avatar : null, initial(who), "avatar"),
			h("div", { class: "who" },
				h("div", { class: "nm", text: who }),
				addr !== "" && addr !== who ? h("div", { class: "ad", text: addr }) : null),
			h("div", { class: "when", title: writtenTitle(m), text: fullDate(when) }));

		const article = h("article", { class: "msg", "aria-labelledby": "wm-msg-subject" }, top, sender);
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
		cur.body = body;
		cur.article = article;
		cur.frame = null;
		if (keepFocus)
			heading.focus({ preventScroll: true });
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

	// prep: prepareHtml()'s result.  Mail is shown on a light page unless
	// it says it supports a dark one; then the app's colors are used.
	function frame(slot, prep) {
		const id = cur.id;
		const allowed = imagesOn.has(id);
		const look = prep.dark ? colors() : null;
		const build = (images) => frameDoc(prep.html, { images, theme: look });
		const f = document.createElement("iframe");
		if (prep.remote && !allowed) {
			const bar = h("div", { class: "imgbar" }, icon("image"),
				h("span", { class: "grow", text: t("reader.label_images_hidden") }),
				h("button", { class: "btn ghost", type: "button", text: t("reader.button_show_images"),
					onclick: () => {
						imagesOn.add(id);
						bar.remove();
						live.textContent = "";
						f.srcdoc = build(true);
						// the button is gone: focus stays in the reader
						// (inside the frame, Escape could not reach it)
						cur.heading.focus({ preventScroll: true });
					} }));
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
	}

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
	}

	function destroy() {
		dead = true;
		close();
		ro.disconnect();
		mo.disconnect();
		if (mq.removeEventListener)
			mq.removeEventListener("change", retheme);
		kept.clear();
	}

	return { open, close, focus, destroy, current: () => (cur ? cur.id : null) };
}
