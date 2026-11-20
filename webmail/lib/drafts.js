// Drafts: messages being written in the web mail client, with attachments
// uploaded in chunks.  A pure library: no API methods, no web or session
// globals.
//
// var Drafts = load("drafts.js");
//
// ctx is the API's mailbox context (Api.context_factory()):
//   usernum, alias  the user
//   userdata        the user's UserData (webmail/lib/userdata.js)
//   config          Config.load(): max_drafts, max_draft_bytes,
//                   max_attachment_bytes, max_body_bytes
//   store           an opened Store (exec/load/mailbox.js); used only by
//                   create() for reply, replyall and forward, and only
//                   through its ownership-checked public methods get()
//                   and part_file()
//   domains         optional: the From domains the user may use
//                   (Address.domains_for(usernum) when absent)
//   now             optional: a function returning a time_t (time())
//
// On disk (every name chosen here, never by a client):
//   <userdata dir>/drafts/<id>/draft.json   the draft (below)
//   <userdata dir>/drafts/<id>/a<n>         attachment n's bytes
//   <userdata dir>/drafts/<id>.sending/     a draft claimed by the send
//                                           step (claim() below)
//   <userdata dir>/webmail.drafts.json      {v:1}; its lock
//                                           (webmail.drafts.lock) is the
//                                           per-user drafts lock
// <id> is 16 lowercase hexadecimal characters from cryptlib's random
// number generator (as Session's keys); every method refuses an id that
// does not match ^[0-9a-f]{16}$, and an attachment id that does not match
// ^a[1-9][0-9]{0,8}$, before any path is built.  The file name a client
// gives an attachment is metadata only (draft.json); it never becomes a
// path.
//
// draft.json: {v:1, id, rev, mode ("new", "reply", "replyall",
// "forward"), created, updated (time_t), to, cc, bcc, subject, body,
// from_domain, priority, in_reply_to (the source's message id, "" for
// none), reply_message_id (the source's Message-ID, captured at create,
// "" for none), next (the number of the next attachment), attachments:
// [{id, name, type, total, received, complete, upload, touched, last:
// {offset, length, hash} or null}] [, claimed (time_t, while being
// sent)]}.  read_draft() checks this shape (types and ranges, attachment
// ids); a draft.json that does not have it is unreadable, exactly like
// one that is not JSON: get/save/attach/detach/remove/claim report
// notfound (reason "unreadable"), list and count skip it, sweep removes it
// after 24 hours.
//
// Locking: every method runs under the per-user drafts lock
// (UserData.locked("drafts"): the lock of UserData.update("drafts")),
// so two tabs, an upload and a sweep never interleave; a lock not taken
// within UserData's timeout is { code: "conflict", reason: "busy" }.  No
// public method calls another (the lock is not reentrant).  A process
// that dies holding the lock leaves its lock file; JsonFiles takes it
// over once it is older than its max_age (60 s), so a crash delays that
// user's draft calls (busy) for up to that long and affects no other
// user.  draft.json is replaced atomically (JsonFiles: temp file then
// rename).  A chunk is appended to the attachment file
// before draft.json records it; if the process dies in between, the file
// is longer than "received" and the next chunk for it first truncates the
// file back to "received", so a retried chunk is never appended twice.
//
// Revisions: rev counts saves of the text fields (save()); attach and
// detach do not change it.  save() must be given the rev it last saw;
// an older one is { code: "conflict", reason: "stale", rev: <current> }
// and nothing is written.  The client then reads the draft (get()), tells
// the user it was changed in another window, and either loads that
// version or saves its own again with the current rev.
//
// Quotas are measured on disk, never from a counter: max_drafts counts
// the draft directories that hold a readable draft.json; max_draft_bytes
// is the size of every file in every draft directory, claimed ones
// included (attachments, draft.json with the text, and anything left
// over), checked by attach(), save() and a forward; max_attachment_bytes
// is the size of one draft's attachment files.  So one user's drafts
// take at most about max_draft_bytes on disk, plus one draft.json
// (bounded by the field limits) and leftovers younger than 24 hours.
//
// Symbolic links: the host offers no lstat() (file_isdir, file_exists
// and file_size follow links, fullpath() is lexical), so a link cannot be
// detected.  Only an actor with write access to the user's data
// directory outside the web app can plant one: every path component
// below the user directory is chosen here, from validated ids.  What is
// checked: a draft path is a directory and an attachment path is not.
//
// Sending (the send step's contract): claim(ctx, id, {send,
// confirm_resend}) checks that every attachment is complete, writes the
// send's record (send.json) and renames drafts/<id>/ to
// drafts/<id>.sending/, recording the time in draft.json; the claimed
// draft is invisible to every other method (notfound) and to a second
// claim.  A draft marked maybe_sent (a send whose outcome could not be
// known) is claimed only with confirm_resend: true (else conflict,
// reason "maybe_sent").  The send step keeps its record and its files
// (send.*) in the claimed directory, so they live exactly as long as the
// claim: claimed() reads the claim, set_send() replaces the record.  It
// then calls finish() (sent: the directory is removed) or release()
// (failed: renamed back, the send's files removed, optionally marked
// maybe_sent; under a new id if a draft with that id exists again).
// sweep() leaves a claim with a send record to the send step once its
// heartbeat is opts.live seconds old (stale_sends, and opts.recover): the
// send step's recovery releases one that saved nothing and completes or
// returns the rest; a claim older than one hour whose send saved nothing
// is renamed back by sweep() itself.  Ownership: a record names its
// owner (a token) and a heartbeat (beat); set_send, release and finish
// fail with conflict "taken" for any other token, set_send also for a
// phase earlier than the stored one; take() moves the claim to a new
// owner by compare-and-set once the heartbeat is old.
//
// Errors are thrown as { code, message } with a fixed message, code one
// of invalid, notfound, quota, toolarge, conflict, forbidden, failed,
// plus, where stated, reason, rev or received.  quota carries reason
// "drafts" (too many drafts) or "bytes".  A Store error from create()
// keeps its code and the Store's own fixed message.  Anything else (a
// file that cannot be written) is logged and thrown as
// { code: "failed" }; no message names a path.
//
// Interface (each documented where it is defined):
//   Drafts.create(ctx, {mode, source_id})   the new draft
//   Drafts.get(ctx, id)                     the draft
//   Drafts.list(ctx)                        [summary], newest first
//   Drafts.count(ctx)                       number of drafts
//   Drafts.save(ctx, id, fields)            the draft after the save
//   Drafts.remove(ctx, id)                  true
//   Drafts.attach(ctx, id, chunk)           {id, received, complete}
//   Drafts.detach(ctx, id, attachment_id)   the draft after the removal
//   Drafts.sweep(ctx [, now] [, {recover, live}])
//                                           {drafts, files, recovered,
//                                           stale_sends}
//   Drafts.claim(ctx, id [, {send, confirm_resend}])
//                                           {draft, dir, files}
//   Drafts.claimed(ctx, id)                 {dir, draft, files, claimed,
//                                           send} or null
//   Drafts.set_send(ctx, id, record, token) true
//   Drafts.take(ctx, id, seen, token, now, window)  the record, now owned
//   Drafts.release(ctx, id [, {token, maybe_sent, maybe_sent_id}])
//                                           the draft, back
//   Drafts.finish(ctx, id [, token])        true
//   Drafts.usage(ctx)                       {drafts, bytes}
//   Drafts.limits(ctx)                      {max_drafts, max_draft_bytes,
//                                           max_attachment_bytes,
//                                           max_body_bytes, max_attachments,
//                                           max_chunk}

"use strict";

