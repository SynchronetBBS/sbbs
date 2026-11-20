// The folder pane's contents: the built-in folders, the user's folders and
// the storage meter, from a folder.list result.
//
// const folders = createFolders(onOpen, { onNew })
//   onOpen(id) is called when a folder is chosen (click, Enter or Space);
//   onNew(button) when the New folder button beside "Your folders" is
//   pressed (the heading and the button show only with onNew, or when
//   there are user folders).
// folders.el                 the element to put in the pane
// folders.update(result)     a folder.list result {folders, usage}; an
//                            entry of kind "drafts" shows its total
// folders.setCurrent(id)     marks the open folder (null: none, e.g. while
//                            searching all mail)
// folders.focusCurrent()     focus on the open folder's button
// folderName(f)              a folder's display name (localized for the
//                            built-in ones)
//
// Keyboard: Up and Down move between the folders, Home and End to the
// first and last; Enter or Space opens one (they are buttons).

import { h, clear } from "./dom.js";
import { icon } from "./icons.js";
import { t } from "./i18n.js";
import { size, number } from "./format.js";

const ICONS = { inbox: "inbox", starred: "star", sent: "send", spam: "spam", trash: "trash", archive: "archive",
	drafts: "file" };

export function folderName(f) {
	if (!f)
		return "";
	if (f.kind === "user")
		return typeof f.name === "string" && f.name !== "" ? f.name : f.id.slice(2);
	return t("folders." + f.id);
}

export function createFolders(onOpen, opts) {
	const onNew = opts && typeof opts.onNew === "function" ? opts.onNew : null;
	const builtin = h("ul", { class: "fnav" });
	const newBtn = onNew ? h("button", { class: "ib", type: "button", "aria-label": t("folders.label_new_folder"),
		title: t("folders.label_new_folder"), onclick: (e) => onNew(e.currentTarget) }, icon("plus")) : null;
	const userHead = h("div", { class: "fhead", hidden: !onNew },
		h("h3", { id: "wm-user-folders", text: t("folders.label_your_folders") }), newBtn);
	const user = h("ul", { class: "fnav", "aria-labelledby": "wm-user-folders", hidden: true });
	const meterText = h("span");
	const meterFill = h("span", { class: "meter-fill" });
	const meterBar = h("span", { class: "meter-bar", "aria-hidden": "true" }, meterFill);
	const meter = h("div", { class: "meter", hidden: true },
		h("span", { class: "vh", text: t("main.label_storage_heading") + ": " }), meterBar, meterText);
	const el = h("div", { class: "fbody", onkeydown: onKey }, builtin, userHead, user, meter);
	let current = null;

	function button(f) {
		const unread = typeof f.unread === "number" && f.unread > 0 ? f.unread : 0;
		// Drafts: how many there are
		if (f.kind === "drafts") {
			const n = typeof f.total === "number" && f.total > 0 ? f.total : 0;
			const b = h("button", { class: "fbtn", type: "button", "data-folder": f.id, onclick: () => onOpen(f.id) },
				icon("file"), h("span", { class: "name ell", text: folderName(f) }),
				n ? h("span", { class: "count muted", "aria-hidden": "true", text: number(n) }) : null,
				n ? h("span", { class: "vh", text: ", " + t("compose.label_drafts_count", { n, count: number(n) }) }) : null);
			if (f.id === current)
				b.setAttribute("aria-current", "true");
			return h("li", null, b);
		}
		const b = h("button", {
			class: "fbtn", type: "button", "data-folder": f.id,
			onclick: () => onOpen(f.id)
		},
			icon(f.kind === "user" ? "folder" : (ICONS[f.id] || "folder")),
			h("span", { class: "name ell", text: folderName(f) }),
			unread ? h("span", { class: "count", "aria-hidden": "true", text: number(unread) }) : null,
			unread ? h("span", { class: "vh", text: ", " + t("list.label_unread_count", { n: unread, count: number(unread) }) })
				: null);
		if (f.id === current)
			b.setAttribute("aria-current", "true");
		return h("li", null, b);
	}

	function update(r) {
		const list = r && Array.isArray(r.folders) ? r.folders : [];
		// Rebuilt whole (a few dozen buttons at most); focus stays on the
		// same folder
		const focused = document.activeElement && el.contains(document.activeElement)
			? document.activeElement.getAttribute("data-folder") : null;
		clear(builtin);
		clear(user);
		for (const f of list) {
			if (f && typeof f.id === "string")
				(f.kind === "user" ? user : builtin).append(button(f));
		}
		user.hidden = user.childElementCount === 0;
		userHead.hidden = user.hidden && !onNew;
		const u = r && r.usage;
		if (u && typeof u.bytes === "number") {
			const quota = typeof u.quota === "number" && u.quota > 0 ? u.quota : 0;
			meterText.textContent = quota
				? t("main.label_storage", { used: size(u.bytes), quota: size(quota) })
				: t("main.label_storage_used", { used: size(u.bytes) });
			meterBar.hidden = !quota;
			if (quota)
				meterFill.style.width = Math.min(100, Math.max(0, u.bytes * 100 / quota)).toFixed(1) + "%";
			meter.hidden = false;
		} else
			meter.hidden = true;
		if (focused) {
			const b = el.querySelector('[data-folder="' + CSS.escape(focused) + '"]');
			if (b)
				b.focus({ preventScroll: true });
		}
	}

	function setCurrent(id) {
		current = id;
		for (const b of el.querySelectorAll(".fbtn")) {
			if (b.getAttribute("data-folder") === id)
				b.setAttribute("aria-current", "true");
			else
				b.removeAttribute("aria-current");
		}
	}

	function focusCurrent() {
		const b = el.querySelector('.fbtn[aria-current="true"]') || el.querySelector(".fbtn");
		if (b)
			b.focus({ preventScroll: true });
	}

	function onKey(e) {
		if (!e.target.classList || !e.target.classList.contains("fbtn"))
			return;
		const all = Array.from(el.querySelectorAll(".fbtn"));
		const i = all.indexOf(e.target);
		let j;
		if (e.key === "ArrowDown")
			j = Math.min(all.length - 1, i + 1);
		else if (e.key === "ArrowUp")
			j = Math.max(0, i - 1);
		else if (e.key === "Home")
			j = 0;
		else if (e.key === "End")
			j = all.length - 1;
		else
			return;
		e.preventDefault();
		all[j].focus();
	}

	return { el, update, setCurrent, focusCurrent, newButton: () => newBtn };
}
