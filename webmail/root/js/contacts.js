// The contacts view: the user's own address book (contact.list), with
// add, edit, delete and "Write to".  Shown in the mail frame in place of
// the list and the reader (app.js puts it in the aux pane).
//
// const view = createContacts({ write })
//   write(entries, opener): open compose for a new message to entries
//   (strings as typed in a To field: "Name" <address>, or an address)
// view.el          the view's content (below the pane's bar)
// view.focus()     focus on the search field
// view.destroy()
//
// The list: one item per contact, with its name, its addresses and its
// note, as text, and three buttons: Write to (with several addresses, a
// menu asks which), Edit and Delete (after a confirmation).  Up and Down
// move between the contacts (to the same button in the next one).  The
// search field narrows the list as the user types (name, addresses and
// note, case ignored; each contact's row is kept and only hidden, and
// the typing is debounced, so a full book of 500 stays quick); the number
// shown is announced.  Add and Edit open
// a dialog with the name, the addresses (one per line) and the note; the
// server checks the addresses, and its text for a refusal is shown beside
// the field it is about (the dialog stays open; a refused address names
// its line, which is selected).  A name or note the server shortened is
// reported ("Saved as ...").  A contact deleted elsewhere meanwhile: the
// dialog says so and offers to save it as a new one.  The server's limits
// apply (contacts, addresses per contact, lengths): their texts are the
// server's.  Contacts the user writes to are added by the server when a
// message is sent; the list is read again each time the view opens.

import { h, clear } from "./dom.js";
import { icon } from "./icons.js";
import { t } from "./i18n.js";
import { rpc } from "./rpc.js";
import { notice, busy } from "./notice.js";
import { openDialog, confirmDialog, openMenu } from "./dialog.js";
import { avatarBadge } from "./avatar.js";
import { number } from "./format.js";
import { entryOf } from "./compose.js";

const MAX_ADDRS = 10;	// the server's limit per contact (webmail/lib/contacts.js)
const SEARCH_DELAY = 120;	// ms after the last key

