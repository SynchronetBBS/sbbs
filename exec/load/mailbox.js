// One user's mailbox: the system mail base (the Inbox) and the user's
// private e-mail base (folders, Sent copies, Trash) presented as one.
//
// var Store = load("mailbox.js");
// var store = new Store({usernum, alias, name, shared_path, data_dir, options,
//	now});
// store.open();
// store.folders();				// [{id, name, kind, total, unread}]
// store.list(folder_id, {offset, limit, search});	// {total, items}
// store.state();				// change fingerprint (a string)
// store.resolve(id);				// {kind, base, number, header, role}
// store.close();
//
// Constructor: alias and name are kept but not used. shared_path is the
// shared base's path (default: the configured "mail" base), data_dir the
// data directory (default system.data_dir), now a function returning a
// time_t (default time(); tests inject a clock). options, each optional:
// quota_bytes (default system.mailbox_quota; 0 = unlimited), trash_days
// (default system.max_trash_age; 0 = Trash never expires),
// max_text_part_bytes (default 1048576) and slice_size (default 200: the
// messages handled per summary lock by the calls that work in slices).
//
// folders(): the built-in folders (kind "builtin", name "") in the order
// inbox, starred, sent, spam, trash, archive, then the user folders (kind
// "user", name the display name); total and unread count messages.
// list(): items {id, from, from_addr, to, subject, date (time_t written),
// zone (written time zone), arrived (time_t imported), size (bytes, see
// Summary), read, replied, starred, attach, priority ("high", "low" or
// ""), folder (the folder id the message is in)}, plus extra:true for a
// sent extra and, in Sent and "all", delivery (see "Delivery" below);
// newest arrival first. limit is 1 to 100 (default 50).
//
// Write side (each takes an array of ids and returns one result per id, in
// order: {id, ok:true, new_id} or {id, ok:false, error, message}, error a
// code below or "failed" when a base could not be written or something
// unexpected was thrown (the detail is logged, the message is generic and
// never names a server path); one failure never stops the rest):
// store.set(ids, {read, starred, spam});	// spam: Inbox/Spam only
// store.move(ids, folder_id);			// "inbox", "archive", "u:<tag>"
// store.remove(ids);				// to Trash; in Trash: for real
// store.restore(ids);				// Trash back to where it was (see below)
// store.empty_trash();				// results for the messages removed
// store.purge_expired([now]);			// null if run today; [] if no private base
// store.folder_create(name);			// {id, name}
// store.folder_rename(folder_id, name);	// {id, name, errors}
// store.folder_delete(folder_id);		// results: messages put in Trash
// store.usage();				// {bytes, quota}
//
// A bad folder name throws { code: "invalid", reason } with reason one of
// "empty", "invalid" (a character above 0xFF or a control character),
// "toolong" (over 64 characters), "reserved" (a built-in folder's name)
// or "duplicate" (the same as another folder's, ASCII case ignored).
//
// Reading one message (each checks ownership like resolve()):
// store.get(id, {mark_read, head});	// list item + {headers, comment, text, html, parts}
// store.original(id);			// {rfc822, stored, external_files}
// store.raw_file(id);			// {path, name, size, temp:true}
// store.part_file(id, pid);		// {path, name, type, size, temp}
//
// get(): headers is {from, from_addr, to, cc, reply_to, date, message_id,
// reply_id, net_type ("local", "internet", "fido", "qwk" or "other"),
// to_domain}, all UTF-8. from is the display name of the stored original
// From line when there is one, else the from field; to and cc are the
// stored recipient lists when present (RFC 2047 words decoded), else the
// to field; reply_to the reply-to list, else the reply-to address;
// to_domain the domain of the SMTP forward path (or of an Internet
// recipient address), "" for local mail. comment is the header's comment
// fields (SMB_COMMENT), one per line, or "". text and html are {b64,
// charset} or null (see MimeRead.model() for which parts they are); the
// bytes are written by save_msg_part() to a temp file and base64 encoded,
// never read with get_msg_body(). A message that is not MIME (no
// MIME-Version or Content-Type, so get_msg_parts() gives one text/plain
// part whose charset smblib derives from the header) has Ctrl-A and ANSI
// codes stripped (strip_ctrl_a(), then strip_ansi()), whatever its
// charset. A body part in cp437 is converted with utf8_encode() (after
// the stripping: strip_ansi() and strip_ctrl_a() work on the single-byte
// text) and returned as "utf-8"; other charsets are returned as stored. A
// body part over options.max_text_part_bytes (checked against the size
// get_msg_parts() reports and again against the saved file) or that
// cannot be read is null and listed first in parts. parts is the MIME
// attachments {pid, name, type, size, cid, inline}, then the file
// attachments {pid: "f<n>", name, type, size} (size -1: missing, or not
// a file this user may be served: see file_dir()). head: the list item
// only (after mark_read), nothing else read. mark_read sets
// MSG_READ once, on a shared message only when it is to the user, never
// on a sent extra; a busy summary or a failed write leaves it unread
// (logged) rather than failing the read.
// original(): rfc822 is get_rfc822_header() (822header.js: ASCII, its
// non-ASCII converted or replaced by '?') made safe by rfc822_block(): no
// stored CR/LF can forge a header line or end the block early; stored the
// lines of dump_msg_header(), line breaks inside one replaced by a space,
// each kept if valid UTF-8, else from CP437; external_files true for a
// file-attached message (whose files are not in raw_file()).
// raw_file(): rfc822_block() (it ends with the one blank line), then the
// body as stored (save_msg_part() part "0", copied in 64 KB pieces). The
// MIME header fields are header fields, not part of the stored body, so
// nothing is repeated. name is the subject reduced to [A-Za-z0-9 ._-]
// plus ".eml" ("message.eml" when nothing is left), then
// MimeRead.safe_name() ("_CON.eml" for a subject "CON").
// part_file(): pid is checked against get_msg_parts() of that message (or,
// "f<n>", against its file attachments). A MIME part is decoded to a
// temp file (temp true: the caller deletes it); a file attachment is the
// real file (temp false: never delete it); a missing one is notfound.
// Temp files are under system.temp_dir with generated names; none that
// is not handed to the caller is left behind. A base that cannot be read
// throws {code: "failed"} (the detail is logged).
//
// new_id is the message's id after the call ("p<n>" after a move into the
// private base, "s<n>" after one back to the shared base, null once it is
// removed or hidden). Arguments that make the whole call meaningless (ids
// not an array, a bad or unknown folder, a bad folder name) are thrown.
// The time a message is put in Trash comes from the constructor's "now"
// (a function returning a time_t; default time()), so tests can set it.
//
// Write rules:
// - Every write to the private base is made inside one Summary.modify()
//   per call, or per slice for empty_trash, purge_expired, folder_delete
//   and folder_rename: those handle options.slice_size messages per
//   Summary.modify() (the lock is released between slices, so no call
//   holds it for an unbounded time), each slice complete in itself
//   (journal reconciled, summary and side files saved); messages removed
//   for real are purged once, at the end of the call, in a short
//   Summary.modify() of its own. If the lock cannot be taken every id
//   gets "conflict" and nothing was written; folder_rename/folder_delete
//   and purge_expired throw it instead when it is their first slice. A
//   later slice that cannot run leaves the earlier slices done: its ids
//   and those after it are reported (conflict, or failed), and
//   mailbox.folders.json is updated only for what was done. A
//   move_msg() into the private base that returns false may still have
//   left a copy there, so the call's modify reports no change list and
//   the summary is rebuilt from the headers.
// - Moves between the bases use move_msg() (attachment files stay where
//   they are and go with the copy) and are journaled in
//   mailbox.moving.json (see below). Every write call first reconciles
//   the journal under the summary lock, before any move: an entry whose
//   source is gone is dropped (the move finished); otherwise the copy the
//   pending move left is pinned by NUMBER, or the entry is dropped if
//   there is none. Into the private base (written only under that lock)
//   only message after+1 can be that copy; into the shared base it is the
//   live record above after, to the user, with the source's import and
//   written times (this user's moves are serialized and reconciled first,
//   so no other move of the user can have made one). The same is done at
//   once for an entry whose move_msg() did not succeed (returned false or
//   threw), so nothing is added to a base while an entry for it is
//   unsettled. A copy is reused, instead of moving again, only by its
//   pinned number (never pinned for two entries): it is retagged as the
//   new action needs and the source is removed. Look-alikes (the same
//   message delivered twice in one second) are never merged. If a needed
//   journal write cannot be saved (busy lock), the rest of the call makes
//   no move between the bases (those ids get conflict); flags, retags and
//   folder changes still run.
// - Only mail received by the user (to_ext the user, not network mail) can
//   go to the shared base: a Sent copy moved to "inbox" is invalid. Nor
//   can mail whose import time is older than the shared base's max_age
//   (whole days, as mail maintenance counts them; 0 = no limit), since
//   the next maintenance would delete it: move to "inbox" fails such an
//   id with "expired". Import times are never changed. If an interrupted
//   move already left such mail's copy in the shared base, that copy is
//   removed and its journal entry cleared, and the private message stays
//   (moved: "expired"; restored: to Archive). Spam restored to the
//   shared base is also subject to the spam age limit, which scripts
//   cannot read: it is not checked, so old spam restored to Spam can be
//   deleted at the next maintenance.
// - A sent extra (role "from") may only be hidden: remove() adds its
//   Message-ID (or, without one, its id) to mailbox.hidden.json and
//   changes nothing else, which hides every recipient's copy of that
//   message; set() (starred included:
//   the header is also the recipient's copy) and move() on it are
//   forbidden, as are any ids of another user's mail. "p" ids are numbers
//   in the user's own private base only.
// - Inbox to Trash records $Was:$Inbox, Spam to Trash $Was:$Spam (and the
//   spam attribute is cleared in the private copy; restore sets it again
//   in the shared base); an untagged private message records $Was:$Archive.
//   Restoring to a user folder that no longer exists adds its tag back to
//   mailbox.folders.json; an unusable $Was: restores to Archive. A message
//   from the Inbox or Spam that cannot go back to the shared base (not
//   mail received by the user, such as mail held for a QWK network
//   account, or older than max_age) is restored to Archive: its result is
//   {id, ok:true, new_id, folder:"archive"}, plus expired:true when it was
//   the age. Tags set by other software (without '$') are kept on every
//   move, in both directions.
// - move() of an Inbox message to "inbox" does nothing; of a Spam message
//   it clears the spam attribute. Moving a Trash message to a folder takes
//   it out of Trash.
// - Removing for real: remove_msg() first, then a file-attached
//   message's files (named in the subject, base names only) are deleted
//   from data_dir/file/<user>.in/ of this user only, and only when the
//   message is to this user; a file that cannot be deleted is logged and
//   does not fail the id (an orphaned file is harmless, a message without
//   its files is not). One purge() at the end of the call; if it fails the
//   removals stand (logged) and the next purge frees them. A Sent copy
//   (in Sent, or in Trash from Sent) removed for real adds its Message-ID
//   to mailbox.hidden.json, so the recipients' copies do not reappear in
//   Sent as extras.
// - Quota (options.quota_bytes, 0 = unlimited) is checked for every move
//   into the private base except a move to Trash (deleting is how a user
//   gets back under quota), against the summary's total. If the message
//   would not fit even with all of Trash removed, the id fails with
//   "quota" and nothing is removed; otherwise the oldest Trash by arrival
//   is removed, one message at a time, until it fits (should Trash run
//   out anyway, "quota", and what was removed stays removed). A move the
//   journal can no longer record (an earlier save failed in this call) is
//   refused with "conflict" before Trash is touched. The incoming size is
//   estimated as Summary counts it, with the header grown by its new tags.
// - folder_rename retags the folder's messages and the $Was: of Trash
//   messages from it, from the summary taken at the start of the call (a
//   message filed there meanwhile keeps the old tag and shows as a folder
//   of its own). A message that cannot be retagged is reported in
//   "errors" (per-id results) and also keeps the old tag. If the messages
//   were changed but mailbox.folders.json stays locked after three tries
//   (about 1 s each), folder_rename and folder_delete still return
//   normally (logged): the messages' tags keep every folder that holds
//   mail listed. When nothing was changed they report it: folder_rename
//   returns the old id (with its errors) if no message could be retagged,
//   and both throw conflict if there was nothing to retag.
// - Once the bases were changed, a side file that cannot be saved (a busy
//   lock or a failed write) never turns the call into a thrown error: it
//   is logged, the per-id results stand, and the rest of the bookkeeping
//   is still done. The summary is then left invalid (rebuilt by the next
//   read); a Trash time not saved counts from when purge_expired() next
//   sees the message; a hide not saved fails its id. If forget_shared()
//   fails after a shared header was rewritten, mailbox.shared.json and
//   mailbox.state.json are deleted (both derived) so no stale fact
//   survives; this also forgets when purge_expired() last ran, so it may
//   run again that day.
//
// Handles: every public call that reads or writes mail closes and reopens
// the bases first, so what resolve() and private_base() return is
// valid only until the next public call. The reason: an open MsgBase reads
// its index and headers through a 64 KB stdio buffer, and a seek inside
// the buffered range does not refill it, so a handle kept open can serve
// stale index or header bytes after another handle rewrote them in place
// (seen with get_index() and get_msg_index() after remove_msg() through a
// second handle).
//
// Loading: the sibling libraries (mailbox_files.js, mailbox_tags.js,
// mailbox_summary.js, mailbox_mime.js, smbdefs.js, 822header.js) are
// loaded from the directory this file was loaded from, found with
// Error().fileName. js.exec_dir cannot be used: it is the directory of the
// top-level script, not of this file.
// If a sibling is not beside this file (this file was copied to mods/,
// say), it is loaded by name through the normal load() search.
//
// The user: usernum must be an integer from 1 to 65535, else the
// constructor throws { code: "invalid" }: the mail base's index holds user
// numbers in 16 bits, and the stock mail code finds a user's mail by that
// field, so mail cannot work for a higher number. The user's directory is
// data_dir + format("user/%04u/", usernum) and the private base is
// "mailbox" in it; no caller-supplied string is part of a path. Reading
// never creates the private base, mailbox.folders.json or
// mailbox.hidden.json; it may create the user's directory and the derived
// cache files in it (mailbox.shared.json, the fingerprint and serial in
// mailbox.state.json, and Summary's files once a private base exists).
//
// Ids: "s<number>" is a message in the shared base, "p<number>" one in the
// private base. Anything else is { code: "invalid" }. Ownership: a shared
// message belongs to the user when its header's to_ext is the user's
// number (role "to") or, read-only, when its from_ext is (role "from", a
// sent extra); every private message belongs to the user. resolve()
// enforces this, and every list row from the shared base is checked
// against its header the first time it is seen (see the facts cache).
//
// Errors are thrown as { code, message }, code one of notfound, forbidden,
// invalid, quota, toolarge, conflict, failed, expired (also the codes of
// per-id results). Failures of the bases
// themselves (cannot open, cannot write) are thrown as Error. Every public
// method passes what it throws through outgoing(): the message of a
// { code } error (thrown or in a per-id result) is a fixed text for its
// code (other fields, such as a folder name's "reason", are kept), and an
// Error's is a fixed generic text, so no message leaving Store names a
// server path; the original is logged.
//
// Folders: inbox, starred, sent, spam, trash, archive, then the user's
// folders as "u:<tag>": those in mailbox.folders.json (an array of tags,
// in display order), then any other folder tag found on a private message
// (sorted), so filed mail is never unreachable. A private message with no
// folder tag (untagged private mail) is shown in Archive. A user folder's
// name is Tags.decode() of its tag, or the tag itself if the tag is not
// canonical (foreign).
//
// Side files (JsonFiles, prefix "mailbox.", in the user's directory):
//   mailbox.folders.json  array of user-folder tags (Tags.encode(name)) in
//                         display order; written by the write side only
//   mailbox.hidden.json   array of the sent extras the user hid:
//                         "m:<Message-ID>" (every shared copy with that
//                         Message-ID) or "s<number>" (a message without
//                         one; an entry of this form for a message with a
//                         Message-ID also hides every copy of it); written
//                         by the write side only, which drops entries that
//                         no longer name a live shared record from the user
//   mailbox.shared.json   {v, gen, recs: {number: [time, own, id, reply_id,
//                         to_net_type, sent, attempts, row]}}:
//                         the facts cache (derived; deleting it is safe);
//                         row is the list row's header fields [tags, from,
//                         from_addr, to, subject, date, zone, arrived,
//                         priority, size, attach], null when own is 0. gen
//                         is bumped by forget_shared(); a save whose gen
//                         no longer matches the file is skipped
//   mailbox.state.json    shared: {stamps, fp}, serial, and expired (the
//                         time_t purge_expired last ran), beside Summary's
//                         {bytes, count, stamps}
//   mailbox.trash.json    {"<private number>": time_t put in Trash}; set
//                         when a message is put in Trash, dropped when it
//                         leaves, pruned by purge_expired(). A Trash
//                         message with no entry counts as put in Trash
//                         when purge_expired() first sees it (recorded
//                         then). Not a tag (tags are for folders and flags)
//                         and not the header's import time, which is kept
//   mailbox.moving.json   the move journal: {"<source id>": {to: "p"|"s",
//                         msgid, imported, written, after, tags [, copy]}},
//                         read and written only under the summary lock,
//                         and written only when something in it changed.
//                         An entry is written before a move between the
//                         bases and cleared once the source is gone.
//                         msgid ("" for none), imported and written
//                         (time_t) identify the source; after is the
//                         destination's last message number before the
//                         move; copy, once pinned, is the number of the
//                         copy the move left (see "reconcile" below)
//
// Membership: inbox = shared, to the user, not spam; spam = shared, to
// the user, spam attribute; starred = see Stars, either base, not in
// Trash; sent = private $Sent plus shared records from the user (not
// hidden by mailbox.hidden.json) whose Message-ID no private message
// has, one per Message-ID, flagged extra:true (mail to oneself is in both
// Inbox and Sent); trash, archive and user folders = private by tag. "all" (search
// only) is everything except Spam and Trash, each message once. Deleted
// messages are never listed.
//
// Stars: a starred message has the MSG_VALIDATED attribute, in either
// base (the bit the stock IMAP service shows as \Flagged, so a star set
// here shows there; the IMAP service lets only an operator set the flag
// today, #1369). set({starred}) sets or
// clears it with put_msg_header(), which writes the header and the index
// record; move_msg() carries the attribute to the copy, so a star stays
// with a message moved between the bases, to Trash and back. Whether a
// row is starred is read on every call: from the index record (shared)
// or the summary (private), never cached.
//
// Delivery (every Sent item): "queued" {waiting, attempts} while delivery
// copies with its Message-ID, from the user, to a network address and not
// yet sent are in the shared base; else "failed" if it has $Failed or a
// delivery-failure notice (to the user, no-reply attribute, reply_id equal
// to its Message-ID) is found, which also adds $Failed to a private copy;
// else "unsent" if it has $Unsent; else "sent".
//
// Cost: one get_index() pass over the shared base per call. Headers of the
// user's shared records are read once each and the facts needed for
// membership (owner check, Message-ID, reply_id, network type, sent) and
// the header fields of a list row are cached in mailbox.shared.json,
// keyed by number and import time; later calls read only records not seen
// before. A list row's attributes (read, replied, starred, spam) come from
// the index on every call. A change this library makes to a shared
// header's cached fields (its tags, a move away) calls forget_shared(); a
// change of its attributes alone only counts as a change for state().
// Listing or counting Sent also re-reads the headers of the user's
// outbound network records not yet marked sent (for the delivery state).
// A header field of a list row changed by something else in place (the
// same number and import time) is not seen until forget_shared() or a
// deletion of the cache.
//
// Owners: a shared record is a candidate for the user when its 16-bit
// index field is the user's number; the owner is confirmed from the
// header (to_ext, or from_ext for a sent extra) when the record is first
// seen, and only then are its list fields cached. A record that is not
// the user's after all is never listed (fact own 0, no row).
//
// Strings: display strings and search are UTF-8 byte strings; search
// matches case-insensitively for ASCII letters only.

