// The message list: a virtual list (only the rows in view, plus a screenful
// above and below, are in the page; row elements are reused as the list
// scrolls) over a sparse cache of message.list pages.
//
// const list = createList({ fetch, labelledBy, onOpen, onSelect, empty, failed,
//                           folderTag, onTotal })
//   fetch(view, offset, limit, { first, background })
//                               a Promise of a message.list result; first is
//                               true for a view's first load, which shows
//                               the failed state rather than waiting for an
//                               unreachable server; background is true for
//                               a refresh the app makes on its own and for
//                               the reloads of rows already shown that
//                               follow it
//   onSelect(item, opening)     the selection moved to item (null: a row
//                               not loaded yet); opening is true when
//                               onOpen follows (a click or Enter), false
//                               for the arrow and paging keys
//   onOpen(item, how)           a row was chosen: how is "key" (Enter or
//                               Space) or "click"
//   empty(view)                 the node to show when the view has no messages
//   failed(view, error, retry)  the node to show when its first page failed
//   folderTag(item)             the folder name to show on a row, or ""
//   onTotal(view)               the view shown has (a new) total
// list.el                       the element to put in the pane
// list.view(key, props)         the cached view for key (a folder plus a
//                               search), made with props when new
// list.show(view)               shows a view: its cached rows and scroll
//                               position at once when it has them (then
//                               refreshed), else loads its first page
// list.refresh(background)      reloads the first page of the view shown and
//                               merges it: new messages at the top keep the
//                               rows in view and the selection where they
//                               were; the other pages are then reloaded as
//                               they come into view.  One refresh of a
//                               view at a time: a call while one runs gets
//                               that one's Promise
// list.current()                the view shown
// list.near(id, delta)          the loaded item delta rows from the message
//                               with this id in the view shown, or null
// list.update(id, patch)        changes fields of the message with this id
//                               in the view shown (its row is painted
//                               again); returns the item or null
// list.focus()
// list.destroy()
//
// A view: { key, total (null until loaded), items (sparse, by offset),
// scrollTop, selIndex, selId, ... } plus the props it was made with.
//
// Rows have one height, measured once per layout (wide or phone) and when
// the text size changes; each is placed with a transform.  The list is a
// listbox: the scroller has focus and aria-activedescendant names the
// selected row; rows carry aria-setsize and aria-posinset since most of
// them are not in the page.

import { h, clear } from "./dom.js";
import { icon } from "./icons.js";
import { t } from "./i18n.js";
import { busy } from "./notice.js";
import { listDate, fullDate } from "./format.js";

export const PAGE = 50;
const MAX_VIEWS = 20;
const RETRY_AFTER = 10000;	// a page that failed is not asked for again sooner
const PHONE = "(max-width: 899.98px)";