// What goes into a To field for an address of a contact: an Internet
// address with the contact's name in front (compose's entryOf, which
// quotes it as needed), anything else (a user name on this BBS, a FidoNet
// or QWK address) as it is
function entryFor(name, addr) {
	const n = typeof name === "string" ? name.trim() : "";
	if (n === "" || !/^[^@\s"<>]+@[^@\s:\/"<>]+\.[^@\s:\/"<>]+$/.test(addr))
		return addr;
	return entryOf(n, addr);
}

function initial(text) {
	const c = Array.from(String(text || "").trim())[0];
	return c ? c.toUpperCase() : "?";
}

// The name shown for a contact: its name, else its first address
function shownName(c) {
	return c.name !== "" ? c.name : (c.addrs[0] || "");
}

function clean(c) {
	if (!c || typeof c !== "object" || typeof c.id !== "string")
		return null;
	return { id: c.id, name: typeof c.name === "string" ? c.name : "",
		addrs: Array.isArray(c.addrs) ? c.addrs.filter((a) => typeof a === "string") : [],
		note: typeof c.note === "string" ? c.note : "" };
}

function byName(a, b) {
	const x = shownName(a).toLowerCase();
	const y = shownName(b).toLowerCase();
	return x < y ? -1 : (x > y ? 1 : 0);
}

export function createContacts(o) {
	let contacts = null;	// [contact], null until loaded
	let dead = false;

	const searchId = "wm-contacts-search";
	const search = h("input", { id: searchId, type: "search", class: "aux-search", autocomplete: "off",
		spellcheck: "false", placeholder: t("contacts.placeholder_search"), "aria-label": t("contacts.placeholder_search"),
		"aria-controls": "wm-contacts-list" });
	const addBtn = h("button", { class: "btn primary", type: "button", onclick: (e) => edit(null, e.currentTarget) },
		icon("plus"), h("span", { text: t("contacts.button_add") }));
	const live = h("p", { class: "vh", role: "status", "aria-live": "polite" });
	const listEl = h("ul", { class: "clist", id: "wm-contacts-list", "aria-label": t("contacts.label_contacts") });
	const stateEl = h("div", { class: "aux-state" });
	const scroll = h("div", { class: "aux-scroll" }, listEl, stateEl);
	const el = h("div", { class: "aux-body contacts" },
		h("div", { class: "aux-tools" }, search, addBtn), live, scroll);

	let searchTimer = 0;
	search.addEventListener("input", () => {
		clearTimeout(searchTimer);
		searchTimer = setTimeout(filter, SEARCH_DELAY);
	});
	listEl.addEventListener("keydown", onKey);

	// ---- Loading ----

	function load() {
		clear(stateEl);
		const p = rpc.call("contact.list", null).then((r) => {
			if (dead)
				return;
			contacts = (Array.isArray(r.contacts) ? r.contacts : []).map(clean).filter((c) => c !== null);
			contacts.sort(byName);
			render();
		}, (e) => {
			if (dead)
				return;
			clear(listEl);
			clear(stateEl);
			stateEl.append(h("div", { class: "empty failed", role: "alert" },
				h("span", { class: "badge" }, icon("alert")),
				h("p", { class: "lead", text: t("contacts.label_load_failed") }),
				e && e.message && e.error !== "failed" ? h("p", { text: e.message }) : null,
				h("button", { class: "btn", type: "button", text: t("list.button_try_again"), onclick: () => {
					search.focus();
					load();
				} })));
		});
		busy(stateEl, p);
	}

	// ---- The list ----

	function matches(c, q) {
		if (q === "")
			return true;
		if (c.name.toLowerCase().includes(q) || c.note.toLowerCase().includes(q))
			return true;
		return c.addrs.some((a) => a.toLowerCase().includes(q));
	}

	// Every contact's row, built again when the list changes; the search
	// only hides rows (filter)
	function render() {
		if (contacts === null)
			return;
		clear(listEl);
		for (const c of contacts) {
			const r = row(c);
			r.wmContact = c;
			listEl.append(r);
		}
		filter();
	}

	function filter() {
		clearTimeout(searchTimer);
		searchTimer = 0;
		if (contacts === null || dead)
			return;
		const raw = search.value.trim();
		const q = raw.toLowerCase();
		let n = 0;
		for (const r of listEl.children) {
			const on = matches(r.wmContact, q);
			r.hidden = !on;
			if (on)
				n++;
		}
		clear(stateEl);
		listEl.hidden = n === 0;
		if (contacts.length === 0) {
			stateEl.append(h("div", { class: "empty" },
				h("span", { class: "badge" }, icon("users")),
				h("p", { class: "lead", text: t("contacts.label_none") }),
				h("p", { text: t("contacts.label_none_help") }),
				h("button", { class: "btn", type: "button", text: t("contacts.button_add"),
					onclick: (e) => edit(null, e.currentTarget) })));
		} else if (n === 0) {
			stateEl.append(h("div", { class: "empty" },
				h("span", { class: "badge" }, icon("search")),
				h("p", { class: "lead", text: t("contacts.label_no_results", { query: raw }) }),
				h("button", { class: "btn", type: "button", text: t("list.button_clear_search"), onclick: () => {
					search.value = "";
					filter();
					search.focus();
				} })));
		}
		const text = q === "" ? t("contacts.label_count", { n, count: number(n) })
			: t("contacts.label_matches", { n, count: number(n) });
		if (live.textContent !== text)
			live.textContent = text;
	}

	function row(c) {
		const name = shownName(c);
		const writeBtn = h("button", { class: "btn cwrite", type: "button", "data-act": "write",
			"aria-label": t("contacts.button_write_to_name", { name }), title: t("contacts.button_write_to"),
			"aria-haspopup": c.addrs.length > 1 ? "dialog" : null,
			onclick: (e) => writeTo(c, e.currentTarget) },
			icon("send"), h("span", { class: "cw-label", text: t("contacts.button_write_to") }));
		const editBtn = h("button", { class: "ib", type: "button", "data-act": "edit",
			"aria-label": t("contacts.button_edit_name", { name }), title: t("contacts.button_edit"),
			onclick: (e) => edit(c, e.currentTarget) }, icon("pencil"));
		const delBtn = h("button", { class: "ib", type: "button", "data-act": "delete",
			"aria-label": t("contacts.button_delete_name", { name }), title: t("contacts.button_delete"),
			onclick: (e) => remove(c, e.currentTarget) }, icon("trash"));
		const info = h("div", { class: "cinfo" },
			h("h3", { class: "cname ell", text: name }),
			h("ul", { class: "caddrs", "aria-label": t("contacts.label_addresses") },
				c.addrs.map((a) => h("li", { class: "ell", text: a }))),
			c.note !== "" ? h("p", { class: "cnote", text: c.note }) : null);
		return h("li", { class: "crow", "data-id": c.id },
			avatarBadge(null, initial(name), "avatar"), info,
			h("div", { class: "cacts" }, writeBtn, editBtn, delBtn));
	}

	// Up and Down: the same button of the next or previous contact
	function onKey(e) {
		if (e.key !== "ArrowDown" && e.key !== "ArrowUp")
			return;
		const b = e.target.closest ? e.target.closest("button[data-act]") : null;
		const li = b ? b.closest(".crow") : null;
		if (!li)
			return;
		let next = li;
		do
			next = e.key === "ArrowDown" ? next.nextElementSibling : next.previousElementSibling;
		while (next && next.hidden);
		if (!next)
			return;
		e.preventDefault();
		const to = next.querySelector('button[data-act="' + b.getAttribute("data-act") + '"]');
		if (to)
			to.focus();
	}

	function focusRow(id, act) {
		const li = listEl.querySelector('.crow[data-id="' + CSS.escape(id) + '"]');
		const b = li ? li.querySelector('button[data-act="' + act + '"]') : null;
		if (b) {
			b.focus();
			return true;
		}
		return false;
	}

	// ---- Write to ----

	function writeTo(c, anchor) {
		if (c.addrs.length === 1) {
			o.write([entryFor(c.name, c.addrs[0])], anchor);
			return;
		}
		openMenu(anchor, { label: t("contacts.label_write_which"), title: t("contacts.label_write_which"),
			items: c.addrs.map((a) => ({ label: a, icon: "mail", onclick: () => o.write([entryFor(c.name, a)], anchor) })) });
	}

	// ---- Add and edit ----

	function edit(c, opener) {
		const id = "wm-ct-" + Math.random().toString(36).slice(2);
		const field = (key, label, control, hint) => {
			control.id = id + "-" + key;
			const err = h("p", { class: "field-err", id: id + "-" + key + "-err", role: "alert", hidden: true });
			const hintEl = hint ? h("p", { class: "field-hint", id: id + "-" + key + "-hint", text: hint }) : null;
			control.setAttribute("aria-describedby", (hintEl ? hintEl.id + " " : "") + err.id);
			return { control, err, el: h("div", { class: "field" }, h("label", { for: control.id, text: label }), control, hintEl, err) };
		};
		const nameF = field("name", t("contacts.label_name"),
			h("input", { type: "text", autocomplete: "off", spellcheck: "false" }));
		const addrsF = field("addrs", t("contacts.label_addresses"),
			h("textarea", { rows: "3", autocomplete: "off", spellcheck: "false", autocapitalize: "none" }),
			t("contacts.label_addresses_help", { count: MAX_ADDRS }));
		const noteF = field("note", t("contacts.label_note"), h("textarea", { rows: "3" }));
		const general = h("div", { class: "dlg-err", role: "alert", hidden: true });
		if (c) {
			nameF.control.value = c.name;
			addrsF.control.value = c.addrs.join("\n");
			noteF.control.value = c.note;
		}
		let busyNow = false;
		const form = h("form", { class: "dlg-form", novalidate: true, onsubmit: (e) => {
			e.preventDefault();
			save();
		} }, nameF.el, addrsF.el, noteF.el, general);
		const d = openDialog({ title: t(c ? "contacts.label_edit_contact" : "contacts.label_new_contact"), body: [form],
			initial: nameF.control, returnTo: opener,
			buttons: [
				{ label: t("main.button_cancel"), onclick: () => d.close() },
				{ label: t("main.button_save"), primary: true, onclick: () => save() }
			] });

		// Deleted elsewhere meanwhile: the dialog becomes a new contact's
		function becameNew() {
			c = null;
			const title = d.dialog.querySelector(".dlg-head h2");
			if (title)
				title.textContent = t("contacts.label_new_contact");
			const ok = d.dialog.querySelector(".dlg-foot .btn.primary");
			if (ok)
				ok.textContent = t("contacts.button_save_new");
		}

		function showError(f, text) {
			for (const x of [nameF, addrsF, noteF]) {
				x.err.hidden = true;
				x.control.removeAttribute("aria-invalid");
			}
			general.hidden = true;
			if (f) {
				f.err.textContent = text;
				f.err.hidden = false;
				f.control.setAttribute("aria-invalid", "true");
				f.control.focus();
			} else {
				general.textContent = text;
				general.hidden = false;
			}
		}

		function save() {
			if (busyNow)
				return;
			// the addresses and the line each is on (a refusal names it)
			const lines = addrsF.control.value.split(/\r\n|\r|\n/);
			const addrs = [];
			const lineOf = [];
			lines.forEach((a, n) => {
				if (a.trim() !== "") {
					addrs.push(a.trim());
					lineOf.push(n);
				}
			});
			if (addrs.length === 0) {
				showError(addrsF, t("contacts.label_need_address"));
				return;
			}
			if (addrs.length > MAX_ADDRS) {
				showError(addrsF, t("contacts.label_too_many_addresses", { count: MAX_ADDRS }));
				return;
			}
			const args = { name: nameF.control.value.trim(), addrs, note: noteF.control.value.trim() };
			if (c)
				args.id = c.id;
			busyNow = true;
			rpc.call("contact.set", args).then((r) => {
				busyNow = false;
				const saved = clean(r.contact);
				if (dead || !saved)
					return;
				if (contacts === null)
					contacts = [];
				const i = contacts.findIndex((x) => x.id === saved.id);
				if (i >= 0)
					contacts[i] = saved;
				else
					contacts.push(saved);
				contacts.sort(byName);
				// a new contact the search would hide is shown
				if (!matches(saved, search.value.trim().toLowerCase()))
					search.value = "";
				render();
				d.close("keep-focus");
				if (!focusRow(saved.id, "edit"))
					addBtn.focus();
				// the server shortens a long name or note: say what was kept
				if (saved.name !== args.name)
					notice(t("contacts.label_saved_as", { name: saved.name }));
				else if (saved.note !== args.note)
					notice(t("contacts.label_note_cut"));
				else
					notice(t("contacts.label_added"));
			}, (e) => {
				busyNow = false;
				if (dead)
					return;
				if (e && e.error === "notfound" && c) {
					becameNew();
					showError(null, t("contacts.label_gone"));
					return;
				}
				const reason = e && e.reason;
				const f = reason === "name" ? nameF : (reason === "note" ? noteF : (reason === "addrs" ? addrsF : null));
				const text = e && e.message ? e.message : t("errors.failed");
				if (f === addrsF && typeof e.index === "number" && e.index >= 0 && e.index < lineOf.length) {
					// the line the server refused, named and selected
					const n = lineOf[e.index];
					showError(addrsF, t("contacts.label_bad_line", { line: n + 1, reason: text }));
					const start = lines.slice(0, n).reduce((x, l) => x + l.length + 1, 0);
					addrsF.control.setSelectionRange(start, start + lines[n].length);
					return;
				}
				showError(f, text);
			});
		}
	}

	// ---- Delete ----

	async function remove(c, opener) {
		const name = shownName(c);
		const yes = await confirmDialog({ title: t("contacts.button_delete"),
			text: t("contacts.label_delete_confirm", { name }), ok: t("contacts.button_delete"), danger: true });
		if (!yes || dead)
			return;
		// the contact to move to afterwards: the next one shown, else the
		// one before
		const li = opener.closest(".crow");
		const shownSibling = (dir) => {
			let x = li;
			do
				x = dir > 0 ? x.nextElementSibling : x.previousElementSibling;
			while (x && x.hidden);
			return x;
		};
		const near = li ? shownSibling(1) || shownSibling(-1) : null;
		const nextId = near ? near.getAttribute("data-id") : null;
		rpc.call("contact.delete", { id: c.id }).then(() => {
			if (dead)
				return;
			contacts = contacts.filter((x) => x.id !== c.id);
			render();
			if (!(nextId && focusRow(nextId, "delete")))
				(search.value !== "" ? search : addBtn).focus();
			notice(t("contacts.label_removed"));
		}, (e) => {
			if (dead)
				return;
			if (e && e.error === "notfound") {
				// already gone
				contacts = contacts.filter((x) => x.id !== c.id);
				render();
				search.focus();
			}
			notice(e && e.message ? e.message : t("errors.failed"));
		});
	}

	load();

	return {
		el,
		focus: () => search.focus(),
		destroy: () => {
			dead = true;
			clearTimeout(searchTimer);
		}
	};
}
