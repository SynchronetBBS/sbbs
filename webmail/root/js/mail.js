// The mail view's folders, message list and reader: which folder or search
// is shown, the bars above the list, opening messages, and checking for
// changes.
//
// const mail = createMail({ layout, folderPane, listPane, readerPane, listBar, listTitle, pollMs,
//                           limits, user, host })
//   folderPane, listPane, readerPane: the panes to fill (after their bars);
//   listBar: the list pane's bar (a Refresh button is added to it);
//   listTitle: the list pane's heading.  poweredBy: the system's version
//   notice, shown at the bottom of the folder pane.  The page title shows the folder
//   (and the Inbox's unread count) before the shell's title.  limits, user:
//   session.get's, for compose; host(): the element compose goes in.
//   onContacts(opener), onSettings(opener): the Contacts and Settings
//   entries at the bottom of the folder pane were chosen; onNavigate(): a
//   folder or a search was chosen (the contacts or settings view, when
//   shown, makes way for the mail).
//
// Writing: Compose (at the top of the folder pane; a floating button over
// the list on a phone), and the reader's Reply, Reply all and Forward,
// open compose.js.  The Drafts folder (folder.list's drafts count) lists
// the drafts (draftlist.js) in place of the message list; choosing one
// opens it in compose.  After a send the folders and the list are loaded
// again.
// mail.search      the top bar's search form (createSearch)
// mail.start()     loads the folders and the Inbox and starts checking
// mail.setAux(name) marks the Contacts or Settings entry as the view shown
//                  ("contacts", "settings"; null: the mail again)
// mail.usage()     folder.list's usage ({bytes, quota}), or null
// mail.destroy()
//
// Changes: a poll (rpc "changes", sent alone so the server does not count
// it as activity, and not retried: a failed poll waits for the next one)
// runs every pollMs while the page is visible.  Whenever a reply carries
// a state token other than the one the folders and list were last loaded
// at (a poll's or any other's), the folder list and the list's first page
// are loaded again and merged: one refresh at a time, and another only
// for a token newer than the one the last refresh brought.  Such a
// refresh is marked background, so it does not keep the session alive
// either.  When the Inbox's total rose, the new mail is announced in a
// polite live region.
//
// Reading: choosing a row (click, Enter) opens it in the reader (on a
// phone the reader pane is shown, with a history entry); Enter also moves
// focus to the message.  On wide screens, moving through the list with
// the keys while a message is open opens each message the selection
// stops on.  Escape in the reader returns focus to the list.  Opening an
// unread message marks its row read and lowers the unread counts at once;
// if the server then fails to mark it, both are put back.

import { h, clear } from "./dom.js";
import { icon } from "./icons.js";
import { t } from "./i18n.js";
import { rpc } from "./rpc.js";
import { notice, noticeAction, setNoticeHome } from "./notice.js";
import { createFolders, folderName } from "./folders.js";
import { createList } from "./list.js";
import { createSearch } from "./search.js";
import { createReader } from "./reader.js";
import { poweredBy } from "./powered.js";
import { createPoller } from "./poll.js";
import { number } from "./format.js";
import { createActions } from "./actions.js";
import { openMenu, promptDialog, confirmDialog, openDialog } from "./dialog.js";
import { partUrl } from "./attachments.js";
import { createCompose } from "./compose.js";
import { createDraftList } from "./draftlist.js";

const MIN_POLL = 15000;
const DEFAULT_POLL = 60000;