export function createList(opts) {
	const spacer = h("div", { class: "list-spacer", role: "none" });
	const scroller = h("div", {
		class: "list-scroll", role: "listbox", tabindex: "0", "aria-labelledby": opts.labelledBy
	}, spacer);
	const stateEl = h("div", { class: "list-state", tabindex: "-1", hidden: true });
	const el = h("div", { class: "list-body", tabindex: "-1" }, scroller, stateEl);
	const mq = window.matchMedia(PHONE);
	const views = new Map();
	const live = new Map();		// index -> row element
	const free = [];
	let v = null;
	let rowH = 0;
	let rowKey = "";
	let raf = 0;
	let pendingScroll = null;	// applied once the scroller has a size

	// ---- Views ----

	function view(key, props) {
		let w = views.get(key);
		if (w) {
			views.delete(key);	// most recently used last
			views.set(key, w);
			return w;
		}
		w = Object.assign({ key, total: null, items: [], fresh: [], gen: 1, inflight: new Set(), failed: new Map(),
			scrollTop: 0, selIndex: -1, selId: null, error: null, loading: null, refreshing: null, seq: 0,
			bgGen: 0, focusAfter: false }, props);
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
	}

	function show(w) {
		if (v && v !== w)
			v.busy = false;	// its scrollTop is kept by the scroll handler
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
		(scroller.hidden ? stateEl : scroller).focus({ preventScroll: true });
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
			if (seq === w.seq)	// a later reply never gives way to an older one
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
			// the selection by id where it shows up again
			w.items = [];
			w.fresh = [];
			if (w.selId !== null)
				w.selIndex = -1;
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
		let had = true;
		for (let i = p * PAGE; i < Math.min(w.total, (p + 1) * PAGE) && had; i++)
			had = w.items[i] !== undefined;
		w.inflight.add(p);
		opts.fetch(w, p * PAGE, PAGE, { background: had && w.bgGen === gen }).then((r) => {
			w.inflight.delete(p);
			if (w.gen !== gen) {	// refreshed meanwhile: ask again
				if (w === v)
					later();
				return;
			}
			if (r.total !== w.total) {
				// the folder changed under the list: keep this page only
				w.items = [];
				w.fresh = [];
				w.gen++;
				w.total = r.total;
				if (w.selId !== null)
					w.selIndex = -1;
				if (w === v)
					paintTotal();
			}
			fill(w, p * PAGE, r.items);
			if (w === v)
				later();
		}, () => {
			w.inflight.delete(p);
			w.failed.set(p, Date.now());
		});
	}

	// ---- Rows ----

	function newRow() {
		const r = {};
		r.unread = h("span", { class: "vh", text: t("list.label_unread") + ", " });
		r.from = h("span", { class: "from ell" }, r.unread, r.fromText = document.createTextNode(""));
		r.prio = h("span", { class: "tag prio" });
		r.state = h("span", { class: "tag" });
		r.folder = h("span", { class: "tag" });
		r.clip = h("span", { class: "mark", title: t("list.label_attachment") }, icon("clip", "sm"),
			h("span", { class: "vh", text: ", " + t("list.label_attachment") }));
		r.replied = h("span", { class: "mark", title: t("list.label_replied") }, icon("reply", "sm"),
			h("span", { class: "vh", text: ", " + t("list.label_replied") }));
		r.star = h("span", { class: "mark star", title: t("list.label_starred") }, icon("star", "sm"),
			h("span", { class: "vh", text: ", " + t("list.label_starred") }));
		r.date = h("span", { class: "date" });
		r.subj = h("span", { class: "subj ell" });
		r.sk = h("span", { class: "sk-lines", "aria-hidden": "true" }, h("span", { class: "sk" }), h("span", { class: "sk" }));
		r.skText = h("span", { class: "vh", text: t("list.label_loading"), hidden: true });
		r.el = h("div", { class: "row", role: "option", "aria-selected": "false" },
			h("span", { class: "r1" }, r.from,
				h("span", { class: "marks" }, r.prio, r.state, r.folder, r.clip, r.replied, r.star), r.date),
			r.subj, r.sk, r.skText);
		r.el.wmRow = r;
		r.item = undefined;
		spacer.append(r.el);
		return r;
	}

	function shown(node, on) {
		if (node.hidden === on)
			node.hidden = !on;
	}

	function paintItem(r, it) {
		const sent = it.folder === "sent";
		const who = sent ? (it.to || "") : (it.from || "");
		const name = who !== "" ? who : t("list.label_no_sender");
		r.fromText.data = sent ? t("list.label_to_name", { name }) : name;
		r.subj.textContent = typeof it.subject === "string" && it.subject !== "" ? it.subject : t("list.label_no_subject");
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
		const st = it.delivery && typeof it.delivery.state === "string" ? it.delivery.state : "";
		const stKey = { queued: "reader.label_state_queued", failed: "reader.label_state_failed",
			unsent: "reader.label_state_not_sent" }[st];
		shown(r.state, !!stKey);
		if (stKey) {
			r.state.textContent = t(stKey);
			r.state.classList.toggle("danger", st === "failed");
		}
		const fname = opts.folderTag ? opts.folderTag(it) : "";
		shown(r.folder, fname !== "");
		if (fname)
			r.folder.textContent = fname;
		shown(r.clip, it.attach === true);
		shown(r.replied, it.replied === true);
		shown(r.star, it.starred === true);
		r.date.textContent = listDate(it.arrived);
		let title = t("list.label_received", { date: fullDate(it.arrived) });
		if (typeof it.date === "number" && Math.abs(it.date - it.arrived) >= 60)
			title += "\n" + t("list.label_written", { date: fullDate(it.date) });
		r.date.title = title;
	}

	function paint(r, i) {
		const it = v.items[i];
		const sel = i === v.selIndex;
		if (r.index !== i) {
			r.index = i;
			r.el.id = "wm-row-" + i;
			r.el.style.transform = "translateY(" + (i * rowH) + "px)";
			r.el.setAttribute("aria-posinset", String(i + 1));
		}
		if (r.total !== v.total) {
			r.total = v.total;
			r.el.setAttribute("aria-setsize", String(v.total));
		}
		if (r.sel !== sel) {
			r.sel = sel;
			r.el.setAttribute("aria-selected", String(sel));
		}
		if (r.item === it)
			return;
		r.item = it;
		const skel = it === undefined;
		r.el.classList.toggle("skel", skel);
		r.el.setAttribute("aria-busy", String(skel));
		shown(r.skText, skel);
		if (skel)
			r.el.classList.remove("unread");
		else
			paintItem(r, it);
	}

	function release(i, r) {
		live.delete(i);
		r.el.hidden = true;
		r.el.removeAttribute("id");
		r.index = -1;
		free.push(r);
	}

	function releaseAll() {
		for (const [i, r] of live)
			release(i, r);
		active(null);
	}

	// aria-activedescendant, and a class the focus ring depends on (the
	// selected row shows focus itself; without one the list does)
	function active(r) {
		if (r)
			scroller.setAttribute("aria-activedescendant", r.el.id);
		else
			scroller.removeAttribute("aria-activedescendant");
		scroller.classList.toggle("has-active", !!r);
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
		if (typeof opts.onTotal === "function")
			opts.onTotal(v);
	}

	function showState(node) {
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
			if (v && v.total === 0)
				releaseAll();
			return;
		}
		if (pendingScroll !== null) {
			applyScroll();
			return;
		}
		const H = scroller.clientHeight;
		const top = scroller.scrollTop;
		const per = Math.ceil(H / rowH);
		const first = Math.max(0, Math.floor(top / rowH) - per);
		const last = Math.min(v.total - 1, Math.floor((top + H) / rowH) + per);
		for (const [i, r] of live) {
			if (i < first || i > last)
				release(i, r);
		}
		for (let i = first; i <= last; i++) {
			let r = live.get(i);
			if (!r) {
				r = free.pop() || newRow();
				r.el.hidden = false;
				live.set(i, r);
			}
			paint(r, i);
		}
		want(first, last);
		active(live.get(v.selIndex) || null);
	}

	function later() {
		if (!raf)
			raf = requestAnimationFrame(render);
	}

	// ---- Selection and keyboard ----

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

	function update(id, patch) {
		const i = indexOf(id);
		if (i < 0)
			return null;
		const it = v.items[i];
		Object.assign(it, patch);
		const r = live.get(i);
		if (r) {
			r.item = undefined;	// painted again
			paint(r, i);
		}
		return it;
	}

	function select(i, open, how) {
		if (!v || !v.total)
			return;
		i = Math.max(0, Math.min(v.total - 1, i));
		v.selIndex = i;
		const it = v.items[i];
		v.selId = it ? it.id : null;
		// keep it in view
		const H = scroller.clientHeight;
		const y = i * rowH;
		if (y < scroller.scrollTop)
			scroller.scrollTop = y;
		else if (y + rowH > scroller.scrollTop + H)
			scroller.scrollTop = y + rowH - H;
		render();
		if (typeof opts.onSelect === "function")
			opts.onSelect(it || null, open === true);
		if (open && it && typeof opts.onOpen === "function")
			opts.onOpen(it, how);
	}

	scroller.addEventListener("keydown", (e) => {
		if (!v || !v.total || e.altKey || e.ctrlKey || e.metaKey)
			return;
		const per = Math.max(1, Math.floor(scroller.clientHeight / rowH) - 1);
		const at = v.selIndex;
		let i;
		switch (e.key) {
			case "ArrowDown":
				i = at < 0 ? Math.floor(scroller.scrollTop / rowH) : at + 1;
				break;
			case "ArrowUp":
				i = at < 0 ? Math.floor(scroller.scrollTop / rowH) : at - 1;
				break;
			case "PageDown":
				i = (at < 0 ? Math.floor(scroller.scrollTop / rowH) : at) + per;
				break;
			case "PageUp":
				i = (at < 0 ? Math.floor(scroller.scrollTop / rowH) : at) - per;
				break;
			case "Home":
				i = 0;
				break;
			case "End":
				i = v.total - 1;
				break;
			case "Enter":
			case " ":
				e.preventDefault();
				select(at < 0 ? Math.floor(scroller.scrollTop / rowH) : at, true, "key");
				return;
			default:
				return;
		}
		e.preventDefault();
		select(i, false);
	});

	spacer.addEventListener("click", (e) => {
		const row = e.target.closest(".row");
		if (row && row.wmRow && row.wmRow.index >= 0)
			select(row.wmRow.index, true, "click");
	});

	// A list that is not displayed (the reader pane on a phone) has no
	// scroll position of its own: the view's is kept and put back when the
	// list is shown again
	let hiddenBefore = false;
	scroller.addEventListener("scroll", () => {
		if (v && pendingScroll === null && scroller.clientHeight > 0)
			v.scrollTop = scroller.scrollTop;
		later();
	}, { passive: true });

	const ro = new ResizeObserver(() => {
		const h0 = scroller.clientHeight === 0;
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
		update,
		focus: () => scroller.focus({ preventScroll: true }),
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