var Drafts = (function () {
	var HERE = (function () {
		var f = String(new Error().fileName || "");
		var i = Math.max(f.lastIndexOf('/'), f.lastIndexOf('\\'));
		return i >= 0 ? f.substr(0, i + 1) : "";
	})();
	var Address = load({}, HERE !== "" && file_exists(HERE + "address.js") ? HERE + "address.js" : "address.js");

	var ID_RE = /^[0-9a-f]{16}$/;
	var ATT_RE = /^a[1-9][0-9]{0,8}$/;
	var UPLOAD_RE = /^[A-Za-z0-9_-]{8,64}$/;
	var LOCK_NAME = "drafts";
	var DRAFT = "draft";	// draft.json through JsonFiles(dir, "")
	var DAY = 86400;
	var CLAIM_AGE = 3600;	// seconds before sweep() returns a claimed draft
	var SEND = "send";	// a claim's send record, send.json, and the send's files send.*
	var SENDING = ".sending";
	var MAX_CHUNK = 1500000;	// decoded bytes per attach() call
	var MAX_CHUNK_B64 = 2000000;	// its base64 length
	var MAX_ATTACHMENTS = 100;	// per draft
	var MAX_ADDR_FIELD = Address.MAX_INPUT;	// to, cc, bcc: bytes
	var OCTET = "application/octet-stream";
	var B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	var FIELDS = { to: true, cc: true, bcc: true, subject: true, body: true, from_domain: true,
		priority: true, rev: true };
	var MODES = { "new": true, reply: true, replyall: true, forward: true };
	var MESSAGES = {
		invalid: "invalid draft request",
		notfound: "no such draft",
		quota: "draft limit reached",
		toolarge: "attachments too large",
		conflict: "changed or busy; try again",
		forbidden: "not allowed",
		failed: "the draft could not be saved",
		expired: "expired"
	};
	var seq = 0;

	function error(code, reason, extra)
	{
		var e = { code: code, message: MESSAGES[code] || MESSAGES.failed };
		if (reason !== undefined)
			e.reason = reason;
		if (extra)
			for (var k in extra)
				e[k] = extra[k];
		return e;
	}

	function now_of(ctx)
	{
		if (ctx && typeof ctx.now === "function") {
			var t = ctx.now();
			if (typeof t === "number" && isFinite(t))
				return t;
		}
		return time();
	}

	function to_hex(bytes)
	{
		var s = "";
		for (var i = 0; i < bytes.length; i++) {
			var c = bytes.charCodeAt(i);
			s += (c < 16 ? "0" : "") + c.toString(16);
		}
		return s;
	}

	// 16 lowercase hex characters from cryptlib's random number generator
	// (see session.js: an HMAC under a generated key)
	function new_id()
	{
		var c = new CryptContext(CryptContext.ALGO["HMAC-SHA2"]);
		c.generate_key();
		c.encrypt("webmail draft " + (seq++) + " " + system.timer);
		var mac = c.hashvalue;
		if (typeof mac !== "string" || mac.length < 8)
			throw new Error("draft id generation failed");
		var id = to_hex(mac.substr(0, 8));
		if (!ID_RE.test(id))
			throw new Error("draft id generation failed");
		return id;
	}

	function check_id(id)
	{
		if (typeof id !== "string" || !ID_RE.test(id))
			throw error("invalid", "id");
		return id;
	}

	// ASCII-only helpers (toLowerCase and \s are not byte-safe here)
	function lower(s)
	{
		return s.replace(/[A-Z]+/g, function (m) {
			return m.toLowerCase();
		});
	}

	function ascii_trim(s)
	{
		var a = 0;
		var b = s.length;
		while (a < b && (s.charAt(a) === " " || s.charAt(a) === "\t" || s.charAt(a) === "\r" || s.charAt(a) === "\n"))
			a++;
		while (b > a && (s.charAt(b - 1) === " " || s.charAt(b - 1) === "\t" || s.charAt(b - 1) === "\r" || s.charAt(b - 1) === "\n"))
			b--;
		return s.substring(a, b);
	}

	// Well-formed UTF-8 as a byte string: no char code above 0xFF, no
	// overlong form, surrogate or value above U+10FFFF
	function valid_utf8(s)
	{
		if (!/[^\x00-\x7F]/.test(s))
			return true;
		var n = s.length;
		for (var i = 0; i < n; i++) {
			var c = s.charCodeAt(i);
			if (c < 0x80)
				continue;
			var need, min;
			if (c >= 0xC2 && c <= 0xDF) {
				need = 1;
				min = 0x80;
			} else if (c >= 0xE0 && c <= 0xEF) {
				need = 2;
				min = 0x800;
			} else if (c >= 0xF0 && c <= 0xF4) {
				need = 3;
				min = 0x10000;
			} else
				return false;
			if (i + need >= n)
				return false;
			var v = c & (need === 1 ? 0x1F : need === 2 ? 0x0F : 0x07);
			for (var j = 1; j <= need; j++) {
				var d = s.charCodeAt(i + j);
				if ((d & 0xC0) !== 0x80 || d > 0xFF)
					return false;
				v = (v << 6) | (d & 0x3F);
			}
			if (v < min || v > 0x10FFFF || (v >= 0xD800 && v <= 0xDFFF))
				return false;
			i += need;
		}
		return true;
	}

	function text_field(v, max, name, allow_breaks)
	{
		if (typeof v !== "string")
			throw error("invalid", name);
		if (v.length > max)
			throw error("toolarge", name);
		if (!valid_utf8(v))
			throw error("invalid", name);
		var bad = allow_breaks ? /[\x00-\x08\x0B\x0C\x0E-\x1F\x7F]/ : /[\x00-\x1F\x7F]/;
		if (bad.test(v))
			throw error("invalid", name);
		return v;
	}

	// The display name of an attachment: the base name of what the
	// client sent, without control characters, at most 128 bytes; not
	// empty, "." or ".."
	function clean_name(name)
	{
		if (typeof name !== "string" || name.length > 4096 || !valid_utf8(name))
			throw error("invalid", "name");
		var i = Math.max(name.lastIndexOf("/"), name.lastIndexOf("\\"));
		var base = Address.clean(name.substr(i + 1), Address.MAX_FILENAME);
		if (base === "" || base === "." || base === "..")
			throw error("invalid", "name");
		return base;
	}

	// A media type: type/subtype of RFC 6838 restricted-name characters,
	// lowercased, parameters dropped; anything else is
	// application/octet-stream
	function clean_type(type)
	{
		if (typeof type !== "string" || type.length > 255)
			return OCTET;
		var semi = type.indexOf(";");
		var t = ascii_trim(semi >= 0 ? type.substr(0, semi) : type);
		if (!/^[A-Za-z0-9][A-Za-z0-9!#$&^_.+-]{0,126}\/[A-Za-z0-9][A-Za-z0-9!#$&^_.+-]{0,126}$/.test(t))
			return OCTET;
		return lower(t);
	}

	// Strict base64: only the alphabet, length a multiple of 4, padding
	// only at the end, zero trailing bits, and the decoded bytes encode
	// back to the same text.  Returns the bytes ("" for "") or throws.
	function decode_b64(b64)
	{
		if (typeof b64 !== "string")
			throw error("invalid", "b64");
		if (b64.length > MAX_CHUNK_B64)
			throw error("invalid", "chunk");
		if (b64 === "")
			return "";
		if (b64.length % 4 !== 0 || /[^A-Za-z0-9+\/=]/.test(b64))
			throw error("invalid", "b64");
		var eq = b64.indexOf("=");
		var pad = 0;
		if (eq >= 0) {
			pad = b64.length - eq;
			if (pad > 2 || (pad === 2 && b64.charAt(b64.length - 1) !== "="))
				throw error("invalid", "b64");
			var last = B64.indexOf(b64.charAt(eq - 1));
			if ((pad === 2 && (last & 0x0F) !== 0) || (pad === 1 && (last & 0x03) !== 0))
				throw error("invalid", "b64");
		}
		var bytes = base64_decode(b64);
		if (typeof bytes !== "string" || bytes.length !== b64.length / 4 * 3 - pad
			|| base64_encode(bytes) !== b64)
			throw error("invalid", "b64");
		return bytes;
	}

	function non_negative_int(v)
	{
		return typeof v === "number" && isFinite(v) && v >= 0 && Math.floor(v) === v && v <= 0x7FFFFFFFFFFF;
	}

	// ---------------------------------------------------------------
	// Paths and files

	function root_of(ctx)
	{
		return ctx.userdata.dir + "drafts/";
	}

	function dir_of(ctx, id)
	{
		return root_of(ctx) + check_id(id) + "/";
	}

	function att_path(dir, att_id)
	{
		if (typeof att_id !== "string" || !ATT_RE.test(att_id))
			throw error("invalid", "attachment");
		return dir + att_id;
	}

	function files_of(ctx, dir)
	{
		var JF = ctx.userdata.files.constructor;
		return new JF(dir, "");
	}

	function is_obj(o)
	{
		return o !== null && typeof o === "object" && !(o instanceof Array);
	}

	var TEXT_KEYS = ["id", "mode", "to", "cc", "bcc", "subject", "body", "from_domain", "priority",
		"in_reply_to", "reply_message_id"];
	var INT_KEYS = ["rev", "next", "created", "updated"];

	function att_ok(a)
	{
		if (!is_obj(a) || typeof a.id !== "string" || !ATT_RE.test(a.id))
			return false;
		if (!non_negative_int(a.total) || !non_negative_int(a.received) || a.received > a.total)
			return false;
		if (typeof a.complete !== "boolean" || typeof a.name !== "string" || typeof a.type !== "string"
			|| typeof a.upload !== "string" || !non_negative_int(a.touched))
			return false;
		if (a.last !== null && !(is_obj(a.last) && non_negative_int(a.last.offset)
			&& non_negative_int(a.last.length) && typeof a.last.hash === "string"))
			return false;
		return true;
	}

	// Whether d has the shape of a draft (see the header)
	function draft_ok(d)
	{
		var i;
		if (!is_obj(d) || d.v !== 1)
			return false;
		for (i = 0; i < TEXT_KEYS.length; i++)
			if (typeof d[TEXT_KEYS[i]] !== "string")
				return false;
		for (i = 0; i < INT_KEYS.length; i++)
			if (!non_negative_int(d[INT_KEYS[i]]))
				return false;
		if (!ID_RE.test(d.id) || !MODES.hasOwnProperty(d.mode))
			return false;
		if (d.claimed !== undefined && !non_negative_int(d.claimed))
			return false;
		if (d.maybe_sent !== undefined && !non_negative_int(d.maybe_sent))
			return false;
		if (d.maybe_sent_id !== undefined && typeof d.maybe_sent_id !== "string")
			return false;
		if (!(d.attachments instanceof Array) || d.attachments.length > MAX_ATTACHMENTS)
			return false;
		var seen = {};
		for (i = 0; i < d.attachments.length; i++) {
			var a = d.attachments[i];
			if (!att_ok(a) || seen.hasOwnProperty(a.id))
				return false;
			seen[a.id] = true;
		}
		return true;
	}

	// The draft in dir, or null when there is no readable draft.json (not
	// JSON, or not the shape of a draft); never throws
	function read_draft(ctx, dir)
	{
		try {
			if (!file_isdir(dir))
				return null;
			var d = files_of(ctx, dir).read(DRAFT, null);
			return draft_ok(d) ? d : null;
		} catch (e) {
			Drafts.log(LOG_WARNING, "drafts: cannot read " + dir + ": " + e);
			return null;
		}
	}

	// A claimed draft's directory; dir_of() and sending_of() build every
	// draft directory path, from a checked id
	function sending_of(ctx, id)
	{
		return root_of(ctx) + check_id(id) + SENDING + "/";
	}

	function load_draft(ctx, id)
	{
		var dir = dir_of(ctx, id);
		if (!file_isdir(dir))
			throw error("notfound");
		var d = read_draft(ctx, dir);
		if (d === null || d.id !== id)
			throw error("notfound", "unreadable");
		return { dir: dir, draft: d };
	}

	function write_draft(ctx, dir, d)
	{
		files_of(ctx, dir).write(DRAFT, d);
	}

	// Entries of a directory: {dirs: [names], files: [names]}
	function entries(dir)
	{
		var out = { dirs: [], files: [] };
		var list = directory(dir + "*");
		for (var i = 0; i < list.length; i++) {
			var p = list[i];
			var c = p.charAt(p.length - 1);
			if (c === "/" || c === "\\") {
				var n = p.substr(0, p.length - 1);
				n = n.substr(Math.max(n.lastIndexOf("/"), n.lastIndexOf("\\")) + 1);
				if (n !== "." && n !== "..")
					out.dirs.push(n);
			} else
				out.files.push(p.substr(Math.max(p.lastIndexOf("/"), p.lastIndexOf("\\")) + 1));
		}
		return out;
	}

	function dir_bytes(dir)
	{
		var e = entries(dir);
		var n = 0;
		for (var i = 0; i < e.files.length; i++)
			n += Math.max(0, file_size(dir + e.files[i]));
		return n;
	}

	// The id in a directory name: {id, sending} or null
	function name_of(n)
	{
		var sending = n.length === 16 + SENDING.length && n.substr(16) === SENDING;
		var id = sending ? n.substr(0, 16) : n;
		return ID_RE.test(id) ? { id: id, sending: sending } : null;
	}

	// {drafts: readable drafts, bytes: every file in every draft dir,
	// claimed ones included}
	function usage_of(ctx)
	{
		var root = root_of(ctx);
		var out = { drafts: 0, bytes: 0 };
		if (!file_isdir(root))
			return out;
		var e = entries(root);
		for (var i = 0; i < e.dirs.length; i++) {
			var nm = name_of(e.dirs[i]);
			if (nm === null)
				continue;
			var dir = root + e.dirs[i] + "/";
			out.bytes += dir_bytes(dir);
			if (!nm.sending && read_draft(ctx, dir) !== null)
				out.drafts++;
		}
		return out;
	}

	function att_size(dir, a)
	{
		var p = att_path(dir, a.id);
		return file_exists(p) && !file_isdir(p) ? Math.max(0, file_size(p)) : 0;
	}

	// Bytes of a draft's attachment files on disk
	function attached_bytes(dir, d)
	{
		var n = 0;
		for (var i = 0; i < d.attachments.length; i++)
			n += att_size(dir, d.attachments[i]);
		return n;
	}

	function remove_tree(dir)
	{
		var e = entries(dir);
		file_remove(dir + "draft.json");
		for (var i = 0; i < e.files.length; i++)
			file_remove(dir + e.files[i]);
		if (!rmdir(dir) && file_isdir(dir))
			Drafts.log(LOG_WARNING, "drafts: cannot remove directory " + dir);
	}

	// ---------------------------------------------------------------
	// The lock and error mapping

	// fn() runs under the per-user drafts lock; its result is returned.
	// A lock that cannot be taken is conflict, reason "busy".  The lock
	// alone is taken (UserData.locked: no file rewritten for it); a
	// UserData without locked() (an older copy, a test's stand-in) takes it
	// through update(), which also rewrites webmail.drafts.json.
	function locked(ctx, fn)
	{
		var result;
		var ran = false;
		var run = function () {
			ran = true;
			result = fn();
		};
		try {
			if (typeof ctx.userdata.locked === "function")
				ctx.userdata.locked(LOCK_NAME, run);
			else {
				ctx.userdata.update(LOCK_NAME, { v: 1 }, function () {
					run();
					return { v: 1 };
				});
			}
		} catch (e) {
			if (!ran && e !== null && typeof e === "object" && e.code === "conflict")
				throw error("conflict", "busy");
			throw e;
		}
		return result;
	}

	// A Store call: its { code } errors are marked so they leave with
	// the Store's own fixed message
	function via_store(fn)
	{
		try {
			return fn();
		} catch (e) {
			if (e !== null && typeof e === "object" && !(e instanceof Error) && typeof e.code === "string")
				e.from_store = true;
			throw e;
		}
	}

	var PASS = { reason: true, rev: true, received: true };

	function outgoing(name, e)
	{
		if (e !== null && typeof e === "object" && typeof e.code === "string" && MESSAGES[e.code]
			&& !(e instanceof Error)) {
			var msg = MESSAGES[e.code];
			if (e.from_store === true)
				msg = typeof e.message === "string" && e.message.length <= 200
					&& !/[\/\\\x00-\x1f]/.test(e.message) ? e.message : "the message could not be used";
			var out = { code: e.code, message: msg };
			for (var k in PASS)
				if (e[k] !== undefined && (typeof e[k] === "string" || typeof e[k] === "number"))
					out[k] = e[k];
			return out;
		}
		Drafts.log(LOG_ERR, "drafts." + name + ": " + (e && e.message ? e.message : String(e))
			+ (e && e.fileName ? " (" + e.fileName + ":" + e.lineNumber + ")" : ""));
		return error("failed");
	}

	// ---------------------------------------------------------------
	// Shapes

	function public_att(dir, a)
	{
		return { id: a.id, name: a.name, type: a.type, size: a.complete ? a.total : att_size(dir, a),
			total: a.total, complete: a.complete === true };
	}

	function public_draft(dir, d)
	{
		var atts = [];
		for (var i = 0; i < d.attachments.length; i++)
			atts.push(public_att(dir, d.attachments[i]));
		var out = { id: d.id, rev: d.rev, mode: d.mode, to: d.to, cc: d.cc, bcc: d.bcc, subject: d.subject,
			body: d.body, from_domain: d.from_domain, priority: d.priority, in_reply_to: d.in_reply_to,
			reply_message_id: d.reply_message_id, created: d.created, updated: d.updated,
			attachments: atts };
		if (d.maybe_sent) {
			out.maybe_sent = d.maybe_sent;
			if (d.maybe_sent_id)
				out.maybe_sent_id = d.maybe_sent_id;
		}
		return out;
	}

	function blank_draft(id, mode, now)
	{
		return { v: 1, id: id, rev: 1, mode: mode, created: now, updated: now, to: "", cc: "", bcc: "",
			subject: "", body: "", from_domain: "", priority: "", in_reply_to: "", reply_message_id: "",
			next: 1, attachments: [] };
	}

	function allowed_domains(ctx)
	{
		if (ctx.domains instanceof Array)
			return ctx.domains;
		return Address.domains_for(ctx.usernum);
	}

	// ---------------------------------------------------------------
	// Reply fields

	function alnum_key(s)
	{
		return lower(String(s).replace(/[^A-Za-z0-9]/g, ""));
	}

	// Whether text is one recipient that Address.parse_list() reads and
	// Address.resolve() accepts as network mail of this type
	function accepted(text, net_type, kind)
	{
		try {
			var l = Address.parse_list(text);
			if (l.length !== 1)
				return false;
			var r = Address.resolve(l[0], function () {
				return 0;
			}, function () {
				return net_type;
			});
			return r.kind === kind;
		} catch (e) {
			return false;
		}
	}

	// A name as a local part: atext runs joined by dots ("John Doe" ->
	// "John.Doe")
	function dotted(name)
	{
		return name.replace(/[^A-Za-z0-9!#$%&'*+\/=?\^_`{|}~-]+/g, ".").replace(/\.+/g, ".")
			.replace(/^\.|\.$/g, "");
	}

	// An Internet recipient as a draft holds it: Address.format()'s form
	// (with its checks; it throws invalid), except that a name that is not
	// ASCII stays plain UTF-8 text in a quoted string, not encoded words.
	// A draft is text the user reads and edits; the send step parses it
	// (parse_list reads the name back) and format() encodes the name from
	// its own bytes when the header is written.
	function inet_text(name, addr)
	{
		var text = Address.format(name, addr);
		var n = Address.clean(String(name || "").replace(/\s+/g, " "), Address.MAX_NAME);
		if (!/[\x80-\xFF]/.test(n))
			return text;
		return '"' + n.replace(/[\\"]/g, "\\$&") + '" <' + addr + ">";
	}

	function quoted(name, addr)
	{
		var n = name.replace(/["\\]/g, "");
		return n === "" ? addr : '"' + n + '" <' + addr + ">";
	}

	// The source's sender as a recipient {text, key}, in a form
	// parse_list() and resolve() accept: Internet through Address.format;
	// FidoNet as "Name" <Name@zone:net/node[.point]>; QWK as
	// "Name" <Name@QWKID> (no route); local mail by the sender's name.
	// null when it cannot be written so (the caller counts it dropped).
	function sender_entry(h)
	{
		var name = Address.clean(String(h.from || ""), Address.MAX_NAME);
		var addr = Address.clean(String(h.from_addr || ""), Address.MAX_ADDR);
		var net = h.net_type;
		var text;
		if (net === "local" || (addr === "" && net !== "internet" && net !== "fido" && net !== "qwk")) {
			if (net !== "local" || name === "")
				return null;
			try {
				var l = Address.parse_list(name);
				if (l.length !== 1 || l[0].addr !== name || name.indexOf("@") >= 0)
					return null;
			} catch (e) {
				return null;
			}
			return { text: name, key: lower(name), local: true };
		}
		if (net === "internet") {
			try {
				text = inet_text(name, addr);
			} catch (e) {
				return null;
			}
			return accepted(text, Address.NET_INTERNET, "internet") ? { text: text, key: lower(addr) } : null;
		}
		var type = net === "fido" ? Address.NET_FIDO : net === "qwk" ? Address.NET_QWK : -1;
		if (type < 0 || addr === "")
			return null;
		var full = addr;
		if (addr.indexOf("@") < 0) {
			var local = dotted(name);
			if (local === "")
				return net === "fido" && accepted(addr, type, net) ? { text: addr, key: lower(addr) } : null;
			full = local + "@" + addr;
		}
		text = quoted(name, full);
		return accepted(text, type, net) ? { text: text, key: lower(full) } : null;
	}

	// The recipients of a stored To or CC line: {list: [{text, key,
	// name, addr}], dropped}; a line parse_list() refuses is dropped
	// whole (counted as one)
	function entries_of(line)
	{
		line = Address.clean(String(line || ""), MAX_ADDR_FIELD);
		if (line === "")
			return { list: [], dropped: 0 };
		var l;
		try {
			l = Address.parse_list(line);
		} catch (e) {
			return { list: [], dropped: 1 };
		}
		var out = [];
		for (var i = 0; i < l.length; i++) {
			var e = l[i];
			var text;
			if (e.addr.indexOf("@") < 0)
				text = e.addr;
			else {
				try {
					text = inet_text(e.name, e.addr);
				} catch (x) {
					text = quoted(e.name, e.addr);
				}
			}
			out.push({ text: text, key: lower(e.addr), name: e.name, addr: e.addr });
		}
		return { list: out, dropped: 0 };
	}

	// The recipients of a structured list from the Store (headers.to_list,
	// cc_list, reply_to_list: [{name, addr}], the entries and addresses
	// read from the raw header, each name decoded as text): {list, dropped}
	// as entries_of().  An address the parser does not read back as itself,
	// alone, is dropped (counted); a name is only ever a name (inet_text()
	// quotes it), so it cannot add a recipient.
	function entries_from(items)
	{
		var out = [];
		var dropped = 0;
		for (var i = 0; i < items.length; i++) {
			var it = items[i] || {};
			var addr = Address.clean(String(it.addr || ""), Address.MAX_ADDR);
			var name = Address.clean(String(it.name || ""), Address.MAX_NAME);
			var l = null;
			try {
				l = Address.parse_list(addr);
			} catch (e) {
				l = null;
			}
			if (addr === "" || l === null || l.length !== 1 || l[0].addr !== addr) {
				dropped++;
				continue;
			}
			var text = addr;
			if (addr.indexOf("@") >= 0) {
				try {
					text = inet_text(name, addr);
				} catch (x) {
					text = quoted(name, addr);
				}
			}
			out.push({ text: text, key: lower(addr), name: name, addr: addr });
		}
		return { list: out, dropped: dropped };
	}

	// A recipient field of the source: its structured list when the Store
	// gives one, else the line (a local message's to field)
	function field_entries(list, line)
	{
		if (list instanceof Array && list.length > 0)
			return entries_from(list);
		return entries_of(line);
	}

	function is_me(entry, ctx, allowed)
	{
		var addr = entry.addr !== undefined ? entry.addr : entry.key;
		if (addr.indexOf("@") >= 0)
			return Address.is_me(addr, ctx.alias, allowed);
		return alnum_key(addr) !== "" && alnum_key(addr) === alnum_key(ctx.alias);
	}

	// The entries without the user, without those whose key is in seen,
	// and without repeats; adds the kept keys to seen
	function filtered(list, ctx, allowed, seen, drop_me)
	{
		var out = [];
		for (var i = 0; i < list.length; i++) {
			var e = list[i];
			if (seen.hasOwnProperty(e.key) || (drop_me && is_me(e, ctx, allowed)))
				continue;
			seen[e.key] = true;
			out.push(e);
		}
		return out;
	}

	// The entries joined with ", ", dropping whole entries from the end
	// past MAX_ADDR_FIELD bytes; counts them in counter.dropped
	function joined(list, counter)
	{
		var parts = [];
		var len = 0;
		for (var i = 0; i < list.length; i++) {
			var add = (parts.length > 0 ? 2 : 0) + list[i].text.length;
			if (len + add > MAX_ADDR_FIELD) {
				counter.dropped += list.length - i;
				break;
			}
			parts.push(list[i].text);
			len += add;
		}
		return parts.join(", ");
	}

	// Reply and reply-all fields.  Rule:
	// - The source is the user's own message when it is in Sent (or is a
	//   sent extra) or its sender is the user (Address.is_me, or a local
	//   name equal to the alias).  Then, as mail clients do, the reply
	//   goes to the source's To recipients (without the user; if that
	//   leaves none, to the sender, i.e. the user), and reply-all adds the
	//   source's CC recipients as cc.
	// - Otherwise the reply goes to the Reply-To recipients, else to the
	//   sender.  Reply-all removes the user from every list (Reply-To
	//   included; a Reply-To naming only the user falls back to the
	//   sender) and sets cc to the source's To and CC recipients without
	//   the user, the reply's recipients and repeats.
	// Recipients that cannot be written in a form the address parser
	// accepts, and whole entries past the field limit, are left out and
	// counted in counter.dropped.
	function reply_fields(ctx, d, msg, all, counter)
	{
		var h = msg.headers || {};
		var allowed = allowed_domains(ctx);
		var sender = sender_entry(h);
		var own = msg.folder === "sent" || msg.extra === true
			|| (sender !== null && is_me(sender, ctx, allowed));
		var to_e = field_entries(h.to_list, h.to);
		var cc_e = field_entries(h.cc_list, h.cc);
		var seen = {};
		var to;
		var cc = [];
		if (own) {
			counter.dropped += to_e.dropped;
			to = filtered(to_e.list, ctx, allowed, seen, true);
			if (to.length === 0 && sender !== null)
				to = [sender];
			if (all) {
				counter.dropped += cc_e.dropped;
				cc = filtered(cc_e.list, ctx, allowed, seen, true);
			}
		} else {
			var rt = field_entries(h.reply_to_list, h.reply_to);
			counter.dropped += rt.dropped;
			to = filtered(rt.list, ctx, allowed, seen, all);
			if (to.length === 0) {
				if (sender === null)
					counter.dropped++;
				else {
					seen[sender.key] = true;
					to = [sender];
				}
			}
			if (all) {
				counter.dropped += to_e.dropped + cc_e.dropped;
				cc = filtered(to_e.list.concat(cc_e.list), ctx, allowed, seen, true);
			}
		}
		d.to = joined(to, counter);
		d.cc = joined(cc, counter);
		if (allowed.length > 0) {
			var dom = Address.reply_domain(h.to_domain || "", allowed[0], allowed);
			d.from_domain = dom === lower(allowed[0]) ? "" : dom;
		}
		d.in_reply_to = msg.id;
		d.reply_message_id = Address.clean(String(h.message_id || ""), 998);
	}

	// ---------------------------------------------------------------
	// Public methods

	// create(ctx, {mode, source_id}): a new draft.  mode "new" (no
	// source), "reply", "replyall" or "forward" (source_id: a message id
	// the Store resolves for this user; its errors, such as forbidden or
	// notfound, are passed on with the Store's message).  reply and
	// replyall: to and cc by the rule at reply_fields(); from_domain =
	// Address.reply_domain() of the domain the source was sent to (""
	// when that is the default, the first allowed domain); in_reply_to
	// and reply_message_id.  A reply's result has dropped_recipients: the
	// number of the source's recipients left out because they cannot be
	// written in a form the address parser accepts or past the field
	// limit (0 normally), so the client can tell the user.
	// forward: every attachment of the source the user may be served
	// (Store.get parts with a size of 0 or more), copied in through
	// Store.part_file().  The quotas apply to the copies; any failure
	// removes the new draft entirely (all or nothing).  The text is not
	// quoted: the browser builds the quoted body and the subject.
	// quota (reason "drafts") when the user already has max_drafts.
	function create(ctx, opts)
	{
		opts = opts || {};
		var mode = opts.mode === undefined ? "new" : opts.mode;
		if (typeof mode !== "string" || !MODES.hasOwnProperty(mode))
			throw error("invalid", "mode");
		var msg = null;
		if (mode !== "new") {
			if (typeof opts.source_id !== "string" || opts.source_id === "")
				throw error("invalid", "source_id");
			msg = via_store(function () {
				return ctx.store.get(opts.source_id, {});
			});
		} else if (opts.source_id !== undefined && opts.source_id !== null && opts.source_id !== "")
			throw error("invalid", "source_id");
		return locked(ctx, function () {
			var cfg = ctx.config;
			var use = usage_of(ctx);
			if (use.drafts >= cfg.max_drafts)
				throw error("quota", "drafts");
			var id;
			var dir;
			for (var tries = 0; ; tries++) {
				id = new_id();
				dir = dir_of(ctx, id);
				if (!file_isdir(dir) && !file_isdir(sending_of(ctx, id)))
					break;
				if (tries > 5)
					throw new Error("no unused draft id");
			}
			var now = now_of(ctx);
			var d = blank_draft(id, mode, now);
			var counter = { dropped: 0 };
			if (mode === "reply" || mode === "replyall")
				reply_fields(ctx, d, msg, mode === "replyall", counter);
			if (!mkpath(dir) && !file_isdir(dir))
				throw new Error("cannot create " + dir);
			var done = false;
			try {
				if (mode === "forward")
					copy_parts(ctx, d, dir, msg, use.bytes);
				write_draft(ctx, dir, d);
				done = true;
			} finally {
				if (!done)
					remove_tree(dir);
			}
			var out = public_draft(dir, d);
			if (mode === "reply" || mode === "replyall")
				out.dropped_recipients = counter.dropped;
			return out;
		});
	}

	function copy_parts(ctx, d, dir, msg, user_bytes)
	{
		var cfg = ctx.config;
		var parts = msg.parts || [];
		var total = 0;
		for (var i = 0; i < parts.length; i++) {
			var p = parts[i];
			if (typeof p.pid !== "string" || !(p.size >= 0))
				continue;
			if (d.attachments.length >= MAX_ATTACHMENTS)
				throw error("toolarge", "count");
			var pf = via_store(function () {
				return ctx.store.part_file(msg.id, p.pid);
			});
			try {
				var size = file_size(pf.path);
				if (total + size > cfg.max_attachment_bytes)
					throw error("toolarge", "total");
				if (user_bytes + total + size > cfg.max_draft_bytes)
					throw error("quota", "bytes");
				var att_id = "a" + d.next;
				var dest = att_path(dir, att_id);
				if (!file_copy(pf.path, dest) || file_size(dest) !== size)
					throw new Error("cannot copy a forwarded part to " + dest);
				var name;
				try {
					name = clean_name(String(pf.name || p.name || ""));
				} catch (e) {
					name = "attachment";
				}
				d.attachments.push({ id: att_id, name: name, type: clean_type(pf.type || p.type), total: size,
					received: size, complete: true, upload: "", touched: d.updated, last: null });
				d.next++;
				total += size;
			} finally {
				if (pf.temp === true)
					file_remove(pf.path);
			}
		}
	}

	// get(ctx, id): the draft {id, rev, mode, to, cc, bcc, subject, body,
	// from_domain, priority, in_reply_to, reply_message_id, created,
	// updated, attachments: [{id, name, type, size (bytes received), total,
	// complete}]}
	function get(ctx, id)
	{
		check_id(id);
		return locked(ctx, function () {
			var x = load_draft(ctx, id);
			return public_draft(x.dir, x.draft);
		});
	}

	// list(ctx): [{id, rev, to, cc, bcc, subject, updated, attachments
	// (count)}], most recently updated first
	function list(ctx)
	{
		return locked(ctx, function () {
			var root = root_of(ctx);
			var out = [];
			if (!file_isdir(root))
				return out;
			var e = entries(root);
			for (var i = 0; i < e.dirs.length; i++) {
				if (!ID_RE.test(e.dirs[i]))
					continue;
				var d = read_draft(ctx, root + e.dirs[i] + "/");	// never throws
				if (d === null || d.id !== e.dirs[i])
					continue;
				var row = { id: d.id, rev: d.rev, to: d.to, cc: d.cc, bcc: d.bcc, subject: d.subject,
					updated: d.updated, attachments: d.attachments.length };
				if (d.maybe_sent)
					row.maybe_sent = d.maybe_sent;
				out.push(row);
			}
			out.sort(function (a, b) {
				return b.updated - a.updated || (a.id < b.id ? -1 : a.id > b.id ? 1 : 0);
			});
			return out;
		});
	}

	function count(ctx)
	{
		return locked(ctx, function () {
			return usage_of(ctx).drafts;
		});
	}

	// usage(ctx): {drafts, bytes} as the quotas count them
	function usage(ctx)
	{
		return locked(ctx, function () {
			return usage_of(ctx);
		});
	}

	// save(ctx, id, fields): fields.rev (required) is the revision the
	// client last saw; any of to, cc, bcc (strings of at most
	// Address.MAX_INPUT bytes without control characters, parsed by the
	// send step), subject (Address.clean, 255 bytes), body (at most
	// config.max_body_bytes, line breaks and tabs allowed, no other
	// control characters), from_domain ("" or one of the user's allowed
	// domains), priority ("high", "low" or "") replace the stored value.
	// All text must be well-formed UTF-8.  An unknown field is invalid
	// (reason the field's name, if short and plain); a stale rev is
	// conflict (reason "stale", rev the current one).  The new draft.json
	// counts against max_draft_bytes (the user's draft files with the old
	// draft.json replaced by the new): over it is quota, reason "bytes".
	// A refused save changes nothing.
	function save(ctx, id, fields)
	{
		check_id(id);
		if (fields === null || typeof fields !== "object" || fields instanceof Array)
			throw error("invalid", "fields");
		var k;
		for (k in fields) {
			if (!FIELDS.hasOwnProperty(k))
				throw error("invalid", /^[a-z_]{1,32}$/.test(k) ? k : "field");
		}
		if (!non_negative_int(fields.rev))
			throw error("invalid", "rev");
		var ch = {};
		var addr = ["to", "cc", "bcc"];
		for (var i = 0; i < addr.length; i++)
			if (fields[addr[i]] !== undefined)
				ch[addr[i]] = text_field(fields[addr[i]], MAX_ADDR_FIELD, addr[i], false);
		if (fields.subject !== undefined) {
			text_field(fields.subject, 4096, "subject", true);
			ch.subject = Address.clean(fields.subject, Address.MAX_SUBJECT);
		}
		if (fields.body !== undefined)
			ch.body = text_field(fields.body, ctx.config.max_body_bytes, "body", true);
		if (fields.priority !== undefined) {
			if (fields.priority !== "" && fields.priority !== "high" && fields.priority !== "low")
				throw error("invalid", "priority");
			ch.priority = fields.priority;
		}
		if (fields.from_domain !== undefined) {
			var fd = fields.from_domain;
			if (typeof fd !== "string" || fd.length > 253)
				throw error("invalid", "from_domain");
			fd = lower(fd);
			if (fd !== "") {
				var allowed = allowed_domains(ctx);
				var found = false;
				for (i = 0; i < allowed.length; i++)
					if (typeof allowed[i] === "string" && lower(allowed[i]) === fd)
						found = true;
				if (!found)
					throw error("invalid", "from_domain");
			}
			ch.from_domain = fd;
		}
		return locked(ctx, function () {
			var x = load_draft(ctx, id);
			var d = x.draft;
			if (fields.rev !== d.rev)
				throw error("conflict", "stale", { rev: d.rev });
			for (var key in ch)
				d[key] = ch[key];
			d.rev++;
			d.updated = now_of(ctx);
			var old_size = Math.max(0, file_size(x.dir + "draft.json"));
			var new_size = JSON.stringify(d).length;
			if (usage_of(ctx).bytes - old_size + new_size > ctx.config.max_draft_bytes)
				throw error("quota", "bytes");
			write_draft(ctx, x.dir, d);
			return public_draft(x.dir, d);
		});
	}

	// remove(ctx, id): the draft and its files; true
	function remove(ctx, id)
	{
		check_id(id);
		return locked(ctx, function () {
			var x = load_draft(ctx, id);
			remove_tree(x.dir);
			return true;
		});
	}

	// attach(ctx, id, {upload, name, type, offset, total, b64}): one chunk
	// of an attachment, returns {id, received, complete}.
	//   upload  the client's key for this file (8 to 64 of A-Z a-z 0-9 _
	//           -), the same on every chunk of one file and different
	//           for each file; it tells a retried first chunk from a new
	//           file.  The attachment's id is the server's ("a<n>").
	//   name, type  read from the first chunk only (clean_name,
	//           clean_type); duplicate names are allowed
	//   offset  must equal the bytes received so far, except that a chunk
	//           identical to the last one accepted (same offset, length
	//           and SHA-256 of the bytes) is acknowledged without being
	//           appended; otherwise conflict (reason "offset", received)
	//   total   the file's declared size, the same on every chunk
	//           (invalid otherwise); 0 is allowed (an empty file: one
	//           chunk with b64 "")
	//   b64     strict base64 of at most 1500000 bytes (invalid otherwise)
	// Limits, in this order: toolarge when the draft's attachments would
	// pass config.max_attachment_bytes (on the first chunk with the
	// declared total, and with the bytes on disk on every chunk); quota
	// (reason "bytes") when the user's draft files would pass
	// config.max_draft_bytes (likewise); invalid (reason "beyond") for a
	// chunk that goes past the declared total.  An attachment is complete
	// when received equals total; a chunk for a complete attachment that
	// is not the identical retry is conflict.
	function attach(ctx, id, chunk)
	{
		check_id(id);
		if (chunk === null || typeof chunk !== "object")
			throw error("invalid", "chunk");
		if (typeof chunk.upload !== "string" || !UPLOAD_RE.test(chunk.upload))
			throw error("invalid", "upload");
		if (!non_negative_int(chunk.offset))
			throw error("invalid", "offset");
		if (!non_negative_int(chunk.total))
			throw error("invalid", "total");
		var bytes = decode_b64(chunk.b64);
		if (bytes.length > MAX_CHUNK)
			throw error("invalid", "chunk");
		if (bytes.length === 0 && chunk.total !== 0)
			throw error("invalid", "chunk");
		var hash = sha256_calc(bytes, true);
		return locked(ctx, function () {
			var cfg = ctx.config;
			var x = load_draft(ctx, id);
			var d = x.draft;
			var dir = x.dir;
			var now = now_of(ctx);
			var a = null;
			for (var i = 0; i < d.attachments.length; i++)
				if (d.attachments[i].upload === chunk.upload)
					a = d.attachments[i];
			var use;
			if (a === null) {
				// A new file
				if (chunk.offset !== 0)
					throw error("conflict", "offset", { received: 0 });
				if (d.attachments.length >= MAX_ATTACHMENTS)
					throw error("toolarge", "count");
				var declared = 0;
				for (i = 0; i < d.attachments.length; i++)
					declared += Math.max(d.attachments[i].total, att_size(dir, d.attachments[i]));
				if (declared + Math.max(chunk.total, bytes.length) > cfg.max_attachment_bytes)
					throw error("toolarge", "total");
				use = usage_of(ctx);
				if (use.bytes + Math.max(chunk.total, bytes.length) > cfg.max_draft_bytes)
					throw error("quota", "bytes");
				if (bytes.length > chunk.total)
					throw error("invalid", "beyond");
				a = { id: "a" + d.next, name: clean_name(chunk.name), type: clean_type(chunk.type),
					total: chunk.total, received: 0, complete: false, upload: chunk.upload,
					touched: now, last: null };
				var path = att_path(dir, a.id);
				if (file_isdir(path))
					throw new Error("attachment path is a directory: " + path);
				append(path, bytes, true);
				a.received = bytes.length;
				a.complete = a.received === a.total;
				a.last = { offset: 0, length: bytes.length, hash: hash };
				d.attachments.push(a);
				d.next++;
				d.updated = now;
				write_draft(ctx, dir, d);
				return { id: a.id, received: a.received, complete: a.complete };
			}
			if (chunk.total !== a.total)
				throw error("invalid", "total");
			var apath = att_path(dir, a.id);
			if (file_isdir(apath))
				throw new Error("attachment path is a directory: " + apath);
			// The file must hold exactly what draft.json says was received
			var on_disk = file_exists(apath) ? file_size(apath) : -1;
			if (on_disk !== a.received) {
				if (on_disk > a.received)
					truncate(apath, a.received);
				else {
					a.received = Math.max(0, on_disk);
					a.complete = false;
					a.last = null;
					d.updated = now;
					write_draft(ctx, dir, d);
					if (on_disk < 0)
						append(apath, "", true);
					throw error("conflict", "offset", { received: a.received });
				}
			}
			var last = a.last;
			if (last && chunk.offset === last.offset && bytes.length === last.length && hash === last.hash
				&& chunk.offset + bytes.length === a.received)
				return { id: a.id, received: a.received, complete: a.complete === true };
			if (a.complete || chunk.offset !== a.received)
				throw error("conflict", "offset", { received: a.received });
			if (attached_bytes(dir, d) + bytes.length > cfg.max_attachment_bytes)
				throw error("toolarge", "total");
			use = usage_of(ctx);
			if (use.bytes + bytes.length > cfg.max_draft_bytes)
				throw error("quota", "bytes");
			if (a.received + bytes.length > a.total)
				throw error("invalid", "beyond");
			append(apath, bytes, false);
			a.received += bytes.length;
			a.complete = a.received === a.total;
			a.last = { offset: chunk.offset, length: bytes.length, hash: hash };
			a.touched = now;
			d.updated = now;
			write_draft(ctx, dir, d);
			return { id: a.id, received: a.received, complete: a.complete };
		});
	}

	function append(path, bytes, create)
	{
		var before = create ? 0 : file_size(path);
		var f = new File(path);
		if (!f.open(create ? "wb" : "ab"))
			throw new Error("cannot open " + path + ": " + f.error);
		var ok = (bytes === "" || f.write(bytes)) && f.flush() && f.error === 0;
		f.close();
		if (!ok || file_size(path) !== before + bytes.length) {
			truncate(path, before);
			throw new Error("cannot write " + path);
		}
	}

	function truncate(path, size)
	{
		var f = new File(path);
		if (!f.open("r+b"))
			throw new Error("cannot open " + path + " to truncate: " + f.error);
		var ok = f.truncate(size);
		f.close();
		if (!ok || file_size(path) !== size)
			throw new Error("cannot truncate " + path);
	}

	// detach(ctx, id, attachment_id): removes the attachment (complete or
	// not; an upload in progress is cancelled); the draft after it.
	// notfound for an unknown attachment id.  The record goes first, then
	// the file (a file left behind is an orphan the sweep removes).
	function detach(ctx, id, att_id)
	{
		check_id(id);
		if (typeof att_id !== "string" || !ATT_RE.test(att_id))
			throw error("invalid", "attachment");
		return locked(ctx, function () {
			var x = load_draft(ctx, id);
			var d = x.draft;
			var keep = [];
			var found = false;
			for (var i = 0; i < d.attachments.length; i++) {
				if (d.attachments[i].id === att_id)
					found = true;
				else
					keep.push(d.attachments[i]);
			}
			if (!found)
				throw error("notfound", "attachment");
			d.attachments = keep;
			d.updated = now_of(ctx);
			write_draft(ctx, x.dir, d);
			var p = att_path(x.dir, att_id);
			if (file_exists(p) && !file_isdir(p) && !file_remove(p))
				Drafts.log(LOG_WARNING, "drafts: cannot remove " + p);
			return public_draft(x.dir, d);
		});
	}

	function has_text(d)
	{
		var f = ["to", "cc", "bcc", "subject", "body"];
		for (var i = 0; i < f.length; i++)
			if (typeof d[f[i]] === "string" && ascii_trim(d[f[i]]) !== "")
				return true;
		return false;
	}

	// The newest modification time of a directory and its files
	function newest(dir)
	{
		var t = file_date(dir);
		var e = entries(dir);
		for (var i = 0; i < e.files.length; i++)
			t = Math.max(t, file_date(dir + e.files[i]));
		return t;
	}

	// Renames a directory (paths end with a separator)
	function rename_dir(from, to)
	{
		return file_rename(from.substr(0, from.length - 1), to.substr(0, to.length - 1));
	}

	// One draft directory's part of sweep()
	function sweep_draft(ctx, root, name, limit, out)
	{
		var dir = root + name + "/";
		var d = read_draft(ctx, dir);
		if (d === null || d.id !== name) {
			if (newest(dir) < limit) {
				remove_tree(dir);
				out.drafts++;
			}
			return;
		}
		var keep = [];
		var dropped = [];
		for (var j = 0; j < d.attachments.length; j++) {
			var a = d.attachments[j];
			if (!a.complete && !(a.touched >= limit))
				dropped.push(a);
			else
				keep.push(a);
		}
		if (keep.length === 0 && !has_text(d) && !(d.updated >= limit)) {
			remove_tree(dir);
			out.drafts++;
			return;
		}
		if (dropped.length > 0) {
			d.attachments = keep;
			write_draft(ctx, dir, d);
			for (j = 0; j < dropped.length; j++) {
				if (file_remove(att_path(dir, dropped[j].id)))
					out.files++;
			}
		}
		var named = { "draft.json": true };
		for (j = 0; j < keep.length; j++)
			named[keep[j].id] = true;
		var files = entries(dir).files;
		for (j = 0; j < files.length; j++) {
			if (named.hasOwnProperty(files[j]))
				continue;
			if (file_date(dir + files[j]) < limit && file_remove(dir + files[j]))
				out.files++;
		}
	}

	// The send record of a claimed directory (send.json), or null
	function send_record(ctx, sdir)
	{
		try {
			var r = files_of(ctx, sdir).read(SEND, null);
			return is_obj(r) ? r : null;
		} catch (e) {
			return null;
		}
	}

	// Whether a claim's send may have saved something (a Sent copy or
	// delivery copies): its record names a phase past "claimed"; only the
	// send step can undo or finish that
	function send_started(rec)
	{
		return rec !== null && rec.phase !== "claimed";
	}

	// A fresh draft id that is neither a draft nor a claim
	function unused_id(ctx)
	{
		for (var tries = 0; tries < 10; tries++) {
			var id = new_id();
			if (!file_isdir(dir_of(ctx, id)) && !file_isdir(sending_of(ctx, id)))
				return id;
		}
		throw new Error("no unused draft id");
	}

	// The names of the send's files in directory dir (send.json, send.*)
	function send_file_names(dir)
	{
		var e = entries(dir);
		var out = [];
		for (var i = 0; i < e.files.length; i++)
			if (e.files[i].substr(0, SEND.length + 1) === SEND + ".")
				out.push(e.files[i]);
		return out;
	}

	function remove_send_files(dir)
	{
		var names = send_file_names(dir);
		for (var i = 0; i < names.length; i++)
			if (!file_remove(dir + names[i]))
				Drafts.log(LOG_WARNING, "drafts: cannot remove " + dir + names[i]);
	}

	// Claim 'id' (directory sdir, draft d or null) becomes a draft again,
	// renamed back to its id, or to a new id when that id is a draft again
	// (so no claim is ever stranded), in this order: (1) draft.json is
	// written INSIDE the claimed directory, claimed dropped, the new id,
	// maybe_sent set when given; (2) the directory is renamed; (3) the
	// send's files (send.json and send.*) are removed.  A crash or a failed
	// rename before (2) leaves the claim with its record (the send step
	// recovers it again) and a draft.json already carrying the mark; after
	// (2) the draft is back, marked; send files left by a crash after it
	// are removed by the next claim or, unnamed, by sweep() after a day.
	// Returns {dir, draft}.
	function unclaim(ctx, id, sdir, d, maybe_sent, maybe_sent_id)
	{
		var new_id = id;
		if (file_isdir(dir_of(ctx, id)))
			new_id = unused_id(ctx);
		var dir = dir_of(ctx, new_id);
		if (d !== null) {
			delete d.claimed;
			d.id = new_id;
			if (typeof maybe_sent === "number") {
				d.maybe_sent = maybe_sent;
				if (typeof maybe_sent_id === "string" && maybe_sent_id !== "")
					d.maybe_sent_id = maybe_sent_id;
			}
			write_draft(ctx, sdir, d);
		}
		if (Drafts.hook)
			Drafts.hook("unclaim:written", sdir);
		if (!rename_dir(sdir, dir))
			throw new Error("cannot rename " + sdir + " to " + dir);
		if (Drafts.hook)
			Drafts.hook("unclaim:renamed", dir);
		remove_send_files(dir);
		return { dir: dir, draft: d };
	}

	// A claimed draft whose heartbeat (else claim time) is 'live' seconds
	// old: listed in stale_sends, for the send step's recovery, when it has
	// a send record (a send that started is never renamed back blindly;
	// one that saved nothing is released by the recovery).  Older than
	// CLAIM_AGE and nothing saved: goes back to being a draft here (also
	// when no recovery runs); with no readable record but the send's files
	// present, nothing is known of the send, so it comes back marked
	// maybe_sent.
	function sweep_claim(ctx, id, now, live, out)
	{
		var sdir = sending_of(ctx, id);
		var d = read_draft(ctx, sdir);
		var rec = send_record(ctx, sdir);
		var since = d !== null && d.claimed !== undefined ? d.claimed : newest(sdir);
		if (rec !== null && typeof rec.t === "number")
			since = Math.max(since, rec.t);
		if (rec !== null && typeof rec.beat === "number")
			since = Math.max(since, rec.beat);
		if (send_started(rec)) {
			if (since < now - live)
				out.stale_sends.push(id);
			return;
		}
		if (since < now - CLAIM_AGE) {
			var unknown = rec === null && send_file_names(sdir).length > 0;
			unclaim(ctx, id, sdir, d, unknown ? now : undefined);
			out.recovered++;
			return;
		}
		if (rec !== null && since < now - live)
			out.stale_sends.push(id);
	}

	// sweep(ctx [, now] [, opts]): first, of the claimed drafts
	// (drafts/<id>.sending/), returns to the drafts every one claimed (or
	// last advanced by its send) more than an hour ago whose send saved
	// nothing yet, by renaming it back, never deleting it (under a new id
	// when its id is a draft again; marked maybe_sent when its record is
	// unreadable but the send's files exist).  A claim with a send record
	// whose heartbeat is opts.live seconds old (default: an hour; the API
	// passes Send.LIVE) is not touched: its id is listed in stale_sends
	// and, when opts.recover is a function, opts.recover(id) is called for
	// it after the drafts lock is released (the send step's recovery:
	// Send.recover; a throw is logged).  Then removes, when untouched for 24 hours
	// (older than now - 86400): incomplete attachments (record and file);
	// drafts with no recipients, subject, body or attachments; files in a
	// draft directory that draft.json does not name (left by a failed
	// write or a crash); draft directories without a readable draft.json.
	// Nothing younger is removed, nor anything outside drafts/ or in a
	// directory whose name is not a draft id.  A failure on one directory
	// is logged and the sweep goes on with the next.  Returns {drafts,
	// files, recovered, stale_sends}: the numbers removed and recovered,
	// and the ids of stale claims left to the send step.
	function sweep(ctx, now, opts)
	{
		if (typeof now !== "number" || !isFinite(now))
			now = now_of(ctx);
		var limit = now - DAY;
		var live = opts && typeof opts.live === "number" && opts.live > 0 ? opts.live : CLAIM_AGE;
		var out = locked(ctx, function () {
			var out = { drafts: 0, files: 0, recovered: 0, stale_sends: [] };
			var root = root_of(ctx);
			if (!file_isdir(root))
				return out;
			var e = entries(root);
			var i;
			for (i = 0; i < e.dirs.length; i++) {
				var nm = name_of(e.dirs[i]);
				if (nm === null || !nm.sending)
					continue;
				try {
					sweep_claim(ctx, nm.id, now, live, out);
				} catch (x) {
					Drafts.log(LOG_WARNING, "drafts.sweep: " + root + e.dirs[i] + ": " + x);
				}
			}
			e = entries(root);
			for (i = 0; i < e.dirs.length; i++) {
				if (!ID_RE.test(e.dirs[i]))
					continue;
				try {
					sweep_draft(ctx, root, e.dirs[i], limit, out);
				} catch (x) {
					Drafts.log(LOG_WARNING, "drafts.sweep: " + root + e.dirs[i] + ": " + x);
				}
			}
			return out;
		});
		if (opts && typeof opts.recover === "function") {
			for (var i = 0; i < out.stale_sends.length; i++) {
				try {
					opts.recover(out.stale_sends[i]);
				} catch (x) {
					Drafts.log(LOG_WARNING, "drafts.sweep: recovery of " + out.stale_sends[i] + ": "
						+ (x && x.code ? x.code : x && x.message ? x.message : String(x))
						+ (x && x.reason ? " (" + x.reason + ")" : ""));
				}
			}
		}
		return out;
	}

	function send_files(dir, d)
	{
		var files = [];
		for (var i = 0; i < d.attachments.length; i++) {
			var a = d.attachments[i];
			var p = att_path(dir, a.id);
			files.push({ id: a.id, name: a.name, type: a.type, size: a.total, path: p });
		}
		return files;
	}

	// claim(ctx, id [, opts]): takes the draft for sending (opts: see the
	// header: send, the record written as send.json; confirm_resend).  Checks that every
	// attachment is complete and its file holds exactly its size (else
	// conflict, reason "incomplete"), records the time in draft.json as
	// claimed and renames drafts/<id>/ to drafts/<id>.sending/.  Returns
	// {draft (as get()), dir (the claimed directory), files: [{id, name,
	// type, size, path}]}; the paths stay valid until release() or
	// finish().  A claimed draft is notfound to every other method and to
	// a second claim.
	function claim(ctx, id, opts)
	{
		check_id(id);
		opts = opts || {};
		var rec = opts.send;
		if (rec !== undefined && !is_obj(rec))
			throw error("invalid", "send");
		return locked(ctx, function () {
			var x = load_draft(ctx, id);
			var d = x.draft;
			if (d.maybe_sent && opts.confirm_resend !== true)
				throw error("conflict", "maybe_sent");
			for (var i = 0; i < d.attachments.length; i++) {
				var a = d.attachments[i];
				var p = att_path(x.dir, a.id);
				if (!a.complete || !file_exists(p) || file_isdir(p) || file_size(p) !== a.total)
					throw error("conflict", "incomplete");
			}
			var sdir = sending_of(ctx, id);
			if (file_isdir(sdir))
				throw error("conflict", "sending");
			d.claimed = now_of(ctx);
			remove_send_files(x.dir);	// left by an interrupted unclaim()
			write_draft(ctx, x.dir, d);
			if (rec !== undefined)
				files_of(ctx, x.dir).write(SEND, rec);
			if (!rename_dir(x.dir, sdir)) {
				delete d.claimed;
				write_draft(ctx, x.dir, d);
				file_remove(files_of(ctx, x.dir).path(SEND));
				throw new Error("cannot rename " + x.dir + " to " + sdir);
			}
			return { draft: public_draft(sdir, d), dir: sdir, files: send_files(sdir, d) };
		});
	}

	// The order of a send record's phases: a write never moves backwards
	var PHASES = { claimed: 0, copy: 1, delivering: 2, delivered: 3, charging: 4, done: 5 };

	function phase_rank(rec)
	{
		return rec !== null && PHASES.hasOwnProperty(rec.phase) ? PHASES[rec.phase] : 0;
	}

	// Throws conflict "taken" unless 'token' owns claim directory sdir (a
	// record with no owner is anyone's)
	function check_owner(ctx, sdir, token)
	{
		var rec = send_record(ctx, sdir);
		if (rec !== null && rec.owner !== undefined && rec.owner !== token)
			throw error("conflict", "taken");
		return rec;
	}

	function same_state(a, b)
	{
		var keys = ["sid", "phase", "owner", "beat"];
		if (a === null || b === null)
			return a === b;
		for (var i = 0; i < keys.length; i++)
			if (a[keys[i]] !== b[keys[i]])
				return false;
		return true;
	}

	// take(ctx, id, seen, token, now, window): take over a claim's send by
	// compare-and-set, under the drafts lock: the record must still be
	// 'seen' (the record as the caller last read it, null for none: same
	// sid, phase, owner and beat; else conflict "taken") and its heartbeat
	// (beat, else the claim time) at least 'window' seconds old (else
	// conflict "sending").  Writes owner = token and beat = now; returns
	// the record.  notfound when not claimed.
	function take(ctx, id, seen, token, now, window)
	{
		check_id(id);
		if (typeof token !== "string" || token === "" || typeof now !== "number")
			throw error("invalid", "token");
		return locked(ctx, function () {
			var sdir = sending_of(ctx, id);
			if (!file_isdir(sdir))
				throw error("notfound");
			var rec = send_record(ctx, sdir);
			if (!same_state(rec, seen === undefined ? null : seen))
				throw error("conflict", "taken");
			var beat = rec !== null && typeof rec.beat === "number" ? rec.beat : null;
			if (beat === null) {
				var d = read_draft(ctx, sdir);
				beat = d !== null && d.claimed !== undefined ? d.claimed : newest(sdir);
			}
			if (now - beat < window)
				throw error("conflict", "sending");
			if (rec === null)
				rec = { phase: "claimed" };
			rec.owner = token;
			rec.beat = now;
			files_of(ctx, sdir).write(SEND, rec);
			return rec;
		});
	}

	// release(ctx, id [, {token, maybe_sent, maybe_sent_id}]): the send
	// failed (token: the owner's, see take(); maybe_sent_id: the Message-ID
	// of the send whose outcome is unknown, kept on the draft); the claimed
	// draft becomes a draft again (its send's files removed, claimed
	// dropped, maybe_sent set to the time given) and is returned (as
	// get()).  If a draft with that id exists, the returned draft has a
	// new id.  notfound when it is not claimed.
	function release(ctx, id, opts)
	{
		check_id(id);
		opts = opts || {};
		var maybe = typeof opts.maybe_sent === "number" ? opts.maybe_sent : undefined;
		return locked(ctx, function () {
			var sdir = sending_of(ctx, id);
			if (!file_isdir(sdir))
				throw error("notfound");
			check_owner(ctx, sdir, opts.token);
			var d = read_draft(ctx, sdir);
			if (d === null)
				throw error("notfound", "unreadable");
			var u = unclaim(ctx, id, sdir, d, maybe, opts.maybe_sent_id);
			return public_draft(u.dir, u.draft);
		});
	}

	// claim_age(ctx, id): null when the draft is not claimed by a send
	// (its directory is not drafts/<id>.sending/), else the age in
	// seconds of the claim's heartbeat (send.json's beat, else the claim
	// time).  Read WITHOUT the drafts lock, so a busy lock never hides a
	// claim: the claim is the atomic rename of the draft's directory and
	// send.json is replaced atomically; a heartbeat that cannot be read
	// counts as a fresh claim (0).
	function claim_age(ctx, id)
	{
		check_id(id);
		var sdir = sending_of(ctx, id);
		if (!file_isdir(sdir))
			return null;
		var beat = null;
		try {
			var rec = send_record(ctx, sdir);
			if (rec !== null && typeof rec.beat === "number")
				beat = rec.beat;
			else {
				var d = read_draft(ctx, sdir);
				beat = d !== null && typeof d.claimed === "number" ? d.claimed : newest(sdir);
			}
		} catch (e) {
			return 0;
		}
		if (typeof beat !== "number" || !isFinite(beat))
			return 0;
		return Math.max(0, now_of(ctx) - beat);
	}

	// is_claimed(ctx, id): true when the draft is claimed by a send
	function is_claimed(ctx, id)
	{
		return claim_age(ctx, id) !== null;
	}

	// claimed(ctx, id): a claim's state, or null when the draft is not
	// claimed: {dir, draft (as get(), null if unreadable), files (as
	// claim()), claimed (time_t), send (its send.json, or null)}
	function claimed(ctx, id)
	{
		check_id(id);
		return locked(ctx, function () {
			var sdir = sending_of(ctx, id);
			if (!file_isdir(sdir))
				return null;
			var d = read_draft(ctx, sdir);
			return { dir: sdir, draft: d === null ? null : public_draft(sdir, d),
				files: d === null ? [] : send_files(sdir, d),
				claimed: d !== null && d.claimed !== undefined ? d.claimed : newest(sdir),
				send: send_record(ctx, sdir) };
		});
	}

	// set_send(ctx, id, record, token): replaces the claim's send.json
	// (written atomically under the drafts lock) when 'token' owns the
	// claim and the record's phase is not earlier than the stored one;
	// else conflict "taken" and nothing is written.  The record written
	// carries owner = token.  true; notfound when not claimed.
	function set_send(ctx, id, rec, token)
	{
		check_id(id);
		if (!is_obj(rec))
			throw error("invalid", "send");
		return locked(ctx, function () {
			var sdir = sending_of(ctx, id);
			if (!file_isdir(sdir))
				throw error("notfound");
			var cur = check_owner(ctx, sdir, token);
			if (phase_rank(rec) < phase_rank(cur))
				throw error("conflict", "taken");
			if (token !== undefined)
				rec.owner = token;
			files_of(ctx, sdir).write(SEND, rec);
			return true;
		});
	}

	// finish(ctx, id): the draft was sent; the claimed directory and its
	// files are removed.  true; notfound when it is not claimed.
	function finish(ctx, id, token)
	{
		check_id(id);
		return locked(ctx, function () {
			var sdir = sending_of(ctx, id);
			if (!file_isdir(sdir))
				throw error("notfound");
			check_owner(ctx, sdir, token);
			remove_tree(sdir);
			return true;
		});
	}

	// limits(ctx): the limits for the client (no file is touched)
	function limits(ctx)
	{
		var c = ctx.config;
		return { max_drafts: c.max_drafts, max_draft_bytes: c.max_draft_bytes,
			max_attachment_bytes: c.max_attachment_bytes, max_body_bytes: c.max_body_bytes,
			max_attachments: MAX_ATTACHMENTS, max_chunk: MAX_CHUNK };
	}

	var Drafts = {
		hook: null,	// tests: hook(point, dir) inside unclaim(); a throw stops it there
		MAX_CHUNK: MAX_CHUNK,
		MAX_ATTACHMENTS: MAX_ATTACHMENTS,
		MAX_ADDR_FIELD: MAX_ADDR_FIELD,
		valid_utf8: valid_utf8,
		clean_name: clean_name,
		clean_type: clean_type,
		log: function (level, text) {
			log(level, text);
		}
	};
	var PUBLIC = { create: create, get: get, list: list, count: count, usage: usage, save: save,
		remove: remove, attach: attach, detach: detach, sweep: sweep, claim: claim, release: release,
		finish: finish, limits: limits, claimed: claimed, is_claimed: is_claimed, claim_age: claim_age,
		set_send: set_send, take: take };
	for (var name in PUBLIC) {
		(function (name, fn) {
			Drafts[name] = function () {
				try {
					return fn.apply(null, arguments);
				} catch (e) {
					throw outgoing(name, e);
				}
			};
		})(name, PUBLIC[name]);
	}

	return Drafts;
})();

Drafts;
