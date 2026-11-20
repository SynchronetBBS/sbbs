// The mail frame's panes: three side by side at 900 px and wider, with two
// draggable dividers; one at a time below that, with the History API so the
// phone's back gesture returns to the previous pane.
//
// const layout = createLayout(shell, { folders, list, reader, aux })
//   shell: the element holding the panes (its data-pane names the pane
//   shown on a phone; the width properties are set on it); the three
//   panes, each with a heading marked data-pane-title; aux (optional):
//   a view shown in place of the list and the reader at every width
//   (contacts, settings).
// layout.show(name, { push, opener })
//   shows a pane.  On a phone, push adds a history entry, so Back returns
//   to the pane before it, and focus moves into the pane and back to
//   opener (default: the focused element) when it closes.  The aux view
//   does this at every width: it always has its own history entry, and
//   Back returns to the mail.
// layout.back()     the previous pane (history.back() when it is ours)
// layout.current()  the pane shown on a phone
// layout.isPhone()
// layout.reset()    the list pane, with this page's history entry as the base;
//                   call it once the frame is in the page
// layout.destroy()  removes the listeners and dividers
// layout.onchange   called with the pane name after any change

import { h, store } from "./dom.js";
import { t } from "./i18n.js";

const PHONE = "(max-width: 899.98px)";
const PARENT = { folders: "list", list: "list", reader: "list", aux: "list" };
// Shown with a history entry and focus at every width, not on a phone only
const FULL = { aux: true };
const STORE_KEY = "webmail.panes";
const STEP = 16;		// arrow key step, px
const BIG_STEP = 64;		// with Shift
const LIMITS = {
	folders: { min: 160, max: 400 },
	list: { min: 280, maxShare: 0.6 }
};
const READER_MIN = 320;

