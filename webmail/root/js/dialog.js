// Modal dialogs and pop-up menus.
//
// openDialog({ title, body, buttons, returnTo, className, initial })
//                         a modal dialog: title (text), body (nodes),
//                         buttons [{ label, primary, danger, onclick, href }]
//                         in its footer (href: a link, e.g. a download).  Escape, the Close button and a
//                         click beside it close it; focus stays inside
//                         while it is open, starts on initial (default:
//                         the first button of the footer, else Close) and
//                         goes back to returnTo (default: the focused
//                         element) when it closes.  Returns { el, close }.
// confirmDialog({ title, text, ok, danger })
//                         a Promise of true (ok) or false
// promptDialog({ title, label, value, ok, submit })
//                         a dialog with one text field; submit(value) is
//                         called on OK (or Enter) and returns a Promise of
//                         null (done: the dialog closes) or a text to show
//                         under the field (the dialog stays).  A Promise
//                         of true when it was done, false when cancelled.
// openMenu(anchor, { label, items, filter })
//                         a pop-up list of buttons under anchor (a sheet
//                         at the bottom on a phone): items [{ label, icon,
//                         onclick, danger, disabled, pressed }] (pressed:
//                         true or false for an item that is a choice
//                         that is on or off); Up/Down move between them,
//                         Escape or a click elsewhere closes it and focus
//                         goes back to anchor (also before an item's
//                         onclick runs).  filter: a field above the
//                         items that narrows them by label (placeholder
//                         text filter).  Returns { el, close }.
// closePopups()           closes an open menu (not a dialog)
//
// Text is put in as text only.

import { h, clear } from "./dom.js";
import { icon } from "./icons.js";
import { t } from "./i18n.js";

const PHONE = "(max-width: 899.98px)";

function focusables(el) {
	return Array.from(el.querySelectorAll("a[href], button:not([disabled]), input:not([disabled]), textarea, "
		+ "[tabindex]:not([tabindex='-1'])")).filter((e) => !e.hidden && e.getClientRects().length > 0);
}

function root() {
	return document.getElementById("app") || document.body;
}

// ---- Dialogs ----

const stack = [];

export function openDialog(o) {
	closePopups();
	const returnTo = o.returnTo || document.activeElement;
	const titleId = "wm-dlg-" + Math.random().toString(36).slice(2);
	const close = h("button", { class: "ib", type: "button", "aria-label": t("main.button_close"),
		title: t("main.button_close"), onclick: () => done() }, icon("x"));
	const foot = h("div", { class: "dlg-foot" });
	for (const b of o.buttons || []) {
		const cls = "btn" + (b.primary ? " primary" : "") + (b.danger ? " danger" : "");
		foot.append(b.href ? h("a", { class: cls, href: b.href, text: b.label })
			: h("button", { class: cls, type: "button", text: b.label, onclick: b.onclick }));
	}
	const dialog = h("div", { class: "dlg" + (o.className ? " " + o.className : ""), role: "dialog",
		"aria-modal": "true", "aria-labelledby": titleId },
		h("div", { class: "dlg-head" }, h("h2", { class: "ell", id: titleId, text: o.title }), close),
		h("div", { class: "dlg-body" }, o.body || []),
		foot.childElementCount ? foot : null);
	const el = h("div", { class: "dlg-wrap", onclick: (e) => {
		if (e.target === el)
			done();
	} }, dialog);
	let open = true;
	const onFocus = (e) => {
		if (open && stack[stack.length - 1] === me && !dialog.contains(e.target))
			(focusables(dialog)[0] || close).focus({ preventScroll: true });
	};
	dialog.addEventListener("keydown", (e) => {
		if (e.key === "Escape") {
			e.preventDefault();
			e.stopPropagation();
			done();
			return;
		}
		if (e.key !== "Tab")
			return;
		const f = focusables(dialog);
		if (f.length === 0)
			return;
		if (e.shiftKey && document.activeElement === f[0]) {
			e.preventDefault();
			f[f.length - 1].focus();
		} else if (!e.shiftKey && document.activeElement === f[f.length - 1]) {
			e.preventDefault();
			f[0].focus();
		}
	});
	function done(how) {
		if (!open)
			return;
		open = false;
		document.removeEventListener("focusin", onFocus, true);
		el.remove();
		const i = stack.indexOf(me);
		if (i >= 0)
			stack.splice(i, 1);
		if (typeof o.onClose === "function")
			o.onClose(how);
		if (returnTo && returnTo.isConnected && how !== "keep-focus")
			returnTo.focus({ preventScroll: true });
	}
	const me = { el, close: done, dialog };
	stack.push(me);
	root().append(el);
	document.addEventListener("focusin", onFocus, true);
	(o.initial || foot.querySelector(".btn") || close).focus({ preventScroll: true });
	return me;
}

export function confirmDialog(o) {
	return new Promise((resolve) => {
		let answer = false;
		const d = openDialog({ title: o.title, body: [h("p", { class: "dlg-text", text: o.text })],
			buttons: [
				{ label: t("main.button_cancel"), onclick: () => d.close() },
				{ label: o.ok, primary: !o.danger, danger: !!o.danger, onclick: () => {
					answer = true;
					d.close();
				} }
			],
			onClose: () => resolve(answer) });
	});
}

