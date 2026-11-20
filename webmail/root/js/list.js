// The message list: a grid of messages, virtual (only the rows in view,
// plus a screenful above and below, are in the page; row elements are
// reused as the list scrolls) over a sparse cache of message.list pages.
//
// const list = createList({ fetch, labelledBy, onOpen, onSelect, empty, failed,
//                           folderTag, onTotal, onChecked, onKey, onStar, onNotice })
//   fetch(view, offset, limit, { first, background })
//                               a Promise of a message.list result; first is
//                               true for a view's first load, which shows
//                               the failed state rather than waiting for an
//                               unreachable server; background is true for
//                               a refresh the app makes on its own and for
//                               the reloads of rows already shown that
//                               follow it
//   onSelect(item, opening)     the focus moved to the row of item (null: a
//                               row not loaded yet) with the arrow or paging
//                               keys (opening false), or by a click or Enter
//                               (opening true: onOpen follows)
//   onOpen(item, how)           a row was chosen: how is "key" (Enter) or
//                               "click"
//   empty(view)                 the node to show when the view has no messages
//   failed(view, error, retry)  the node to show when its first page failed
//   folderTag(item)             the folder name to show on a row, or ""
//   onTotal(view)               the view shown has (a new) total
//   onChecked(count)            the number of checked rows changed
//   onKey(name, item)           an action key in the grid: "delete" (Delete,
//                               Backspace), "star" (s) or "undo" (z); item
//                               is the row with the focus
//   onRemap(oldId, newId)       a message's id changed (list.update with
//                               an id): whatever else holds the old id
//                               follows
//   onStar(item)                the row's star button was pressed
//   onNotice(item)              the row's delivery report button was pressed
// list.el                       the element to put in the pane
// list.view(key, props)         the cached view for key (a folder plus a
//                               search), made with props when new
// list.show(view)               shows a view: its cached rows and scroll
//                               position at once when it has them (then
//                               refreshed), else loads its first page.  A
//                               view other than the one shown clears the
//                               checked rows.
// list.refresh(background)      reloads the first page of the view shown and
//                               merges it: new messages at the top keep the
//                               rows in view and the focus where they
//                               were; the other pages are then reloaded as
//                               they come into view.  One refresh of a
//                               view at a time: a call while one runs gets
//                               that one's Promise
// list.current()                the view shown
// list.near(id, delta)          the loaded item delta rows from the message
//                               with this id in the view shown, or null
// list.update(id, patch)        changes fields of the message with this id
//                               in the view shown (its row is painted
//                               again); returns the item or null.  A new
//                               id in patch moves the focus, the checks,
//                               the range anchor and the open mark with
//                               it, and onRemap is called
// list.remove(ids)              takes the messages with these ids out of the
//                               view shown at once (the rows below move
//                               up); returns what reinsert() needs to put
//                               them back
// list.reinsert(removed)        puts rows taken out by remove() back where
//                               they were (when the same view is shown)
// list.checked()                the checked rows: [{id, folder}] (folder:
//                               the item's folder, or the view's)
// list.checkAll(on)             checks (or clears) every loaded row
// list.checkState()             { count, loaded }: checked rows, and loaded
//                               rows of the view shown
// list.clearChecked()
// list.hold(on)                 a change made here is under way (on) / has
//                               settled (off; holds nest): until then a page
//                               the server sends with another total (from
//                               before the change) is not taken
// list.setOpen(id)              marks the row of the message open in the
//                               reader (aria-current), null for none
// list.focus()                  focus on the grid's current control
// list.destroy()
//
// A view: { key, total (null until loaded), items (sparse, by offset),
// scrollTop, selIndex, selId (the row with the grid's focus), ... } plus
// the props it was made with.
//
// The grid (WAI-ARIA grid pattern): one row per message, each with a
// checkbox, the message (a button that opens it), a delivery report
// button on a failed Sent row that has one, and a star button.  The grid
// is one tab stop (roving tabindex): Up/Down move the row, Left/Right
// between the row's controls, Home/End (and Ctrl+Home/End) to the first
// and last row, PageUp/PageDown by a screenful; Space checks the row
// (on the checkbox and the message), Enter opens it, Shift+Up/Down and
// Shift+click check from the last checked row to the new one (rows of the
// range not loaded yet are loaded first, up to MAX_RANGE rows), Ctrl+click
// (Cmd+click) checks one, Ctrl+A (Cmd+A) checks every loaded row, Escape
// clears the checks, z is Undo while one is offered.  The checkbox alone
// carries a row's checked state (rows have no aria-selected, the grid is
// not aria-multiselectable), so it is announced once.  The focus follows the message, not the element:
// the row with the focus is always kept in the page, and when the rows
// move (new mail, a removal) focus moves to its control; when the list
// starts over and the message is not in the page again, focus stays on the
// row at the same position.  Focus inside the grid never drops to the page.
//
// Rows have one height, measured once per layout (wide or phone) and when
// the text size changes; each is placed with a transform.  Rows carry
// aria-rowindex and the grid aria-rowcount, since most rows are not in the
// page.

import { h, clear } from "./dom.js";
import { icon } from "./icons.js";
import { t } from "./i18n.js";
import { busy, notice } from "./notice.js";
import { number } from "./format.js";
import { listDate, fullDate, senderText } from "./format.js";

export const PAGE = 50;
const MAX_VIEWS = 20;
const RETRY_AFTER = 10000;	// a page that failed is not asked for again sooner
const PHONE = "(max-width: 899.98px)";
const MAIN = 1;			// the column of the message button
const MAX_RANGE = 500;		// rows a range check loads at most

