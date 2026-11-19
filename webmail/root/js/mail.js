// The mail view's folders, message list and reader: which folder or search
// is shown, the bars above the list, opening messages, and checking for
// changes.
//
// const mail = createMail({ layout, folderPane, listPane, readerPane, listBar, listTitle, pollMs })
//   folderPane, listPane, readerPane: the panes to fill (after their bars);
//   listBar: the list pane's bar (a Refresh button is added to it);
//   listTitle: the list pane's heading.  poweredBy: the system's version
//   notice, shown at the bottom of the folder pane.  The page title shows the folder
//   (and the Inbox's unread count) before the shell's title.
// mail.search      the top bar's search form (createSearch)
// mail.start()     loads the folders and the Inbox and starts checking
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
import { notice } from "./notice.js";
import { createFolders, folderName } from "./folders.js";
import { createList } from "./list.js";
import { createSearch } from "./search.js";
import { createReader } from "./reader.js";
import { poweredBy } from "./powered.js";
import { createPoller } from "./poll.js";
import { number } from "./format.js";

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
	let poller = null;
	let dead = false;		// destroyed: late replies change nothing
	const pageTitle = document.title;

	const folders = createFolders(openFolder);
	folderPane.append(folders.el);
	const powered = poweredBy(o.poweredBy);
	if (powered)
		folderPane.append(powered);

	const search_ = createSearch(runSearch);

	const live = h("div", { class: "vh", role: "status", "aria-live": "polite" });
	const folderNote = h("div", { class: "folder-note", hidden: true });
	const resultText = h("span", { class: "grow ell" });
	const scopeFolder = h("button", { type: "button", onclick: () => setScope("folder") });
	const scopeAll = h("button", { type: "button", text: t("folders.all_mail"), onclick: () => setScope("all") });
	const searchInfo = h("div", { class: "searchinfo", hidden: true },
		h("div", { class: "si-top" },
			resultText,
			h("button", { class: "linkbtn", type: "button", text: t("list.button_clear_search"),
				onclick: () => clearSearch(true) })),
		h("div", { class: "seg scope", role: "group", "aria-label": t("list.label_search_in") }, scopeFolder, scopeAll));
	const titleText = document.createTextNode(t("list.label_messages"));
	const titleCount = h("small", { hidden: true });
	clear(listTitle);
	listTitle.append(titleText, titleCount);
	listBar.append(h("button", {
		class: "ib", type: "button", "aria-label": t("main.label_refresh"), title: t("main.label_refresh"),
		onclick: () => refreshAll(false)
	}, icon("refresh")));

	const list = createList({
		labelledBy: listTitle.id,
		fetch: fetchPage,
		empty: emptyState,
		failed: failedState,
		folderTag,
		onTotal: barText,
		onSelect: (it, opening) => {
			if (!opening && it && !layout.isPhone() && reader.current() !== null && reader.current() !== it.id)
				reader.open(it, { delay: 250 });
		},
		onOpen: (it, how) => {
			if (layout.isPhone())
				layout.show("reader", { push: true, opener: list.el.querySelector(".list-scroll") });
			reader.open(it, { focus: how === "key" });
		}
	});
	listPane.append(searchInfo, folderNote, list.el, live);

	const reader = createReader({
		pane: o.readerPane,
		nextOf: (id) => list.near(id, 1),
		onRead: markRead,
		onEscape: () => {
			if (layout.isPhone())
				layout.back();
			else
				list.focus();
		}
	});

	// ---- Loading ----

	// A view's first load is not retried while the server cannot be
	// reached: the list says so at once and offers Try again
	function fetchPage(view, offset, limit, how) {
		const args = { folder: view.query ? (view.scope === "all" ? "all" : view.folder) : view.folder, offset, limit };
		if (view.query)
			args.search = view.query;
		return rpc.call("message.list", args, { retry: how.first !== true, background: how.background === true });
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
		data = r;
		folders.update(r);
		folders.setCurrent(search && search.scope === "all" ? null : folder);
		const inbox = find("inbox");
		if (inbox) {
			// new mail: more messages in the Inbox (a message marked unread
			// again is not new)
			const n = inbox.total - (inboxTotal === null ? inbox.total : inboxTotal);
			if (n > 0)
				announce(t("main.label_new_mail", { n, count: number(n) }));
			inboxTotal = inbox.total;
		}
		if (!find(folder) && folder !== "inbox") {
			openFolder("inbox");	// removed elsewhere
			return;
		}
		bars();
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
		if (refreshing)
			return refreshing;
		refreshing = Promise.all([loadFolders(background), list.refresh(background)]).then(([r]) => {
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
			folders.setCurrent(search && search.scope === "all" ? null : folder);
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
		const props = search ? { folder, query: search.query, scope: search.scope } : { folder, query: "", scope: "" };
		list.show(list.view(key(), props));
		folders.setCurrent(search && search.scope === "all" ? null : folder);
		bars();
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
		folderNote.textContent = note;
		folderNote.hidden = note === "";

		searchInfo.hidden = !search;
		if (search) {
			scopeFolder.textContent = nameOf(folder);
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
		if (poller)
			poller.stop();
		if (rpc.hooks.state === onState)
			rpc.hooks.state = null;
		list.destroy();
		reader.destroy();
		document.title = pageTitle;
	}

	return { search: search_, start, destroy, list, folders, reader };
}