"use strict";

var Store = (function () {
	var HERE = (function () {
		var f = String(new Error().fileName || "");
		var i = Math.max(f.lastIndexOf('/'), f.lastIndexOf('\\'));
		return i >= 0 ? f.substr(0, i + 1) : "";
	})();

	function sibling(name, scope)
	{
		var path = HERE + name;
		if (HERE === "" || !file_exists(path))
			path = name;
		return load(scope || {}, path);
	}

	var JsonFiles = sibling("mailbox_files.js");
	var Tags = sibling("mailbox_tags.js");
	var Summary = sibling("mailbox_summary.js");
	var MimeRead = sibling("mailbox_mime.js");
	var D = {};
	sibling("smbdefs.js", D);
	// Adds get_rfc822_header() to MsgBase.HeaderPrototype (as for the IMAP
	// service); it resolves its own dependencies in this scope
	sibling("822header.js");

	var MAX_LIMIT = 100;
	var SLICE_SIZE = 200;	// messages per summary lock in a long call
	var DEFAULT_LIMIT = 50;
	var FACTS_VERSION = 3;
	var BLOCK = 256;	// SHD_BLOCK_LEN and SDT_BLOCK_LEN (smbdefs.h)
	var ID_RE = /^[sp][1-9][0-9]{0,9}$/;
	var WS_RE = /[ \t\r\n\f\v]/;
	var BUILTIN = ["inbox", "starred", "sent", "spam", "trash", "archive"];

	// Fact row fields (mailbox.shared.json "recs", keyed by number)
	var F_TIME = 0;		// index time, to detect a reused number
	var F_OWN = 1;		// bit 1: to_ext is the user, bit 2: from_ext is
	var F_ID = 2;		// Message-ID
	var F_RID = 3;		// reply_id
	var F_NET = 4;		// to_net_type
	var F_DONE = 5;		// NETMSG_SENT seen (never cleared by the mail server)
	var F_TRIES = 6;	// delivery_attempts when last read
	var F_ROW = 7;		// the list row's header fields (row_fields())
	var OWN_TO = 1;
	var OWN_FROM = 2;

	function error(code, message)
	{
		return { code: code, message: message };
	}

	function slash(p)
	{
		var c = p.charAt(p.length - 1);
		return (c == '/' || c == '\\') ? p : p + '/';
	}

	function lower(s)	// ASCII-only lower-casing (UTF-8 byte strings)
	{
		return s.replace(/[A-Z]/g, function (c) {
			return String.fromCharCode(c.charCodeAt(0) + 32);
		});
	}

	function stamps(path)	// size and mtime of a base's .shd and .sid
	{
		return [file_size(path + ".shd"), file_date(path + ".shd"),
			file_size(path + ".sid"), file_date(path + ".sid")];
	}

	function same(a, b)
	{
		if (!(a instanceof Array) || !(b instanceof Array) || a.length != b.length)
			return false;
		for (var i = 0; i < a.length; i++)
			if (a[i] !== b[i])
				return false;
		return true;
	}

	function priority(p)
	{
		if (p == 1 || p == 2)
			return "high";
		if (p == 4 || p == 5)
			return "low";
		return "";
	}

	function valid_facts(c)
	{
		return c !== null && typeof c == "object" && c.v === FACTS_VERSION
			&& c.recs !== null && typeof c.recs == "object";
	}

	function facts_gen(c)	// 0 for a missing or unusable file
	{
		return (valid_facts(c) && typeof c.gen == "number") ? c.gen : 0;
	}

	function is_conflict(e)
	{
		return e !== null && typeof e == "object" && e.code === "conflict";
	}

	// A user-folder tag as it may appear in a folder id: one tag, not reserved
	function valid_user_tag(tag)
	{
		return typeof tag == "string" && tag.length > 0 && tag.charAt(0) != '$'
			&& !WS_RE.test(tag);
	}

	function folder_name(tag)
	{
		return Tags.canonical(tag) ? Tags.decode(tag) : tag;
	}

	// The folder id of a private message, from its parsed tags
	function private_folder(p)
	{
		if (p.folder == Tags.SENT)
			return "sent";
		if (p.folder == Tags.TRASH)
			return "trash";
		if (p.folder == Tags.ARCHIVE || p.folder === "")
			return "archive";
		return "u:" + p.folder;
	}

	// Whether a message is starred: the MSG_VALIDATED attribute (attr from
	// the index or the summary)
	function is_starred(attr)
	{
		return (attr & D.MSG_VALIDATED) != 0;
	}

	function Store(o)
	{
		o = o || {};
		var u = o.usernum;
		if (typeof u != "number" || u !== Math.floor(u) || u < 1)
			throw error("invalid", "usernum is not a positive integer");
		if (u > 0xFFFF)
			throw error("invalid", "user numbers above 65535 do not fit the mail index");
		this.usernum = u;
		this.alias = o.alias;
		this.name = o.name;
		this.shared_path = o.shared_path || null;
		this.data_dir = slash(o.data_dir || system.data_dir);
		this.user_dir = this.data_dir + format("user/%04u/", u);
		this.private_path = this.user_dir + "mailbox";
		var opt = o.options || {};
		this.options = {
			trash_days: opt.trash_days !== undefined ? opt.trash_days : system.max_trash_age,
			quota_bytes: opt.quota_bytes !== undefined ? opt.quota_bytes : system.mailbox_quota,
			max_text_part_bytes: opt.max_text_part_bytes !== undefined
				? opt.max_text_part_bytes : 1048576,
			slice_size: opt.slice_size > 0 ? opt.slice_size : SLICE_SIZE
		};
		this.now = typeof o.now == "function" ? o.now : function () { return time(); };
		this.jf = new JsonFiles(this.user_dir, "mailbox.");
		this.smb = null;	// the shared base
		this.pmb = null;	// the private base, once it exists and is opened
	}

	Store.prototype.open = function ()
	{
		if (this.smb !== null)
			return;
		this.open_shared();
	};

	Store.prototype.open_shared = function ()
	{
		var mb = this.shared_path ? new MsgBase(this.shared_path, true, true) : new MsgBase("mail");
		if (!mb.open())
			throw new Error("cannot open " + (this.shared_path || "mail") + ": " + mb.last_error);
		this.smb = mb;
	};

	Store.prototype.close = function ()
	{
		if (this.smb !== null)
			this.smb.close();
		if (this.pmb !== null)
			this.pmb.close();
		this.smb = null;
		this.pmb = null;
	};

	// The open private base; null if it does not exist and create is false
	Store.prototype.private_base = function (create)
	{
		if (this.pmb !== null)
			return this.pmb;
		if (!create && !file_exists(this.private_path + ".shd"))
			return null;
		mkpath(this.user_dir);
		var mb = new MsgBase(this.private_path, true, true);
		if (!mb.open())
			throw new Error("cannot open " + this.private_path + ": " + mb.last_error);
		this.pmb = mb;
		return mb;
	};

	// Called at the start of every public method that reads: reopens the
	// bases, so nothing is read through a handle that has buffered older
	// file contents. (An open MsgBase reads its index through a 64 KB stdio
	// buffer that a seek inside the buffered range does not refill, so a
	// handle kept open misses changes made through other handles, as
	// observed with get_index() and get_msg_index().) Objects returned by
	// resolve() are valid until the next public call.
	Store.prototype.refresh = function ()
	{
		if (this.smb === null)
			throw new Error("Store is not open");
		this.close();
		this.open_shared();
	};

	Store.prototype.shared_base = function ()
	{
		if (this.smb === null)
			throw new Error("Store is not open");
		return this.smb;
	};

	// The private summary rows (Summary.get), [] if there is no base
	Store.prototype.private_rows = function ()
	{
		var pmb = this.private_base(false);
		if (pmb === null)
			return [];
		return Summary.get(pmb, this.jf, { data_dir: this.data_dir });
	};

	// One get_index() pass: this user's live (not deleted) shared records
	Store.prototype.scan = function ()
	{
		var idx = this.shared_base().get_index() || [];
		var u16 = this.usernum & 0xFFFF;
		var r = { to: [], from: [], all: [] };
		for (var i = 0; i < idx.length; i++) {
			var x = idx[i];
			if (x.attr & (D.MSG_DELETE | D.MSG_VOTE))
				continue;
			var to = x.to == u16;
			var from = x.from == u16;
			if (to)
				r.to.push(x);
			if (from)
				r.from.push(x);
			if (to || from)
				r.all.push(x);
		}
		return r;
	};

	// Read a live shared header by number (not expanded); null if gone
	Store.prototype.shared_header = function (num)
	{
		var h = this.shared_base().get_msg_header(false, num, false);
		if (h === null || h === undefined)
			return null;
		if (h.number != num || (h.attr & (D.MSG_DELETE | D.MSG_VOTE)))
			return null;
		return h;
	};

	Store.prototype.read_fact = function (rec)
	{
		var h = this.shared_header(rec.number);
		if (h === null)
			return null;
		var own = 0;
		if (parseInt(h.to_ext, 10) === this.usernum)
			own |= OWN_TO;
		if (parseInt(h.from_ext, 10) === this.usernum)
			own |= OWN_FROM;
		// The list fields only of the user's own records: another user's
		// subject and sender never go into this user's cache file
		return [rec.time, own, h.id || "", h.reply_id || "",
			h.to_net_type || 0, (h.netattr & D.NETMSG_SENT) ? 1 : 0, h.delivery_attempts || 0,
			own ? row_fields(summarize_shared(this.shared_base(), h)) : null];
	};

	// Facts for every record in scan.all, from mailbox.shared.json where
	// cached, else from the header. Saves the cache when it changed and the
	// user's directory exists. Returns { num: fact } (records whose header
	// is gone are left out).
	Store.prototype.facts = function (scan)
	{
		var c = this.jf.read("shared", null);
		var cached = valid_facts(c) ? c.recs : {};
		var gen = facts_gen(c);
		var out = {};
		var dirty = false;
		for (var i = 0; i < scan.all.length; i++) {
			var rec = scan.all[i];
			var f = cached[rec.number];
			if (!(f instanceof Array) || f[F_TIME] !== rec.time || (f[F_OWN] && !valid_row(f))) {
				f = this.read_fact(rec);
				if (f === null)
					continue;
				f.fresh = true;
				dirty = true;
			}
			out[rec.number] = f;
		}
		if (!dirty) {
			for (var k in cached)
				if (out[k] === undefined) {
					dirty = true;
					break;
				}
		}
		if (dirty)
			this.save_facts(out, gen);
		out.gen = gen;
		return out;
	};

	// Replace the cached facts with 'facts' (live records only), unless the
	// file's generation is no longer 'gen' (forget_shared() ran since the
	// facts were read: they may be stale, so nothing is written)
	Store.prototype.save_facts = function (facts, gen)
	{
		var recs = {};
		for (var k in facts)
			if (facts[k] instanceof Array)
				recs[k] = facts[k].slice(0);
		var skip = {};
		try {
			this.jf.update("shared", null, function (c) {
				if (facts_gen(c) !== gen)
					throw skip;	// leaves the file as it is
				return { v: FACTS_VERSION, gen: gen, recs: recs };
			});
		} catch (e) {
			// A derived cache: not saving it costs header reads next time
			if (e !== skip && !is_conflict(e))
				log(LOG_WARNING, "mailbox: user " + this.usernum + ": facts cache not saved: " + e);
		}
	};

	// Drop cached facts for these shared message numbers (call after
	// changing a shared header's tags), and note the change for state()
	Store.prototype.forget_shared = function (numbers)
	{
		this.jf.update("shared", null, function (c) {
			var gen = facts_gen(c) + 1;
			if (!valid_facts(c))
				return { v: FACTS_VERSION, gen: gen, recs: {} };
			for (var i = 0; i < numbers.length; i++)
				delete c.recs[numbers[i]];
			c.gen = gen;
			return c;
		});
		this.bump_serial();
	};

	// Note a change for state() that no cached fact depends on (an
	// attribute of a shared message, which list() reads from the index)
	Store.prototype.bump_serial = function ()
	{
		this.jf.update("state", {}, function (s) {
			if (s === null || typeof s != "object" || s instanceof Array)
				s = {};
			s.serial = (typeof s.serial == "number" ? s.serial : 0) + 1;
			return s;
		});
	};

	Store.prototype.hidden = function ()
	{
		var list = this.jf.read("hidden", []);
		var set = {};
		if (list instanceof Array)
			for (var i = 0; i < list.length; i++)
				if (typeof list[i] == "string")
					set[list[i]] = true;
		return set;
	};

	// The user folder tags, in display order
	Store.prototype.user_tags = function (rows)
	{
		var list = this.jf.read("folders", []);
		var tags = [];
		var seen = {};
		var i;
		if (list instanceof Array) {
			for (i = 0; i < list.length; i++) {
				var t = list[i];
				if (valid_user_tag(t) && !seen[t]) {
					seen[t] = true;
					tags.push(t);
				}
			}
		}
		var extra = [];
		for (i = 0; i < rows.length; i++) {
			var p = Tags.parse(rows[i].tags);
			var f = p.folder;
			if (f !== "" && f.charAt(0) != '$' && !seen[f]) {
				seen[f] = true;
				extra.push(f);
			}
		}
		extra.sort();
		return tags.concat(extra);
	};

	// Everything list() and folders() need, from one index pass:
	// cands: [{kind, num, arrived, folder, starred, unread, extra, msgid, row, rec}]
	// (row: private summary entry; rec: shared index record)
	Store.prototype.collect = function (want_sent)
	{
		var scan = this.scan();
		var facts = this.facts(scan);
		var rows = this.private_rows();
		var cands = [];
		var i;
		var pids = {};

		for (i = 0; i < rows.length; i++) {
			var row = rows[i];
			var p = Tags.parse(row.tags);
			var folder = private_folder(p);
			if (row.msgid)
				pids[row.msgid] = true;
			cands.push({ kind: "p", num: row.num, arrived: row.arrived, folder: folder,
				starred: is_starred(row.attr) && folder != "trash", unread: !(row.attr & D.MSG_READ),
				extra: false, msgid: row.msgid, row: row, parsed: p });
		}
		for (i = 0; i < scan.to.length; i++) {
			var rec = scan.to[i];
			var f = facts[rec.number];
			if (f === undefined || !(f[F_OWN] & OWN_TO))
				continue;
			cands.push({ kind: "s", num: rec.number, arrived: rec.time,
				folder: (rec.attr & D.MSG_SPAM) ? "spam" : "inbox",
				starred: is_starred(rec.attr),
				unread: !(rec.attr & D.MSG_READ),
				extra: false, msgid: f[F_ID], rec: rec, fact: f });
		}
		var info = { scan: scan, facts: facts, rows: rows, pids: pids };
		if (want_sent) {
			// Hidden by Message-ID (every copy of the message), or by number
			// (a message without one; an older list's entry for one with a
			// Message-ID also hides every copy of it)
			var hidden = this.hidden();
			var ids_seen = {};
			for (i = 0; i < scan.from.length; i++) {
				f = facts[scan.from[i].number];
				if (f !== undefined && f[F_ID] !== "" && hidden["s" + scan.from[i].number])
					hidden["m:" + f[F_ID]] = true;
			}
			for (i = 0; i < scan.from.length; i++) {
				rec = scan.from[i];
				f = facts[rec.number];
				if (f === undefined || !(f[F_OWN] & OWN_FROM) || hidden["s" + rec.number])
					continue;
				var id = f[F_ID];
				if (id !== "" && hidden["m:" + id])
					continue;
				if (id !== "") {
					if (pids[id] || ids_seen[id])
						continue;
					ids_seen[id] = true;
				}
				cands.push({ kind: "s", num: rec.number, arrived: rec.time, folder: "sent",
					starred: false, unread: false, extra: true, msgid: id, rec: rec, fact: f });
			}
		}
		info.cands = cands;
		return info;
	};

	Store.prototype.folders = function ()
	{
		this.refresh();
		var info = this.collect(true);
		var counts = {};
		var i;
		function count(id, unread) {
			var c = counts[id];
			if (c === undefined)
				c = counts[id] = { total: 0, unread: 0 };
			c.total++;
			if (unread)
				c.unread++;
		}
		for (i = 0; i < info.cands.length; i++) {
			var c = info.cands[i];
			count(c.folder, c.unread);
			if (c.starred)
				count("starred", c.unread);
		}
		var out = [];
		for (i = 0; i < BUILTIN.length; i++) {
			var n = counts[BUILTIN[i]] || { total: 0, unread: 0 };
			out.push({ id: BUILTIN[i], name: "", kind: "builtin", total: n.total, unread: n.unread });
		}
		var tags = this.user_tags(info.rows);
		for (i = 0; i < tags.length; i++) {
			var id = "u:" + tags[i];
			n = counts[id] || { total: 0, unread: 0 };
			out.push({ id: id, name: folder_name(tags[i]), kind: "user", total: n.total,
				unread: n.unread });
		}
		return out;
	};

	function blocks(len)
	{
		return Math.ceil(len / BLOCK) * BLOCK;
	}

	function header_length(msgbase, hdr)
	{
		var lines = msgbase.dump_msg_header(hdr) || [];
		for (var i = 0; i < lines.length; i++) {
			var m = lines[i].match(/^header_length\s+(\d+)/);
			if (m)
				return parseInt(m[1], 10);
		}
		return BLOCK;
	}

	// A summary entry (as Summary.get's) for a shared header. size is the
	// header and data blocks; file attachments are not counted here.
	function summarize_shared(msgbase, hdr)
	{
		var num = hdr.number;	// a non-NULL field first (lazy fields)
		var auxattr = hdr.auxattr;
		var utf8 = (auxattr & D.MSG_HFIELDS_UTF8) != 0;
		function str(s) {
			if (s === undefined || s === null)
				return "";
			s = String(s);
			return utf8 ? s : utf8_encode(s);
		}
		var ctype = hdr.content_type || "";
		return {
			num: num, tags: hdr.tags || "", from: str(hdr.from), from_addr: str(hdr.from_net_addr),
			to: str(hdr.to), subject: str(hdr.subject), date: hdr.when_written_time,
			attr: hdr.attr, size: blocks(header_length(msgbase, hdr)) + blocks(hdr.data_length || 0),
			attach: (auxattr & (D.MSG_FILEATTACH | D.MSG_MIMEATTACH)) != 0
				|| /^multipart\/mixed\b/i.test(ctype),
			arrived: hdr.when_imported_time, zone: hdr.when_written_zone_offset,
			priority: hdr.priority || 0, msgid: hdr.id || ""
		};
	}

	// The cached fields of a shared summary entry (F_ROW), and the entry
	// back from them with the index record's attributes
	function row_fields(s)
	{
		return [s.tags, s.from, s.from_addr, s.to, s.subject, s.date, s.zone, s.arrived,
			s.priority, s.size, s.attach ? 1 : 0];
	}

	function row_summary(num, rec, f)
	{
		var r = f[F_ROW];
		return { num: num, tags: r[0], from: r[1], from_addr: r[2], to: r[3], subject: r[4],
			date: r[5], zone: r[6], arrived: r[7], priority: r[8], size: r[9],
			attach: r[10] == 1, attr: rec.attr, msgid: f[F_ID] };
	}

	function valid_row(f)
	{
		var r = f === undefined ? undefined : f[F_ROW];
		return r instanceof Array && r.length == 11;
	}

	// The summary entry for a candidate; null if its header is gone or
	// (for a shared one) no longer the user's. A shared one comes from the
	// facts cache when it holds the row (collect() kept only records whose
	// facts say they are the user's), else from the header.
	Store.prototype.summary_of = function (c)
	{
		if (c.kind == "p")
			return c.row;
		if (c.sum !== undefined)
			return c.sum;
		if (valid_row(c.fact)) {
			c.sum = row_summary(c.num, c.rec, c.fact);
			return c.sum;
		}
		var h = this.shared_header(c.num);
		c.sum = null;
		if (h === null)
			return null;
		var own = c.extra ? h.from_ext : h.to_ext;
		if (parseInt(own, 10) !== this.usernum)
			return null;
		c.sum = summarize_shared(this.shared_base(), h);
		return c.sum;
	};

	function to_item(c, s)
	{
		var it = {
			id: c.kind + c.num, from: s.from, from_addr: s.from_addr, to: s.to,
			subject: s.subject, date: s.date, zone: s.zone, arrived: s.arrived, size: s.size,
			read: c.extra ? true : (s.attr & D.MSG_READ) != 0,
			replied: (s.attr & D.MSG_REPLIED) != 0,
			starred: c.extra ? false : (is_starred(s.attr) && c.folder != "trash"),
			attach: s.attach, priority: priority(s.priority), folder: c.folder
		};
		if (c.extra)
			it.extra = true;
		return it;
	}

	// Delivery facts for this user's sent mail: queued copies by
	// Message-ID and the reply_ids of delivery-failure notices. Re-reads
	// the headers of outbound network records not yet seen sent.
	Store.prototype.delivery_facts = function (info)
	{
		var queued = {};
		var failed = {};
		var scan = info.scan;
		var facts = info.facts;
		var changed = false;
		var i;
		for (i = 0; i < scan.from.length; i++) {
			var rec = scan.from[i];
			var f = facts[rec.number];
			if (f === undefined || !(f[F_OWN] & OWN_FROM) || f[F_NET] == D.NET_NONE
				|| f[F_DONE] || f[F_ID] === "")
				continue;
			if (!f.fresh) {
				var h = this.shared_header(rec.number);
				if (h === null)
					continue;
				var done = (h.netattr & D.NETMSG_SENT) ? 1 : 0;
				var tries = h.delivery_attempts || 0;
				if (done != f[F_DONE] || tries != f[F_TRIES]) {
					f[F_DONE] = done;
					f[F_TRIES] = tries;
					changed = true;
				}
				if (done)
					continue;
			}
			var q = queued[f[F_ID]];
			if (q === undefined)
				q = queued[f[F_ID]] = { waiting: 0, attempts: 0 };
			q.waiting++;
			if (f[F_TRIES] > q.attempts)
				q.attempts = f[F_TRIES];
		}
		for (i = 0; i < scan.to.length; i++) {
			rec = scan.to[i];
			f = facts[rec.number];
			// When the mail server starts setting reply_id on its failure
			// notices, also require evidence that the notice is from the system
			if (f !== undefined && (f[F_OWN] & OWN_TO) && (rec.attr & D.MSG_NOREPLY) && f[F_RID] !== "")
				failed[f[F_RID]] = true;
		}
		if (changed)
			this.save_facts(facts, facts.gen);
		return { queued: queued, failed: failed };
	};

	// Add $Failed to these private messages (through Summary.modify)
	Store.prototype.mark_failed = function (nums)
	{
		var pmb = this.private_base(false);
		if (pmb === null || nums.length < 1)
			return;
		// A read must not fail over a derived tag: the row still shows
		// failed (from the notice) and the tag is tried again next time
		try {
			this.modify_failed(pmb, nums);
		} catch (e) {
			this.log_original("$Failed not recorded", e);
		}
	};

	// Each message on its own: one that cannot be retagged is logged and
	// the others are still recorded
	Store.prototype.modify_failed = function (pmb, nums)
	{
		var self = this;
		Summary.modify(pmb, this.jf, function () {
			var changed = [];
			for (var i = 0; i < nums.length; i++) {
				try {
					var h = pmb.get_msg_header(false, nums[i], false);
					if (!live(h, nums[i]))
						continue;
					var p = Tags.parse(h.tags);
					if (p.failed)
						continue;
					p.failed = true;
					h.tags = Tags.format(p);
					self.put_header(pmb, nums[i], h);
					changed.push(nums[i]);
				} catch (e) {
					self.log_original("$Failed not recorded on p" + nums[i], e);
				}
			}
			return { changed: changed };
		}, { data_dir: this.data_dir });
	};

	// Sets c.delivery on every Sent candidate; records a notice as $Failed
	Store.prototype.add_delivery = function (info)
	{
		var d = this.delivery_facts(info);
		var mark = [];
		for (var i = 0; i < info.cands.length; i++) {
			var c = info.cands[i];
			if (c.folder != "sent")
				continue;
			var failed = (c.kind == "p" && c.parsed.failed) || (c.msgid !== "" && d.failed[c.msgid]);
			if (c.kind == "p" && !c.parsed.failed && failed)
				mark.push(c.num);
			var q = c.msgid !== "" ? d.queued[c.msgid] : undefined;
			if (q !== undefined)
				c.delivery = { state: "queued", waiting: q.waiting, attempts: q.attempts };
			else if (failed)
				c.delivery = { state: "failed" };
			else if (c.kind == "p" && c.parsed.unsent)
				c.delivery = { state: "unsent" };
			else
				c.delivery = { state: "sent" };
		}
		this.mark_failed(mark);
	};

	function order(a, b)
	{
		return (b.arrived - a.arrived) || (b.num - a.num) || (a.kind < b.kind ? -1 : a.kind > b.kind ? 1 : 0);
	}

	function matches(s, needle)
	{
		return lower(s.from).indexOf(needle) >= 0 || lower(s.from_addr).indexOf(needle) >= 0
			|| lower(s.to).indexOf(needle) >= 0 || lower(s.subject).indexOf(needle) >= 0;
	}

	// Which candidates a folder id selects; throws invalid / notfound
	Store.prototype.selector = function (folder_id, search, info)
	{
		if (typeof folder_id != "string")
			throw error("invalid", "folder id is not a string");
		if (folder_id == "all") {
			if (search === "")
				throw error("invalid", "\"all\" needs a search");
			return function (c) { return c.folder != "spam" && c.folder != "trash"; };
		}
		if (folder_id == "starred")
			return function (c) { return c.starred; };
		if (folder_id.substr(0, 2) == "u:") {
			var tag = folder_id.substr(2);
			if (!valid_user_tag(tag))
				throw error("invalid", "bad folder id");
			if (this.user_tags(info.rows).indexOf(tag) < 0)
				throw error("notfound", "no such folder");
		} else if (BUILTIN.indexOf(folder_id) < 0)
			throw error("invalid", "unknown folder id");
		return function (c) { return c.folder == folder_id; };
	};

	Store.prototype.list = function (folder_id, opts)
	{
		opts = opts || {};
		var search = opts.search === undefined || opts.search === null ? "" : String(opts.search);
		var offset = parseInt(opts.offset, 10);
		if (isNaN(offset) || offset < 0)
			offset = 0;
		var limit = parseInt(opts.limit, 10);
		if (isNaN(limit))
			limit = DEFAULT_LIMIT;
		limit = Math.max(1, Math.min(MAX_LIMIT, limit));

		// Validate before the (costly) collection where possible
		if (typeof folder_id != "string")
			throw error("invalid", "folder id is not a string");
		if (folder_id == "all" && search === "")
			throw error("invalid", "\"all\" needs a search");

		this.refresh();
		var want_sent = folder_id == "sent" || folder_id == "all";
		var info = this.collect(want_sent);
		var sel = this.selector(folder_id, search, info);
		var list = [];
		var seen = {};	// mail to oneself is two candidates: keep the first
		for (var i = 0; i < info.cands.length; i++) {
			var cand = info.cands[i];
			var key = cand.kind + cand.num;
			if (sel(cand) && !seen[key]) {
				seen[key] = true;
				list.push(cand);
			}
		}
		list.sort(order);

		if (search !== "") {
			var needle = lower(search);
			var hits = [];
			for (i = 0; i < list.length; i++) {
				var s = this.summary_of(list[i]);
				if (s !== null && matches(s, needle))
					hits.push(list[i]);
			}
			list = hits;
		}
		var page = list.slice(offset, offset + limit);
		if (want_sent)
			this.add_delivery(info);
		var items = [];
		var total = list.length;
		for (i = 0; i < page.length; i++) {
			var c = page[i];
			s = this.summary_of(c);
			if (s === null) {	// vanished, or not the user's after all
				total--;
				continue;
			}
			var it = to_item(c, s);
			if (c.delivery !== undefined)
				it.delivery = c.delivery;
			items.push(it);
		}
		return { total: total, items: items };
	};

	// Resolve an id to {kind, base, number, header, role}, enforcing
	// ownership. header is read with expand_fields false, so it can be
	// written back with put_msg_header(). role is "to" (a shared message to
	// the user), "from" (a shared message only from the user: read-only)
	// or "owner" (private).
	Store.prototype.resolve = function (id)
	{
		check_id(id);
		this.refresh();
		return this.resolve_now(id);
	};

	function check_id(id)
	{
		if (typeof id != "string" || !ID_RE.test(id) || parseInt(id.substr(1), 10) > 0xFFFFFFFF)
			throw error("invalid", "bad message id");
	}

	// resolve() without the refresh, for use inside one public call (the
	// write side resolves every id through this, after one refresh)
	Store.prototype.resolve_now = function (id)
	{
		check_id(id);
		var kind = id.charAt(0);
		var num = parseInt(id.substr(1), 10);
		var mb = kind == "s" ? this.shared_base() : this.private_base(false);
		if (mb === null)
			throw error("notfound", "no such message");
		var h = mb.get_msg_header(false, num, false);
		if (h === null || h === undefined || h.number != num || (h.attr & (D.MSG_DELETE | D.MSG_VOTE)))
			throw error("notfound", "no such message");
		var role = "owner";
		if (kind == "s") {
			if (parseInt(h.to_ext, 10) === this.usernum)
				role = "to";
			else if (parseInt(h.from_ext, 10) === this.usernum)
				role = "from";
			else
				throw error("forbidden", "not your message");
		}
		return { kind: kind, base: mb, number: num, header: h, role: role };
	};

	// The fingerprint of this user's records in the shared base: count,
	// highest number, and sum of number * 31 + attr, deleted ones included
	Store.prototype.shared_fingerprint = function ()
	{
		var idx = this.shared_base().get_index() || [];
		var u16 = this.usernum & 0xFFFF;
		var count = 0;
		var max = 0;
		var sum = 0;
		for (var i = 0; i < idx.length; i++) {
			var x = idx[i];
			if ((x.attr & D.MSG_VOTE) || (x.to != u16 && x.from != u16))
				continue;
			count++;
			if (x.number > max)
				max = x.number;
			sum = (sum + x.number * 31 + x.attr) % 4503599627370496;	// 2^52
		}
		return count + "." + max + "." + sum;
	};

	function file_stamp(path)
	{
		return file_exists(path) ? file_size(path) + "." + file_date(path) : "-";
	}

	// A string that changes when this user's mail changes. The shared
	// fingerprint is cached in mailbox.state.json against the shared base's
	// stamps (other fields of that file are kept). A change that leaves the
	// shared base's files the same size within the same second as the
	// cached stamps is not seen until the next change.
	Store.prototype.state = function ()
	{
		this.refresh();
		var sst = stamps(this.shared_base().file);
		var s = this.jf.read("state", null);
		var fp = null;
		var serial = 0;
		if (s !== null && typeof s == "object") {
			if (typeof s.serial == "number")
				serial = s.serial;
			if (s.shared !== null && typeof s.shared == "object" && same(s.shared.stamps, sst)
				&& typeof s.shared.fp == "string")
				fp = s.shared.fp;
		}
		if (fp === null) {
			fp = this.shared_fingerprint();
			var rec = { stamps: sst, fp: fp };
			var base = this.shared_base().file;
			try {
				this.jf.update("state", {}, function (o) {
					if (o === null || typeof o != "object" || o instanceof Array)
						o = {};
					if (same(stamps(base), sst))	// not changed since
						o.shared = rec;
					return o;
				});
			} catch (e) {
				if (!is_conflict(e))
					throw e;
			}
		}
		return [stamps(this.private_path).join("."), file_stamp(this.jf.path("folders")),
			file_stamp(this.jf.path("hidden")), serial, fp].join(":");
	};

	// ---- Write side ----

	var WAS_INBOX = "$Inbox";
	var WAS_SPAM = "$Spam";
	var DAY = 24 * 60 * 60;
	var TRIM_RE = /^[ \t\r\n\f\v]+|[ \t\r\n\f\v]+$/g;

	function done(id, new_id)
	{
		return { id: id, ok: true, new_id: new_id === undefined ? id : new_id };
	}

	// The fixed message for each error code: no message leaving Store
	// carries anything else (a library message can name a server path, as
	// JsonFiles' "locked: <path>" does); the original is logged
	var MESSAGES = {
		notfound: "not found",
		forbidden: "not allowed",
		invalid: "invalid request",
		quota: "the mailbox is full",
		toolarge: "too large",
		conflict: "the mailbox is busy, try again",
		failed: "the operation failed",
		expired: "too old to return to the Inbox"
	};
	var BASE_FAILURE = "the mailbox could not be read or written";

	function is_coded(e)
	{
		return e !== null && typeof e == "object" && !(e instanceof Error) && typeof e.code == "string";
	}

	// Log what is about to be replaced by a generic message
	Store.prototype.log_original = function (what, e)
	{
		var level = LOG_DEBUG;
		var text;
		if (is_coded(e)) {
			if (e.code == "conflict" || e.code == "failed")
				level = LOG_WARNING;
			text = e.code + ": " + e.message;
		} else {
			level = LOG_ERR;
			text = String(e);
			if (e !== null && typeof e == "object" && e.fileName)
				text += " (" + e.fileName + ":" + e.lineNumber + ")";
		}
		log(level, "mailbox: user " + this.usernum + ": " + what + ": " + text);
	};

	// An exception as it leaves Store: a library error as {code, message}
	// with the fixed message for its code (other fields, such as reason,
	// kept); anything else as an Error with a generic message
	Store.prototype.outgoing = function (what, e)
	{
		if (is_coded(e)) {
			var msg = MESSAGES[e.code] || MESSAGES.failed;
			if (e.message === msg)
				return e;	// already mapped
			this.log_original(what, e);
			var out = {};
			for (var k in e)
				out[k] = e[k];
			out.message = msg;
			return out;
		}
		if (e instanceof Error && e.message === BASE_FAILURE)
			return e;
		this.log_original(what, e);
		return new Error(BASE_FAILURE);
	};

	// A per-id failure: {id, ok:false, error, message} with the fixed
	// message for the code; "failed" for anything that is not a library
	// error (an unexpected exception)
	Store.prototype.failure = function (id, e)
	{
		var code = is_coded(e) && MESSAGES[e.code] !== undefined ? e.code : "failed";
		if (!is_coded(e) || e.message !== MESSAGES[code])
			this.log_original(String(id), e);
		return { id: id, ok: false, error: code, message: MESSAGES[code] };
	};

	function live(h, num)
	{
		return h !== null && h !== undefined && h.number == num && !(h.attr & (D.MSG_DELETE | D.MSG_VOTE));
	}

	function folder_of(row)
	{
		return Tags.parse(row.tags).folder;
	}

	// The tags of mailbox.folders.json as a clean array
	function folder_list(list)
	{
		var out = [];
		if (list instanceof Array)
			for (var i = 0; i < list.length; i++)
				if (valid_user_tag(list[i]) && out.indexOf(list[i]) < 0)
					out.push(list[i]);
		return out;
	}

	function name_error(reason)
	{
		return { code: "invalid", reason: reason, message: "folder name: " + reason };
	}

	// The tag of a user-folder id ("u:<tag>"); throws invalid
	function user_folder_tag(folder_id)
	{
		if (typeof folder_id != "string" || folder_id.substr(0, 2) != "u:" || !valid_user_tag(folder_id.substr(2)))
			throw error("invalid", "not a user folder id");
		return folder_id.substr(2);
	}

	// Record (time_t) or drop (null) the moment a private message was put
	// in Trash; written to mailbox.trash.json when the call ends
	function set_trash(ctx, num, when)
	{
		ctx.trash[num] = when;
	}

	function new_ctx(rows, pmb, now)
	{
		var ctx = { rows: rows, pmb: pmb, now: now, by_num: {}, bytes: 0, added: [], removed: [],
			changed: [], forget: [], trash: {}, hidden: [], folders: [], purge: false, gone: {},
			used: {}, trash_order: null, journal: {}, journal_ok: false, keep: null, unknown: false,
			touched: false };
		for (var i = 0; i < rows.length; i++) {
			ctx.by_num[rows[i].num] = rows[i];
			ctx.bytes += rows[i].size;
		}
		return ctx;
	}

	Store.prototype.put_header = function (mb, num, h)
	{
		if (!mb.put_msg_header(false, num, h))
			throw this.base_failure("put_msg_header #" + num, mb);
	};

	// One write call: refresh the bases, read the private summary, then run
	// op(id, ctx) for every id inside one Summary.modify() (directly when
	// there is no private base), then update the side files. ids may be a
	// function(ctx) returning them (it may throw, failing the whole call);
	// create may be a function returning whether to create the private
	// base. A summary lock that cannot be taken reports conflict for every
	// id (strict: throws it), nothing having been written. slice (from
	// run_sliced): busy is set when that happens, and purge is set instead
	// of purging.
	Store.prototype.run = function (ids, create, op, strict, slice)
	{
		if (typeof ids != "function" && !(ids instanceof Array))
			throw error("invalid", "ids is not an array");
		this.refresh();
		var opts = { data_dir: this.data_dir };
		var pmb = this.private_base(typeof create == "function" ? create.call(this) : create);
		var ctx = new_ctx(pmb === null ? [] : Summary.get(pmb, this.jf, opts), pmb, this.now());
		if (typeof ids == "function")
			ids = ids.call(this, ctx);
		var results = [];
		var self = this;
		function body() {
			if (ctx.pmb !== null)
				self.reconcile(ctx);
			for (var i = 0; i < ids.length; i++) {
				try {
					results.push(op.call(self, ids[i], ctx));
				} catch (e) {
					results.push(self.failure(ids[i], e));
				}
			}
			// Free what was removed, inside the summary lock (purge changes
			// the base's files, which would invalidate the summary outside)
			if (ctx.purge && slice)
				slice.purge = true;
			else if (ctx.purge && ctx.pmb.purge() === false)
				log(LOG_WARNING, "mailbox: purge of " + ctx.pmb.file + ": " + ctx.pmb.last_error);
			// A failed move_msg may still have left a copy here: no list of
			// changes can describe that, so the summary is rebuilt
			if (ctx.unknown)
				return null;
			return { added: ctx.added, removed: ctx.removed, changed: ctx.changed };
		}
		if (pmb === null)
			body();
		else {
			try {
				Summary.modify(pmb, this.jf, body, opts);
			} catch (e) {
				if (!is_conflict(e) || results.length > 0 || strict)
					throw e;
				if (slice)
					slice.busy = true;
				for (var i = 0; i < ids.length; i++)
					results.push(this.failure(ids[i], error("conflict", "the mailbox is busy")));
				return results;
			}
		}
		this.finish(ctx, results);
		return results;
	};

	// run() over the ids select(ctx) chooses once, in slices of
	// options.slice_size ids: one run(), so one Summary.modify(), per
	// slice, the lock released between slices, each slice complete in
	// itself (journal reconciled, summary and side files saved). strict
	// applies to the first slice (its lock not taken: thrown, nothing
	// done). A later slice that cannot run reports conflict (or failed) for
	// its ids and every id after it; the earlier slices stand. Removed
	// messages are purged once, at the end (purge_private()).
	Store.prototype.run_sliced = function (select, op, strict)
	{
		var size = this.options.slice_size;
		var all = null;
		var results = [];
		var slice = { purge: false, busy: false };
		var pos = 0;
		var i;
		do {
			var res = null;
			var why = null;
			try {
				res = this.run(function (ctx) {
					if (all === null)
						all = select.call(this, ctx);
					return all.slice(pos, pos + size);
				}, false, op, strict && all === null, slice);
			} catch (e) {
				if (all === null || pos == 0)
					throw e;	// nothing done
				why = e;
			}
			if (res !== null)
				results = results.concat(res);
			pos += size;
			if (why !== null || slice.busy) {
				if (why === null)
					why = error("conflict", "the mailbox is busy");
				for (i = res === null ? pos - size : pos; i < all.length; i++)
					results.push(this.failure(all[i], why));
				break;
			}
		} while (pos < all.length);
		if (slice.purge)
			this.purge_private();
		return results;
	};

	// purge() the private base in a short Summary.modify() of its own; a
	// failure is logged (the space is freed by the next purge)
	Store.prototype.purge_private = function ()
	{
		try {
			this.refresh();
			var pmb = this.private_base(false);
			if (pmb === null)
				return;
			Summary.modify(pmb, this.jf, function () {
				if (pmb.purge() === false)
					log(LOG_WARNING, "mailbox: purge of " + pmb.file + ": " + pmb.last_error);
				return { added: [], removed: [], changed: [] };	// no message changed
			}, { data_dir: this.data_dir });
		} catch (e) {
			this.log_original("purge", e);
		}
	};

	// After a call's writes: the shared facts, mailbox.trash.json,
	// mailbox.hidden.json and mailbox.folders.json. The bases were changed
	// already, so a side file that cannot be saved (busy, or a failed
	// write) is logged and never turns the call into a thrown error; only a
	// hide that could not be recorded fails its id.
	Store.prototype.finish = function (ctx, results)
	{
		var i;
		if (ctx.forget.length > 0)
			this.forget_written(ctx.forget);
		else if (ctx.touched)
			this.touch_state();
		var trash = ctx.trash;
		var keep = ctx.keep;
		var any = keep !== null;
		for (var k in trash)
			any = true;
		if (any) {
			try {
				this.jf.update("trash", {}, function (t) {
					if (t === null || typeof t != "object" || t instanceof Array)
						t = {};
					if (keep !== null) {
						for (var n in t)
							if (!keep[n])
								delete t[n];
					}
					for (n in trash) {
						if (trash[n] === null)
							delete t[n];
						else
							t[n] = trash[n];
					}
					return t;
				});
			} catch (e) {
				// A message in Trash with no record counts from when it is
				// next seen by purge_expired()
				this.log_original("Trash times not saved", e);
			}
		}
		if (ctx.hidden.length > 0) {
			var hid = ctx.hidden;
			try {
				this.save_hidden(hid);
			} catch (e) {
				this.log_original("hidden list not saved", e);
				for (i = 0; i < results.length; i++) {
					if (!results[i].ok)
						continue;
					for (var j = 0; j < hid.length; j++)
						if (hid[j].id === results[i].id)
							results[i] = this.failure(results[i].id, e);
				}
			}
		}
		if (ctx.folders.length > 0) {
			var tags = ctx.folders;
			try {
				this.jf.update("folders", [], function (list) {
					list = folder_list(list);
					for (var j = 0; j < tags.length; j++)
						if (list.indexOf(tags[j]) < 0)
							list.push(tags[j]);
					return list;
				});
			} catch (e) {
				// The restored message carries the tag, so the folder is
				// still listed (after the others)
				this.log_original("folders list not saved", e);
			}
		}
	};

	// The hidden-list entries that still name a live shared record from
	// the user ("s<number>" and "m:<Message-ID>"); null if the shared base
	// cannot be read (then nothing is pruned). The shared base is reopened
	// first (a handle kept open can serve stale index bytes).
	Store.prototype.hidden_live = function ()
	{
		try {
			this.smb.close();
			this.smb = null;
			this.open_shared();
			var scan = this.scan();
			var facts = this.facts(scan);
			var live = {};
			for (var i = 0; i < scan.from.length; i++) {
				var f = facts[scan.from[i].number];
				if (f === undefined || !(f[F_OWN] & OWN_FROM))
					continue;
				live["s" + scan.from[i].number] = true;
				if (f[F_ID] !== "")
					live["m:" + f[F_ID]] = true;
			}
			return live;
		} catch (e) {
			this.log_original("hidden list not pruned", e);
			if (this.smb === null)
				this.open_shared();
			return null;
		}
	};

	// Add these entries ({id, key}) to mailbox.hidden.json, dropping every
	// entry (old or new) that no longer names a live shared record
	Store.prototype.save_hidden = function (entries)
	{
		var live = this.hidden_live();
		this.jf.update("hidden", [], function (list) {
			var out = [];
			var j;
			function add(key) {
				if (typeof key == "string" && (live === null || live[key]) && out.indexOf(key) < 0)
					out.push(key);
			}
			if (list instanceof Array)
				for (j = 0; j < list.length; j++)
					add(list[j]);
			for (j = 0; j < entries.length; j++)
				add(entries[j].key);
			return out;
		});
	};

	// bump_serial() after a shared header's attributes alone were
	// rewritten; if that fails, the change serial's file is dropped
	Store.prototype.touch_state = function ()
	{
		try {
			this.bump_serial();
		} catch (e) {
			this.log_original("change serial not updated, file deleted", e);
			file_remove(this.jf.path("state"));
		}
	};

	// forget_shared() after shared headers were rewritten; if that fails
	// (its lock cannot be taken, or a write fails), drop the facts cache
	// and the change serial's file (both derived, safe to delete) so no
	// stale fact survives and state() changes
	Store.prototype.forget_written = function (numbers)
	{
		try {
			this.forget_shared(numbers);
		} catch (e) {
			this.log_original("shared facts not updated, cache deleted", e);
			file_remove(this.jf.path("shared"));
			file_remove(this.jf.path("state"));
		}
	};

	// Rewrite a private message's tags from parsed tags p
	Store.prototype.retag = function (r, ctx, p)
	{
		var tags = Tags.format(p);
		if (tags === (r.header.tags || ""))
			return;
		r.header.tags = tags;
		this.put_header(r.base, r.number, r.header);
		ctx.changed.push(r.number);
	};

	// The size a shared message will have in the private base: as Summary
	// counts it, with the header grown by the new tags (an estimate: the
	// header's stored length can differ by a few bytes)
	Store.prototype.incoming_size = function (mb, h, tags)
	{
		var size = blocks(header_length(mb, h) + tags.length + 4) + blocks(h.data_length || 0);
		if (h.auxattr & D.MSG_FILEATTACH) {
			var to = parseInt(h.to_ext, 10);
			if (to > 0) {
				var dir = this.data_dir + format("file/%04u.in/", to);
				var names = Summary.attach_names(h.subject || "");
				for (var i = 0; i < names.length; i++) {
					var n = file_size(dir + names[i]);
					if (n > 0)
						size += n;
				}
			}
		}
		return size;
	};

	// Make room for 'incoming' bytes under the quota by removing the oldest
	// Trash (by arrival), one at a time. Throws quota, removing nothing,
	// when the message could not fit even with all of Trash removed (by
	// the summary's sizes); also if Trash runs out anyway.
	Store.prototype.make_room = function (ctx, incoming)
	{
		var quota = this.options.quota_bytes;
		if (!(quota > 0) || ctx.bytes + incoming <= quota)
			return;
		var i;
		if (ctx.trash_order === null) {
			ctx.trash_order = [];
			for (i = ctx.rows.length - 1; i >= 0; i--)	// rows are newest first
				if (folder_of(ctx.rows[i]) == Tags.TRASH)
					ctx.trash_order.push(ctx.rows[i]);
		}
		var trash_bytes = 0;
		for (i = 0; i < ctx.trash_order.length; i++)
			if (!ctx.gone[ctx.trash_order[i].num])
				trash_bytes += ctx.trash_order[i].size;
		if (ctx.bytes - trash_bytes + incoming > quota)
			throw error("quota", "the mailbox is full");
		while (ctx.bytes + incoming > quota) {
			var row = null;
			while (ctx.trash_order.length > 0 && row === null) {
				row = ctx.trash_order.shift();
				if (ctx.gone[row.num])
					row = null;
			}
			if (row === null)
				throw error("quota", "the mailbox is full");
			var h = ctx.pmb.get_msg_header(false, row.num, false);
			if (!live(h, row.num) || Tags.parse(h.tags).folder != Tags.TRASH) {
				ctx.gone[row.num] = true;	// gone or moved meanwhile: not counted
				ctx.bytes -= row.size;
				continue;
			}
			this.destroy(ctx, row.num, h);
		}
	};

	// A base write that failed: the detail (which can name server paths)
	// goes to the log, the caller gets a generic "failed"
	Store.prototype.base_failure = function (what, mb)
	{
		log(LOG_ERR, "mailbox: user " + this.usernum + ": " + what + " in " + mb.file + ": " + mb.last_error);
		return error("failed", MESSAGES.failed);
	};

	// Remove a private message for real: the message, then its attachment
	// files (only from the user's own incoming directory, and only for mail
	// to the user; a file that cannot be removed is logged: an orphaned
	// file is harmless, a message without its files is not); the purge
	// follows at the end of the call
	Store.prototype.destroy = function (ctx, num, h)
	{
		var attached = (h.auxattr & D.MSG_FILEATTACH) != 0;
		var p = Tags.parse(h.tags);
		var msgid = h.id || "";
		if (!ctx.pmb.remove_msg(false, num))
			throw this.base_failure("remove_msg #" + num, ctx.pmb);
		// A Sent copy gone for real: keep the recipients' copies of it from
		// showing in Sent as extras
		if (msgid !== "" && (p.folder == Tags.SENT || p.was == Tags.SENT))
			ctx.hidden.push({ id: null, key: "m:" + msgid });
		ctx.removed.push(num);
		ctx.gone[num] = true;
		ctx.purge = true;
		set_trash(ctx, num, null);
		var row = ctx.by_num[num];
		if (row !== undefined)
			ctx.bytes -= row.size;
		if (attached)
			this.delete_attachments(h);
	};

	Store.prototype.delete_attachments = function (h)
	{
		if (parseInt(h.to_ext, 10) !== this.usernum)
			return;	// not the user's files
		var dir = this.data_dir + format("file/%04u.in/", this.usernum);
		var names = Summary.attach_names(h.subject || "");
		for (var i = 0; i < names.length; i++) {
			var path = dir + names[i];
			if (file_exists(path) && !file_remove(path))
				log(LOG_WARNING, "mailbox: user " + this.usernum + ": cannot remove attachment " + path);
		}
	};

	// ---- The move journal (mailbox.moving.json) ----
	// Every journal read and write happens inside the call's
	// Summary.modify() (lock order: summary, then moving), so this user's
	// moves are serialized with it; the file is read without its own lock
	// (every writer holds the summary lock, as the reader does) and written
	// only when something in it changed. Before a move between the bases
	// an entry keyed by the source id records the source's Message-ID (""
	// for none), import and written times, the destination's last message
	// number before the move ("after") and the destination tags. Once the
	// source is gone the entry is cleared. reconcile() runs at the start of
	// every write call, and settle() right after every move_msg() that did
	// not succeed (returned false or threw): each pins the copy a pending
	// move left by NUMBER ("copy") or drops the entry. A copy is reused
	// only by that number, and no number is pinned for two entries.
	// Invariant: nothing is added to a base while an entry for that
	// destination is unpinned and unsettled. If a needed journal write
	// (a new entry, a pin, a drop) cannot be saved, ctx.journal_ok is
	// cleared and the call makes no further move between the bases (each
	// such id gets conflict); the next call's reconcile then finds the base
	// exactly as the saved entry describes it.

	// A source or a copy: Message-ID and times (on unexpanded headers an
	// absent Message-ID is undefined, "" in the entry)
	function entry_matches(e, h)
	{
		return e !== null && typeof e == "object" && e.imported === h.when_imported_time
			&& e.written === h.when_written_time && e.msgid === (h.id || "");
	}

	// The destination of an entry: "p" (private) for a shared source
	function dest_of(key)
	{
		return key.charAt(0) == "s" ? "p" : "s";
	}

	// The copy numbers pinned by entries for destination 'to', but 'key'
	function pinned(journal, to, key)
	{
		var out = {};
		for (var k in journal)
			if (k !== key && dest_of(k) == to && typeof journal[k].copy == "number")
				out[journal[k].copy] = true;
		return out;
	}

	// The live destination header 'num' if it matches entry e, else null
	Store.prototype.copy_header = function (ctx, e, to_private, num)
	{
		var mb = to_private ? ctx.pmb : this.shared_base();
		if (mb === null || typeof num != "number")
			return null;
		var h = mb.get_msg_header(false, num, false);
		if (!live(h, num) || !entry_matches(e, h))
			return null;
		if (!to_private && parseInt(h.to_ext, 10) !== this.usernum)
			return null;
		return h;
	};

	// Entry e for source 'key' reconciled: null to drop it, or e with the
	// copy's number pinned in e.copy. taken: numbers other entries for the
	// same destination have pinned.
	Store.prototype.reconcile_entry = function (ctx, key, e, taken)
	{
		if (!ID_RE.test(key) || e === null || typeof e != "object" || typeof e.after != "number")
			return null;
		var num = parseInt(key.substr(1), 10);
		var to_private = key.charAt(0) == "s";
		var src = to_private ? this.shared_base() : ctx.pmb;
		if (src === null || ctx.pmb === null)
			return null;
		var h = src.get_msg_header(false, num, false);
		if (!live(h, num) || !entry_matches(e, h))
			return null;	// the source is gone: the move finished
		if (e.copy !== undefined) {
			if (taken[e.copy] || this.copy_header(ctx, e, to_private, e.copy) === null)
				return null;
			return e;
		}
		var copy = null;
		if (to_private) {
			// Every write to the private base is made under the summary
			// lock, and entries are settled before anything else is added,
			// so the only message a pending move can have added is the one
			// after 'after'. This assumes the base's last message number
			// never decreases: renumbering it (fixsmb, smbutil) while a
			// move is pending can make this rule pin the wrong message.
			var n = e.after + 1;
			if (ctx.pmb.last_msg > e.after && !taken[n] && this.copy_header(ctx, e, true, n) !== null)
				copy = n;
		} else {
			// This user's moves are serialized by the summary lock and
			// settled at once, so a matching record to the user above
			// 'after' not pinned by another entry can only be the pending
			// move's copy
			var idx = this.shared_base().get_index() || [];
			var u16 = this.usernum & 0xFFFF;
			for (var i = 0; i < idx.length && copy === null; i++) {
				var x = idx[i];
				if (x.number > e.after && !taken[x.number] && x.to == u16 && x.time === e.imported
					&& !(x.attr & (D.MSG_DELETE | D.MSG_VOTE))
					&& this.copy_header(ctx, e, false, x.number) !== null)
					copy = x.number;
			}
		}
		if (copy === null)
			return null;	// no copy was made
		e.copy = copy;
		return e;
	};

	// Save ctx.journal; false (logged) if it could not be saved. needed:
	// the call then makes no further move between the bases.
	Store.prototype.journal_write = function (ctx, needed)
	{
		var j = {};
		for (var k in ctx.journal)
			j[k] = ctx.journal[k];
		try {
			this.jf.update("moving", {}, function () { return j; });
			return true;
		} catch (e) {
			log(LOG_WARNING, "mailbox: user " + this.usernum + ": move journal not saved: "
				+ (e && e.message !== undefined ? e.message : e));
			if (needed)
				ctx.journal_ok = false;
			return false;
		}
	};

	// Reconcile the saved journal into ctx.journal (summary lock held),
	// saving it only if something changed
	Store.prototype.reconcile = function (ctx)
	{
		var j = this.jf.read("moving", null);
		if (j === null && file_exists(this.jf.path("moving")))
			log(LOG_WARNING, "mailbox: user " + this.usernum + ": move journal unreadable, ignored");
		if (j === null || typeof j != "object" || j instanceof Array)
			j = {};
		var before = JSON.stringify(j);
		var out = {};
		var k;
		var e;
		// Pinned entries first, so an unpinned one never takes their copy
		for (var pass = 0; pass < 2; pass++) {
			for (k in j) {
				e = j[k];
				var has_copy = e !== null && typeof e == "object" && e.copy !== undefined;
				if (has_copy != (pass == 0))
					continue;
				e = this.reconcile_entry(ctx, k, e, pinned(out, dest_of(k), k));
				if (e !== null)
					out[k] = e;
			}
		}
		ctx.journal = out;
		ctx.journal_ok = true;
		if (JSON.stringify(out) !== before)
			this.journal_write(ctx, true);
	};

	// Settle entry 'key' after its move_msg() did not succeed; then save
	Store.prototype.settle = function (ctx, key)
	{
		var e = null;
		try {
			e = this.reconcile_entry(ctx, key, ctx.journal[key], pinned(ctx.journal, dest_of(key), key));
		} catch (x) {
			// Cannot tell: keep the unsettled entry (saved as it was) and
			// make no further move in this call
			log(LOG_ERR, "mailbox: user " + this.usernum + ": cannot settle " + key + ": " + x);
			ctx.journal_ok = false;
			return;
		}
		if (e === null)
			delete ctx.journal[key];
		else
			ctx.journal[key] = e;
		this.journal_write(ctx, true);
	};

	// Before adding to destination 'to': settle every unpinned entry for
	// it (none, unless an earlier save failed); throws conflict if that
	// cannot be saved
	Store.prototype.settle_dest = function (ctx, to, key)
	{
		var changed = false;
		for (var k in ctx.journal) {
			if (k === key || dest_of(k) != to || ctx.journal[k].copy !== undefined)
				continue;
			var e = this.reconcile_entry(ctx, k, ctx.journal[k], pinned(ctx.journal, to, k));
			if (e === null)
				delete ctx.journal[k];
			else
				ctx.journal[k] = e;
			changed = true;
		}
		if (changed && !this.journal_write(ctx, true))
			throw error("conflict", "the move journal could not be saved");
	};

	// Move r (source id 'key') into base 'dest' with move_msg(), journaled:
	// returns the new number; whatever move_msg() does, the entry is
	// settled before this returns or throws
	Store.prototype.cross_move = function (ctx, key, r, dest, changes)
	{
		if (!ctx.journal_ok)
			throw error("conflict", "the move journal could not be saved");
		var to = dest_of(key);
		this.settle_dest(ctx, to, key);
		var h = r.header;
		ctx.journal[key] = { to: to, msgid: h.id || "", imported: h.when_imported_time,
			written: h.when_written_time, after: dest.last_msg, tags: changes.tags };
		if (!this.journal_write(ctx, true)) {
			delete ctx.journal[key];	// not saved: nothing to settle
			throw error("conflict", "the move journal could not be saved");
		}
		var n = false;
		var moved = false;
		try {
			n = r.base.move_msg(r.number, dest, changes);
			moved = typeof n == "number" && n > 0;
		} finally {
			if (moved) {
				// The source is gone: a clear that cannot be saved is
				// dropped by the next reconcile
				delete ctx.journal[key];
				this.journal_write(ctx, false);
			} else {
				if (to == "p")
					ctx.unknown = true;	// a copy may be here: rebuild the summary
				this.settle(ctx, key);
			}
		}
		if (!moved)
			throw this.base_failure("move_msg #" + r.number, r.base);
		return n;
	};

	Store.prototype.journal_clear = function (ctx, key)
	{
		delete ctx.journal[key];
		this.journal_write(ctx, false);
	};

	// The pinned copy of an interrupted move of source 'key' (header h), or
	// null
	Store.prototype.journal_copy = function (ctx, key, h, to_private)
	{
		var e = ctx.journal[key];
		if (e === undefined || typeof e.copy != "number" || !entry_matches(e, h))
			return null;
		if (to_private ? ctx.used[e.copy] : ctx.used["s" + e.copy])
			return null;
		return this.copy_header(ctx, e, to_private, e.copy);
	};

	// Move shared message r into the private base with these tags; returns
	// the private number. check_quota is false for a move to Trash
	// (deleting is how a user gets back under quota).
	Store.prototype.move_in = function (r, ctx, tags, check_quota)
	{
		var h = r.header;
		var attr = h.attr & ~D.MSG_SPAM;
		var pmb = ctx.pmb;
		var key = "s" + r.number;
		var ph = this.journal_copy(ctx, key, h, true);
		if (ph !== null) {	// finish an interrupted move
			var dup = ph.number;
			ctx.used[dup] = true;
			if ((ph.tags || "") !== tags || ph.attr !== attr) {
				ph.tags = tags;
				ph.attr = attr;
				this.put_header(pmb, dup, ph);
				ctx.changed.push(dup);
			}
			if (!r.base.remove_msg(false, r.number))
				throw this.base_failure("remove_msg #" + r.number, r.base);
			ctx.forget.push(r.number);
			this.journal_clear(ctx, key);
			return dup;
		}
		// A move the journal cannot record is refused before Trash is
		// touched (cross_move would refuse it after make_room)
		if (!ctx.journal_ok)
			throw error("conflict", "the move journal could not be saved");
		var size = this.incoming_size(r.base, h, tags);
		if (check_quota)
			this.make_room(ctx, size);
		var n = this.cross_move(ctx, key, r, pmb, { tags: tags, attr: attr });
		ctx.added.push(n);
		ctx.used[n] = true;
		ctx.bytes += size;
		ctx.forget.push(r.number);
		return n;
	};

	// The shared base's age limit in days (0: none), which mail maintenance
	// applies to the index (import) time of every message in it
	Store.prototype.inbox_max_age = function ()
	{
		var n = this.shared_base().max_age;
		return typeof n == "number" && n > 0 ? n : 0;
	};

	// Why private message h cannot go back to the shared base, or null:
	// invalid unless it is mail received by the user (to the user, not
	// network mail); expired when its import time is older than the shared
	// base's max_age (as mail maintenance counts it: whole days), also when
	// an interrupted move already left its copy there (see out_refusal).
	// The spam age limit, which scripts cannot read, is not checked.
	Store.prototype.inbox_refusal = function (h, now)
	{
		if (parseInt(h.to_ext, 10) !== this.usernum || (h.to_net_type || D.NET_NONE) != D.NET_NONE)
			return error("invalid", "only mail received by the user can go to the Inbox");
		var days = this.inbox_max_age();
		var t = h.when_imported_time;
		if (days > 0 && now > t && Math.floor((now - t) / DAY) > days)
			return error("expired", "older than the Inbox keeps mail");
		return null;
	};

	// inbox_refusal() for private message r. When r is expired and an
	// interrupted move already left its copy in the shared base, that copy
	// (which maintenance would delete, leaving the user none) is removed
	// and the journal entry cleared; r itself is kept. A copy that cannot
	// be removed fails the id (failed), the entry kept. A clear that cannot
	// be saved stops further moves between the bases in the call.
	Store.prototype.out_refusal = function (r, ctx)
	{
		var why = this.inbox_refusal(r.header, ctx.now);
		if (why === null || why.code != "expired")
			return why;
		var key = "p" + r.number;
		var sh = this.journal_copy(ctx, key, r.header, false);
		if (sh !== null) {
			var smb = this.shared_base();
			if (!smb.remove_msg(false, sh.number))
				throw this.base_failure("remove_msg #" + sh.number, smb);
			ctx.used["s" + sh.number] = true;
			ctx.forget.push(sh.number);
			delete ctx.journal[key];
			this.journal_write(ctx, true);
		}
		return why;
	};

	// Move private message r back to the shared base (Inbox, or Spam); only
	// what out_refusal() allows. Its flags and foreign tags go with it;
	// its folder does not.
	Store.prototype.move_out = function (id, r, ctx, p, spam)
	{
		var h = r.header;
		var key = "p" + r.number;
		var why = this.out_refusal(r, ctx);
		if (why !== null)
			throw why;
		var sh = this.journal_copy(ctx, key, h, false);
		var smb = this.shared_base();
		var attr = spam ? (h.attr | D.MSG_SPAM) : (h.attr & ~D.MSG_SPAM);
		var tags = Tags.format({ other: p.other });
		var s;
		if (sh !== null) {	// finish an interrupted move
			s = sh.number;
			ctx.used["s" + s] = true;
			if (sh.attr !== attr || (sh.tags || "") !== tags) {
				sh.attr = attr;
				sh.tags = tags;
				this.put_header(smb, s, sh);
				ctx.forget.push(s);
			}
			if (!r.base.remove_msg(false, r.number))
				throw this.base_failure("remove_msg #" + r.number, r.base);
			this.journal_clear(ctx, key);
		} else {
			s = this.cross_move(ctx, key, r, smb, { tags: tags, attr: attr });
			ctx.used["s" + s] = true;
		}
		ctx.removed.push(r.number);
		ctx.gone[r.number] = true;
		set_trash(ctx, r.number, null);
		var row = ctx.by_num[r.number];
		if (row !== undefined)
			ctx.bytes -= row.size;
		return done(id, "s" + s);
	};

	Store.prototype.op_set = function (id, ctx, ch)
	{
		var r = this.resolve_now(id);
		if (r.role == "from")
			throw error("forbidden", "a sent message is read-only");
		if (ch.spam !== undefined && r.kind != "s")
			throw error("invalid", "spam applies to Inbox and Spam only");
		var h = r.header;
		var attr = h.attr;
		if (ch.read !== undefined)
			attr = ch.read ? (attr | D.MSG_READ) : (attr & ~D.MSG_READ);
		if (ch.spam !== undefined)
			attr = ch.spam ? (attr | D.MSG_SPAM) : (attr & ~D.MSG_SPAM);
		if (ch.starred !== undefined)
			attr = ch.starred ? (attr | D.MSG_VALIDATED) : (attr & ~D.MSG_VALIDATED);
		if (attr === h.attr)
			return done(id);
		h.attr = attr;
		this.put_header(r.base, r.number, h);
		if (r.kind == "p")
			ctx.changed.push(r.number);
		else
			ctx.touched = true;	// attributes come from the index: no fact changed
		return done(id);
	};

	// tag: the destination folder tag, null for the Inbox
	Store.prototype.op_move = function (id, ctx, tag)
	{
		var r = this.resolve_now(id);
		if (r.role == "from")
			throw error("forbidden", "a sent message is read-only");
		var h = r.header;
		var p = Tags.parse(h.tags, { shared: r.kind == "s" });
		if (r.kind == "s") {
			if (tag === null) {	// Spam to Inbox: not spam
				if (h.attr & D.MSG_SPAM) {
					h.attr &= ~D.MSG_SPAM;
					this.put_header(r.base, r.number, h);
					ctx.forget.push(r.number);
				}
				return done(id);
			}
			var n = this.move_in(r, ctx, Tags.format({ folder: tag, other: p.other }), true);
			return done(id, "p" + n);
		}
		if (tag === null)
			return this.move_out(id, r, ctx, p, false);
		if (p.folder == Tags.TRASH)
			set_trash(ctx, r.number, null);
		p.folder = tag;
		p.was = "";
		this.retag(r, ctx, p);
		return done(id);
	};

	Store.prototype.op_remove = function (id, ctx)
	{
		var r = this.resolve_now(id);
		if (r.role == "from") {	// a sent extra: hide it, nothing else
			var mid = r.header.id || "";
			ctx.hidden.push({ id: id, key: mid !== "" ? "m:" + mid : id });
			return done(id, null);
		}
		var h = r.header;
		var p = Tags.parse(h.tags, { shared: r.kind == "s" });
		if (r.kind == "s") {
			var was = (h.attr & D.MSG_SPAM) ? WAS_SPAM : WAS_INBOX;
			var n = this.move_in(r, ctx, Tags.format({ folder: Tags.TRASH, was: was, other: p.other }), false);
			set_trash(ctx, n, ctx.now);
			return done(id, "p" + n);
		}
		if (p.folder == Tags.TRASH) {
			this.destroy(ctx, r.number, h);
			return done(id, null);
		}
		p.was = p.folder === "" ? Tags.ARCHIVE : p.folder;
		p.folder = Tags.TRASH;
		this.retag(r, ctx, p);
		set_trash(ctx, r.number, ctx.now);
		return done(id);
	};

	Store.prototype.op_restore = function (id, ctx)
	{
		var r = this.resolve_now(id);
		var p = r.kind == "p" ? Tags.parse(r.header.tags) : null;
		if (p === null || p.folder != Tags.TRASH)
			throw error("invalid", "not in Trash");
		var was = p.was;
		var why = null;
		if (was == WAS_INBOX || was == WAS_SPAM) {
			// Back to the shared base when it may go there, else to Archive
			why = this.out_refusal(r, ctx);
			if (why === null)
				return this.move_out(id, r, ctx, p, was == WAS_SPAM);
			was = Tags.ARCHIVE;
		}
		if (valid_user_tag(was))
			ctx.folders.push(was);	// recreated if it was deleted
		else if (was != Tags.SENT)
			was = Tags.ARCHIVE;
		p.folder = was;
		p.was = "";
		this.retag(r, ctx, p);
		set_trash(ctx, r.number, null);
		var out = done(id);
		if (why !== null) {
			out.folder = "archive";
			if (why.code == "expired")
				out.expired = true;
		}
		return out;
	};

	// Remove a Trash message for real (empty_trash, purge_expired)
	Store.prototype.op_destroy_trash = function (id, ctx)
	{
		var r = this.resolve_now(id);
		if (r.kind != "p" || Tags.parse(r.header.tags).folder != Tags.TRASH)
			throw error("invalid", "not in Trash");
		this.destroy(ctx, r.number, r.header);
		return done(id, null);
	};

	function trash_ids(rows)
	{
		var out = [];
		for (var i = 0; i < rows.length; i++)
			if (folder_of(rows[i]) == Tags.TRASH)
				out.push("p" + rows[i].num);
		return out;
	}

	Store.prototype.set = function (ids, changes)
	{
		if (changes === null || typeof changes != "object")
			throw error("invalid", "changes is not an object");
		var ch = {};
		var keys = ["read", "starred", "spam"];
		for (var i = 0; i < keys.length; i++)
			if (changes[keys[i]] !== undefined && changes[keys[i]] !== null)
				ch[keys[i]] = !!changes[keys[i]];
		return this.run(ids, false, function (id, ctx) {
			return this.op_set(id, ctx, ch);
		});
	};

	Store.prototype.move = function (ids, folder_id)
	{
		if (!(ids instanceof Array))
			throw error("invalid", "ids is not an array");
		if (typeof folder_id != "string")
			throw error("invalid", "folder id is not a string");
		var tag = null;
		if (folder_id == "archive")
			tag = Tags.ARCHIVE;
		else if (folder_id != "inbox")
			tag = user_folder_tag(folder_id);
		return this.run(function (ctx) {
			if (tag !== null && tag != Tags.ARCHIVE && this.user_tags(ctx.rows).indexOf(tag) < 0)
				throw error("notfound", "no such folder");
			return ids;
		}, tag !== null, function (id, ctx) {
			return this.op_move(id, ctx, tag);
		});
	};

	Store.prototype.remove = function (ids)
	{
		if (!(ids instanceof Array))
			throw error("invalid", "ids is not an array");
		// The private base is created only for a shared message to the user
		// (hiding a sent extra changes nothing else)
		function create() {
			for (var i = 0; i < ids.length; i++) {
				if (typeof ids[i] != "string" || ids[i].charAt(0) != "s")
					continue;
				try {
					if (this.resolve_now(ids[i]).role == "to")
						return true;
				} catch (e) {
					// reported by op_remove
				}
			}
			return false;
		}
		return this.run(ids, create, this.op_remove);
	};

	Store.prototype.restore = function (ids)
	{
		return this.run(ids, false, this.op_restore);
	};

	Store.prototype.empty_trash = function ()
	{
		return this.run_sliced(function (ctx) {
			return trash_ids(ctx.rows);
		}, this.op_destroy_trash, false);
	};

	// Remove Trash older than trash_days (counted from when each message
	// was put in Trash), at most once a day: null when skipped, else the
	// results for the messages removed
	Store.prototype.purge_expired = function (now)
	{
		if (typeof now != "number")
			now = this.now();
		if (!file_exists(this.private_path + ".shd"))
			return [];	// no private base, so no Trash: nothing written
		var s = this.jf.read("state", null);
		var last = (s !== null && typeof s == "object" && typeof s.expired == "number") ? s.expired : null;
		if (last !== null && now >= last && now - last < DAY)
			return null;
		var days = this.options.trash_days;
		var t = this.jf.read("trash", {});
		if (t === null || typeof t != "object" || t instanceof Array)
			t = {};
		var res = this.run_sliced(function (ctx) {
			var out = [];
			ctx.keep = {};
			for (var i = 0; i < ctx.rows.length; i++) {
				var num = ctx.rows[i].num;
				if (folder_of(ctx.rows[i]) != Tags.TRASH)
					continue;
				ctx.keep[num] = true;
				var when = t[num];
				if (typeof when != "number") {	// first seen now
					when = now;
					set_trash(ctx, num, now);
				}
				if (days > 0 && when <= now - days * DAY)
					out.push("p" + num);
			}
			return out;
		}, this.op_destroy_trash, true);
		try {
			this.jf.update("state", {}, function (o) {
				if (o === null || typeof o != "object" || o instanceof Array)
					o = {};
				o.expired = now;
				return o;
			});
		} catch (e) {
			this.log_original("expiry time not saved", e);	// it may run again today
		}
		return res;
	};

	// The names of these folder tags, leaving out one (except)
	Store.prototype.folder_names = function (tags, except)
	{
		var names = [];
		for (var i = 0; i < tags.length; i++)
			if (tags[i] !== except)
				names.push(folder_name(tags[i]));
		return names;
	};

	// Check a new folder name against the others; returns its tag
	function new_folder_tag(name, names)
	{
		var reason = Tags.validate(name, names);
		if (reason !== null)
			throw name_error(reason);
		return Tags.encode(String(name).replace(TRIM_RE, ""));
	}

	Store.prototype.folder_create = function (name)
	{
		this.refresh();
		var tags = this.user_tags(this.private_rows());
		var tag = new_folder_tag(name, this.folder_names(tags));
		if (tags.indexOf(tag) >= 0)
			throw name_error("duplicate");
		this.jf.update("folders", [], function (list) {
			list = folder_list(list);
			if (list.indexOf(tag) >= 0)
				throw name_error("duplicate");
			list.push(tag);
			return list;
		});
		return { id: "u:" + tag, name: folder_name(tag) };
	};

	Store.prototype.folder_rename = function (folder_id, name)
	{
		var tag = user_folder_tag(folder_id);
		this.refresh();
		var opts = { data_dir: this.data_dir };
		var pmb = this.private_base(false);
		var rows = pmb === null ? [] : Summary.get(pmb, this.jf, opts);
		var tags = this.user_tags(rows);
		if (tags.indexOf(tag) < 0)
			throw error("notfound", "no such folder");
		var newtag = new_folder_tag(name, this.folder_names(tags, tag));
		if (newtag === tag)
			return { id: folder_id, name: folder_name(tag), errors: [] };
		if (tags.indexOf(newtag) >= 0)
			throw name_error("duplicate");
		var nums = [];
		for (var i = 0; i < rows.length; i++) {
			var p = Tags.parse(rows[i].tags);
			if (p.folder === tag || (p.folder == Tags.TRASH && p.was === tag))
				nums.push(rows[i].num);
		}
		var self = this;
		var errors = [];
		var renamed = 0;
		// In slices, one Summary.modify() each (the lock is released
		// between them). A message that fails is reported in errors and
		// keeps the old tag (it shows as a folder of that name); what was
		// retagged is still recorded in the summary. A slice that cannot
		// take the lock: the first throws conflict (nothing changed), a
		// later one reports its messages and the rest in errors.
		function retag_slice(part) {
			var changed = [];
			for (var j = 0; j < part.length; j++) {
				try {
					var h = pmb.get_msg_header(false, part[j], false);
					if (!live(h, part[j]))
						continue;
					var q = Tags.parse(h.tags);
					if (q.folder === tag)
						q.folder = newtag;
					else if (q.folder == Tags.TRASH && q.was === tag)
						q.was = newtag;
					else
						continue;
					h.tags = Tags.format(q);
					self.put_header(pmb, part[j], h);
					changed.push(part[j]);
				} catch (e) {
					errors.push(self.failure("p" + part[j], e));
				}
			}
			renamed += changed.length;
			return { changed: changed };
		}
		var size = this.options.slice_size;
		for (var start = 0; start < nums.length; start += size) {
			var part = nums.slice(start, start + size);
			try {
				if (start > 0) {	// another writer may have run between slices
					this.refresh();
					pmb = this.private_base(false);
					if (pmb === null)
						throw error("notfound", "no such message");
				}
				Summary.modify(pmb, this.jf, function () {
					return retag_slice(part);
				}, opts);
			} catch (e) {
				if (start == 0)
					throw e;	// nothing was changed
				for (i = start; i < nums.length; i++)
					errors.push(this.failure("p" + nums[i], e));
				break;
			}
		}
		// Nothing renamed although there was something to rename: the
		// folder keeps its name, and the folders list is not touched
		if (renamed == 0 && errors.length > 0)
			return { id: folder_id, name: folder_name(tag), errors: errors };
		var saved = this.update_folders(function (list) {
			var k = list.indexOf(tag);
			if (k >= 0)
				list[k] = newtag;
			else if (list.indexOf(newtag) < 0)
				list.push(newtag);
			return list;
		});
		if (!saved && renamed == 0)
			throw error("conflict", "the mailbox is busy");	// nothing was changed
		return { id: "u:" + newtag, name: folder_name(newtag), errors: errors };
	};

	// Change mailbox.folders.json, trying a few times; false if its lock
	// could not be taken (logged). Used after the messages were changed:
	// a stale folders.json is the recoverable state (the messages' tags
	// still list every folder that holds mail).
	Store.prototype.update_folders = function (fn)
	{
		var timeout = this.jf.timeout;
		this.jf.timeout = Math.min(timeout, 1000);	// about 3 s at most
		try {
			for (var attempt = 0; attempt < 3; attempt++) {
				try {
					this.jf.update("folders", [], function (list) {
						return fn(folder_list(list));
					});
					return true;
				} catch (e) {
					if (!is_conflict(e))
						throw e;
				}
			}
		} finally {
			this.jf.timeout = timeout;
		}
		log(LOG_WARNING, "mailbox: user " + this.usernum + ": folders list busy, not updated");
		return false;
	};

	Store.prototype.folder_delete = function (folder_id)
	{
		var tag = user_folder_tag(folder_id);
		var res = this.run_sliced(function (ctx) {
			if (this.user_tags(ctx.rows).indexOf(tag) < 0)
				throw error("notfound", "no such folder");
			var out = [];
			for (var i = 0; i < ctx.rows.length; i++)
				if (folder_of(ctx.rows[i]) === tag)
					out.push("p" + ctx.rows[i].num);
			return out;
		}, function (id, ctx) {
			var r = this.resolve_now(id);
			var p = Tags.parse(r.header.tags);
			if (p.folder !== tag)
				throw error("notfound", "no longer in the folder");
			p.folder = Tags.TRASH;
			p.was = tag;
			this.retag(r, ctx, p);
			set_trash(ctx, r.number, ctx.now);
			return done(id);
		}, true);
		// A message still in the folder keeps the folder listed (by its tag)
		for (var i = 0; i < res.length; i++)
			if (!res[i].ok && res[i].error != "notfound")
				return res;
		var saved = this.update_folders(function (list) {
			var k = list.indexOf(tag);
			if (k >= 0)
				list.splice(k, 1);
			return list;
		});
		if (!saved) {
			var moved = false;
			for (i = 0; i < res.length; i++)
				if (res[i].ok)
					moved = true;
			if (!moved)	// nothing was changed
				throw error("conflict", "the mailbox is busy");
		}
		return res;
	};

	// {bytes, quota}: the summed stored size of the private base's messages
	Store.prototype.usage = function ()
	{
		this.refresh();
		var pmb = this.private_base(false);
		return { bytes: pmb === null ? 0 : Summary.usage(pmb, this.jf, { data_dir: this.data_dir }).bytes,
			quota: this.options.quota_bytes };
	};

	// ---- Reading one message ----

	var PART_ID_RE = /^[0-9]{1,6}(\.[0-9]{1,6}){0,19}$/;
	var FILE_PID_RE = /^f[1-9][0-9]{0,5}$/;
	var CHUNK = 65536;

	// A display-string converter for a header: UTF-8 as is when auxattr
	// has MSG_HFIELDS_UTF8, else from CP437
	function hstr(h)
	{
		var utf8 = (h.auxattr & D.MSG_HFIELDS_UTF8) != 0;
		return function (s) {
			if (s === undefined || s === null)
				return "";
			s = String(s);
			return utf8 ? s : utf8_encode(s);
		};
	}

	// A line of unknown origin to UTF-8: kept if valid UTF-8, else CP437
	function line_utf8(s)
	{
		s = String(s);
		return str_is_utf8(s) ? s : utf8_encode(s);
	}

	function net_name(type)
	{
		switch (type || D.NET_NONE) {
			case D.NET_NONE:
				return "local";
			case D.NET_INTERNET:
				return "internet";
			case D.NET_FIDO:
				return "fido";
			case D.NET_QWK:
				return "qwk";
		}
		return "other";
	}

	// The display name of the stored original From line, "" if none
	function from_display(h, str)
	{
		var list = h.field_list || [];
		for (var i = 0; i < list.length; i++) {
			if (list[i].type != D.RFC822FROM)
				continue;
			var m = String(list[i].data).match(/^[ \t]*(.*?)[ \t]*<[^<>]*>[ \t]*$/);
			if (!m || m[1] === "")
				return "";
			var name = m[1];
			if (name.length >= 2 && name.charAt(0) == '"' && name.charAt(name.length - 1) == '"')
				name = name.substr(1, name.length - 2).replace(/\\(.)/g, "$1");
			return MimeRead.decode_words(name, str).replace(/^[ \t]+|[ \t]+$/g, "");
		}
		return "";
	}

	// The domain of the address the message was delivered to (the SMTP
	// forward path, else the recipient's network address), "" if none
	function to_domain(h)
	{
		var addr = h.forward_path || "";
		if (addr === "" && (h.to_net_type || D.NET_NONE) == D.NET_INTERNET)
			addr = h.to_net_addr || "";
		addr = String(addr).replace(/^[ \t<]+|[ \t>]+$/g, "");
		var at = addr.lastIndexOf('@');
		if (at < 0)
			return "";
		var dom = addr.substr(at + 1).toLowerCase();
		return /^[a-z0-9]([a-z0-9.-]*[a-z0-9])?$/.test(dom) ? dom : "";
	}

	function comment_of(h, str)
	{
		var list = h.field_list || [];
		var out = [];
		for (var i = 0; i < list.length; i++)
			if (list[i].type == D.SMB_COMMENT)
				out.push(str(list[i].data));
		return out.join("\r\n");
	}

	function header_values(h)
	{
		var str = hstr(h);
		var from = h.from;	// a non-NULL field first (lazy fields)
		var shown = from_display(h, str);
		var reply_to = "";
		if (h.replyto_list)
			reply_to = MimeRead.decode_words(h.replyto_list, str);
		else if (h.replyto_net_addr)
			reply_to = str(h.replyto_net_addr);
		return {
			from: shown !== "" ? shown : str(from),
			from_addr: h.from_net_type ? str(h.from_net_addr) : "",
			to: h.to_list ? MimeRead.decode_words(h.to_list, str) : str(h.to),
			cc: h.cc_list ? MimeRead.decode_words(h.cc_list, str) : "",
			reply_to: reply_to,
			date: str(h.date),
			message_id: str(h.id),
			reply_id: str(h.reply_id),
			net_type: net_name(h.from_net_type),
			to_domain: to_domain(h)
		};
	}

	// A subject as a download file name: safe characters only, ".eml"
	// (then MimeRead.safe_name(), for device names such as CON)
	function eml_name(subject)
	{
		var s = String(subject).replace(/[^A-Za-z0-9 ._-]+/g, "_").replace(/_+/g, "_");
		s = s.replace(/^[ _.]+|[ _]+$/g, "").substr(0, 100).replace(/[ _.]+$/, "");
		return MimeRead.safe_name((s === "" ? "message" : s) + ".eml", "message/rfc822");
	}

	var LINE_BREAK_RE = /[ \t]*(\r\n|\r|\n)[ \t]*/g;

	function one_line(v)
	{
		return typeof v == "string" ? v.replace(LINE_BREAK_RE, " ") : v;
	}

	// The header block of get_rfc822_header() (822header.js), made safe:
	// stored header fields can hold CR/LF (a script's save_msg(), an
	// imported subject), which would forge header lines and end the block
	// early. get_rfc822_header() is run on a plain copy of the header whose
	// string values (and field_list data) have every line break replaced by
	// a space, since a forged line looks like any other once it is in the
	// output. The result is then checked line by line: a field line
	// (name:) or a continuation (space or tab) is kept; any other line
	// becomes a continuation of the previous field and empty ones are
	// dropped; the block ends with exactly one blank line.
	function rfc822_block(h)
	{
		var num = h.number;	// a non-NULL field first (lazy fields)
		var copy = { number: num };
		for (var k in h) {
			var v = h[k];
			if (k == "rfc822" || k == "field_list" || typeof v == "function")
				continue;
			copy[k] = one_line(v);
		}
		var list = h.field_list || [];
		copy.field_list = [];
		for (var i = 0; i < list.length; i++)
			copy.field_list.push({ type: list[i].type, data: one_line(String(list[i].data)) });
		var block = MsgBase.HeaderPrototype.get_rfc822_header.call(copy);
		var lines = String(block).split(/\r\n|\r|\n/);
		var out = [];
		for (i = 0; i < lines.length; i++) {
			var line = lines[i];
			if (/^[!-9;-~]+:/.test(line) || (/^[ \t]/.test(line) && out.length > 0 && /[^ \t]/.test(line)))
				out.push(line);
			else if (/[^ \t]/.test(line) && out.length > 0)
				out.push(" " + line);
		}
		return out.join("\r\n") + "\r\n\r\n";
	}

	function read_all(path)
	{
		var f = new File(path);
		if (!f.open("rb"))
			return null;
		var s = f.read();
		f.close();
		return typeof s == "string" ? s : null;
	}

	// A new file name under system.temp_dir (nothing is created)
	Store.prototype.temp_path = function (kind)
	{
		var dir = slash(system.temp_dir);
		mkpath(dir);
		var p;
		do {
			p = dir + format("mailbox_%s_%u_%x_%x.tmp", kind, this.usernum, time(), random(0x7fffffff));
		} while (file_exists(p));
		return p;
	};

	// A failure reading a message: the detail (which can name server
	// paths) goes to the log, the caller gets a generic "failed"
	Store.prototype.read_failure = function (what, mb)
	{
		log(LOG_ERR, "mailbox: user " + this.usernum + ": " + what + " in " + mb.file + ": " + mb.last_error);
		return error("failed", MESSAGES.failed);
	};

	// The live header with fields expanded (for display), else notfound
	function display_header(r)
	{
		var h = r.base.get_msg_header(false, r.number);
		if (!live(h, r.number))
			throw error("notfound", "no such message");
		return h;
	}

	// Set MSG_READ once: a shared message only when it is to the user (a
	// sent extra never), a private one inside Summary.modify(). A read
	// does not fail over it: a busy summary or a failed write leaves the
	// message unread (logged).
	Store.prototype.mark_read = function (r)
	{
		if (r.role == "from" || (r.header.attr & D.MSG_READ))
			return;
		var self = this;
		try {
			if (r.kind == "s") {
				r.header.attr |= D.MSG_READ;
				this.put_header(r.base, r.number, r.header);
				this.forget_written([r.number]);
				return;
			}
			var pmb = r.base;
			var num = r.number;
			Summary.modify(pmb, this.jf, function () {
				var h = pmb.get_msg_header(false, num, false);
				if (!live(h, num) || (h.attr & D.MSG_READ))
					return { changed: [] };
				h.attr |= D.MSG_READ;
				self.put_header(pmb, num, h);
				return { changed: [num] };
			}, { data_dir: this.data_dir });
		} catch (e) {
			if (e instanceof Error || !e || (e.code !== "conflict" && e.code !== "failed"))
				throw e;
			if (e.code == "conflict")
				log(LOG_WARNING, "mailbox: user " + this.usernum + ": not marked read (busy)");
		}
	};

	// The list item (as list() returns it) of a resolved message
	Store.prototype.item_of = function (r)
	{
		var c;
		var s = null;
		if (r.kind == "p") {
			var rows = this.private_rows();
			for (var i = 0; i < rows.length && s === null; i++)
				if (rows[i].num == r.number)
					s = rows[i];
			if (s === null)
				s = summarize_shared(r.base, r.header);
			var p = Tags.parse(s.tags);
			c = { kind: "p", num: r.number, extra: false, folder: private_folder(p), parsed: p };
		} else {
			s = summarize_shared(r.base, r.header);
			c = { kind: "s", num: r.number, extra: r.role == "from",
				folder: r.role == "from" ? "sent" : ((s.attr & D.MSG_SPAM) ? "spam" : "inbox") };
		}
		return to_item(c, s);
	};

	// The directory this user may be served file attachments from for
	// header h: their own incoming files, or their outgoing files for mail
	// they sent to no user number; null otherwise. A name is a base name
	// (no separator, ':' or control character) and the path is also
	// checked lexically to stay in the directory. A symbolic link in the
	// directory cannot be told from a file: the JavaScript API has no
	// lstat().
	Store.prototype.file_dir = function (h)
	{
		var to = parseInt(h.to_ext, 10);
		if (to > 0)
			return to === this.usernum ? this.data_dir + format("file/%04u.in/", to) : null;
		if (parseInt(h.from_ext, 10) === this.usernum)
			return this.data_dir + format("file/%04u.out/", this.usernum);
		return null;
	};

	// MimeRead.file_parts() of h, with path null and size -1 for a file
	// this user may not be served
	Store.prototype.file_parts = function (h)
	{
		var list = MimeRead.file_parts(h, this.data_dir);
		var dir = this.file_dir(h);
		var root = dir === null ? null : fullpath(dir);
		for (var i = 0; i < list.length; i++) {
			var f = list[i];
			if (f.path === null || dir === null || f.path.substr(0, dir.length) !== dir
				|| fullpath(f.path).substr(0, root.length) !== root) {
				f.path = null;
				f.size = -1;
			}
		}
		return list;
	};

	// The parts list of a message (get_msg_parts), or throws failed
	Store.prototype.parts_of = function (r)
	{
		var parts = r.base.get_msg_parts(r.number);
		if (!(parts instanceof Array))
			throw this.read_failure("get_msg_parts #" + r.number, r.base);
		return parts;
	};

	function part_entry(part)
	{
		var cid = typeof part.content_id == "string" ? part.content_id : "";
		return { pid: part.id, name: MimeRead.part_name(part), type: MimeRead.safe_type(part.content_type),
			size: part.size, cid: cid, inline: cid !== "" };
	}

	// A body part as {b64, charset}; null (and the part added to 'list')
	// when it is over max_text_part_bytes or cannot be read. plain: the
	// message is not MIME (BBS text: Ctrl-A and ANSI codes are stripped).
	Store.prototype.body_of = function (r, part, plain, list)
	{
		var max = this.options.max_text_part_bytes;
		if (!(part.size <= max)) {
			list.push(part_entry(part));
			return null;
		}
		var tmp = this.temp_path("part");
		var data = null;
		try {
			if (!r.base.save_msg_part(r.number, part.id, tmp))
				this.read_failure("save_msg_part #" + r.number + " part " + part.id, r.base);
			else if (file_size(tmp) <= max)	// the reported size is not trusted
				data = read_all(tmp);
		} finally {
			file_remove(tmp);
		}
		if (data === null) {
			list.push(part_entry(part));
			return null;
		}
		var charset = part.charset || "";
		if (plain)
			data = strip_ansi(strip_ctrl_a(data));
		if (charset == "cp437") {
			data = utf8_encode(data);
			charset = "utf-8";
		}
		return { b64: data === "" ? "" : base64_encode(data), charset: charset };
	};

	// The message: the list item plus {headers, comment, text, html, parts}.
	// opts.mark_read sets the read attribute (see mark_read); opts.head
	// returns the list item alone.
	Store.prototype.get = function (id, opts)
	{
		opts = opts || {};
		var r = this.resolve(id);
		if (opts.mark_read)
			this.mark_read(r);
		if (opts.head)
			return this.item_of(r);
		var h = display_header(r);
		var out = this.item_of(r);
		var str = hstr(h);
		out.headers = header_values(h);
		out.comment = comment_of(h, str);

		var parts = r.base.get_msg_parts(r.number);
		if (!(parts instanceof Array)) {
			this.read_failure("get_msg_parts #" + r.number, r.base);	// logged; no bodies
			parts = [];
		}
		var plain = !(h.mime_version && h.content_type);
		var m = MimeRead.model(parts);
		var by_id = {};
		for (var i = 0; i < parts.length; i++)
			by_id[parts[i].id] = parts[i];
		var bodies = [];
		out.text = m.text_id === null ? null : this.body_of(r, by_id[m.text_id], plain, bodies);
		out.html = m.html_id === null ? null : this.body_of(r, by_id[m.html_id], plain, bodies);
		var list = bodies.concat(m.attachments);
		var files = this.file_parts(h);
		for (i = 0; i < files.length; i++)
			list.push({ pid: files[i].pid, name: files[i].name, type: files[i].type, size: files[i].size });
		out.parts = list;
		return out;
	};

	// Show original: {rfc822, stored, external_files}, all UTF-8
	Store.prototype.original = function (id)
	{
		var r = this.resolve(id);
		var h = display_header(r);
		var stored = r.base.dump_msg_header(h) || [];
		var lines = [];
		for (var i = 0; i < stored.length; i++)	// each one line: no stored CR/LF
			lines.push(line_utf8(one_line(stored[i])));
		return { rfc822: line_utf8(rfc822_block(h)), stored: lines,
			external_files: (h.auxattr & D.MSG_FILEATTACH) != 0 };
	};

	// The message as an RFC 822 file: {path, name, size, temp:true}; the
	// caller deletes the file. rfc822_block() ends with the blank line;
	// the stored body follows undecoded (part "0"), copied in pieces.
	Store.prototype.raw_file = function (id)
	{
		var r = this.resolve(id);
		var h = display_header(r);
		var head = rfc822_block(h);
		var body = this.temp_path("body");
		var path = this.temp_path("raw");
		var ok = false;
		try {
			if (!r.base.save_msg_part(r.number, "0", body))
				throw this.read_failure("save_msg_part #" + r.number + " body", r.base);
			var src = new File(body);
			var dst = new File(path);
			if (!src.open("rb"))
				throw this.read_failure("open of the saved body of #" + r.number, r.base);
			try {
				if (!dst.open("wb"))
					throw this.read_failure("create of " + path + " for #" + r.number, r.base);
				try {
					var good = dst.write(head);
					var chunk;
					while (good && (chunk = src.read(CHUNK)) && chunk.length > 0)
						good = dst.write(chunk);
				} finally {
					dst.close();
				}
				if (!good || src.error)
					throw this.read_failure("write of " + path + " for #" + r.number, r.base);
			} finally {
				src.close();
			}
			ok = true;
		} finally {
			file_remove(body);
			if (!ok)
				file_remove(path);
		}
		return { path: path, name: eml_name(hstr(h)(h.subject)), size: file_size(path), temp: true };
	};

	// One part as a file: {path, name, type, size, temp}. A MIME part
	// (pid from get_msg_parts for this message) is decoded into a temp file
	// the caller deletes (temp true); a file attachment ("f<n>") is the
	// real file (temp false: the caller must not delete it).
	Store.prototype.part_file = function (id, pid)
	{
		if (typeof pid != "string" || (!FILE_PID_RE.test(pid) && !PART_ID_RE.test(pid)))
			throw error("invalid", "bad part id");
		var r = this.resolve(id);
		var i;
		if (pid.charAt(0) == "f") {
			var files = this.file_parts(display_header(r));
			for (i = 0; i < files.length; i++) {
				var f = files[i];
				if (f.pid === pid && f.path !== null && f.size >= 0 && file_exists(f.path))
					return { path: f.path, name: f.name, type: f.type, size: file_size(f.path), temp: false };
			}
			throw error("notfound", "no such attachment");
		}
		var parts = this.parts_of(r);
		var part = null;
		for (i = 0; i < parts.length && part === null; i++)
			if (parts[i].id === pid)
				part = parts[i];
		if (part === null)
			throw error("notfound", "no such part");
		var path = this.temp_path("part");
		if (!r.base.save_msg_part(r.number, part.id, path)) {
			file_remove(path);
			throw this.read_failure("save_msg_part #" + r.number + " part " + part.id, r.base);
		}
		return { path: path, name: MimeRead.part_name(part), type: MimeRead.safe_type(part.content_type),
			size: file_size(path), temp: true };
	};

	// Every public method: what it throws leaves through outgoing()
	var PUBLIC = ["open", "close", "private_base", "folders", "list", "state", "resolve",
		"forget_shared", "set", "move", "remove", "restore", "empty_trash", "purge_expired",
		"folder_create", "folder_rename", "folder_delete", "usage", "get", "original", "raw_file",
		"part_file"];
	for (var pi = 0; pi < PUBLIC.length; pi++) {
		(function (name) {
			var fn = Store.prototype[name];
			if (typeof fn != "function")
				throw new Error("mailbox.js: no method " + name);
			Store.prototype[name] = function () {
				try {
					return fn.apply(this, arguments);
				} catch (e) {
					throw this.outgoing(name, e);
				}
			};
		})(PUBLIC[pi]);
	}

	return Store;
})();

Store;