export function createMail(o) {
	const { layout, folderPane, listPane, listBar, listTitle } = o;
	let data = null;		// the last folder.list result
	let folder = "inbox";
	let search = null;		// { query, scope: "folder" | "all" }
	let seen = null;		// the state token the folders and list were loaded at
	let refreshing = null;
	let inboxTotal = null;
	let inboxTop = null;		// the newest Inbox message's id, as last seen
	const restoredHere = new Set();	// ids put (back) into the Inbox here
	let poller = null;
	let dead = false;		// destroyed: late replies change nothing
	let writes = 0;			// writes under way: refreshes wait for them
	let draftsLoad = null;		// the Drafts folder's last load (not in the background)
	const pageTitle = document.title;

	// a folder the user chose (not one opened by the app itself)
	const folders = createFolders((id) => {
		navigate();
		openFolder(id);
	}, { onNew: newFolder });
	const composeLabel = t("main.label_compose");
	const composeBtn = h("button", { class: "btn primary compose-btn", type: "button",
		onclick: (e) => compose.open({ mode: "new", opener: e.currentTarget }) }, icon("pencil"), h("span", { text: composeLabel }));
	// Contacts and Settings, below the folders
	const auxBtn = (name, label, ic, f) => h("button", { class: "fbtn", type: "button", "data-aux": name,
		onclick: (e) => {
			if (typeof f === "function")
				f(e.currentTarget);
		} }, icon(ic), h("span", { class: "name ell", text: label }));
	const auxBtns = [auxBtn("contacts", t("contacts.label_contacts"), "users", o.onContacts),
		auxBtn("settings", t("settings.label_settings"), "sliders", o.onSettings)];
	const auxNav = h("ul", { class: "fnav aux-nav" }, auxBtns.map((b) => h("li", null, b)));
	auxNav.addEventListener("keydown", (e) => {
		const i = auxBtns.indexOf(e.target);
		if (i < 0 || (e.key !== "ArrowDown" && e.key !== "ArrowUp"))
			return;
		const j = i + (e.key === "ArrowDown" ? 1 : -1);
		if (j >= 0 && j < auxBtns.length) {
			e.preventDefault();
			auxBtns[j].focus();
		}
	});
	let aux = null;
	folderPane.append(composeBtn, folders.el, auxNav);
	const powered = poweredBy(o.poweredBy);
	if (powered)
		folderPane.append(powered);

	const search_ = createSearch(runSearch);

	const live = h("div", { class: "vh", role: "status", "aria-live": "polite" });
	const folderNoteText = h("span", { class: "grow" });
	const emptyTrashBtn = h("button", { class: "linkbtn", type: "button", text: t("folders.button_empty_trash"),
		hidden: true, onclick: () => {
			const f = find("trash");
			const n = f && typeof f.total === "number" ? f.total : 0;
			if (n > 0)
				acts.emptyTrash(n);
		} });
	const folderNote = h("div", { class: "folder-note", hidden: true }, folderNoteText, emptyTrashBtn);
	const resultText = h("span", { class: "grow ell" });
	const scopeFolder = h("button", { type: "button", onclick: () => setScope("folder") });
	const scopeAll = h("button", { type: "button", text: t("folders.all_mail"), onclick: () => setScope("all") });
	const searchInfo = h("div", { class: "searchinfo", hidden: true },
		h("div", { class: "si-top" },
			resultText,
			h("button", { class: "linkbtn", type: "button", text: t("list.button_clear_search"),
				onclick: () => clearSearch(true) })),
		h("div", { class: "seg scope", role: "group", "aria-label": t("list.label_search_in") }, scopeFolder, scopeAll));
	// Checking rows: the select-all box (in whichever bar is shown) and the
	// bar of actions on the checked rows, shown instead of the list's bar
	const selAll = h("input", { type: "checkbox", class: "chk", "aria-label": t("list.label_select_all"),
		onclick: () => list.checkAll(list.checkState().count === 0 || selAll.indeterminate) });
	const selBox = h("span", { class: "selall" }, selAll);
	const bulkCount = h("span", { class: "pane-title bulk-count", role: "status" });
	// The list pane's width decides which actions are in the bar and which
	// in More (style.css: bulk-x, bulk-mid, bulk-wide), so the count is
	// never cut short
	const clearBtn = h("button", { class: "ib bulk-x", type: "button", "aria-label": t("list.button_clear_selection"),
		title: t("list.button_clear_selection"), onclick: () => {
			list.clearChecked();
			selAll.focus();
		} }, icon("x"));
	const bulkBar = h("div", { class: "pane-bar bulk", hidden: true }, clearBtn, bulkCount);
	// The actions on the checked rows; which are shown depends on the folder
	const bulkBtn = (name, label, ic, onclick, cls) => {
		const b = h("button", { class: "ib" + (cls ? " " + cls : ""), type: "button", "aria-label": label, title: label,
			"data-act": name, onclick }, icon(ic));
		bulkBar.append(b);
		return b;
	};
	const bulk = {
		del: bulkBtn("delete", t("list.button_delete"), "trash", () => onChecked((e, go) => acts.remove(e, go), true)),
		restore: bulkBtn("restore", t("list.button_restore"), "restore", () => onChecked((e) => acts.restore(e)), "bulk-x"),
		move: bulkBtn("move", t("list.button_move"), "move", (ev) => moveMenu(ev.currentTarget, () => list.checked(), true),
			"bulk-mid"),
		read: bulkBtn("read", t("list.button_mark_read"), "mailopen", () => onChecked((e) => acts.read(e, true)), "bulk-wide"),
		unread: bulkBtn("unread", t("list.button_mark_unread"), "mail", () => onChecked((e) => acts.read(e, false)),
			"bulk-wide"),
		more: bulkBtn("more", t("list.label_more_actions"), "more", (ev) => bulkMore(ev.currentTarget))
	};
	bulk.move.setAttribute("aria-haspopup", "dialog");
	bulk.more.setAttribute("aria-haspopup", "dialog");
	const titleText = document.createTextNode(t("list.label_messages"));
	const titleCount = h("small", { hidden: true });
	clear(listTitle);
	listTitle.append(titleText, titleCount);
	listTitle.before(selBox);
	listBar.after(bulkBar);
	listBar.append(h("button", {
		class: "ib", type: "button", "aria-label": t("main.label_refresh"), title: t("main.label_refresh"),
		onclick: () => refreshAll(false)
	}, icon("refresh")));
	// A user folder's options (rename, delete)
	const folderMenuBtn = h("button", { class: "ib", type: "button", "aria-label": t("folders.label_folder_options"),
		title: t("folders.label_folder_options"), "aria-haspopup": "dialog", hidden: true,
		onclick: (e) => folderMenu(e.currentTarget) }, icon("more"));
	listBar.append(folderMenuBtn);

	const list = createList({
		labelledBy: listTitle.id,
		fetch: fetchPage,
		empty: emptyState,
		failed: failedState,
		folderTag,
		onTotal: (view) => {
			barText(view);
			checkedBar();
			navState();
		},
		onChecked: checkedBar,
		onStar: (it) => acts.star([entry(it)], it.starred !== true),
		onNotice: openNotice,
		onRemap: (from, to) => reader.remap(from, to),
		onKey: (name, it) => {
			if (name === "undo") {
				noticeAction();
				return;
			}
			const sel = list.checked();
			const entries = sel.length > 0 ? sel : (it ? [entry(it)] : []);
			if (entries.length === 0)
				return;
			if (name === "delete")
				acts.remove(entries, finish);
			else if (name === "star" && can("star")) {
				const on = !entries.every((e) => (list.near(e.id, 0) || {}).starred === true);
				acts.star(entries, on);
				finish();
			}
		},
		onSelect: (it, opening) => {
			if (!opening && it && !layout.isPhone() && reader.current() !== null && reader.current() !== it.id)
				openMessage(it, { delay: 250 });
		},
		onOpen: (it, how) => {
			// the header is in place before the pane shows, so focus can
			// go to it on a phone
			openMessage(it, { focus: how === "key" });
			if (layout.isPhone())
				layout.show("reader", { push: true, opener: list.el.querySelector(".list-scroll") });
		}
	});
	const drafts = createDraftList({ onOpen: (r, b) => compose.openDraft(r.id, { opener: b, maybeSent: !!r.maybe_sent }),
		onDelete: deleteDraft,
		kept: () => compose.stashes(),
		onKept: (k) => compose.restoreKept(k.key),
		onDropKept: (k) => {
			compose.dropKept(k.key);
			drafts.load(true).then(() => drafts.focus());
		} });
	drafts.el.hidden = true;
	const fab = h("button", { class: "btn primary fab", type: "button",
		onclick: (e) => compose.open({ mode: "new", opener: e.currentTarget }) }, icon("pencil"), h("span", { text: composeLabel }));
	listPane.append(searchInfo, folderNote, list.el, drafts.el, live, fab);

	// ---- The reader's toolbar ----

	const tbtn = (name, label, ic, onclick, cls, labelled) => h("button", {
		class: "tbtn" + (cls ? " " + cls : ""), type: "button", "aria-label": label, title: label, "data-act": name,
		onclick: onclick ? (e) => onclick(e.currentTarget) : null, disabled: !onclick
	}, icon(ic), labelled ? h("span", { class: "tb-label", text: label }) : null);
	// Reply, Reply all and Forward open compose with the message shown
	const write = (mode) => (b) => {
		const it = reader.item();
		if (it && it.extra !== true)
			compose.open({ mode, item: it, msg: reader.message(), opener: b });
	};
	const tool = {
		reply: tbtn("reply", t("reader.button_reply"), "reply", write("reply"), "", true),
		replyAll: tbtn("replyall", t("reader.button_reply_all"), "replyall", write("replyall"), "tb-wide", true),
		forward: tbtn("forward", t("reader.button_forward"), "forward", write("forward"), "tb-wide", true),
		del: tbtn("delete", t("list.button_delete"), "trash", () => onReader((e) => acts.remove(e))),
		restore: tbtn("restore", t("list.button_restore"), "restore", () => onReader((e) => acts.restore(e))),
		move: tbtn("move", t("list.button_move"), "move", (b) => moveMenu(b, readerEntries)),
		star: tbtn("star", t("list.button_star"), "star", () => {
			const it = reader.item();
			if (it)
				acts.star(readerEntries(), it.starred !== true);
		}),
		unread: tbtn("unread", t("list.button_mark_unread"), "mail", () => onReader((e) => acts.read(e, false)), "tb-wide"),
		spam: tbtn("spam", t("list.button_mark_spam"), "spam", () => {
			const it = reader.item();
			if (it)
				onReader((e) => acts.spam(e, it.folder !== "spam"));
		}, "tb-wide"),
		more: tbtn("more", t("main.button_more"), "more", (b) => readerMore(b)),
		prev: tbtn("prev", t("reader.button_previous"), "up", () => stepMessage(-1, false), "dt-only"),
		next: tbtn("next", t("reader.button_next"), "down", () => stepMessage(1, false), "dt-only")
	};
	tool.move.setAttribute("aria-haspopup", "dialog");
	tool.more.setAttribute("aria-haspopup", "dialog");
	const tools = h("div", { class: "rtb", role: "group", "aria-label": t("reader.label_message_actions"), hidden: true },
		tool.reply, tool.replyAll, tool.forward, h("span", { class: "sep", "aria-hidden": "true" }),
		tool.del, tool.restore, tool.move, tool.star, tool.unread, tool.spam, h("span", { class: "spacer" }),
		tool.prev, tool.next, tool.more);
	if (o.readerBar)
		o.readerBar.append(tools);

	// ---- From the message shown to the one before or after it ----

	// On a phone the list is out of sight while a message is read: a bar
	// under the message goes to its neighbors in the list, and says where
	// it is.  On a wide screen the toolbar has the two buttons.  j and k
	// do the same in the reader, as in other mail programs.
	const nbtn = (label, ic, delta, lead) => h("button", { class: "btn nav-btn", type: "button",
		onclick: () => stepMessage(delta, false) },
		lead ? icon(ic) : null, h("span", { text: label }), lead ? null : icon(ic));
	const navPrev = nbtn(t("reader.button_previous_short"), "up", -1, true);
	const navNext = nbtn(t("reader.button_next_short"), "down", 1, false);
	navPrev.setAttribute("aria-label", t("reader.button_previous"));
	navNext.setAttribute("aria-label", t("reader.button_next"));
	const navPos = h("span", { class: "nav-pos", "aria-live": "off" });
	const readerNav = h("div", { class: "reader-nav", role: "group", "aria-label": t("reader.label_message_nav"), hidden: true },
		navPrev, navPos, navNext);
	let stepping = false;

	// The buttons are aria-disabled, not disabled, so one that was just
	// used keeps the focus at the end of the list
	function navState() {
		const id = reader.current();
		const w = id ? list.where(id) : { index: -1, total: 0 };
		const off = (b, on) => {
			b.setAttribute("aria-disabled", String(on));
			b.classList.toggle("off", on);
		};
		const none = w.index < 0;
		for (const b of [tool.prev, navPrev])
			off(b, none || w.index <= 0);
		for (const b of [tool.next, navNext])
			off(b, none || w.index >= w.total - 1);
		readerNav.hidden = !id || none || w.total < 2;
		navPos.textContent = none ? "" : t("reader.label_position", { n: w.index + 1, total: w.total });
	}

	function stepMessage(delta, focusMessage) {
		const id = reader.current();
		if (!id || stepping)
			return;
		const w = list.where(id);
		if (w.index < 0 || w.index + delta < 0 || w.index + delta >= w.total)
			return;
		stepping = true;
		// a row not loaded yet is fetched; no answer in time is no move
		const timeout = new Promise((done) => setTimeout(() => done(null), 8000));
		Promise.race([list.step(id, delta), timeout]).then((it) => {
			stepping = false;
			if (!it || dead || reader.current() !== id)
				return;
			list.reveal(it.id);
			openMessage(it, { focus: focusMessage });
		}, () => {
			stepping = false;
		});
	}

	function readerEntries() {
		const it = reader.item();
		return it ? [entry(it)] : [];
	}

	function onReader(f) {
		const e = readerEntries();
		if (e.length > 0)
			f(e);
	}

	// The toolbar for the message shown (null: none)
	function toolbar(it) {
		tools.hidden = !it;
		navState();
		if (!it)
			return;
		const f = typeof it.folder === "string" ? it.folder : "";
		const trash = f === "trash";
		const label = t(trash ? "list.button_delete_forever" : "list.button_delete");
		tool.del.setAttribute("aria-label", label);
		tool.del.title = label;
		tool.del.classList.toggle("danger", trash);
		tool.restore.hidden = !trash;
		tool.move.hidden = f === "sent" || it.extra === true;
		tool.star.hidden = trash || f === "spam" || it.extra === true;
		tool.star.setAttribute("aria-pressed", String(it.starred === true));
		tool.spam.hidden = f !== "inbox" && f !== "spam";
		// no reply from Spam, nor to a delivery report shown on its own
		// ... and only once the message has loaded (the quote is made from it)
		for (const b of [tool.reply, tool.replyAll, tool.forward])
			b.disabled = f === "spam" || it.extra === true || !reader.message();
		const sl = t(f === "spam" ? "list.button_not_spam" : "list.button_mark_spam");
		tool.spam.setAttribute("aria-label", sl);
		tool.spam.title = sl;
		clear(tool.spam);
		tool.spam.append(icon(f === "spam" ? "inbox" : "spam"));
	}

	// More: the toolbar's buttons that do not fit (narrow reader), then
	// Show original
	function readerMore(anchor) {
		const it = reader.item();
		if (!it)
			return;
		const fits = (b) => !b.hidden && b.getClientRects().length > 0;
		const items = [];
		for (const b of [tool.replyAll, tool.forward, tool.unread, tool.spam]) {
			if (!b.hidden && !fits(b))
				items.push({ label: b.getAttribute("aria-label"), icon: b.getAttribute("data-act") === "spam" && it.folder === "spam"
					? "inbox" : b.getAttribute("data-act"), disabled: b.disabled, onclick: () => b.click() });
		}
		items.push({ label: t("reader.button_show_original"), icon: "code", onclick: () => showOriginal(it, anchor) });
		openMenu(anchor, { label: t("main.button_more"), items });
	}

	// The message as stored: its header block and the stored details, as
	// text in a fixed-width font, with a download of the whole message
	function showOriginal(it, opener) {
		const head = h("pre", { class: "orig", text: t("reader.label_loading") });
		const stored = h("pre", { class: "orig" });
		const filesNote = h("p", { class: "dlg-text muted", hidden: true, text: t("reader.label_source_files") });
		const storedHead = h("h3", { class: "orig-head", text: t("reader.label_source_stored"), hidden: true });
		const dl = partUrl(it.id, "raw", true);
		const copy = () => {
			const text = head.textContent + (storedHead.hidden ? "" : "\n\n" + stored.textContent);
			// the same text through a field, where the browser can copy it
			const fallback = () => {
				const area = h("textarea", { class: "vh", readonly: true, "aria-hidden": "true" });
				area.value = text;
				head.parentNode.append(area);
				area.select();
				let ok = false;
				try {
					ok = document.execCommand("copy");
				} catch (e) {
					ok = false;
				}
				area.remove();
				if (!ok)
					window.getSelection().selectAllChildren(head.parentNode);
				notice(t(ok ? "reader.label_copied" : "reader.label_copy_select"));
			};
			if (navigator.clipboard && typeof navigator.clipboard.writeText === "function")
				navigator.clipboard.writeText(text).then(() => notice(t("reader.label_copied")), fallback);
			else
				fallback();
		};
		const buttons = [{ label: t("reader.button_copy"), onclick: copy }];
		if (dl)
			buttons.unshift({ label: t("reader.button_download_eml"), href: dl });
		openDialog({ title: t("reader.label_source"), className: "wide", returnTo: opener, buttons,
			body: [h("div", { class: "orig-box" }, h("h3", { class: "orig-head", text: t("reader.label_source_header") }), head,
				storedHead, stored), filesNote] });
		rpc.call("message.original", { id: it.id }).then((r) => {
			const o2 = r.original || {};
			head.textContent = typeof o2.rfc822 === "string" ? o2.rfc822.replace(/\r\n/g, "\n") : "";
			const lines = Array.isArray(o2.stored) ? o2.stored.join("\n") : (typeof o2.stored === "string" ? o2.stored : "");
			stored.textContent = lines;
			storedHead.hidden = lines === "";
			stored.hidden = lines === "";
			filesNote.hidden = o2.external_files !== true;
		}, (e) => {
			head.textContent = e && e.message ? e.message : t("errors.failed");
		});
	}

	const reader = createReader({
		pane: o.readerPane,
		nextOf: (id) => list.near(id, 1),
		onRead: markRead,
		onShow: toolbar,
		onItem: (id, fields) => list.update(id, fields),
		inboxDays: () => (data && typeof data.inbox_days === "number" ? data.inbox_days : 0),
		onEscape: () => {
			if (layout.isPhone())
				layout.back();
			else
				list.focus();
		}
	});

	function openMessage(it, how) {
		list.setOpen(it.id);
		reader.open(it, how);
	}

	o.readerPane.append(readerNav);
	o.readerPane.addEventListener("keydown", (e) => {
		if ((e.key !== "j" && e.key !== "k") || e.defaultPrevented || e.altKey || e.ctrlKey || e.metaKey)
			return;
		const tag = e.target && e.target.tagName;
		if (tag === "INPUT" || tag === "TEXTAREA" || tag === "SELECT" || (e.target && e.target.isContentEditable))
			return;
		e.preventDefault();
		stepMessage(e.key === "j" ? 1 : -1, true);
	});

	function closeReader() {
		const inReader = o.readerPane.contains(document.activeElement);
		reader.close();
		list.setOpen(null);
		if (layout.isPhone() && layout.current() === "reader")
			layout.back();
		else if (inReader)
			list.focus();
	}

	// ---- Actions ----

	const acts = createActions({
		list,
		view: viewFolder,
		nameOf,
		begin: () => {
			writes++;
			list.hold(true);
		},
		end: (unsure) => {
			writes = Math.max(0, writes - 1);
			list.hold(false);
			if (writes > 0 || dead)
				return;
			// counts and rows as the server has them now; after a write
			// that may or may not have been done, one that waits for the
			// connection (and shows that it does)
			refreshAll(!unsure);
		},
		closeIf: (ids) => {
			const cur = reader.current();
			if (cur !== null && ids.includes(cur))
				closeReader();
		},
		restored: (ids) => {
			for (const id of ids)
				restoredHere.add(id);
		}
	});
	acts.patch = (id, fields) => reader.patch(id, fields);

	// The folder the list shows ("" for a search of all mail)
	function viewFolder() {
		return search && search.scope === "all" ? "" : folder;
	}

	function entry(it) {
		return { id: it.id, folder: typeof it.folder === "string" ? it.folder : viewFolder() };
	}

	// What can be done in the folder shown
	function can(what) {
		const f = viewFolder();
		switch (what) {
			case "move":
				return f !== "sent";
			case "restore":
				return f === "trash";
			case "star":
				return f !== "spam" && f !== "trash";
			case "spam":
				return f === "inbox";
			case "notspam":
				return f === "spam";
		}
		return true;
	}

	// After an action on the checked rows: they are no longer checked; focus
	// that was in the bulk bar goes to the list
	function finish() {
		const inBar = bulkBar.contains(document.activeElement);
		list.clearChecked();
		if (inBar)
			list.focus();
	}

	// f(entries, go) on the checked rows; asks: f may ask the user first and
	// calls go() when it goes ahead (until then the rows stay checked)
	function onChecked(f, asks) {
		const e = list.checked();
		if (e.length === 0)
			return;
		if (asks)
			f(e, finish);
		else {
			f(e);
			finish();
		}
	}

	// The folders a message can be moved to: Inbox, Archive and the user's,
	// without the folder shown
	function moveTargets() {
		const out = [];
		const here = viewFolder();
		const all = data && Array.isArray(data.folders) ? data.folders : [];
		for (const f of all) {
			if ((f.id === "inbox" || f.id === "archive" || f.kind === "user") && f.id !== here)
				out.push(f);
		}
		return out;
	}

	// entries(): the messages to move (asked when a folder is chosen)
	function moveMenu(anchor, entries, fromBar) {
		openMenu(anchor, {
			label: t("list.label_move_to"), title: t("list.label_move_to"),
			filter: t("list.placeholder_move_filter"),
			items: moveTargets().map((f) => ({
				label: nameOf(f.id),
				icon: f.kind === "user" ? "folder" : (f.id === "inbox" ? "inbox" : "archive"),
				onclick: () => {
					const e = entries();
					if (e.length === 0)
						return;
					acts.move(e, f.id);
					if (fromBar)
						finish();
				}
			}))
		});
	}

	function bulkMore(anchor) {
		// the bar's buttons the pane is too narrow for, then the rest
		const off = (b) => !b.hidden && b.getClientRects().length === 0;
		const items = [];
		if (off(bulk.restore))
			items.push({ label: t("list.button_restore"), icon: "restore", onclick: () => onChecked((e) => acts.restore(e)) });
		if (off(bulk.move))
			items.push({ label: t("list.button_move"), icon: "move", onclick: () => moveMenu(anchor, () => list.checked(), true) });
		if (off(bulk.read))
			items.push({ label: t("list.button_mark_read"), icon: "mailopen", onclick: () => onChecked((e) => acts.read(e, true)) });
		if (off(bulk.unread))
			items.push({ label: t("list.button_mark_unread"), icon: "mail", onclick: () => onChecked((e) => acts.read(e, false)) });
		if (can("star")) {
			items.push({ label: t("list.button_star"), icon: "star", onclick: () => onChecked((e) => acts.star(e, true)) });
			items.push({ label: t("list.button_unstar"), icon: "star", onclick: () => onChecked((e) => acts.star(e, false)) });
		}
		if (can("spam"))
			items.push({ label: t("list.button_mark_spam"), icon: "spam", onclick: () => onChecked((e) => acts.spam(e, true)) });
		if (can("notspam"))
			items.push({ label: t("list.button_not_spam"), icon: "inbox", onclick: () => onChecked((e) => acts.spam(e, false)) });
		if (off(clearBtn))
			items.push({ label: t("list.button_clear_selection"), icon: "x", onclick: () => {
				list.clearChecked();
				selAll.focus();
			} });
		openMenu(anchor, { label: t("list.label_more_actions"), items });
	}

	// The bulk bar's buttons for the folder shown
	function bulkButtons() {
		const trash = viewFolder() === "trash";
		const label = t(trash ? "list.button_delete_forever" : "list.button_delete");
		bulk.del.setAttribute("aria-label", label);
		bulk.del.title = label;
		bulk.del.classList.toggle("danger", trash);
		bulk.restore.hidden = !can("restore");
		bulk.move.hidden = !can("move");
	}

	// ---- The user's folders: create, rename, delete ----

	// The text for a refused folder name
	const REASONS = { empty: "folders.label_reason_empty", invalid: "folders.label_reason_invalid",
		toolong: "folders.label_reason_toolong", reserved: "folders.label_reason_reserved",
		duplicate: "folders.label_reason_duplicate" };
	function nameError(e) {
		if (e && e.error === "invalid" && typeof e.reason === "string" && REASONS[e.reason])
			return t(REASONS[e.reason]);
		return e && e.message ? e.message : t("errors.failed");
	}

	// A folder write: refreshes the folders after it; a Promise of the
	// result, or rejects with the error
	function folderWrite(method, args) {
		writes++;
		let unsure = false;
		return rpc.call(method, args).then((r) => r, (e) => {
			unsure = !!(e && e.unsure);
			throw e;
		}).finally(() => {
			writes = Math.max(0, writes - 1);
			if (writes === 0 && !dead)
				refreshAll(!unsure);
		});
	}

	function newFolder(opener) {
		promptDialog({ title: t("folders.label_new_folder"), label: t("folders.label_folder_name"),
			ok: t("folders.button_create"), returnTo: opener,
			submit: (name) => folderWrite("folder.create", { name }).then((r) => {
				notice(t("folders.label_created", { name: r.folder && typeof r.folder.name === "string" ? r.folder.name : name }));
				return null;
			}, nameError) });
	}

	function folderMenu(anchor) {
		const f = find(folder);
		if (!f || f.kind !== "user")
			return;
		openMenu(anchor, { label: t("folders.label_folder_options"), items: [
			{ label: t("folders.label_rename_folder"), icon: "pencil", onclick: () => renameFolder(f, anchor) },
			{ label: t("folders.label_delete_folder"), icon: "trash", danger: true, onclick: () => deleteFolder(f, anchor) }
		] });
	}

	function renameFolder(f, opener) {
		const old = folderName(f);
		promptDialog({ title: t("folders.label_rename_folder"), label: t("folders.label_folder_name"), value: old,
			ok: t("folders.button_rename"), returnTo: opener,
			submit: (name) => folderWrite("folder.rename", { id: f.id, name }).then((r) => {
				const nf = r.folder || {};
				if (typeof nf.id === "string" && folder === f.id && nf.id !== f.id) {
					// the folder's id is its name: show it under the new id
					folder = nf.id;
					showView();
				}
				notice(t("folders.label_renamed", { name: typeof nf.name === "string" ? nf.name : name }));
				return null;
			}, nameError) });
	}

	async function deleteFolder(f, opener) {
		const n = typeof f.total === "number" ? f.total : 0;
		const name = folderName(f);
		const yes = await confirmDialog({ title: t("folders.label_delete_folder"),
			text: n > 0 ? t("folders.label_delete_confirm", { n, count: number(n), name })
				: t("folders.label_delete_confirm_empty", { name }),
			ok: t("folders.button_delete_folder"), danger: true });
		if (!yes)
			return;
		folderWrite("folder.delete", { id: f.id }).then((r) => {
			const moved = Array.isArray(r.results) ? r.results.filter((x) => x.ok === true).length : 0;
			if (folder === f.id)
				openFolder("inbox");
			notice(moved > 0 ? t("folders.label_folder_deleted_n", { n: moved, count: number(moved), name })
				: t("folders.label_folder_deleted", { name }));
			list.focus();
		}, (e) => notice(e && e.message ? e.message : t("errors.failed")));
	}

	// A delivery report (the notice of a failed Sent message) in the reader
	function openNotice(it) {
		const id = it.delivery && it.delivery.notice;
		if (typeof id !== "string")
			return;
		openMessage({ id, folder: "inbox", read: true, subject: "", from: "" }, { focus: true });
		if (layout.isPhone())
			layout.show("reader", { push: true, opener: list.el.querySelector(".list-scroll") });
	}

	// The bars for the checked rows: the count, the select-all box's state
	function checkedBar() {
		if (dead)
			return;
		const st = list.checkState();
		const on = st.count > 0;
		const had = document.activeElement === selAll;
		selAll.disabled = st.loaded === 0;	// nothing to check
		selAll.checked = on && st.count >= st.loaded;
		selAll.indeterminate = on && st.count < st.loaded;
		bulkCount.textContent = on ? t("list.label_selected", { n: st.count, count: number(st.count) }) : "";
		if (bulkBar.hidden === !on)
			return;
		if (on)
			bulkButtons();
		bulkBar.hidden = !on;
		listBar.hidden = on;
		if (on)
			bulkCount.before(selBox);
		else
			listTitle.before(selBox);
		if (had)
			selAll.focus();
	}

	// ---- Loading ----

	// A view's first load is not retried while the server cannot be
	// reached: the list says so at once and offers Try again
	function fetchPage(view, offset, limit, how) {
		const args = { folder: view.query ? (view.scope === "all" ? "all" : view.folder) : view.folder, offset, limit };
		if (view.query)
			args.search = view.query;
		return rpc.call("message.list", args, { retry: how.first !== true, background: how.background === true }).then((r) => {
			// the newest Inbox message known before any new mail is announced
			if (inboxTop === null && !view.query && view.folder === "inbox" && offset === 0 && r.items && r.items[0])
				inboxTop = r.items[0].id;
			// the rows are in the list after this: the reader's place, again
			setTimeout(() => {
				if (!dead)
					navState();
			}, 0);
			return r;
		});
	}

	function find(id) {
		return data && Array.isArray(data.folders) ? data.folders.find((f) => f.id === id) : null;
	}

	function nameOf(id) {
		const f = find(id);
		if (f)
			return folderName(f);
		return id.startsWith("u:") ? id.slice(2) : t("folders." + id);
	}

	// The folder.list result, or null after a failure (shown to the user
	// unless the call was made in the background)
	function loadFolders(background) {
		return rpc.call("folder.list", null, { background }).then((r) => {
			if (dead)
				return null;
			applyFolders(r);
			return r;
		}, (e) => {
			if (!dead && !background)
				notice(e.message);
			return null;
		});
	}

	function applyFolders(r) {
		// Drafts: a folder of the drafts (count beside the folders)
		if (typeof r.drafts === "number" && Array.isArray(r.folders) && !r.folders.some((f) => f.id === "drafts")) {
			const at = r.folders.findIndex((f) => f.id === "sent");
			r.folders.splice(at >= 0 ? at + 1 : r.folders.length, 0, { id: "drafts", kind: "drafts", total: r.drafts });
		}
		data = r;
		folders.update(r);
		markFolder();
		const inbox = find("inbox");
		if (inbox) {
			// new mail: more messages in the Inbox (a message marked unread
			// again is not new), and a newest one not seen before (not one
			// restored or moved back here)
			const n = inbox.total - (inboxTotal === null ? inbox.total : inboxTotal);
			if (n > 0)
				newMail(n);
			inboxTotal = inbox.total;
		}
		if (!find(folder) && folder !== "inbox") {
			// removed (elsewhere, or emptied: a folder only a message's tag
			// named); focus that was in the list stays in it
			const had = listPane.contains(document.activeElement) || document.activeElement === document.body;
			openFolder("inbox");
			if (had)
				list.focus();
			return;
		}
		bars();
	}

	function newMail(n) {
		rpc.call("message.list", { folder: "inbox", offset: 0, limit: 1 }, { background: true, retry: false }).then((r) => {
			const top = r && Array.isArray(r.items) && r.items[0] ? r.items[0].id : null;
			const was = inboxTop;
			if (dead || top === null)
				return;
			inboxTop = top;
			if (top !== was && !restoredHere.has(top))
				announce(t("main.label_new_mail", { n, count: number(n) }));
		}, () => {});
	}

	function announce(text) {
		live.textContent = "";
		requestAnimationFrame(() => {
			if (!dead)
				live.textContent = text;
		});
	}

	// Load the folders and the list's first page again (one request), one
	// refresh at a time.  The panes then show the state token of the
	// folder.list reply; when a newer token has arrived meanwhile, one more
	// refresh follows.  background: made on its own, not for the user.
	function refreshAll(background) {
		if (writes > 0)
			return Promise.resolve();	// after them: a reply now could undo what shows
		if (refreshing)
			return refreshing;
		refreshing = Promise.all([loadFolders(background),
			folder === "drafts" && !search ? drafts.load(true) : list.refresh(background)]).then(([r]) => {
			refreshing = null;
			if (dead || r === null)
				return;
			const token = rpc.stateOf(r);
			if (typeof token === "string" && token !== "")
				seen = token;
			if (newer())
				return refreshAll(true);
		});
		return refreshing;
	}

	// A token other than the one the panes show has arrived
	function newer() {
		return seen !== null && rpc.state !== "" && rpc.state !== seen;
	}

	function onState() {
		if (!dead && !refreshing && newer())
			refreshAll(true);
	}

	// The reply's state token decides (through the state hook, or here when
	// it arrived while a refresh was running), never this poll's "changed":
	// a waiting poll can be answered with a newer one's reply
	function poll() {
		return rpc.call("changes", { state: seen === null ? "" : seen }, { alone: true, retry: false }).then(() => {
			if (!dead && !refreshing && newer())
				return refreshAll(true);
		});
	}

	// The row of item read (true) or unread again (false), and the unread
	// counts with it; false when it already was
	function markRead(item, read) {
		if (dead || item.extra === true)
			return false;
		const row = list.near(item.id, 0);
		const was = row ? row.read === true : item.read === true;
		item.read = read;
		if (was === read)
			return false;
		if (row)
			list.update(item.id, { read });
		const adjust = (id) => {
			const f = find(id);
			if (f && typeof f.unread === "number")
				f.unread = Math.max(0, f.unread + (read ? -1 : 1));
		};
		adjust(item.folder);
		if (item.starred === true && item.folder !== "starred")
			adjust("starred");
		if (data) {
			folders.update(data);
			markFolder();
		}
		bars();
		return true;
	}

	// ---- Folders and searches ----

	function key() {
		return search ? "s\n" + (search.scope === "all" ? "" : folder) + "\n" + search.query : "f\n" + folder;
	}

	function showView() {
		reader.close();
		list.setOpen(null);
		const inDrafts = folder === "drafts" && !search;
		drafts.el.hidden = !inDrafts;
		list.el.hidden = inDrafts;
		selBox.hidden = inDrafts;
		if (inDrafts) {
			list.clearChecked();
			draftsLoad = drafts.load(false);
			markFolder();
			bars();
			return;
		}
		const props = search ? { folder, query: search.query, scope: search.scope } : { folder, query: "", scope: "" };
		list.show(list.view(key(), props));
		markFolder();
		bars();
	}

	function navigate() {
		if (typeof o.onNavigate === "function")
			o.onNavigate();
	}

	// The contacts or settings view is shown (name) or the mail again (null)
	function setAux(name) {
		aux = name || null;
		for (const b of auxBtns) {
			if (b.getAttribute("data-aux") === aux)
				b.setAttribute("aria-current", "true");
			else
				b.removeAttribute("aria-current");
		}
		markFolder();
	}

	// The folder shown, in the folder pane (none while searching all mail
	// or while the contacts or settings view is shown)
	function markFolder() {
		folders.setCurrent(aux || (search && search.scope === "all") ? null : folder);
	}

	function openFolder(id) {
		search = null;
		search_.set("");
		folder = id;
		showView();
		if (layout.isPhone() && layout.current() === "folders")
			layout.back();
	}

	function runSearch(q) {
		navigate();
		if (q === "") {
			clearSearch();
			return;
		}
		const scope = search ? search.scope : (folder === "spam" || folder === "trash" ? "folder" : "all");
		search = { query: q, scope };
		showView();
	}

	// focus: put focus in the search field (the control used goes away)
	function clearSearch(focus) {
		if (!search)
			return;
		search = null;
		search_.set("");
		showView();
		if (focus === true)
			search_.focus();
	}

	function setScope(scope) {
		if (!search || search.scope === scope)
			return;
		search = { query: search.query, scope };
		showView();
	}

	// ---- Title, notices and states ----

	function bars() {
		if (dead)
			return;
		const f = find(folder);
		const name = search ? t("list.label_search_results") : nameOf(folder);
		titleText.data = name;
		// the unread count beside the title for the Inbox only
		const unread = !search && folder === "inbox" && f && f.unread > 0 ? f.unread : 0;
		titleCount.hidden = !unread;
		titleCount.textContent = unread ? t("list.label_unread_count", { n: unread, count: number(unread) }) : "";
		document.title = unread
			? t("main.label_page_title_unread", { n: unread, folder: name, count: number(unread), name: pageTitle })
			: t("main.label_page_title", { folder: name, name: pageTitle });

		let note = "";
		if (!search && folder === "trash" && data && typeof data.trash_days === "number")
			note = data.trash_days > 0 ? t("folders.label_trash_notice", { days: data.trash_days })
				: t("folders.label_trash_kept");
		else if (!search && folder === "spam")
			note = t("folders.label_spam_notice");
		folderNoteText.textContent = note;
		folderNote.hidden = note === "";
		const trash = !search && folder === "trash" ? find("trash") : null;
		emptyTrashBtn.hidden = !(trash && trash.total > 0);
		const f2 = find(folder);
		folderMenuBtn.hidden = !!search || !(f2 && f2.kind === "user");

		searchInfo.hidden = !search;
		if (search) {
			scopeFolder.textContent = nameOf(folder);
			scopeFolder.hidden = folder === "drafts";	// drafts are not searched
			scopeFolder.setAttribute("aria-pressed", String(search.scope === "folder"));
			scopeAll.setAttribute("aria-pressed", String(search.scope === "all"));
			barText(list.current());
		}
	}

	function barText(view) {
		if (!search || !view || view !== list.current())
			return;
		resultText.textContent = view.total === null ? t("list.label_loading")
			: t("list.label_results", { n: view.total, count: number(view.total), query: view.query });
	}

	function folderTag(it) {
		const v = list.current();
		if (!v || typeof it.folder !== "string")
			return "";
		if ((v.query && v.scope === "all") || (!v.query && v.folder === "starred"))
			return nameOf(it.folder);
		return "";
	}

	function emptyState(view) {
		if (view.query) {
			return h("div", { class: "empty" },
				h("span", { class: "badge" }, icon("search")),
				h("p", { class: "lead", text: t("list.label_no_results", { query: view.query }) }),
				h("p", { text: t(view.scope === "all" ? "list.label_search_all_help" : "list.label_search_help") }),
				h("button", { class: "btn", type: "button", text: t("list.button_clear_search"),
					onclick: () => clearSearch(true) }));
		}
		const which = { inbox: ["inbox", "list.label_empty_inbox"], trash: ["trash", "list.label_empty_trash"],
			sent: ["send", "list.label_empty_sent"] }[view.folder] || ["folder", "list.label_empty_folder"];
		return h("div", { class: "empty" },
			h("span", { class: "badge" }, icon(which[0])),
			h("p", { class: "lead", text: t(which[1]) }));
	}

	function failedState(view, err, retry) {
		const b = h("button", { class: "btn", type: "button", text: t("list.button_try_again"), onclick: retry });
		return h("div", { class: "empty failed", role: "alert" },
			h("span", { class: "badge" }, icon("alert")),
			h("p", { class: "lead", text: t("list.label_load_failed") }),
			err && err.message && err.error !== "failed" ? h("p", { text: err.message }) : null,
			b);
	}

	// ---- Start and stop ----

	function start() {
		rpc.hooks.state = onState;
		setNoticeHome(() => list.focus());
		Promise.all([loadFolders(false), (showView(), list.current().loading)]).then(([r]) => {
			if (dead || seen !== null)
				return;
			const token = r ? rpc.stateOf(r) : undefined;
			seen = typeof token === "string" ? token : rpc.state;
			if (newer())
				refreshAll(true);
		});
		const ms = typeof o.pollMs === "number" && o.pollMs >= MIN_POLL ? o.pollMs : DEFAULT_POLL;
		poller = createPoller(ms, poll);
	}

	function destroy() {
		dead = true;
		compose.destroy();
		setNoticeHome(null);
		if (poller)
			poller.stop();
		if (rpc.hooks.state === onState)
			rpc.hooks.state = null;
		list.destroy();
		reader.destroy();
		document.title = pageTitle;
	}

	// ---- Compose ----

	const compose = createCompose({
		host: o.host,
		// "Open Drafts" (a stalled send): its draft.list completes the send
		// or gives the draft back
		openDrafts: () => {
			navigate();
			openFolder("drafts");
			return draftsLoad;
		},
		limits: o.limits,
		user: o.user,
		onSent: (info) => {
			if (dead)
				return;
			if (info.sourceId && (info.mode === "reply" || info.mode === "replyall"))
				list.update(info.sourceId, { replied: true });
			refreshAll(false);
		},
		onDrafts: () => {
			if (!dead)
				draftsCount();
		},
		onClosed: () => {
			if (folder === "drafts" && !search)
				drafts.focus();
			else
				list.focus();
		}
	});

	// A draft deleted from the Drafts folder, after a confirmation
	async function deleteDraft(r, button) {
		const yes = await confirmDialog({ title: t("compose.button_delete_draft"), text: t("compose.label_delete_draft_confirm"),
			ok: t("compose.button_delete_draft"), danger: true });
		if (!yes)
			return;
		rpc.call("draft.delete", { id: r.id }, { retry: false }).then(() => {
			compose.forget(r.id);	// not saved back by an open panel or a retry
			notice(t("compose.label_draft_deleted"));
			draftsCount(true);
		}, (e) => {
			notice(e && e.message ? e.message : t("errors.failed"));
			if (button.isConnected)
				button.focus();
		});
	}

	// The Drafts count in the folder pane and the Drafts list, soon after a
	// change and together (one background folder.list and draft.list for
	// several changes), so the count and the rows agree.  focus: focus on
	// the list afterwards.
	let countTimer = 0;
	let countFocus = false;
	function draftsCount(focus) {
		countFocus = countFocus || focus === true;
		clearTimeout(countTimer);
		countTimer = setTimeout(() => {
			if (dead || writes > 0)
				return;
			const inDrafts = folder === "drafts" && !search;
			const f = countFocus;
			countFocus = false;
			Promise.all([loadFolders(true), inDrafts ? drafts.load(true) : null]).then(() => {
				if (f && inDrafts && !dead)
					drafts.focus();
			});
		}, 300);
	}

	return { search: search_, start, destroy, list, folders, reader, compose, setAux,
		usage: () => (data && data.usage && typeof data.usage === "object" ? data.usage : null) };
}
