// Acting on messages: star, read, delete, move, spam, restore, delete
// forever, empty Trash.  Every change shows at once (the rows change or
// leave the list) and is put back for the messages the server could not
// change (rpc.optimistic, through rpc.bulk); one notice says what was done
// and how many could not be.  Delete, move, spam and restore offer Undo
// until the next action or UNDO_MS.
//
// const acts = createActions({ list, view, nameOf, begin, end, closeIf, restored })
//   list                 the message list (list.js)
//   view()               the folder id the list shows ("inbox", "u:...";
//                        "starred" and searches show several folders)
//   nameOf(id)           a folder's display name
//   begin(), end(unsure) a write starts / has settled (unsure: the server
//                        could not be reached, so it may have been done)
//   closeIf(ids)         the reader shows one of these: close it
//   restored(ids)        message ids put back into the Inbox here (not new mail)
// entries: [{ id, folder }] (list.checked() or one row's)
// acts.star(items, on)   acts.read(entries, on)
// acts.remove(entries)   to Trash (in Trash: for good, after asking)
// acts.move(entries, target)
// acts.spam(entries, on) acts.restore(entries)
// acts.emptyTrash(count)
// acts.patch             set by the app: patch(id, fields) also applies a
//                        change to the message shown in the reader

import { t } from "./i18n.js";
import { rpc } from "./rpc.js";
import { notice, dismissNotice } from "./notice.js";
import { confirmDialog } from "./dialog.js";
import { number } from "./format.js";

const UNDO_MS = 10000;

