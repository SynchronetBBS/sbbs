// The Drafts folder: the user's drafts (draft.list), newest first, each a
// button that opens it in compose.
//
// const drafts = createDraftList({ onOpen, onDelete, kept, onKept, onDropKept })
//   onOpen(row, button): a draft was chosen (row: its draft.list row)
//   onDelete(row, button): its Delete button was pressed
//   kept(): text kept in this tab that was not saved ([{key, subject,
//   to, at}], compose.stashes()), listed first, marked "Not saved";
//   onKept(k) opens one in compose, onDropKept(k) removes it
// drafts.el              the element to put in the list pane
// drafts.load(background)
//                        lists the drafts again.  Not background when the
//                        user opens the folder: then the server also
//                        removes stale empty drafts and completes
//                        interrupted sends (a background draft.list only
//                        lists).  A Promise of the rows, or null.
// drafts.focus()         focus on the first draft (or the list)
// drafts.count()         the number of drafts listed
//
// Keyboard: Up and Down move between the drafts, Home and End to the first
// and last; Enter or Space opens one.  A draft whose send may have gone
// out (maybe_sent) is marked "May have been sent".

import { h, clear } from "./dom.js";
import { icon } from "./icons.js";
import { t } from "./i18n.js";
import { rpc } from "./rpc.js";
import { busy } from "./notice.js";
import { listDate, fullDate } from "./format.js";

export function createDraftList(o) {
	const ul = h("ul", { class: "dlist", "aria-labelledby": "list-title" });
	const state = h("div", { class: "list-state", hidden: true });
	const el = h("div", { class: "drafts-view", onkeydown: onKey }, ul, state);
	let rows = [];
	let seq = 0;

	function names(r) {
		const all = [r.to, r.cc, r.bcc].filter((x) => typeof x === "string" && x.trim() !== "").join(", ");
		return all;
	}

	function row(r) {
		const to = names(r);
		const subj = typeof r.subject === "string" && r.subject !== "" ? r.subject : t("list.label_no_subject");
		const marks = h("span", { class: "marks" });
		if (r.maybe_sent)
			marks.append(h("span", { class: "tag danger", text: t("compose.label_maybe_sent_tag") }));
		if (r.attachments > 0)
			marks.append(h("span", { class: "mark", title: t("list.label_attachment") }, icon("clip", "sm"),
				h("span", { class: "vh", text: ", " + t("list.label_attachment") })));
		const b = h("button", { class: "row-main draft-row", type: "button", "data-draft": r.id,
			onclick: (e) => o.onOpen(r, e.currentTarget) },
		h("span", { class: "r1" },
			h("span", { class: "from ell", text: to !== "" ? t("list.label_to_name", { name: to })
				: t("compose.label_no_recipients") }),
			marks,
			h("span", { class: "date", title: fullDate(r.updated), text: listDate(r.updated) })),
		h("span", { class: "subj ell", text: subj }));
		const label = t("compose.button_delete_draft_name", { name: subj });
		const del = h("button", { class: "ib", type: "button", "aria-label": label, title: label,
			onclick: (e) => o.onDelete(r, e.currentTarget) }, icon("trash"));
		return h("li", { class: "drow" }, b, del);
	}

	function keptRow(k) {
		const subj = k.subject !== "" ? k.subject : t("list.label_no_subject");
		const b = h("button", { class: "row-main draft-row", type: "button", "data-draft": "kept:" + k.key,
			onclick: () => o.onKept(k) },
		h("span", { class: "r1" },
			h("span", { class: "from ell", text: k.to !== "" ? t("list.label_to_name", { name: k.to })
				: t("compose.label_no_recipients") }),
			h("span", { class: "marks" }, h("span", { class: "tag danger", text: t("compose.label_not_saved_tag") }))),
		h("span", { class: "subj ell", text: subj }));
		const label = t("compose.button_delete_draft_name", { name: subj });
		return h("li", { class: "drow" }, b, h("button", { class: "ib", type: "button", "aria-label": label, title: label,
			onclick: () => o.onDropKept(k) }, icon("trash")));
	}

	function render(list) {
		const had = el.contains(document.activeElement) ? document.activeElement.getAttribute("data-draft") : null;
		rows = list;
		clear(ul);
		const kept = typeof o.kept === "function" ? o.kept() : [];
		for (const k of kept)
			ul.append(keptRow(k));
		for (const r of list)
			ul.append(row(r));
		state.hidden = list.length + kept.length > 0;
		clear(state);
		if (list.length + kept.length === 0) {
			state.append(h("div", { class: "empty" },
				h("span", { class: "badge" }, icon("file")),
				h("p", { class: "lead", text: t("compose.label_no_drafts") })));
		}
		if (had) {
			const b = ul.querySelector('[data-draft="' + CSS.escape(had) + '"]') || ul.querySelector(".draft-row");
			if (b)
				b.focus({ preventScroll: true });
		}
	}

	function load(background) {
		const my = ++seq;
		const p = rpc.call("draft.list", null, { background: background === true, retry: background !== true })
			.then((r) => {
				if (my !== seq)
					return null;
				const list = Array.isArray(r.drafts) ? r.drafts.filter((x) => x && typeof x.id === "string") : [];
				render(list);
				return list;
			}, (e) => {
				if (my !== seq || background)
					return null;
				clear(ul);
				clear(state);
				state.hidden = false;
				state.append(h("div", { class: "empty failed", role: "alert" },
					h("span", { class: "badge" }, icon("alert")),
					h("p", { class: "lead", text: t("list.label_load_failed") }),
					e && e.message ? h("p", { text: e.message }) : null,
					h("button", { class: "btn", type: "button", text: t("list.button_try_again"), onclick: () => load(false) })));
				return null;
			});
		if (!background)
			busy(state.hidden ? ul : state, p);
		return p;
	}

	function focus() {
		const b = ul.querySelector(".draft-row");
		if (b)
			b.focus({ preventScroll: true });
		else {
			const x = state.querySelector("button");
			if (x)
				x.focus({ preventScroll: true });
		}
	}

	function onKey(e) {
		if (!e.target.classList || !e.target.classList.contains("draft-row"))
			return;
		const all = Array.from(ul.querySelectorAll(".draft-row"));
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

	return { el, load, focus, count: () => rows.length };
}