export function createList(opts) {
	const spacer = h("div", { class: "list-spacer", role: "none" });
	const scroller = h("div", {
		class: "list-scroll", role: "grid", tabindex: "0", "aria-labelledby": opts.labelledBy,
		"aria-rowcount": "0"
	}, spacer);
	const stateEl = h("div", { class: "list-state", tabindex: "-1", hidden: true });
	const el = h("div", { class: "list-body", tabindex: "-1" }, scroller, stateEl);
	const mq = window.matchMedia(PHONE);
	const views = new Map();
	const live = new Map();		// index -> row
	const free = [];
	const checked = new Map();	// id -> folder
	let anchor = null;		// the id range checks start from
	let openId = null;
	let col = MAIN;			// the focused column
	let v = null;
	let rowH = 0;
	let rowKey = "";
	let raf = 0;
	let pendingScroll = null;	// applied once the scroller has a size
	let held = 0;			// changes made here under way
	let lost = false;		// a row holding the focus was released

	// ---- Views ----

	function view(key, props) {
		let w = views.get(key);
		if (w) {
			views.delete(key);	// most recently used last
			views.set(key, w);
			return w;
		}
		w = Object.assign({ key, total: null, items: [], fresh: [], gen: 1, mut: 0, inflight: new Set(),
			failed: new Map(), scrollTop: 0, selIndex: -1, selId: null, error: null, loading: null,
			refreshing: null, seq: 0, bgGen: 0, focusAfter: false }, props);
		views.set(key, w);
		while (views.size > MAX_VIEWS) {
			const oldest = views.keys().next().value;
			if (views.get(oldest) === v)
				break;
			views.delete(oldest);
		}
		return w;
	}

	function fill(w, offset, items) {
		for (let i = 0; i < items.length; i++) {
			w.items[offset + i] = items[i];
			w.fresh[offset + i] = w.gen;
			if (w.selId !== null && items[i].id === w.selId)
				w.selIndex = offset + i;
		}
		if (w.selIndex >= 0 && w.selId === null && w.items[w.selIndex])
			w.selId = w.items[w.selIndex].id;
		settleWaiters(w, -1);
	}

	// Callers of step() waiting for a row not loaded yet: answered when
	// its page arrives, or with null when it cannot (the page failed, the
	// row is past the end now, another view is shown)
	const waiters = [];	// { w, index, done }
	function settleWaiters(w, failedPage) {
		for (let k = waiters.length - 1; k >= 0; k--) {
			const x = waiters[k];
			let it = null;
			if (x.w === v) {
				if (x.w !== w)
					continue;
				it = w.items[x.index];
				if (it === undefined && x.index < w.total && Math.floor(x.index / PAGE) !== failedPage)
					continue;
			}
			waiters.splice(k, 1);
			x.done(it || null);
		}
	}

	function show(w) {
		if (v && v !== w) {
			v.busy = false;	// its scrollTop is kept by the scroll handler
			clearChecked();
		}
		for (const n of el.querySelectorAll(".list-skel"))
			n.remove();
		v = w;
		releaseAll();
		if (w.total === null) {
			showState(null);
			spacer.style.height = "0px";
			load(w);
			return;
		}
		paintTotal();
		pendingScroll = w.scrollTop;
		applyScroll();
		refresh();
	}

	// The first load of a view: skeleton rows if it takes a moment
	function load(w) {
		if (w.loading) {
			if (w === v && !w.busy) {
				scroller.hidden = true;
				w.busy = true;
				busy(el, w.loading, () => skeletons(w)).finally(() => {
					w.busy = false;
				});
			}
			return w.loading;
		}
		w.error = null;
		const p = opts.fetch(w, 0, PAGE, { first: true }).then((r) => {
			w.loading = null;
			w.total = r.total;
			w.items = [];
			w.fresh = [];
			w.gen++;
			fill(w, 0, r.items);
			if (w === v) {
				paintTotal();
				pendingScroll = 0;
				applyScroll();
				refocus(w);
			}
		}, (err) => {
			w.loading = null;
			w.error = err;
			if (w === v) {
				showState(opts.failed(w, err, () => {
					if (w === v) {
						// the button goes away: focus stays in the list
						w.focusAfter = true;
						el.focus({ preventScroll: true });
						showState(null);
						load(w);
					}
				}));
				refocus(w);
			}
		});
		w.loading = p;
		return load(w);
	}

	// After Try again: focus on the rows, or on the state shown instead
	function refocus(w) {
		if (!w.focusAfter)
			return;
		w.focusAfter = false;
		if (document.activeElement !== el && document.activeElement !== document.body)
			return;	// the user moved on
		if (scroller.hidden)
			stateEl.focus({ preventScroll: true });
		else
			focus();
	}

	function refresh(background) {
		const w = v;
		if (!w)
			return Promise.resolve();
		if (w.total === null)
			return load(w);
		if (w.refreshing)
			return w.refreshing;
		const seq = ++w.seq;
		w.refreshing = opts.fetch(w, 0, PAGE, { background: background === true }).then((r) => {
			if (seq === w.seq)	// a later reply (or a change made here) wins over an older one
				merge(w, r, background === true);
		}, () => {
			// the rows shown stay; the next refresh tries again
		}).finally(() => {
			w.refreshing = null;
		});
		return w.refreshing;
	}

	// Merge a fresh first page into w
	function merge(w, r, background) {
		const first = w.items[0];
		let k = -1;
		if (first)
			k = r.items.findIndex((x) => x.id === first.id);
		const shifted = k > 0 && r.total === w.total + k;
		if (shifted) {
			const items = [];
			const fresh = [];
			for (const i of Object.keys(w.items)) {
				items[+i + k] = w.items[i];
				fresh[+i + k] = w.fresh[i];
			}
			w.items = items;
			w.fresh = fresh;
			if (w.selIndex >= 0)
				w.selIndex += k;
		} else if (!(k === 0 && r.total === w.total)) {
			// moved in some other way: start over from this page, keeping
			// the focus by id where it shows up again
			w.items = [];
			w.fresh = [];
		}
		w.gen++;	// every other page is reloaded when it comes into view
		w.bgGen = background ? w.gen : 0;
		w.failed.clear();
		w.total = r.total;
		fill(w, 0, r.items);
		if (w !== v) {
			if (shifted && w.scrollTop > 0)
				w.scrollTop += k * rowH;
			return;
		}
		const top = scroller.scrollTop;
		paintTotal();
		if (shifted && top > 0)
			scroller.scrollTop = top + k * rowH;
		render();
	}

	// ---- Pages beyond the first ----

	function want(first, last) {
		const w = v;
		const now = Date.now();
		for (let p = Math.floor(first / PAGE); p <= Math.floor(last / PAGE); p++) {
			if (w.inflight.has(p) || now - (w.failed.get(p) || 0) < RETRY_AFTER)
				continue;
			const end = Math.min(w.total, (p + 1) * PAGE);
			let need = false;
			for (let i = p * PAGE; i < end && !need; i++)
				need = w.items[i] === undefined || w.fresh[i] !== w.gen;
			if (need)
				page(w, p);
		}
	}

	// Reloading rows already shown after a background refresh is itself
	// background; fetching rows not yet seen is the user's scrolling
	function page(w, p) {
		const gen = w.gen;
		const mut = w.mut;
		let had = true;
		for (let i = p * PAGE; i < Math.min(w.total, (p + 1) * PAGE) && had; i++)
			had = w.items[i] !== undefined;
		w.inflight.add(p);
		opts.fetch(w, p * PAGE, PAGE, { background: had && w.bgGen === gen }).then((r) => {
			w.inflight.delete(p);
			if (w.gen !== gen || w.mut !== mut) {	// refreshed or changed meanwhile: ask again
				if (w === v)
					later();
				return;
			}
			if (r.total !== w.total && held > 0) {
				// from before a change under way: asked again after it
				return;
			}
			if (r.total !== w.total) {
				// the folder changed under the list: keep this page only
				w.items = [];
				w.fresh = [];
				w.gen++;
				w.total = r.total;
				if (w === v)
					paintTotal();
			}
			fill(w, p * PAGE, r.items);
			if (w === v)
				later();
		}, () => {
			w.inflight.delete(p);
			w.failed.set(p, Date.now());
			settleWaiters(w, p);
		});
	}

	// ---- Rows ----

	function newRow() {
		const r = {};
		r.chk = h("input", { type: "checkbox", class: "chk", tabindex: "-1" });
		r.unread = h("span", { class: "vh", text: t("list.label_unread") + ", " });
		r.from = h("span", { class: "from ell" }, r.unread, r.fromText = document.createTextNode(""));
		r.prio = h("span", { class: "tag prio" });
		r.state = h("span", { class: "tag" });
		r.folder = h("span", { class: "tag" });
		r.clip = h("span", { class: "mark", title: t("list.label_attachment") }, icon("clip", "sm"),
			h("span", { class: "vh", text: ", " + t("list.label_attachment") }));
		r.replied = h("span", { class: "mark", title: t("list.label_replied") }, icon("reply", "sm"),
			h("span", { class: "vh", text: ", " + t("list.label_replied") }));
		r.starText = h("span", { class: "vh", text: ", " + t("list.label_starred") });
		r.date = h("span", { class: "date" });
		r.subj = h("span", { class: "subj ell" });
		r.sk = h("span", { class: "sk-lines", "aria-hidden": "true" }, h("span", { class: "sk" }), h("span", { class: "sk" }));
		r.skText = h("span", { class: "vh", text: t("list.label_loading"), hidden: true });
		r.main = h("button", { class: "row-main", type: "button", tabindex: "-1" },
			h("span", { class: "r1" }, r.from,
				h("span", { class: "marks" }, r.prio, r.state, r.folder, r.clip, r.replied), r.starText, r.date),
			r.subj, r.sk, r.skText);
		r.notice = h("button", { class: "linkbtn notice-btn", type: "button", tabindex: "-1",
			text: t("list.button_report") });
		r.star = h("button", { class: "ib star", type: "button", tabindex: "-1", "aria-pressed": "false" },
			icon("star"));
		r.noticeCell = h("div", { class: "cell c-note", role: "gridcell", hidden: true }, r.notice);
		r.starCell = h("div", { class: "cell c-star", role: "gridcell" }, r.star);
		r.el = h("div", { class: "row", role: "row" },
			h("div", { class: "cell c-chk", role: "gridcell" }, r.chk),
			h("div", { class: "cell c-main", role: "gridcell" }, r.main),
			r.noticeCell, r.starCell);
		r.el.wmRow = r;
		r.item = undefined;
		r.index = -1;
		spacer.append(r.el);
		return r;
	}

	// The row's controls that can take the focus, in order
	function controls(r) {
		const c = [r.chk, r.main];
		if (!r.noticeCell.hidden)
			c.push(r.notice);
		if (!r.starCell.hidden)
			c.push(r.star);
		return c.filter((x) => !x.disabled);
	}

	function shown(node, on) {
		on = !!on;
		if (node.hidden === on)
			node.hidden = !on;
	}

	// The sender (in Sent: the recipient's name) as the row shows it: a
	// sender's name that could pass for an address shows the real address
	// instead (format.js senderText)
	function nameOf(it) {
		const sent = it.folder === "sent";
		const who = sent ? (it.to || "") : senderText(it.from, it.from_addr).text;
		const name = who !== "" ? who : t("list.label_no_sender");
		return sent ? t("list.label_to_name", { name }) : name;
	}

	function subjectOf(it) {
		return typeof it.subject === "string" && it.subject !== "" ? it.subject : t("list.label_no_subject");
	}

	// The delivery state's text key and whether it is shown as a danger
	function deliveryText(it) {
		const d = it.delivery;
		const st = d && typeof d.state === "string" ? d.state : "";
		if (st === "queued") {
			const n = typeof d.attempts === "number" && d.attempts > 1 ? d.attempts : 0;
			return n ? t("list.label_sending_tries", { count: n }) : t("list.label_sending");
		}
		if (st === "sent")
			return t("list.label_delivered");
		if (st === "unsent")
			return t("list.label_unsent");
		if (st === "failed")
			return t("list.label_delivery_failed");
		return "";
	}

	function paintItem(r, it) {
		const name = nameOf(it);
		const subject = subjectOf(it);
		r.fromText.data = name;
		const tip = it.folder === "sent" ? "" : senderText(it.from, it.from_addr).title;
		if (tip !== "")
			r.from.title = tip;
		else
			r.from.removeAttribute("title");
		r.subj.textContent = subject;
		r.el.classList.toggle("unread", it.read !== true);
		shown(r.unread, it.read !== true);
		const prio = it.priority === "high" || it.priority === "low" ? it.priority : "";
		shown(r.prio, prio !== "");
		if (prio) {
			const high = prio === "high";
			r.prio.textContent = t(high ? "list.label_priority_high" : "list.label_priority_low");
			r.prio.title = t(high ? "list.label_importance_high" : "list.label_importance_low");
			r.prio.classList.toggle("high", high);
		}
		const st = deliveryText(it);
		shown(r.state, st !== "");
		if (st) {
			r.state.textContent = st;
			r.state.classList.toggle("danger", it.delivery.state === "failed");
		}
		const fname = opts.folderTag ? opts.folderTag(it) : "";
		shown(r.folder, fname !== "");
		if (fname)
			r.folder.textContent = fname;
		shown(r.clip, it.attach === true);
		shown(r.replied, it.replied === true);
		shown(r.starText, it.starred === true);
		r.date.textContent = listDate(it.arrived);
		let title = t("list.label_received", { date: fullDate(it.arrived) });
		if (typeof it.date === "number" && Math.abs(it.date - it.arrived) >= 60)
			title += "\n" + t("list.label_written", { date: fullDate(it.date) });
		r.date.title = title;
		const vars = { name, subject };
		r.chk.setAttribute("aria-label", t("list.label_select_row", vars));
		r.star.setAttribute("aria-label", t("list.label_star_row", vars));
		r.star.setAttribute("aria-pressed", String(it.starred === true));
		r.star.title = t(it.starred === true ? "list.button_unstar" : "list.button_star");
		// a sent extra cannot be starred (its header is the recipient's
		// copy), nor a message in Trash
		shown(r.starCell, it.extra !== true && it.folder !== "trash");
		const notice = !!it.delivery && it.delivery.state === "failed" && typeof it.delivery.notice === "string";
		shown(r.noticeCell, notice);
		if (notice)
			r.notice.setAttribute("aria-label", t("list.label_report_row", vars));
	}

	function paint(r, i) {
		const it = v.items[i];
		if (r.index !== i) {
			r.index = i;
			r.el.style.transform = "translateY(" + (i * rowH) + "px)";
			r.el.setAttribute("aria-rowindex", String(i + 1));
		}
		const on = !!it && checked.has(it.id);
		if (r.on !== on) {
			r.on = on;
			r.el.classList.toggle("checked", on);
			r.chk.checked = on;
		}
		const cur = !!it && it.id === openId;
		if (r.cur !== cur) {
			r.cur = cur;
			r.el.classList.toggle("cur", cur);
			if (cur)
				r.main.setAttribute("aria-current", "true");
			else
				r.main.removeAttribute("aria-current");
		}
		if (r.item === it)
			return;
		r.item = it;
		const skel = it === undefined;
		r.el.classList.toggle("skel", skel);
		r.el.setAttribute("aria-busy", String(skel));
		shown(r.skText, skel);
		r.chk.disabled = skel;
		if (skel) {
			r.el.classList.remove("unread");
			shown(r.starCell, false);
			shown(r.noticeCell, false);
		} else
			paintItem(r, it);
	}

	function release(i, r) {
		if (r.el.contains(document.activeElement))
			lost = true;	// render() puts it back on the focused row
		live.delete(i);
		r.el.hidden = true;
		r.el.removeAttribute("aria-rowindex");
		r.index = -1;
		free.push(r);
	}

	function releaseAll() {
		for (const [i, r] of live)
			release(i, r);
		scroller.tabIndex = 0;
	}

	// Row height for the current layout and text size
	function measure() {
		const key = (mq.matches ? "p" : "w") + getComputedStyle(document.documentElement).fontSize;
		if (rowH && key === rowKey)
			return true;
		const r = free.pop() || newRow();
		r.item = undefined;
		r.el.hidden = false;
		r.el.style.visibility = "hidden";
		r.el.classList.remove("skel");
		paintItem(r, { from: "M", subject: "M", read: true, arrived: 1, attach: true });
		r.item = null;
		const hgt = r.el.getBoundingClientRect().height;
		r.el.style.visibility = "";
		r.el.hidden = true;
		r.index = -1;
		free.push(r);
		if (!(hgt > 0))
			return false;
		const changed = rowH !== hgt;
		rowH = hgt;
		rowKey = key;
		if (changed && v) {
			for (const x of live.values())
				x.index = -1;	// placed again
			paintTotal();
		}
		return true;
	}

	function paintTotal() {
		showState(v.total === 0 ? opts.empty(v) : null);
		spacer.style.height = (v.total * rowH) + "px";
		scroller.setAttribute("aria-rowcount", String(v.total || 0));
		if (typeof opts.onTotal === "function")
			opts.onTotal(v);
	}

	function showState(node) {
		// the state had the focus and rows come instead: render() puts it on
		// the focused row
		if (!node && !stateEl.hidden && stateEl.contains(document.activeElement))
			lost = true;
		clear(stateEl);
		if (node) {
			stateEl.append(node);
			stateEl.hidden = false;
			scroller.hidden = true;
		} else {
			stateEl.hidden = true;
			scroller.hidden = false;
		}
	}

	function applyScroll() {
		if (pendingScroll === null || !scroller.clientHeight || !measure())
			return;
		scroller.scrollTop = pendingScroll;
		pendingScroll = null;
		render();
	}

	function render() {
		if (raf) {
			cancelAnimationFrame(raf);
			raf = 0;
		}
		if (!v || v.total === null || !v.total || !scroller.clientHeight || !measure()) {
			if (v && v.total === 0) {
				releaseAll();
				if (lost) {
					lost = false;
					el.focus({ preventScroll: true });
				}
			}
			return;
		}
		if (pendingScroll !== null) {
			applyScroll();
			return;
		}
		if (v.selIndex >= v.total) {
			v.selIndex = v.total - 1;
			v.selId = v.items[v.selIndex] ? v.items[v.selIndex].id : null;
		}
		const H = scroller.clientHeight;
		const top = scroller.scrollTop;
		const per = Math.ceil(H / rowH);
		const first = Math.max(0, Math.floor(top / rowH) - per);
		const last = Math.min(v.total - 1, Math.floor((top + H) / rowH) + per);
		const keep = v.selIndex;	// the focused row stays in the page
		for (const [i, r] of live) {
			if ((i < first || i > last) && i !== keep)
				release(i, r);
		}
		const place = (i) => {
			let r = live.get(i);
			if (!r) {
				r = free.pop() || newRow();
				r.el.hidden = false;
				live.set(i, r);
			}
			paint(r, i);
		};
		for (let i = first; i <= last; i++)
			place(i);
		if (keep >= 0 && (keep < first || keep > last))
			place(keep);
		want(first, last);
		stops();
		if (lost) {
			lost = false;
			const r = rowAt(v.selIndex);
			if (r)
				control(r, col).focus({ preventScroll: true });
			else
				el.focus({ preventScroll: true });
		}
	}

	function later() {
		if (!raf)
			raf = requestAnimationFrame(render);
	}

	// ---- Focus: one tab stop ----

	function rowAt(i) {
		return i >= 0 ? live.get(i) || null : null;
	}

	function rowOf(node) {
		const row = node && node.closest ? node.closest(".row") : null;
		return row && row.wmRow && row.wmRow.index >= 0 ? row.wmRow : null;
	}

	// The focused row's control in column c (or the nearest one it has)
	function control(r, c) {
		const all = controls(r);
		return all[Math.max(0, Math.min(all.length - 1, c))] || r.main;
	}

	// The first row in view (the tab stop before any row had the focus)
	function topRow() {
		return v && v.total ? Math.min(v.total - 1, Math.ceil(scroller.scrollTop / Math.max(1, rowH))) : -1;
	}

	// tabindex 0 on the focused control only (before any row had the
	// focus, the first row in view's message); the grid itself is the tab
	// stop while there is no row
	function stops() {
		const r = rowAt(v ? (v.selIndex >= 0 ? v.selIndex : topRow()) : -1);
		const target = r ? control(r, col) : null;
		for (const x of live.values()) {
			for (const c of [x.chk, x.main, x.notice, x.star]) {
				const want = c === target ? 0 : -1;
				if (c.tabIndex !== want)
					c.tabIndex = want;
			}
		}
		scroller.tabIndex = target ? -1 : 0;
		// focus inside the grid on another row than the focused one (the
		// rows moved): it follows the message
		const a = document.activeElement;
		if (target && a !== target && spacer.contains(a) && rowOf(a) !== r)
			target.focus({ preventScroll: true });
	}

	// Focus on the control of row i in column col, the row scrolled into
	// view
	function focusRow(i) {
		if (!v || !v.total)
			return;
		i = Math.max(0, Math.min(v.total - 1, i));
		v.selIndex = i;
		const it = v.items[i];
		v.selId = it ? it.id : null;
		const H = scroller.clientHeight;
		const y = i * rowH;
		if (y < scroller.scrollTop)
			scroller.scrollTop = y;
		else if (y + rowH > scroller.scrollTop + H)
			scroller.scrollTop = y + rowH - H;
		render();
		const r = rowAt(i);
		if (r)
			control(r, col).focus({ preventScroll: true });
	}

	function focus() {
		if (v && v.total === null) {
			// still loading: on the list now, on its row once it is there
			v.focusAfter = true;
			el.focus({ preventScroll: true });
			return;
		}
		if (!v || !v.total || scroller.hidden) {
			(scroller.hidden ? stateEl : scroller).focus({ preventScroll: true });
			return;
		}
		focusRow(v.selIndex >= 0 ? v.selIndex : topRow());
	}

	// Into the grid by Tab or a click on its empty space: to the row
	scroller.addEventListener("focus", (e) => {
		if (e.target === scroller && v && v.total)
			focus();
	});

	// The focus is on node (a control of row r): the grid's focus follows
	function track(r, node) {
		const c = controls(r).indexOf(node);
		if (c >= 0)
			col = c;
		if (v.selIndex !== r.index) {
			v.selIndex = r.index;
			v.selId = r.item ? r.item.id : null;
		}
	}

	spacer.addEventListener("focusin", (e) => {
		const r = rowOf(e.target);
		if (!r || !v)
			return;
		track(r, e.target);
		stops();
	});

	// ---- Checking ----

	function folderOf(it) {
		return typeof it.folder === "string" ? it.folder : (v ? v.folder : "");
	}

	function changed() {
		for (const r of live.values())
			paint(r, r.index);
		if (typeof opts.onChecked === "function")
			opts.onChecked(checked.size);
	}

	function setChecked(it, on) {
		if (!it)
			return;
		if (on)
			checked.set(it.id, folderOf(it));
		else
			checked.delete(it.id);
	}

	// Rows a to b checked or not.  Rows not loaded yet are loaded first
	// (at most MAX_RANGE rows in all; beyond that only the loaded ones are
	// checked, and a notice says how many)
	function range(a, b, on) {
		const lo = Math.min(a, b);
		const hi = Math.max(a, b);
		const set = () => {
			let n = 0;
			for (let i = lo; i <= hi; i++) {
				if (v.items[i]) {
					setChecked(v.items[i], on);
					n++;
				}
			}
			return n;
		};
		const pages = [];
		for (let p = Math.floor(lo / PAGE); p <= Math.floor(hi / PAGE); p++) {
			for (let i = Math.max(lo, p * PAGE); i <= Math.min(hi, p * PAGE + PAGE - 1); i++) {
				if (v.items[i] === undefined) {
					pages.push(p);
					break;
				}
			}
		}
		if (pages.length === 0) {
			set();
			return;
		}
		if (hi - lo + 1 > MAX_RANGE) {
			const n = set();
			notice(t("list.label_range_loaded", { n, count: number(n) }));
			return;
		}
		set();
		const w = v;
		const gen = w.gen;
		const mut = w.mut;
		Promise.all(pages.map((p) => opts.fetch(w, p * PAGE, PAGE, {}))).then((rs) => {
			if (w !== v || w.gen !== gen || w.mut !== mut)
				return;	// the list changed meanwhile: what was loaded stays checked
			if (rs.some((r) => r.total !== w.total))
				return;
			rs.forEach((r, k) => fill(w, pages[k] * PAGE, r.items));
			set();
			changed();
			later();
		}, () => {});
	}

	function anchorIndex() {
		return anchor === null ? -1 : indexOf(anchor);
	}

	function toggle(i, on, extend) {
		const it = v.items[i];
		if (!it)
			return;
		if (on === undefined)
			on = !checked.has(it.id);
		const a = extend ? anchorIndex() : -1;
		if (a >= 0)
			range(a, i, on);
		else
			setChecked(it, on);
		anchor = it.id;
		changed();
	}

	function checkAll(on) {
		if (!v)
			return;
		if (on) {
			for (const k of Object.keys(v.items))
				setChecked(v.items[k], true);
		} else
			checked.clear();
		changed();
	}

	function clearChecked() {
		if (checked.size === 0)
			return;
		checked.clear();
		anchor = null;
		changed();
	}

	function checkState() {
		return { count: checked.size, loaded: v ? Object.keys(v.items).length : 0 };
	}

	// ---- Changes made here ----

	// The index of the message with this id in the view shown, or -1
	function indexOf(id) {
		if (!v || typeof id !== "string")
			return -1;
		if (v.selIndex >= 0 && v.items[v.selIndex] && v.items[v.selIndex].id === id)
			return v.selIndex;
		for (const k of Object.keys(v.items)) {
			if (v.items[k] && v.items[k].id === id)
				return +k;
		}
		return -1;
	}

	function near(id, delta) {
		const i = indexOf(id);
		const it = i >= 0 ? v.items[i + delta] : undefined;
		return it || null;
	}

	// Where message id is in the view: { index, total } (index -1: not in
	// the rows loaded)
	function where(id) {
		return { index: indexOf(id), total: v && v.total ? v.total : 0 };
	}

	// The message delta rows from id, for going through the list from the
	// reader: a promise of the item, null at either end or when it cannot
	// be had.  A row not loaded yet is fetched first.
	function step(id, delta) {
		const i = indexOf(id);
		const j = i + delta;
		if (i < 0 || j < 0 || j >= v.total)
			return Promise.resolve(null);
		if (v.items[j] !== undefined)
			return Promise.resolve(v.items[j]);
		const w = v;
		return new Promise((done) => {
			waiters.push({ w, index: j, done });
			want(j, j);
			// nothing asked for (the page failed a moment ago): no answer
			// would come
			if (!w.inflight.has(Math.floor(j / PAGE)))
				settleWaiters(w, Math.floor(j / PAGE));
		});
	}

	// Message id's row becomes the grid's row, as if arrowed to, without
	// taking the focus: the reader went to it.  A list that is not
	// displayed (the reader pane on a phone) has its kept position moved,
	// so the row is in view when the list shows again.
	function reveal(id) {
		const i = indexOf(id);
		if (i < 0)
			return;
		v.selIndex = i;
		v.selId = id;
		const H = scroller.clientHeight;
		const y = i * rowH;
		if (H > 0) {
			if (y < scroller.scrollTop)
				scroller.scrollTop = y;
			else if (y + rowH > scroller.scrollTop + H)
				scroller.scrollTop = y + rowH - H;
			render();
			return;
		}
		if (!rowH || !shownH)
			return;
		let top = pendingScroll !== null ? pendingScroll : v.scrollTop;
		if (y < top)
			top = y;
		else if (y + rowH > top + shownH)
			top = y + rowH - shownH;
		v.scrollTop = top;
		if (pendingScroll !== null)
			pendingScroll = top;
	}

	function update(id, patch) {
		const i = indexOf(id);
		if (i < 0)
			return null;
		const it = v.items[i];
		Object.assign(it, patch);
		if (typeof patch.id === "string" && patch.id !== id) {
			if (checked.has(id)) {
				checked.set(patch.id, checked.get(id));
				checked.delete(id);
			}
			if (v.selId === id)
				v.selId = patch.id;
			if (anchor === id)
				anchor = patch.id;
			if (openId === id)
				openId = patch.id;
			if (typeof opts.onRemap === "function")
				opts.onRemap(id, patch.id);
		}
		const r = live.get(i);
		if (r) {
			r.item = undefined;	// painted again
			paint(r, i);
		}
		return it;
	}

	// Rows moved here: a refresh or page under way is answered again, and
	// focus inside the grid goes to the focused row
	function moved(w) {
		const had = spacer.contains(document.activeElement);
		w.seq++;
		w.mut++;
		w.refreshing = null;
		for (const [i, r] of Array.from(live)) {
			r.item = undefined;
			release(i, r);
		}
		paintTotal();
		render();
		if (had && w.total > 0)
			focusRow(w.selIndex);
		else if (had)
			el.focus({ preventScroll: true });
	}

	function remove(ids) {
		const w = v;
		if (!w || w.total === null)
			return [];
		const at = [];
		for (const id of ids) {
			const i = indexOf(id);
			if (i >= 0)
				at.push(i);
		}
		at.sort((a, b) => b - a);
		const out = [];
		for (const i of at) {
			out.unshift({ view: w, index: i, item: w.items[i] });
			w.items.splice(i, 1);
			w.fresh.splice(i, 1);
			checked.delete(out[0].item.id);
			if (w.selIndex > i)
				w.selIndex--;
		}
		if (at.length === 0)
			return out;
		w.total -= at.length;
		if (w.selIndex >= w.total)
			w.selIndex = w.total - 1;
		w.selId = w.selIndex >= 0 && w.items[w.selIndex] ? w.items[w.selIndex].id : null;
		moved(w);
		if (typeof opts.onChecked === "function")
			opts.onChecked(checked.size);
		return out;
	}

	function reinsert(removed) {
		const w = v;
		if (!w || !Array.isArray(removed) || removed.length === 0 || removed[0].view !== w || w.total === null)
			return;
		for (const rec of removed) {
			const i = Math.min(rec.index, w.total);
			if (indexOf(rec.item.id) >= 0)
				continue;	// a refresh brought it back already
			w.items.splice(i, 0, rec.item);
			w.fresh.splice(i, 0, w.gen);
			w.total++;
			if (w.selIndex >= i)
				w.selIndex++;
		}
		if (w.selIndex >= 0 && w.items[w.selIndex])
			w.selId = w.items[w.selIndex].id;
		moved(w);
	}

	function hold(on) {
		held = Math.max(0, held + (on ? 1 : -1));
		if (held === 0 && v)
			later();
	}

	function setOpen(id) {
		openId = typeof id === "string" ? id : null;
		if (v)
			for (const r of live.values())
				paint(r, r.index);
	}

	// ---- Keyboard and pointer ----

	// The keyboard focus moved to row i: the caller is told (and onOpen
	// follows when open)
	function select(i, open, how) {
		if (!v || !v.total)
			return;
		focusRow(i);
		const it = v.items[v.selIndex];
		if (typeof opts.onSelect === "function")
			opts.onSelect(it || null, open === true);
		if (open && it && typeof opts.onOpen === "function")
			opts.onOpen(it, how);
	}

	scroller.addEventListener("keydown", (e) => {
		if (!v || !v.total || e.altKey)
			return;
		const r = rowOf(e.target);
		if (r)
			track(r, e.target);
		const mod = e.ctrlKey || e.metaKey;
		const at = v.selIndex >= 0 ? v.selIndex : topRow();
		const per = Math.max(1, Math.floor(scroller.clientHeight / rowH) - 1);
		if (mod) {
			if (e.key === "a" || e.key === "A") {
				e.preventDefault();
				checkAll(true);
			} else if (e.key === "Home" || e.key === "End") {
				e.preventDefault();
				select(e.key === "Home" ? 0 : v.total - 1, false);
			}
			return;
		}
		let i;
		switch (e.key) {
			case "ArrowDown":
				i = v.selIndex < 0 ? at : at + 1;
				break;
			case "ArrowUp":
				i = v.selIndex < 0 ? at : at - 1;
				break;
			case "PageDown":
				i = at + per;
				break;
			case "PageUp":
				i = at - per;
				break;
			case "Home":
				i = 0;
				break;
			case "End":
				i = v.total - 1;
				break;
			case "ArrowLeft":
			case "ArrowRight":
				if (r) {
					e.preventDefault();
					const all = controls(r);
					const c = Math.max(0, Math.min(all.length - 1,
						all.indexOf(e.target) + (e.key === "ArrowLeft" ? -1 : 1)));
					col = c;
					all[c].focus();
				}
				return;
			case " ":
			case "Enter":
				// handled here, not by the controls' own activation:
				// Space checks the row (the star button: stars it), Enter
				// opens it (the star and report buttons: press them)
				if (!r)
					return;
				e.preventDefault();
				if (e.target === r.star) {
					if (r.item && typeof opts.onStar === "function")
						opts.onStar(r.item);
				} else if (e.key === " ")
					toggle(r.index, undefined, e.shiftKey);
				else if (e.target === r.notice) {
					if (r.item && typeof opts.onNotice === "function")
						opts.onNotice(r.item);
				} else
					select(r.index, true, "key");
				return;
			case "Escape":
				if (checked.size > 0) {
					e.preventDefault();
					e.stopPropagation();
					clearChecked();
				}
				return;
			case "Delete":
			case "Backspace":
				if (r && r.item && typeof opts.onKey === "function") {
					e.preventDefault();
					opts.onKey("delete", r.item);
				}
				return;
			case "s":
			case "S":
				if (!e.shiftKey && r && r.item && typeof opts.onKey === "function") {
					e.preventDefault();
					opts.onKey("star", r.item);
				}
				return;
			case "z":
			case "Z":
				if (!e.shiftKey && typeof opts.onKey === "function") {
					e.preventDefault();
					opts.onKey("undo", r ? r.item : null);
				}
				return;
			default:
				return;
		}
		e.preventDefault();
		const from = v.selIndex;
		select(i, false);
		if (e.shiftKey && (e.key === "ArrowDown" || e.key === "ArrowUp")) {
			if (anchor === null || anchorIndex() < 0) {
				anchor = v.items[from] ? v.items[from].id : null;
				if (anchor !== null)
					setChecked(v.items[from], true);
			}
			const a = anchorIndex();
			if (a >= 0)
				range(a, v.selIndex, true);
			changed();
		}
	});

	// A control's own activation by Space comes on key up: the keydown
	// handler above did it already
	scroller.addEventListener("keyup", (e) => {
		if (e.key === " " && rowOf(e.target))
			e.preventDefault();
	});

	spacer.addEventListener("click", (e) => {
		const r = rowOf(e.target);
		if (!r || !v)
			return;
		// a click does not focus a button everywhere (Safari)
		const ctl = controls(r).find((c) => c.contains(e.target));
		if (ctl) {
			track(r, ctl);
			stops();
		}
		if (e.target === r.chk) {
			toggle(r.index, r.chk.checked, e.shiftKey);
			return;
		}
		if (e.target === r.chk.parentNode) {	// the cell around the box: a bigger target
			if (!r.chk.disabled) {
				col = 0;
				focusRow(r.index);
				r.chk.checked = !r.chk.checked;
				toggle(r.index, r.chk.checked, e.shiftKey);
			}
			return;
		}
		if (r.star.contains(e.target)) {
			if (r.item && typeof opts.onStar === "function")
				opts.onStar(r.item);
			return;
		}
		if (r.notice.contains(e.target)) {
			if (r.item && typeof opts.onNotice === "function")
				opts.onNotice(r.item);
			return;
		}
		if (!r.main.contains(e.target))
			return;
		if (e.shiftKey || e.ctrlKey || e.metaKey) {
			col = MAIN;
			focusRow(r.index);
			toggle(r.index, undefined, e.shiftKey);
			return;
		}
		select(r.index, true, e.detail === 0 ? "key" : "click");
	});
	// Shift+click selects text otherwise
	spacer.addEventListener("mousedown", (e) => {
		if (e.shiftKey && rowOf(e.target))
			e.preventDefault();
	});

	// A list that is not displayed (the reader pane on a phone) has no
	// scroll position of its own: the view's is kept and put back when the
	// list is shown again
	let hiddenBefore = false;
	let shownH = 0;	// the list's height when last displayed
	scroller.addEventListener("scroll", () => {
		if (v && pendingScroll === null && scroller.clientHeight > 0)
			v.scrollTop = scroller.scrollTop;
		later();
	}, { passive: true });

	const ro = new ResizeObserver(() => {
		const h0 = scroller.clientHeight === 0;
		if (!h0)
			shownH = scroller.clientHeight;
		if (!h0 && hiddenBefore && v && v.total && pendingScroll === null && scroller.scrollTop !== v.scrollTop)
			pendingScroll = v.scrollTop;
		hiddenBefore = h0;
		applyScroll();
		later();
	});
	ro.observe(scroller);

	// The first page's placeholder: a screenful of skeleton rows; none
	// once the user has gone on to another view (busy() calls this late)
	function skeletons(w) {
		if (w !== v)
			return null;
		const box = h("div", { class: "list-skel" },
			h("span", { class: "vh", role: "status", text: t("list.label_loading") }));
		for (let i = 0; i < 8; i++)
			box.append(h("div", { class: "row skel static", "aria-hidden": "true" },
				h("span", { class: "sk-box" }),
				h("span", { class: "sk-lines" }, h("span", { class: "sk" }), h("span", { class: "sk" }))));
		return box;
	}

	return {
		el,
		view,
		show,
		refresh,
		current: () => v,
		near,
		where,
		step,
		reveal,
		update,
		remove,
		reinsert,
		checked: () => Array.from(checked, ([id, folder]) => ({ id, folder })),
		checkAll,
		checkState,
		clearChecked,
		setOpen,
		hold,
		focus,
		rowCount: () => spacer.childElementCount,
		destroy() {
			ro.disconnect();
			if (raf)
				cancelAnimationFrame(raf);
			raf = 0;
			v = null;
		}
	};
}