export function createActions(o) {
	const list = o.list;
	const acts = { star, read, remove, move, spam, restore, emptyTrash, patch: null };

	function ids(entries) {
		return entries.map((e) => e.id);
	}

	// The view shows each message's folder, not one folder
	function mixed() {
		const v = o.view();
		return v === "starred" || v === "" || v === null;
	}

	function patch(id, fields) {
		list.update(id, fields);
		if (typeof acts.patch === "function")
			acts.patch(id, fields);
	}

	// One write: apply at once, undo(failed ids) for what failed; done(r)
	// with the result when the call went through (r.results per id)
	function write(method, args, apply, undo, done) {
		dismissNotice();
		o.begin();
		let unsure = false;
		return rpc.optimistic(apply, undo, method, args, { quiet: true, onUnsure: () => {
			unsure = true;
		} }).then((r) => {
			if (r)
				done(r);
			o.end(unsure);
			return r;
		}, (e) => {
			o.end(unsure);
			throw e;
		});
	}

	function oks(r) {
		return Array.isArray(r.results) ? r.results.filter((x) => x && x.ok === true) : [];
	}

	// The notice after a write: what was done, then what failed
	function report(r, text, undo) {
		const failed = rpc.failureText(r.results);
		const parts = [text, failed].filter((x) => x);
		if (parts.length === 0)
			return;
		const opts = { ms: undo ? UNDO_MS : 6000 };
		if (undo) {
			opts.action_label = t("main.button_undo");
			opts.on_action = undo;
			opts.action_title = t("list.label_undo_key");
		}
		notice(parts.join(" "), opts);
	}

	function counted(key, n, vars) {
		return t(key, Object.assign({ n, count: number(n) }, vars || {}));
	}

	// ---- Star and read: the rows change ----

	function star(entries, on) {
		const list_ = ids(entries);
		return write("message.set", { ids: list_, starred: on },
			() => list_.forEach((id) => patch(id, { starred: on })),
			(failed) => failed.forEach((id) => patch(id, { starred: !on })),
			(r) => report(r, ""));
	}

	function read(entries, on) {
		const list_ = ids(entries);
		const was = new Map();
		for (const id of list_) {
			const it = list.near(id, 0);
			was.set(id, it ? it.read === true : !on);
		}
		return write("message.set", { ids: list_, read: on },
			() => list_.forEach((id) => patch(id, { read: on })),
			(failed) => failed.forEach((id) => patch(id, { read: was.get(id) })),
			(r) => report(r, ""));
	}

	// ---- The rows leave ----

	// Take the rows out (and the message out of the reader); returns how to
	// put back some of them (not one the server no longer has: another
	// tab or the terminal removed it first)
	function take(list_) {
		o.closeIf(list_);
		const removed = list.remove(list_);
		return (back, codes) => {
			const set = new Set(codes ? back.filter((id) => codes.get(id) !== "notfound") : back);
			list.reinsert(removed.filter((x) => set.has(x.item.id)));
		};
	}

	// Rows put back by an undo: they come back at once, and leave again if
	// the undo fails for them
	// The notice names how many came back (and, from the results, how many
	// did not)
	function putBack(entries, putback, method, args, map) {
		return write(method, args, () => putback(ids(entries)),
			(failed) => {
				// failed holds the ids the call was made with: the rows
				// put back carry the old ids
				const old = failed.map((id) => map.get(id) || id);
				list.remove(old);
			},
			(r) => {
				for (const x of oks(r)) {
					const old = map.get(x.id) || x.id;
					if (typeof x.new_id === "string" && x.new_id !== old)
						list.update(old, { id: x.new_id });
				}
				const n = oks(r).length;
				report(r, n ? counted("list.label_undone_n", n) : "");
			});
	}

	// go(): called once the delete goes ahead (at once, or after the user
	// confirmed it): the caller's own clean-up (checks, focus) waits for it
	async function remove(entries, go) {
		if (entries.length === 0)
			return;
		// those already in Trash are deleted for good: always asked first
		const perm = entries.filter((e) => e.folder === "trash").length;
		const inTrash = perm === entries.length;
		if (perm > 0) {
			const n = entries.length;
			const yes = await confirmDialog({ title: t("list.button_delete_forever"),
				text: inTrash ? counted("list.label_forever_confirm", n)
					: t("list.label_forever_some_confirm", { n: perm, perm: number(perm), count: number(n), rest: number(n - perm) }),
				ok: t("list.button_delete_forever"), danger: true });
			if (!yes)
				return;
		}
		if (typeof go === "function")
			go();
		const list_ = ids(entries);
		const putback = take(list_);
		return write("message.delete", { ids: list_ }, () => {}, putback, (r) => {
			const done = oks(r);
			if (inTrash) {
				report(r, done.length ? counted("list.label_deleted_forever_n", done.length) : "");
				return;
			}
			// Undo: the messages back from Trash (by their ids there)
			const back = done.filter((x) => typeof x.new_id === "string");
			const map = new Map(back.map((x) => [x.new_id, x.id]));
			const text = !done.length ? "" : perm > 0
				? t("list.label_deleted_some_forever", { n: done.length, count: number(done.length), perm: number(perm) })
				: counted("list.label_deleted_n", done.length);
			report(r, text, back.length === 0 ? null : () => {
				const restoredIds = [];
				putBack(back.map((x) => ({ id: x.id })), putback, "message.undelete", { ids: back.map((x) => x.new_id) },
					map).then((r2) => {
					if (r2)
						for (const x of oks(r2))
							if (typeof x.new_id === "string" && x.folder !== "archive")
								restoredIds.push(x.new_id);
					o.restored(restoredIds);
				});
			});
		});
	}

	// Where a message moved out of folder goes back with an undo: a move
	// back, or for Trash a delete, or for Spam a move to the Inbox and the
	// spam mark again
	function back(folder, newIds) {
		if (folder === "trash")
			return rpc.bulk("message.delete", { ids: newIds });
		if (folder === "spam") {
			return rpc.bulk("message.move", { ids: newIds, folder: "inbox" }).then((r) => {
				const again = oks(r).map((x) => (typeof x.new_id === "string" ? x.new_id : x.id));
				if (again.length === 0)
					return r;
				return rpc.bulk("message.set", { ids: again, spam: true }).then((r2) => {
					const bad = new Set(r2.results.filter((x) => x.ok !== true).map((x) => x.id));
					return { ok: true, results: r.results.map((x) => (bad.has(x.new_id) ? { id: x.id, error: "failed" } : x)) };
				});
			});
		}
		return rpc.bulk("message.move", { ids: newIds, folder });
	}

	function move(entries, target) {
		if (entries.length === 0)
			return;
		const list_ = ids(entries);
		const from = new Map(entries.map((e) => [e.id, e.folder]));
		const stay = mixed();
		const name = o.nameOf(target);
		let putback = () => {};
		const was = new Map();
		return write("message.move", { ids: list_, folder: target }, () => {
			if (stay) {
				for (const id of list_) {
					const it = list.near(id, 0);
					was.set(id, it ? it.folder : from.get(id));
					patch(id, { folder: target });
				}
			} else
				putback = take(list_);
		}, (failed, codes) => {
			if (stay)
				failed.forEach((id) => patch(id, { folder: was.get(id) }));
			else
				putback(failed, codes);
		}, (r) => {
			const done = oks(r).filter((x) => typeof x.new_id === "string");
			if (stay)
				for (const x of done)
					if (x.new_id !== x.id)
						list.update(x.id, { id: x.new_id });
			if (target === "inbox")
				o.restored(done.map((x) => x.new_id));	// not new mail
			report(r, done.length ? counted("list.label_moved_n", done.length, { folder: name }) : "",
				done.length === 0 ? null : () => undoMove(done, from, stay, putback, was, target));
		});
	}

	// Each message back to the folder it came from, grouped by folder
	function undoMove(done, from, stay, putback, was, target) {
		dismissNotice();
		o.begin();
		const groups = new Map();
		for (const x of done) {
			const f = from.get(x.id);
			if (!groups.has(f))
				groups.set(f, []);
			groups.get(f).push(x);
		}
		const oldOf = new Map(done.map((x) => [x.new_id, x.id]));
		if (stay)
			for (const x of done)
				patch(x.new_id, { folder: was.get(x.id) });
		else
			putback(done.map((x) => x.id));
		const restoredIds = [];
		let unsure = false;
		const failed = [];
		// a failed id: the row's id now (the moved message's id in a view
		// that kept it, else the id it had before the move)
		const rowId = (movedId) => (stay ? movedId : oldOf.get(movedId) || movedId);
		const all = Array.from(groups, ([folder, xs]) => back(folder, xs.map((x) => x.new_id)).then((r) => {
			for (const y of r.results) {
				if (y.ok !== true)
					failed.push(rowId(y.id));
				else if (typeof y.new_id === "string") {
					list.update(rowId(y.id), { id: y.new_id });
					if (folder === "inbox")
						restoredIds.push(y.new_id);
				}
			}
		}, (e) => {
			if (e && e.unsure)
				unsure = true;
			for (const x of xs)
				failed.push(rowId(x.new_id));
		}));
		return Promise.all(all).then(() => {
			o.restored(restoredIds);
			if (failed.length > 0) {
				// those stay where they were moved
				if (stay)
					failed.forEach((id) => patch(id, { folder: target }));
				else
					list.remove(failed);
				const back = done.length - failed.length;
				notice([back ? counted("list.label_undone_n", back) : "",
					rpc.failureText(done.map((x, k) => (k < failed.length ? { id: x.id, error: "failed" } : { id: x.id, ok: true })))]
					.filter((x) => x).join(" "));
			} else
				notice(counted("list.label_undone_n", done.length));
			o.end(unsure);
		});
	}

	function spam(entries, on) {
		if (entries.length === 0)
			return;
		const list_ = ids(entries);
		const stay = mixed();
		let putback = () => {};
		return write("message.set", { ids: list_, spam: on }, () => {
			if (!stay)
				putback = take(list_);
		}, (failed, codes) => putback(failed, codes), (r) => {
			const done = oks(r).map((x) => x.id);
			if (!on)
				o.restored(done);
			report(r, done.length ? counted(on ? "list.label_spammed_n" : "list.label_not_spam_n", done.length) : "",
				done.length === 0 ? null : () => {
					const map = new Map(done.map((id) => [id, id]));
					putBack(done.map((id) => ({ id })), putback, "message.set", { ids: done, spam: !on },
						map);
				});
		});
	}

	function restore(entries) {
		if (entries.length === 0)
			return;
		const list_ = ids(entries);
		const putback = take(list_);
		return write("message.undelete", { ids: list_ }, () => {}, putback, (r) => {
			const done = oks(r).filter((x) => typeof x.new_id === "string");
			const archived = done.filter((x) => x.folder === "archive").length;
			o.restored(done.filter((x) => x.folder !== "archive").map((x) => x.new_id));
			let text = done.length ? counted("list.label_restored_n", done.length) : "";
			if (archived)
				text += " " + counted("list.label_restored_archive_n", archived);
			const map = new Map(done.map((x) => [x.new_id, x.id]));
			report(r, text, done.length === 0 ? null : () => {
				putBack(done.map((x) => ({ id: x.id })), putback, "message.delete", { ids: done.map((x) => x.new_id) },
					map);
			});
		});
	}

	async function emptyTrash(count) {
		const yes = await confirmDialog({ title: t("folders.button_empty_trash"),
			text: counted("folders.label_empty_trash_confirm", count), ok: t("folders.button_empty_trash"), danger: true });
		if (!yes)
			return null;
		dismissNotice();
		o.begin();
		const showing = o.view() === "trash";
		let unsure = false;
		return rpc.call("trash.empty").then((r) => {
			const n = oks(r).length;
			if (showing)
				list.remove(oks(r).map((x) => x.id));
			report(r, counted("folders.label_trash_emptied", n));
			o.end(false);
			return r;
		}, (e) => {
			unsure = !!(e && e.unsure);
			notice(e && e.message ? e.message : t("errors.failed"));
			o.end(unsure);
			return null;
		});
	}

	return acts;
}