export function createLayout(shell, panes) {
	const mq = window.matchMedia(PHONE);
	const panesEl = panes.list.parentNode;
	const openers = [];
	let depth = 0;
	let pref = loadPref();
	let frame = 0;
	let drag = null;
	const layout = { show, back, current, isPhone, reset, destroy, onchange: null };

	function isPhone() {
		return mq.matches;
	}

	function current() {
		return shell.getAttribute("data-pane") || "list";
	}

	function set(name) {
		shell.setAttribute("data-pane", name);
		if (typeof layout.onchange === "function")
			layout.onchange(name);
	}

	function focusPane(name) {
		const title = panes[name] && panes[name].querySelector("[data-pane-title]");
		if (title)
			title.focus({ preventScroll: true });
	}

	function show(name, opts = {}) {
		if (!panes[name])
			return;
		const from = current();
		set(name);
		if ((!isPhone() && !FULL[name]) || name === from)
			return;
		if (opts.push) {
			openers[depth + 1] = opts.opener || document.activeElement;
			depth++;
			try {
				history.pushState({ webmail: true, pane: name, depth }, "");
			} catch (e) {
				// a browser may refuse; Back then leaves the pane by the button only
			}
		}
		focusPane(name);
	}

	function back() {
		const st = history.state;
		if ((isPhone() || FULL[current()]) && st && st.webmail && st.depth > 0) {
			history.back();
			return;
		}
		set(PARENT[current()] || "list");
	}

	function onPop(e) {
		const st = e.state && e.state.webmail ? e.state : { pane: "list", depth: 0 };
		const was = depth;
		depth = st.depth || 0;
		set(panes[st.pane] ? st.pane : "list");
		const opener = openers[was];
		openers.length = depth + 1;
		if (was > depth && opener && opener.isConnected)
			opener.focus({ preventScroll: true });
		else if (isPhone())
			focusPane(current());
	}

	// Called once the frame is in the page (the widths can be measured then)
	function reset() {
		depth = 0;
		openers.length = 0;
		try {
			history.replaceState({ webmail: true, pane: "list", depth: 0 }, "");
		} catch (e) {
			// keep going without history
		}
		set("list");
		apply();
	}

	// ---- Dividers ----

	function loadPref() {
		const p = { folders: null, list: null };
		try {
			const v = JSON.parse(store.get(STORE_KEY) || "null");
			if (v && typeof v === "object") {
				for (const k of ["folders", "list"])
					if (typeof v[k] === "number" && isFinite(v[k]))
						p[k] = Math.round(v[k]);
			}
		} catch (e) {
			// a damaged value: the defaults
		}
		return p;
	}

	function savePref() {
		store.set(STORE_KEY, pref.folders === null && pref.list === null ? null : JSON.stringify(pref));
	}

	function width(el) {
		return Math.round(el.getBoundingClientRect().width);
	}

	// The range a pane's width may take now, given the other pane's width
	function range(name, total, other) {
		if (name === "folders") {
			const max = Math.min(LIMITS.folders.max, total - other - READER_MIN);
			return { min: LIMITS.folders.min, max: Math.max(LIMITS.folders.min, max) };
		}
		const max = Math.min(Math.floor(total * LIMITS.list.maxShare), total - other - READER_MIN);
		return { min: LIMITS.list.min, max: Math.max(LIMITS.list.min, max) };
	}

	function clamp(v, r) {
		return Math.min(r.max, Math.max(r.min, v));
	}

	// The stylesheet's default widths for this window size
	function defaults() {
		const cs = getComputedStyle(shell);
		return {
			folders: parseInt(cs.getPropertyValue("--wm-folders-def"), 10) || 236,
			list: parseInt(cs.getPropertyValue("--wm-list-def"), 10) || 380
		};
	}

	// The widths to use in a panes area total px wide: the preferences (or
	// the defaults), each within its own limits; when the reader would get
	// less than READER_MIN, the list gives up width first, then the folders
	function fit(total) {
		const d = defaults();
		let f = clamp(pref.folders === null ? d.folders : pref.folders, { min: LIMITS.folders.min, max: LIMITS.folders.max });
		let l = pref.list === null ? d.list : pref.list;
		l = Math.max(LIMITS.list.min, Math.min(l, Math.floor(total * LIMITS.list.maxShare)));
		let over = f + l + READER_MIN - total;
		if (over > 0) {
			const cut = Math.min(over, l - LIMITS.list.min);
			l -= cut;
			over -= cut;
		}
		if (over > 0)
			f -= Math.min(over, f - LIMITS.folders.min);
		return { folders: f, list: l, defaults: d };
	}

	// Set the width properties from the preferences, fitted to the current
	// window, and update the dividers' values
	function apply() {
		frame = 0;
		if (isPhone() || !panesEl.isConnected)
			return;
		const total = width(panesEl);
		if (total === 0)
			return;
		const w = fit(total);
		for (const name of ["folders", "list"]) {
			const prop = "--wm-" + name + "-w";
			if (pref[name] === null && w[name] === w.defaults[name])
				shell.style.removeProperty(prop);
			else
				shell.style.setProperty(prop, w[name] + "px");
		}
		for (const d of dividers) {
			const name = d.dataset.pane;
			const r = range(name, total, name === "folders" ? w.list : w.folders);
			d.setAttribute("aria-valuemin", String(r.min));
			d.setAttribute("aria-valuemax", String(r.max));
			d.setAttribute("aria-valuenow", String(w[name]));
		}
	}

	function later() {
		if (!frame)
			frame = requestAnimationFrame(apply);
	}

	// The width a pane has now
	function widthOf(name) {
		return fit(width(panesEl))[name];
	}

	function setWidth(name, v) {
		const total = width(panesEl);
		const w = fit(total);
		pref[name] = clamp(Math.round(v), range(name, total, name === "folders" ? w.list : w.folders));
		apply();
	}

	function resetWidth(name) {
		pref[name] = null;
		apply();
		savePref();
	}

	function onKey(e) {
		const name = e.currentTarget.dataset.pane;
		const step = e.shiftKey ? BIG_STEP : STEP;
		let w = widthOf(name);
		if (e.key === "ArrowRight")
			w += step;
		else if (e.key === "ArrowLeft")
			w -= step;
		else if (e.key === "Home") {
			e.preventDefault();
			resetWidth(name);
			return;
		} else
			return;
		e.preventDefault();
		setWidth(name, w);
		savePref();
	}

	function onDown(e) {
		if (drag || (e.pointerType === "mouse" && e.button !== 0))
			return;
		const d = e.currentTarget;
		e.preventDefault();
		try {
			d.setPointerCapture(e.pointerId);
		} catch (err) {
			// no capture: the cover still keeps the pointer out of frames
		}
		const cover = h("div", { class: "drag-cover", "aria-hidden": "true" });
		document.body.append(cover);
		shell.classList.add("dragging");
		d.classList.add("active");
		drag = { d, id: e.pointerId, x: e.clientX, w: widthOf(d.dataset.pane), cover };
	}

	function onMove(e) {
		if (!drag || e.pointerId !== drag.id)
			return;
		setWidth(drag.d.dataset.pane, drag.w + (e.clientX - drag.x));
	}

	function onUp(e) {
		if (!drag || (e && e.pointerId !== undefined && e.pointerId !== drag.id))
			return;
		const { d, id, cover } = drag;
		drag = null;
		try {
			if (d.hasPointerCapture(id))
				d.releasePointerCapture(id);
		} catch (err) {
			// already released
		}
		cover.remove();
		shell.classList.remove("dragging");
		d.classList.remove("active");
		savePref();
	}

	function onDouble(e) {
		resetWidth(e.currentTarget.dataset.pane);
	}

	function divider(name, label) {
		const d = h("div", {
			class: "divider",
			role: "separator",
			tabindex: "0",
			"aria-orientation": "vertical",
			"aria-label": label,
			"aria-controls": panes[name].id,
			"data-pane": name
		});
		d.addEventListener("pointerdown", onDown);
		d.addEventListener("pointermove", onMove);
		d.addEventListener("pointerup", onUp);
		d.addEventListener("pointercancel", onUp);
		d.addEventListener("lostpointercapture", onUp);
		d.addEventListener("dblclick", onDouble);
		d.addEventListener("keydown", onKey);
		panesEl.append(d);
		return d;
	}

	const dividers = [
		divider("folders", t("main.label_resize_folders")),
		divider("list", t("main.label_resize_list"))
	];

	function onMedia() {
		if (drag)
			onUp();
		later();
	}

	window.addEventListener("popstate", onPop);
	window.addEventListener("resize", later);
	if (mq.addEventListener)
		mq.addEventListener("change", onMedia);
	else
		mq.addListener(onMedia);

	function destroy() {
		if (drag)
			onUp();
		window.removeEventListener("popstate", onPop);
		window.removeEventListener("resize", later);
		if (mq.removeEventListener)
			mq.removeEventListener("change", onMedia);
		else
			mq.removeListener(onMedia);
		if (frame)
			cancelAnimationFrame(frame);
		for (const d of dividers)
			d.remove();
	}

	return layout;
}