export function promptDialog(o) {
	return new Promise((resolve) => {
		let ok = false;
		let busy = false;
		const id = "wm-prompt-" + Math.random().toString(36).slice(2);
		const input = h("input", { id, type: "text", autocomplete: "off", spellcheck: "false",
			"aria-describedby": id + "-err" });
		input.value = o.value || "";
		const err = h("p", { class: "dlg-err", id: id + "-err", role: "alert", hidden: true });
		const form = h("form", { class: "field", novalidate: true, onsubmit: (e) => {
			e.preventDefault();
			go();
		} }, h("label", { for: id, text: o.label }), input, err);
		const d = openDialog({ title: o.title, body: [form], initial: input, returnTo: o.returnTo,
			buttons: [
				{ label: t("main.button_cancel"), onclick: () => d.close() },
				{ label: o.ok, primary: true, onclick: () => go() }
			],
			onClose: () => resolve(ok) });
		input.select();
		function go() {
			if (busy)
				return;
			busy = true;
			Promise.resolve(o.submit(input.value)).then((msg) => {
				busy = false;
				if (msg === null || msg === undefined) {
					ok = true;
					d.close();
					return;
				}
				err.textContent = msg;
				err.hidden = false;
				input.setAttribute("aria-invalid", "true");
				input.focus();
			}, () => {
				busy = false;
			});
		}
	});
}

// ---- Menus ----

let menu = null;

export function closePopups(restore) {
	if (!menu)
		return;
	const m = menu;
	menu = null;
	m.el.remove();
	m.scrim.remove();
	window.removeEventListener("resize", m.onResize);
	m.anchor.setAttribute("aria-expanded", "false");
	if (restore && m.anchor.isConnected)
		m.anchor.focus({ preventScroll: true });
}

export function openMenu(anchor, o) {
	closePopups();
	const list = h("div", { class: "menu-items" });
	const none = h("p", { class: "none", hidden: true });
	const field = o.filter ? h("input", { type: "text", class: "menu-filter", autocomplete: "off",
		spellcheck: "false", placeholder: o.filter, "aria-label": o.filter }) : null;
	const buttons = [];
	for (const it of o.items) {
		const b = h("button", { class: "mi" + (it.danger ? " danger" : ""), type: "button", disabled: !!it.disabled,
			"aria-pressed": typeof it.pressed === "boolean" ? String(it.pressed) : null,
			onclick: () => {
				// focus back on the anchor first: the item's action moves it
				// on from there (never from a removed button to the page)
				closePopups(true);
				it.onclick();
			} }, it.icon ? icon(it.icon) : null, h("span", { class: "ell", text: it.label }));
		b.wmLabel = it.label.toLowerCase();
		buttons.push(b);
		list.append(b);
	}
	const el = h("div", { class: "pop menu", role: "dialog", "aria-label": o.label },
		o.title ? h("div", { class: "pop-label", text: o.title }) : null, field, list, none);
	const scrim = h("div", { class: "scrim", onclick: () => closePopups(true) });
	function shownButtons() {
		return buttons.filter((b) => !b.hidden && !b.disabled);
	}
	if (field) {
		field.addEventListener("input", () => {
			const q = field.value.trim().toLowerCase();
			for (const b of buttons)
				b.hidden = q !== "" && !b.wmLabel.includes(q);
			none.hidden = shownButtons().length > 0;
			none.textContent = none.hidden ? "" : t("list.label_no_folder_match", { query: field.value.trim() });
		});
	}
	el.addEventListener("keydown", (e) => {
		if (e.key === "Escape") {
			e.preventDefault();
			e.stopPropagation();
			closePopups(true);
			return;
		}
		if (e.key === "Enter" && e.target === field) {
			e.preventDefault();
			const s = shownButtons();
			if (s.length === 1)
				s[0].click();
			return;
		}
		if (e.key !== "ArrowDown" && e.key !== "ArrowUp" && e.key !== "Home" && e.key !== "End")
			return;
		if (e.target === field && (e.key === "Home" || e.key === "End"))
			return;
		const s = shownButtons();
		if (s.length === 0)
			return;
		e.preventDefault();
		let i = s.indexOf(document.activeElement);
		if (e.key === "Home")
			i = 0;
		else if (e.key === "End")
			i = s.length - 1;
		else if (i < 0)
			i = e.key === "ArrowDown" ? 0 : s.length - 1;
		else
			i = Math.max(0, Math.min(s.length - 1, i + (e.key === "ArrowDown" ? 1 : -1)));
		s[i].focus();
	});
	el.addEventListener("focusout", (e) => {
		if (menu && menu.el === el && e.relatedTarget && !el.contains(e.relatedTarget) && e.relatedTarget !== anchor)
			closePopups(false);
	});
	function place() {
		el.style.left = "";
		el.style.top = "";
		if (window.matchMedia(PHONE).matches)
			return;
		const r = anchor.getBoundingClientRect();
		const w = el.offsetWidth;
		el.style.left = Math.max(8, Math.min(r.left, window.innerWidth - w - 8)) + "px";
		const below = r.bottom + 4;
		el.style.top = (below + el.offsetHeight > window.innerHeight - 8
			? Math.max(8, r.top - 4 - el.offsetHeight) : below) + "px";
	}
	const onResize = () => closePopups(false);
	root().append(scrim, el);
	anchor.setAttribute("aria-expanded", "true");
	menu = { el, scrim, anchor, onResize };
	window.addEventListener("resize", onResize);
	place();
	(field || shownButtons()[0] || el).focus({ preventScroll: true });
	return { el, close: closePopups };
}
