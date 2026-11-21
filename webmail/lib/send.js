// Send: sending a saved draft, for the web mail client.  A pure library:
// no API methods, no web or session globals.
//
// var Send = load("send.js");
// Send.send(ctx, draft_id, key [, {confirm_resend}]);
//                           // {ok, id, message_id [, warning]}
// Send.recover(ctx, draft_id);   // a stale claim's send finished or undone
//
// ctx is the API's mailbox context (as for Drafts): usernum, alias, user
// (the sender's User object, or a fake with the same members: number,
// alias, name, email, security.{restrictions, exemptions, credits,
// free_credits}, can_send_mail(), adjust_credits(), sent_email()), store
// (an opened Store: the user's mailbox), userdata, config (max_recipients,
// max_message_bytes, and what Drafts uses), and optionally:
//   domains     the From domains the user may use (default
//               Address.domains_for(usernum))
//   settings    the user's web mail settings (display_name)
//   now         a function returning a time_t (default time())
//   resolvers   Address.system_resolvers()'s result (tests pass fakes)
//   sys         { inet_addr, fido_addrs, inet_cost, fido_cost,
//               inet_alias, fido_alias, user_email(usernum),
//               check_netmail_addr(address), mailbox_full(usernum) }
//               (default: system.inet_addr, system.fido_addr_list,
//               msg_area.inet_netmail_cost and fido_netmail_cost, the
//               NMAIL_ALIAS settings, new User(n).email,
//               system.check_netmail_addr, new User(n).mailbox_full())
//   collect     a function(entries) adding the recipients to the user's
//               contacts (default: Contacts.collect with ctx.userdata;
//               null: none); a failure is logged
//
// key: a client-chosen idempotency key, 16 to 64 of [A-Za-z0-9_-], new
// for each press of Send.  Completed sends are indexed by key per user
// (webmail.sends.json: {key, draft, t, result}, the last 50, kept 24
// hours): a repeat with an indexed key returns the recorded outcome and
// sends nothing; with an indexed key and another draft id it is conflict
// (reason "key").
//
// The send's record lives with the claim: send.json inside the claimed
// draft directory (Drafts.claim/claimed/set_send), as do the files the
// send builds (send.body, send.plain), so all of it lasts exactly as long
// as the claim and nothing about an unfinished send can expire by age or
// count.  Each phase is written before the step it names begins:
//   claimed     the draft is claimed; nothing saved
//   copy        the Sent copy is being saved (its id recorded right after)
//   delivering  the delivery copies are being saved; the record holds the
//               send's unique id (Message-ID), the shared base's last
//               message number before the saves ("after"), every delivery
//               group (header, recipient entries with their destination
//               keys, body file) and what step 5 charges
//   delivered   every recipient has a copy
//   charging    credits and e-mail counters are being applied
//   done        the outcome; then the key is indexed and the claim removed
//
// The Message-ID is "<" + 24 hexadecimal digits from cryptlib's random
// number generator (96 bits, new for every send) + "." + the time in
// hexadecimal + "@" + the From domain + ">": unique per send, never
// derived from the base.  "This send's copies" are the records above
// "after", from the user, with that Message-ID, INCLUDING ones marked
// deleted (the mail server deletes an Internet copy once sent, a local
// recipient may delete theirs) and ones marked sent or in transit: one
// index read plus one header read per record of the user above "after".
//
// The rule: once the phase is "delivering" or later, the message is never
// sent again as a whole.  Recovery (a later send of that draft, with any
// key, or Drafts.sweep's recover callback: Send.recover) of a claim whose
// record is older than LIVE (600 s, so a running send is never taken
// over) does:
//   claimed     the draft is released (nothing was saved)
//   copy        the Sent copy (by its recorded id, or found by Message-ID
//               with Store.find_sent) is removed if it is still $Unsent;
//               the draft is released
//   delivering  this send's copies are looked for: if there is one or
//               more, the recipients that have none get a copy (only
//               they), then as "delivered"; if there is none the outcome
//               is unknown (crash before the save, or the copies packed
//               away): nothing is sent, the draft is released marked
//               maybe_sent (the time), the Sent copy keeps $Unsent, and
//               conflict (reason "unknown") is thrown; such a draft is
//               sent again only by a send with confirm_resend: true
//   delivered   the finish steps, charges included
//   charging    the finish steps without the charges (a crash between
//               the record and the charge loses the charge: never charged
//               twice)
//   done        the key indexed, the claim removed
// Ownership: the record names its owner (a random token) and a heartbeat
// (beat).  Every write of the record, release and finish is a
// compare-and-set on the token (Drafts.set_send, release, finish: conflict
// "taken" for another token; a phase never moves backwards); a worker whose
// write fails has lost the claim and stops at once, with no further save
// and no cleanup, and answers conflict "sending" (asked again later, the
// key or the draft id resolves to the recorded outcome).  The owner
// refreshes the heartbeat right before the Sent copy save, before each
// delivery save call and by the "charging" write, which is the one
// compare-and-set that decides who charges.  Another worker takes a claim
// over (Drafts.take: compare-and-set on the record as it read it) only
// once the heartbeat is LIVE seconds old.  The heartbeat is also
// refreshed right after the message body is built; a build that itself
// takes longer than LIVE can be taken over meanwhile (the builder then
// fails its next write and stops).  LIVE is 600: the longest step
// without a heartbeat is one native save reading the stored body (a 20 MB
// message is about 27 MB encoded; at 50 KB/s from a slow network disk that
// is about 9 minutes).  Correctness does not depend on the window: a
// worker that wakes up late fails its next write.  The one residue: a
// worker stalled INSIDE a native save call that completes after a
// takeover; its copies carry this send's id, so the recovery that took
// over saved only recipients with no copy at that moment, and a duplicate
// for the recipients of that one call is possible.
//
// Which recipients have a copy: after each save call the copies with the
// send's id are read back and each recipient's message number recorded
// (send.json "saved"), matched by destination in the stored form (local
// user number; Internet address without brackets, ASCII case ignored;
// FidoNet address as smb_faddrtoa writes it; QWK id, last route hop, case
// ignored), else by order (the save adds one header per entry, in order).
// Copies this send took back itself (a failed send's rollback) are no
// evidence of delivery: before each removal its number is recorded in
// send.json "recall" as "intent" (the copy re-read and removable at that
// moment), after it "done", or dropped when the removal failed.  A copy
// counts as recalled when "done", or "intent" and deleted without
// NETMSG_SENT or NETMSG_INTRANSIT (a crash between the delete and the
// record); a copy with an intent that is still live counts as delivered,
// and so does one the mail server marked sent or in transit in the
// instant between the check and the removal (re-read after it: no
// "done").  Recalled copies are left out of matching, recorded numbers
// and the count rule, so their recipients get a copy again.
// A recipient is missing only when it has no recorded number and no copy
// matches it; when some copies cannot be matched to recipients, delivery
// counts as complete if there are at least as many copies as recipients,
// else conflict "partial" (never a guess that could send twice).
//
// The finish steps besides charging are repeatable and run in every
// completion: $Unsent removed, the source of a reply marked replied, the
// recipients added to contacts.  Recovery's outcome carries warning
// "recovered".  A send that meets a live claim (heartbeat younger than
// LIVE) gets conflict (reason "sending").  A send of a draft whose send
// completed under another key returns that outcome.  An unknown outcome
// also marks the Sent copy $Unknown (Store.mark_unknown) and keeps the
// send's Message-ID on the draft (maybe_sent_id): a confirmed resend that
// completes removes that earlier Sent copy.
//
// A failed delivery save (an exception, not a crash): this send's copies
// are looked for.  If every one is removable (not deleted, not sent, not
// in transit) and is removed, everything is rolled back (the Sent copy
// removed, the draft released) and failed is thrown.  If any could not be
// removed the message is partly out: the recipients without a copy get
// one, and the send completes; if that save fails too the claim stays in
// "delivering" for a later recovery and conflict (reason "partial") is
// thrown.  A failure before "delivering" removes the Sent copy and
// releases the draft.
//
// Checks (before anything is saved): recipients from To, CC and BCC
// through Address.parse_list() and Address.resolve(), each destination
// once (the first occurrence decides To, CC or BCC); none: invalid
// ("norecipients"); more than max_recipients: invalid ("toomany"); a QWK
// address only when sys.check_netmail_addr() accepts it (it is true only
// when qwk_route() finds a route), else forbidden ("qwk"); FidoNet: a
// recipient name is needed ("fidoname"), and a FidoNet address of the
// system ("fido"); user.can_send_mail(net_type, usernum, count) for every
// recipient, else forbidden ("recipient", with index and entry); a local
// recipient whose mailbox takes no more mail (sys.mailbox_full(): Max
// Messages Waiting, as the mail server and the terminal decide it):
// forbidden ("mailbox_full", with index and entry), so one full
// recipient refuses the whole message; a sysop (user.is_sysop) is not
// refused: the message is sent with warning "mailboxfull";
// attachments with a FidoNet or QWK recipient: invalid ("attachments");
// the netmail cost (Internet and FidoNet, unless the user has the cost
// exemption) against credits plus free credits: forbidden ("credits");
// the From domain ("" is the system's Internet address; another must be
// allowed: forbidden "domain"); the estimated size against
// max_message_bytes: toolarge.
//
// The daily e-mail limit (in can_send_mail) and the credits are checked
// here, before anything is saved, and the counters and credits are
// applied after delivery (phase "charging"), with nothing held between:
// sends running at the same time (several windows) each pass the check,
// so a user can exceed the daily limit, and spend credits beyond the
// balance, by up to the number of sends running at once, as with the
// terminal server on two nodes.  A check under the drafts lock would not
// close it: the counters live in the user record and are applied only
// once delivery is done, possibly minutes later.
//
// Headers: subject through Address.clean (255 bytes); reply_id the
// draft's reply_message_id, left out when over 900 bytes; priority 2 for
// "high", 4 for "low"; auxattr MSG_HFIELDS_UTF8.  to_list and cc_list
// (recipient fields: set on every recipient entry): Internet recipients
// as Address.format(name, address), local recipients as
// Address.format(name, their e-mail address) or left out when that
// address cannot be written so, FidoNet and QWK recipients never; BCC in
// neither; a list over 990 bytes is left out of the delivery copies
// (warning "longlist"; the mail server then prints a To line from the
// copy's own name and address, so an Internet recipient's name goes
// through Address.display_name() with '@' removed).  Internet copies:
// from the display name or the terminal's name (Address.identity),
// from_net_type Internet, from_net_addr and reverse_path the From address
// (as the mail server stores an authenticated user's mail to other
// systems), netattr NETMSG_LOCAL | NETMSG_KILLSENT.  Local copies: from
// the alias, no network fields.  FidoNet and QWK copies: plain text in
// UTF-8 (NUL removed) marked ftn_charset "UTF-8 4" as webv4's postMail()
// does (nothing converted, nothing lost); FidoNet netattr NETMSG_LOCAL |
// NETMSG_KILLSENT, origin address left to SBBSecho; QWK netattr
// NETMSG_LOCAL.  The Sent copy lists every recipient, BCC in an
// RFC822HEADER "Bcc:" field.
//
// Errors are thrown as { code, message } with a fixed message, code one of
// invalid, notfound, forbidden, toolarge, conflict, failed, quota, plus
// reason, index and entry where stated.  Logging: one line per send
// (user, counts by kind, bytes), no address or subject.
//
// Send.hook (tests only): called at named points; a throw of an object
// with crash: true passes through Send untouched (no cleanup), which is
// how the tests model a process that dies at that point.

"use strict";

var Send = (function () {
	var HERE = (function () {
		var f = String(new Error().fileName || "");
		var i = Math.max(f.lastIndexOf('/'), f.lastIndexOf('\\'));
		return i >= 0 ? f.substr(0, i + 1) : "";
	})();

	function sibling(name)
	{
		return load({}, HERE !== "" && file_exists(HERE + name) ? HERE + name : name);
	}

	var Address = sibling("address.js");
	var Drafts = sibling("drafts.js");
	var MimeWrite = sibling("mimewrite.js");
	var Contacts = sibling("contacts.js");
	var D = {};
	load(D, HERE !== "" && file_exists(HERE + "../../exec/load/smbdefs.js")
		? HERE + "../../exec/load/smbdefs.js" : "smbdefs.js");

	var KEY_RE = /^[A-Za-z0-9_-]{16,64}$/;
	var INDEX = "sends";
	var MAX_INDEX = 50;
	var KEEP = 86400;	// seconds a completed send stays indexed
	var LIVE = 600;	// a claim whose record is younger belongs to a running send
	var MAX_LIST = 990;
	var MAX_REPLY_ID = 900;
	var BODY = "send.body";
	var PLAIN = "send.plain";
	var UEXEMPT_NETMAIL_COST = 1 << 18;	// userdefs.js: UFLAG_S
	var MESSAGES = {
		invalid: "the message cannot be sent as it is",
		notfound: "no such draft",
		forbidden: "not allowed",
		toolarge: "the message is too large",
		conflict: "the message is being sent, or may have been sent already",
		failed: "the message could not be sent",
		quota: "the mailbox is full"
	};
	var PASS = { reason: true, index: true, entry: true };
	var seq = 0;

	function error(code, reason, extra)
	{
		var e = { code: code, message: MESSAGES[code] };
		if (reason !== undefined)
			e.reason = reason;
		for (var k in extra || {})
			e[k] = extra[k];
		return e;
	}

	function is_coded(e)
	{
		return e !== null && typeof e === "object" && !(e instanceof Error) && typeof e.code === "string";
	}

	function is_crash(e)
	{
		return e !== null && typeof e === "object" && e.crash === true;
	}

	// The claim was taken over by another worker (its record's owner token
	// is no longer ours): stop at once, no further save, no cleanup
	function lost()
	{
		var e = error("conflict", "sending");
		e.lost = true;
		return e;
	}

	function is_lost(e)
	{
		return e !== null && typeof e === "object" && e.lost === true;
	}

	// Passed through every catch untouched
	function is_stop(e)
	{
		return is_crash(e) || is_lost(e);
	}

	function outgoing(e)
	{
		if (is_crash(e))
			return e;
		if (is_lost(e))
			return error("conflict", "sending");
		if (is_coded(e) && MESSAGES[e.code] !== undefined) {
			var out = { code: e.code, message: MESSAGES[e.code] };
			for (var k in PASS)
				if (e[k] !== undefined && (typeof e[k] === "string" || typeof e[k] === "number"))
					out[k] = e[k];
			return out;
		}
		Send.log(LOG_ERR, "send: " + (e && e.message ? e.message : String(e))
			+ (e && e.fileName ? " (" + e.fileName + ":" + e.lineNumber + ")" : ""));
		return error("failed");
	}

	function text_of(e)
	{
		if (is_coded(e))
			return e.code + (e.reason ? "/" + e.reason : "");
		return e && e.message ? e.message : String(e);
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

	function hook(point, info)
	{
		if (typeof Send.hook === "function")
			Send.hook(point, info);
	}

	function lower(s)
	{
		return String(s).replace(/[A-Z]/g, function (c) {
			return String.fromCharCode(c.charCodeAt(0) + 32);
		});
	}

	function hex8(n)
	{
		var s = (n >>> 0).toString(16).toUpperCase();
		while (s.length < 8)
			s = "0" + s;
		return s;
	}

	// 24 hexadecimal digits (96 bits) from cryptlib's random number
	// generator (an HMAC under a generated key, as drafts.js and session.js)
	function random_hex()
	{
		var c = new CryptContext(CryptContext.ALGO["HMAC-SHA2"]);
		c.generate_key();
		c.encrypt("webmail send " + (seq++) + " " + system.timer);
		var mac = c.hashvalue;
		if (typeof mac !== "string" || mac.length < 12)
			throw new Error("send id generation failed");
		var s = "";
		for (var i = 0; i < 12; i++) {
			var b = mac.charCodeAt(i) & 0xFF;
			s += (b < 16 ? "0" : "") + b.toString(16);
		}
		return s;
	}

	function sys_of(ctx)
	{
		var s = ctx.sys || {};
		function pick(k, def) {
			return s[k] !== undefined ? s[k] : def();
		}
		return {
			inet_addr: lower(String(pick("inet_addr", function () { return system.inet_addr; }))),
			fido_addrs: pick("fido_addrs", function () { return system.fido_addr_list; }) || [],
			inet_cost: pick("inet_cost", function () { return msg_area.inet_netmail_cost; }) || 0,
			fido_cost: pick("fido_cost", function () { return msg_area.fido_netmail_cost; }) || 0,
			alias: {
				inet_alias: pick("inet_alias", function () {
					return (msg_area.inet_netmail_settings & Address.NMAIL_ALIAS) != 0;
				}),
				fido_alias: pick("fido_alias", function () {
					return (msg_area.fido_netmail_settings & Address.NMAIL_ALIAS) != 0;
				})
			},
			user_email: typeof s.user_email === "function" ? s.user_email : function (n) {
				return String(new User(n).email);
			},
			check_netmail_addr: typeof s.check_netmail_addr === "function" ? s.check_netmail_addr
				: function (a) {
					return system.check_netmail_addr(a) === true;
				},
			mailbox_full: typeof s.mailbox_full === "function" ? s.mailbox_full : function (n) {
				return new User(n).mailbox_full() !== 0;
			}
		};
	}

	// ---------------------------------------------------------------
	// The index of completed sends (webmail.sends.json)

	function index_list(o, now)
	{
		var list = (o && o.list instanceof Array) ? o.list : [];
		var kept = [];
		for (var i = 0; i < list.length; i++) {
			var r = list[i];
			if (r && typeof r === "object" && typeof r.key === "string" && typeof r.t === "number"
				&& typeof r.draft === "string" && r.result && typeof r.result === "object"
				&& now - r.t < KEEP && r.t <= now + 60)
				kept.push(r);
		}
		while (kept.length > MAX_INDEX)
			kept.shift();	// oldest first
		return kept;
	}

	function index_get(ctx, key)
	{
		var list = index_list(ctx.userdata.read(INDEX, null), now_of(ctx));
		for (var i = 0; i < list.length; i++)
			if (list[i].key === key)
				return list[i];
		return null;
	}

	function index_by_draft(ctx, draft_id)
	{
		var list = index_list(ctx.userdata.read(INDEX, null), now_of(ctx));
		for (var i = list.length - 1; i >= 0; i--)
			if (list[i].draft === draft_id)
				return list[i];
		return null;
	}

	function index_put(ctx, key, draft_id, result)
	{
		var now = now_of(ctx);
		ctx.userdata.update(INDEX, { v: 1, list: [] }, function (o) {
			var list = index_list(o, now);
			for (var i = 0; i < list.length; i++)
				if (list[i].key === key)
					list.splice(i--, 1);
			list.push({ key: key, draft: draft_id, t: now, result: result });
			while (list.length > MAX_INDEX)
				list.shift();
			return { v: 1, list: list };
		});
	}

	// ---------------------------------------------------------------
	// The claim's record

	// Write the record with a new phase (and the time it was entered)
	function phase(ctx, c, rec, name)
	{
		rec.phase = name;
		rec.t = now_of(ctx);
		beat(ctx, c, rec);
	}

	// Write the record as its owner (compare-and-set on the owner token,
	// Drafts.set_send), refreshing the heartbeat.  A claim taken over by
	// another worker (conflict "taken") or gone (notfound: swept back) is
	// lost: the caller stops.
	function beat(ctx, c, rec)
	{
		rec.beat = now_of(ctx);
		try {
			Drafts.set_send(ctx, c.id, rec, c.token);
		} catch (e) {
			if (is_coded(e) && ((e.code === "conflict" && e.reason === "taken") || e.code === "notfound"))
				throw lost();
			throw e;
		}
	}

	// Drafts.release / finish as the claim's owner; lost when not
	function owned(ctx, fn)
	{
		try {
			return fn();
		} catch (e) {
			if (is_coded(e) && e.code === "conflict" && e.reason === "taken")
				throw lost();
			throw e;
		}
	}

	// ---------------------------------------------------------------
	// The shared base and this send's copies

	function open_shared(ctx)
	{
		var path = ctx.store && ctx.store.shared_path;
		var mb = path ? new MsgBase(path, true, true) : new MsgBase("mail");
		if (!mb.open())
			throw new Error("cannot open the mail base: " + mb.last_error);
		return mb;
	}

	// A copy's destination: the same key for a recipient entry (as saved)
	// and for the header of its copy as read back
	function dest_key(h)
	{
		var t = h.to_net_type || D.NET_NONE;
		var a = String(h.to_net_addr === undefined || h.to_net_addr === null ? "" : h.to_net_addr);
		a = a.replace(/^[ \t]+|[ \t]+$/g, "");
		switch (t) {
			case D.NET_NONE:
				return "u:" + parseInt(h.to_ext, 10);
			case D.NET_INTERNET:	// stored as given: compared without brackets, ASCII case ignored
				return "i:" + lower(a.replace(/^<|>$/g, ""));
			case D.NET_FIDO:	// stored parsed (smb_faddrtoa form)
				return "f:" + lower(h.to) + "@" + Address.fido_canonical(a);
			case D.NET_QWK:	// the save may prefix a route: the last hop is the id
				return "q:" + lower(h.to) + "@" + lower(a.substr(a.lastIndexOf("/") + 1));
		}
		return "?:" + t + ":" + lower(a);
	}

	// This send's copies, deleted ones included: [{num, key, removable}]
	function scan(ctx, rec)
	{
		var mb = open_shared(ctx);
		var out = [];
		try {
			var idx = mb.get_index() || [];
			var u16 = ctx.usernum & 0xFFFF;
			for (var i = 0; i < idx.length; i++) {
				var x = idx[i];
				if (x.number <= rec.after || x.from != u16 || (x.attr & D.MSG_VOTE))
					continue;
				var h = mb.get_msg_header(false, x.number, false);
				if (h === null || h === undefined || h.number != x.number || h.id !== rec.msgid
					|| parseInt(h.from_ext, 10) !== ctx.usernum)
					continue;
				if (recalled(rec, x.number, h))
					continue;	// this send took it back: no evidence of delivery
				out.push({ num: x.number, key: dest_key(h),
					removable: !(h.attr & D.MSG_DELETE) && !(h.netattr & (D.NETMSG_SENT | D.NETMSG_INTRANSIT)) });
			}
		} finally {
			mb.close();
		}
		return out;
	}

	// Remove the removable copies one by one (each header read again
	// first); true when every copy found was removed
	function remove_copies(ctx, c, rec, found)
	{
		if (!rec.recall || typeof rec.recall !== "object")
			rec.recall = {};
		var all = true;
		var mb = open_shared(ctx);
		try {
			for (var i = 0; i < found.length; i++) {
				var n = found[i].num;
				var h = mb.get_msg_header(false, n, false);
				if (!found[i].removable || h === null || h === undefined || h.id !== rec.msgid
					|| (h.attr & D.MSG_DELETE) || (h.netattr & (D.NETMSG_SENT | D.NETMSG_INTRANSIT))) {
					all = false;
					continue;
				}
				// The intent first (a crash now leaves a live copy: delivered)
				rec.recall["n:" + n] = "intent";
				beat(ctx, c, rec);
				hook("recalling", null);
				var ok = mb.remove_msg(false, n);
				// A crash now leaves the intent and a copy deleted without
				// NETMSG_SENT: recalled() counts it as taken back
				hook("removed", null);
				// The mail server may have taken the copy in the instant
				// between the check above and the removal: then it went
				// out and is not recalled (its recipient gets no second)
				if (ok) {
					var after = mb.get_msg_header(false, n, false);
					if (after && (after.netattr & (D.NETMSG_SENT | D.NETMSG_INTRANSIT)))
						ok = false;
				}
				if (ok)
					rec.recall["n:" + n] = "done";
				else {
					delete rec.recall["n:" + n];
					all = false;
				}
				beat(ctx, c, rec);
				hook("recalled", null);
			}
		} finally {
			mb.close();
		}
		return all;
	}

	// Whether copy number n (header h) was taken back by this send: its
	// removal confirmed ("done"), or attempted ("intent") and the copy is
	// deleted but was never sent (the mail server marks a sent copy
	// NETMSG_SENT before deleting it; the copy was removable when the
	// intent was written)
	function recalled(rec, n, h)
	{
		var r = rec.recall && typeof rec.recall === "object" ? rec.recall["n:" + n] : undefined;
		if (r === "done")
			return true;
		return r === "intent" && (h.attr & D.MSG_DELETE) != 0
			&& !(h.netattr & (D.NETMSG_SENT | D.NETMSG_INTRANSIT));
	}

	// Every intended recipient: [{dest, group (index), entry}]
	function intended(rec)
	{
		var out = [];
		for (var g = 0; g < rec.groups.length; g++)
			for (var j = 0; j < rec.groups[g].rcpts.length; j++)
				out.push({ dest: rec.groups[g].rcpts[j].dest, group: g, entry: rec.groups[g].rcpts[j] });
		return out;
	}

	// Record in rec.saved (destination key -> message number) the copies
	// found for 'wanted' recipients not yet recorded, matching the stored
	// destination keys; numbers already recorded are never used twice.
	// Returns the recipients still unmatched.
	function match(rec, wanted, found)
	{
		var used = {};
		var k;
		for (k in rec.saved)
			used[rec.saved[k]] = true;
		var left = [];
		for (var i = 0; i < wanted.length; i++) {
			var w = wanted[i];
			if (rec.saved.hasOwnProperty("d:" + w.dest))
				continue;
			var hit = null;
			for (var j = 0; j < found.length && hit === null; j++)
				if (found[j].key === w.dest && used[found[j].num] !== true)
					hit = found[j];
			if (hit === null) {
				left.push(w);
				continue;
			}
			rec.saved["d:" + w.dest] = hit.num;
			used[hit.num] = true;
		}
		return left;
	}

	// The copies of this send not recorded for any recipient
	function unassigned(rec, found)
	{
		var used = {};
		for (var k in rec.saved)
			used[rec.saved[k]] = true;
		var out = [];
		for (var i = 0; i < found.length; i++)
			if (used[found[i].num] !== true)
				out.push(found[i]);
		return out;
	}

	// The recipients that have no copy, for a completion; [] when delivery
	// is complete.  A recipient has a copy when its number is recorded or a
	// copy of this send matches its stored destination; one still unmatched
	// while copies of this send are unaccounted for is ambiguous: complete
	// when there are at least as many copies as recipients, else conflict
	// "partial" (never a guess that could send twice).
	function missing(ctx, rec)
	{
		var all = intended(rec);
		var found = scan(ctx, rec);
		prune_saved(rec, found);	// recorded copies this send recalled are not copies
		var left = match(rec, all, found);
		if (left.length === 0 || found.length >= all.length)
			return [];
		if (unassigned(rec, found).length > 0)
			throw error("conflict", "partial");
		return left;
	}

	// Save the delivery groups (only the 'only' recipients, when given),
	// as the claim's owner: the heartbeat is refreshed right before each
	// save call, and after each the copies it made are recorded by number
	// (by stored destination; failing that, by order: the save adds one
	// header per entry, in order, so its copies are numbered in the order
	// of its entries)
	function save_groups(ctx, c, rec, only)
	{
		var pick = null;
		if (only) {
			pick = {};
			for (var i = 0; i < only.length; i++)
				pick["d:" + only[i].dest] = true;
		}
		for (var g = 0; g < rec.groups.length; g++) {
			var grp = rec.groups[g];
			var list = [];
			var wanted = [];
			for (var j = 0; j < grp.rcpts.length; j++) {
				var d = grp.rcpts[j].dest;
				if (rec.saved.hasOwnProperty("d:" + d) || (pick !== null && pick["d:" + d] !== true))
					continue;
				list.push(grp.rcpts[j]);
				wanted.push({ dest: d, group: g, entry: grp.rcpts[j] });
			}
			if (list.length === 0)
				continue;
			hook("delivery", grp.kind);
			beat(ctx, c, rec);
			var mb = open_shared(ctx);
			var before;
			try {
				before = mb.last_msg;
				if (!mb.save_msg_from_file(grp.hdr, c.dir + grp.file, list))
					throw new Error("save of the " + grp.kind + " copies: " + mb.last_error);
			} finally {
				mb.close();
			}
			hook("saved", grp.kind);
			var found = scan(ctx, rec);
			var left = match(rec, wanted, found);
			if (left.length > 0) {
				var mine = [];
				var spare = unassigned(rec, found);
				for (j = 0; j < spare.length; j++)
					if (spare[j].num > before)
						mine.push(spare[j]);
				mine.sort(function (a, b) {
					return a.num - b.num;
				});
				if (mine.length === left.length)
					for (j = 0; j < left.length; j++)
						rec.saved["d:" + left[j].dest] = mine[j].num;
			}
			beat(ctx, c, rec);	// the numbers recorded
		}
	}

	// ---------------------------------------------------------------
	// Recipients and checks

	function parse_field(text, base)
	{
		try {
			return Address.parse_list(String(text || ""));
		} catch (e) {
			if (is_coded(e) && typeof e.index === "number" && e.index >= 0)
				e.index += base;
			throw e;
		}
	}

	// [{kind, field, name, addr, usernum, net_type, entry, index,
	// given_name, typed}], each destination once
	function recipients(ctx, draft, resolvers)
	{
		var out = [];
		var seen = {};
		var index = 0;
		var fields = ["to", "cc", "bcc"];
		for (var f = 0; f < fields.length; f++) {
			var list = parse_field(draft[fields[f]], index);
			for (var i = 0; i < list.length; i++, index++) {
				var r;
				try {
					r = Address.resolve(list[i], resolvers.matchuser, resolvers.netaddr_type, resolvers.own);
				} catch (e) {
					if (is_coded(e))
						e.index = index;
					throw e;
				}
				var key = "k:" + (r.kind == "local" ? "u:" + r.usernum : r.kind + ":" + lower(r.addr));
				if (seen[key] === true)
					continue;
				seen[key] = true;
				r.field = fields[f];
				r.index = index;
				r.entry = Address.clean(list[i].addr, 64);
				r.given_name = Address.clean(list[i].name, Address.MAX_NAME);
				r.typed = list[i].addr;
				out.push(r);
			}
		}
		return out;
	}

	// The name a FidoNet or QWK copy is addressed to: the display name,
	// else the part before '@'
	function net_name(r)
	{
		var name = r.given_name;
		var at = r.addr.lastIndexOf("@");
		if (name === "" && at > 0)
			name = r.addr.substr(0, at);
		return Address.clean(name, r.kind == "qwk" ? 25 : 35);
	}

	function check(ctx, draft, files, rcpts, sys)
	{
		var user = ctx.user;
		var count = rcpts.length;
		if (count < 1)
			throw error("invalid", "norecipients");
		var max = ctx.config.max_recipients;
		if (typeof max === "number" && max > 0 && count > max)
			throw error("invalid", "toomany");
		var n = { local: 0, internet: 0, fido: 0, qwk: 0 };
		var full = false;
		for (var i = 0; i < rcpts.length; i++) {
			var r = rcpts[i];
			var where = { index: r.index, entry: r.entry };
			if (r.kind == "qwk") {
				var routed = false;
				try {
					routed = sys.check_netmail_addr(r.addr) === true;
				} catch (e) {
					routed = false;
				}
				if (!routed)
					throw error("forbidden", "qwk", where);
			}
			if (r.kind == "fido" && sys.fido_addrs.length < 1)
				throw error("forbidden", "fido", where);
			if ((r.kind == "fido" || r.kind == "qwk") && net_name(r) === "")
				throw error("invalid", "fidoname", where);
			var net = r.kind == "local" ? Address.NET_NONE : r.net_type;
			var ok = false;
			try {
				ok = user.can_send_mail(net, r.kind == "local" ? r.usernum : 0, count) === true;
			} catch (e) {
				ok = false;
			}
			if (!ok)
				throw error("forbidden", "recipient", where);
			// Not caught: a check that cannot be made fails the send
			if (r.kind == "local" && sys.mailbox_full(r.usernum)) {
				if (user.is_sysop !== true)
					throw error("forbidden", "mailbox_full", where);
				full = true;
			}
			n[r.kind]++;
		}
		if (files.length > 0 && (n.fido > 0 || n.qwk > 0))
			throw error("invalid", "attachments");
		var cost = 0;
		var sec = user.security || {};
		if (!((sec.exemptions || 0) & UEXEMPT_NETMAIL_COST))
			cost = n.internet * sys.inet_cost + n.fido * sys.fido_cost;
		if (cost > 0 && cost > (sec.credits || 0) + (sec.free_credits || 0))
			throw error("forbidden", "credits");
		return { n: n, cost: cost, full: full };
	}

	// The chosen From domain and, with Internet recipients, the From address
	function sender(ctx, draft, sys, n)
	{
		var allowed = ctx.domains instanceof Array ? ctx.domains : Address.domains_for(ctx.usernum);
		var want = lower(String(draft.from_domain || ""));
		var domain = want === "" ? sys.inet_addr : want;
		var ok = false;
		for (var i = 0; i < allowed.length; i++)
			if (lower(String(allowed[i])) === domain)
				ok = true;
		if (want !== "" && !ok)
			throw error("forbidden", "domain");
		var address = "";
		var local = String(draft.from_local || "");
		if (local !== "") {
			// a chosen name: with the domain it must be one of the user's
			// addresses now, whatever it was when the draft was saved
			var mine = ctx.addresses instanceof Array ? ctx.addresses
				: Address.addresses_for(ctx.usernum, { domains: allowed });
			var pick = Address.from_pick(local, want, mine, function (addr) {
				if (typeof ctx.own_address === "function")
					return ctx.own_address(addr) === true;
				return Address.own_address(ctx.usernum, addr, { domains: allowed });
			});
			if (pick === null)
				throw error("forbidden", "domain");
			domain = lower(pick.substr(pick.lastIndexOf("@") + 1));
			if (n.internet > 0)
				address = pick;
		} else if (n.internet > 0) {
			var email = String(ctx.user.email || "");
			var at = email.lastIndexOf("@");
			try {
				address = Address.from_for(at > 0 ? email.substr(0, at) : "", domain, allowed);
			} catch (e) {
				throw error(is_coded(e) && e.code === "forbidden" ? "forbidden" : "invalid", "domain");
			}
		}
		return { domain: domain, address: address };
	}

	// ---------------------------------------------------------------
	// Headers

	// A recipient in a delivery copy's To or CC line, or null (no valid
	// form: FidoNet, QWK, or a local address that cannot be written)
	function list_entry(r, sys)
	{
		try {
			if (r.kind == "internet")
				return Address.format(r.given_name, r.addr);
			if (r.kind == "local")
				return Address.format(r.name, sys.user_email(r.usernum));
		} catch (e) {
			return null;
		}
		return null;
	}

	// A recipient in the Sent copy's lists (never sent anywhere)
	function sent_entry(r, sys)
	{
		var e = list_entry(r, sys);
		if (e !== null)
			return e;
		if (r.kind == "local")
			return Address.display_name(r.name);
		var at = r.addr.lastIndexOf("@");
		return '"' + Address.display_name(net_name(r)) + '" (' + r.addr.substr(at + 1) + ")";
	}

	function lists(rcpts, sys)
	{
		var out = { to: [], cc: [], bcc: [], sent_to: [], sent_cc: [], sent_bcc: [] };
		for (var i = 0; i < rcpts.length; i++) {
			var r = rcpts[i];
			var e = list_entry(r, sys);
			if (e !== null && r.field != "bcc")
				out[r.field].push(e);
			out["sent_" + r.field].push(sent_entry(r, sys));
		}
		return out;
	}

	// The name in a copy's "to" field.  For an Internet copy the mail
	// server prints it as  "to" <address>  (unescaped) when the copy has no
	// To list, or alone when it holds '@': so no quote, bracket, '@' or
	// encoded-word marker; the address alone when nothing is left
	function display_to(r)
	{
		if (r.kind == "local")
			return r.name;
		if (r.kind == "fido" || r.kind == "qwk")
			return net_name(r);
		var name = Address.display_name(r.given_name.replace(/@/g, ""));
		return name !== "" ? name : r.addr;
	}

	function write_text(path, s)
	{
		var f = new File(path);
		if (!f.open("wb"))
			throw new Error("cannot create " + path);
		var ok = f.write(s);
		f.close();
		if (!ok)
			throw new Error("cannot write " + path);
	}

	// ---------------------------------------------------------------
	// Completion (phases delivered, charging, done) and recovery

	function attempt(ctx, what, fn)
	{
		try {
			fn();
		} catch (e) {
			if (is_stop(e))
				throw e;
			Send.log(LOG_ERR, "send: user " + ctx.usernum + ": after sending, " + what + " failed: " + text_of(e));
		}
	}

	// Finish a delivered send from its record's phase; returns the outcome
	function complete(ctx, c, rec, recovered)
	{
		var user = ctx.user;
		if (rec.phase === "delivering" || rec.phase === "delivered") {
			// The compare-and-set that decides who charges: only the owner
			// whose "charging" write succeeded charges, once
			phase(ctx, c, rec, "charging");
			hook("charging", null);
			if (rec.cost > 0)
				attempt(ctx, "charging credits", function () {
					user.adjust_credits(-rec.cost);
				});
			if (rec.feedback > 0)
				attempt(ctx, "counting feedback", function () {
					user.sent_email(rec.feedback, true);
				});
			if (rec.emails > 0)
				attempt(ctx, "counting e-mail", function () {
					user.sent_email(rec.emails);
				});
		}
		// Repeatable steps: run in every completion
		if (rec.sent)
			attempt(ctx, "clearing $Unsent", function () {
				ctx.store.mark_sent(rec.sent);
			});
		if (rec.reply)
			attempt(ctx, "marking the source replied", function () {
				ctx.store.set_replied(rec.reply);
			});
		if (rec.prior)	// the Sent copy of an earlier send whose outcome was unknown
			attempt(ctx, "removing the earlier unknown Sent copy", function () {
				remove_unsent(ctx, { msgid: rec.prior });
			});
		if (rec.collect && rec.collect.length && ctx.collect !== null)
			attempt(ctx, "adding contacts", function () {
				if (typeof ctx.collect === "function")
					ctx.collect(rec.collect);
				else
					Contacts.collect(ctx.userdata, rec.collect);
			});
		var result = rec.result;
		if (!result || typeof result !== "object") {
			result = { ok: true, id: rec.sent || null, message_id: rec.msgid };
			if (recovered)
				result.warning = "recovered";
			else if (rec.warning)
				result.warning = rec.warning;
		}
		if (rec.phase !== "done") {
			rec.result = result;
			attempt(ctx, "recording the outcome", function () {
				phase(ctx, c, rec, "done");
			});
		}
		hook("done", null);
		// Indexed before the claim goes, so a repeat of the key always
		// finds the outcome; if the index cannot be written the claim stays
		// for a later completion (no charge is repeated)
		var indexed = false;
		attempt(ctx, "indexing the outcome", function () {
			index_put(ctx, rec.key, c.id, result);
			indexed = true;
		});
		if (indexed)
			attempt(ctx, "removing the draft", function () {
				owned(ctx, function () {
					Drafts.finish(ctx, c.id, c.token);
				});
			});
		return result;
	}

	// The Sent copies of a send that did not deliver: removed while $Unsent
	function remove_unsent(ctx, rec)
	{
		var ids = [];
		if (rec.sent)
			ids.push(rec.sent);
		if (rec.msgid) {
			try {
				ids = ids.concat(ctx.store.find_sent(rec.msgid));
			} catch (e) {
				Send.log(LOG_WARNING, "send: user " + ctx.usernum + ": Sent copy not looked for: " + text_of(e));
			}
		}
		var done = {};
		for (var i = 0; i < ids.length; i++) {
			if (done[ids[i]] === true)
				continue;
			done[ids[i]] = true;
			try {
				ctx.store.remove_sent(ids[i]);
			} catch (e) {
				if (!is_coded(e) || (e.code !== "notfound" && e.code !== "invalid"))
					Send.log(LOG_WARNING, "send: user " + ctx.usernum + ": unsent copy not removed: " + text_of(e));
			}
		}
	}

	// Recover claim c, which this worker owns (taken with take_over()):
	// returns {result} when the send completed, {released: true} when
	// nothing had been delivered (the draft is a draft again); throws
	// conflict "unknown" or "partial"
	function recover_claim(ctx, c)
	{
		var rec = c.send;
		var ph = rec && typeof rec.phase === "string" ? rec.phase : "claimed";
		if (ph === "claimed" || ph === "copy") {
			if (ph === "copy")
				remove_unsent(ctx, rec);
			owned(ctx, function () {
				Drafts.release(ctx, c.id, { token: c.token });
			});
			return { released: true };
		}
		if (!rec.saved || typeof rec.saved !== "object")
			rec.saved = {};
		if (ph === "delivering") {
			var usable = typeof rec.msgid === "string" && typeof rec.after === "number"
				&& rec.groups instanceof Array;
			if (!usable || scan(ctx, rec).length === 0) {
				mark_unknown(ctx, rec);
				owned(ctx, function () {
					Drafts.release(ctx, c.id, { token: c.token, maybe_sent: now_of(ctx),
						maybe_sent_id: typeof rec.msgid === "string" ? rec.msgid : "" });
				});
				Send.log(LOG_WARNING, "send: user " + ctx.usernum + ": an interrupted send may or may not "
					+ "have been delivered; the draft was returned");
				throw error("conflict", "unknown");
			}
			try {
				save_groups(ctx, c, rec, missing(ctx, rec));
			} catch (e) {
				if (is_stop(e))
					throw e;
				Send.log(LOG_ERR, "send: user " + ctx.usernum + ": copies still missing: " + text_of(e));
				throw error("conflict", "partial");
			}
			phase(ctx, c, rec, "delivered");
		}
		var result = complete(ctx, c, rec, true);
		Send.log(LOG_NOTICE, "send: user " + ctx.usernum + ": an interrupted send was completed");
		return { result: result };
	}

	// The Sent copies of a send whose outcome is unknown get $Unknown
	function mark_unknown(ctx, rec)
	{
		var ids = rec.sent ? [rec.sent] : [];
		try {
			if (typeof rec.msgid === "string")
				ids = ids.concat(ctx.store.find_sent(rec.msgid));
		} catch (e) {
			Send.log(LOG_WARNING, "send: user " + ctx.usernum + ": Sent copy not looked for: " + text_of(e));
		}
		var done = {};
		for (var i = 0; i < ids.length; i++) {
			if (done[ids[i]] === true)
				continue;
			done[ids[i]] = true;
			try {
				ctx.store.mark_unknown(ids[i]);
			} catch (e) {
				Send.log(LOG_WARNING, "send: user " + ctx.usernum + ": Sent copy not marked: " + text_of(e));
			}
		}
	}

	// Take claim c over (its heartbeat older than LIVE) by compare-and-set
	// on the record as read; c.token is then ours.  conflict "sending" when
	// another worker has it (or just took it).
	function take_over(ctx, c)
	{
		var token = random_hex();
		try {
			c.send = Drafts.take(ctx, c.id, c.send, token, now_of(ctx), LIVE);
		} catch (e) {
			if (is_coded(e) && (e.code === "conflict" || e.code === "notfound"))
				throw error("conflict", "sending");
			throw e;
		}
		c.token = token;
		return c;
	}

	// A claim as Drafts.claimed() gives it, with its id
	function claimed(ctx, draft_id)
	{
		var c = Drafts.claimed(ctx, draft_id);
		if (c !== null)
			c.id = draft_id;
		return c;
	}

	function is_live(ctx, c)
	{
		var t = c.send && typeof c.send.beat === "number" ? c.send.beat : c.claimed;
		return now_of(ctx) - t < LIVE;
	}

	// ---------------------------------------------------------------
	// A new send

	// Forget recorded copy numbers that are no longer this send's copies
	function prune_saved(rec, found)
	{
		var live = {};
		for (var i = 0; i < found.length; i++)
			live[found[i].num] = true;
		for (var k in rec.saved)
			if (live[rec.saved[k]] !== true)
				delete rec.saved[k];
	}

	function delivery_failed(ctx, c, rec, err)
	{
		beat(ctx, c, rec);	// still ours? (else lost: stop, touch nothing)
		var found = null;
		try {
			found = scan(ctx, rec);
		} catch (e) {
			Send.log(LOG_ERR, "send: user " + ctx.usernum + ": cannot look for copies: " + text_of(e));
		}
		var rolled_back = false;
		if (found !== null) {
			var any_fixed = false;
			for (var i = 0; i < found.length; i++)
				if (!found[i].removable)
					any_fixed = true;
			try {
				rolled_back = !any_fixed && remove_copies(ctx, c, rec, found);
			} catch (e) {
				if (is_stop(e))
					throw e;
				Send.log(LOG_ERR, "send: user " + ctx.usernum + ": cannot remove copies: " + text_of(e));
			}
		}
		if (rolled_back) {	// nothing out: roll back fully
			remove_unsent(ctx, rec);
			owned(ctx, function () {
				Drafts.release(ctx, c.id, { token: c.token });
			});
			throw err;
		}
		// Partly out (or unknown): never sent again as a whole
		Send.log(LOG_ERR, "send: user " + ctx.usernum + ": delivery failed part-way: " + text_of(err));
		if (found === null)
			throw error("conflict", "partial");
		try {
			save_groups(ctx, c, rec, missing(ctx, rec));
		} catch (e) {
			if (is_stop(e))
				throw e;
			Send.log(LOG_ERR, "send: user " + ctx.usernum + ": copies still missing: " + text_of(e));
			throw error("conflict", "partial");
		}
		phase(ctx, c, rec, "delivered");
		return complete(ctx, c, rec, false);
	}

	function fresh(ctx, draft_id, key, confirm)
	{
		var token = random_hex();
		var rec = { v: 1, key: key, sid: random_hex(), phase: "claimed", t: now_of(ctx), owner: token,
			beat: now_of(ctx), saved: {}, recall: {} };
		var cl = Drafts.claim(ctx, draft_id, { send: rec, confirm_resend: confirm });
		var c = { id: draft_id, dir: cl.dir, token: token };
		var draft = cl.draft;
		if (draft.maybe_sent && draft.maybe_sent_id)
			rec.prior = draft.maybe_sent_id;
		var files = cl.files;
		var n;
		var body_bytes = 0;
		try {
			var sys = sys_of(ctx);
			var resolvers = ctx.resolvers || Address.system_resolvers();
			var rcpts = recipients(ctx, draft, resolvers);
			var chk = check(ctx, draft, files, rcpts, sys);
			n = chk.n;
			var from = sender(ctx, draft, sys, chk.n);
			var max_bytes = ctx.config.max_message_bytes;
			if (typeof max_bytes === "number" && max_bytes > 0 && MimeWrite.estimate(draft.body, files) > max_bytes)
				throw error("toolarge");

			// The body file(s), in the claimed directory
			var now = now_of(ctx);
			var mime = MimeWrite.build(draft.body, files, c.dir + BODY);
			beat(ctx, c, rec);	// a slow build must not look stalled once it is done
			body_bytes = file_size(c.dir + BODY);
			if (chk.n.fido > 0 || chk.n.qwk > 0)
				write_text(c.dir + PLAIN, MimeWrite.text(draft.body).replace(/\x00/g, ""));
			var msgid = "<" + rec.sid + "." + hex8(now) + "@" + from.domain + ">";
			var ls = lists(rcpts, sys);
			var warning;
			var to_list = ls.to.join(", ");
			var cc_list = ls.cc.join(", ");
			if (to_list.length > MAX_LIST || cc_list.length > MAX_LIST) {
				warning = "longlist";
				to_list = "";
				cc_list = "";
			}
			if (chk.full)
				warning = "mailboxfull";
			var common = {
				subject: Address.clean(draft.subject, Address.MAX_SUBJECT),
				id: msgid,
				when_written_time: now,
				auxattr: D.MSG_HFIELDS_UTF8
			};
			if (draft.in_reply_to && draft.reply_message_id) {
				var rid = Address.clean(draft.reply_message_id, 998);
				if (rid !== "" && rid.length <= MAX_REPLY_ID)
					common.reply_id = rid;
			}
			if (draft.priority === "high")
				common.priority = 2;
			else if (draft.priority === "low")
				common.priority = 4;
			var header = function (with_mime) {
				var h = {};
				for (var k in common)
					h[k] = common[k];
				if (with_mime) {
					h.mime_version = mime.mime_version;
					h.content_type = mime.content_type;
					h.field_list = mime.fields.slice(0);
				}
				return h;
			};
			// to_list and cc_list are recipient fields: with a recipient
			// list MsgBase.save_msg() reads them from each entry
			var entry = function (o) {
				if (to_list !== "")
					o.to_list = to_list;
				if (cc_list !== "")
					o.cc_list = cc_list;
				o.dest = dest_key(o);
				return o;
			};
			var by = { local: [], internet: [], fido: [], qwk: [] };
			var i;
			for (i = 0; i < rcpts.length; i++)
				by[rcpts[i].kind].push(rcpts[i]);
			var groups = [];
			var settings = ctx.settings || {};
			var h;
			var list;
			if (by.internet.length > 0) {
				list = [];
				for (i = 0; i < by.internet.length; i++)
					list.push(entry({ to: display_to(by.internet[i]), to_net_type: D.NET_INTERNET,
						to_net_addr: by.internet[i].addr, netattr: D.NETMSG_LOCAL | D.NETMSG_KILLSENT }));
				var ii = Address.identity(ctx.user, "internet", settings, sys.alias);
				h = header(true);
				h.from = ii.from;
				h.from_ext = ii.from_ext;
				h.from_net_type = D.NET_INTERNET;
				h.from_net_addr = from.address;
				h.reverse_path = from.address;
				h.netattr = D.NETMSG_LOCAL;
				groups.push({ kind: "internet", hdr: h, rcpts: list, file: BODY });
			}
			if (by.local.length > 0) {
				list = [];
				for (i = 0; i < by.local.length; i++)
					list.push(entry({ to: by.local[i].name, to_ext: String(by.local[i].usernum) }));
				var il = Address.identity(ctx.user, "local", settings, sys.alias);
				h = header(true);
				h.from = il.from;
				h.from_ext = il.from_ext;
				groups.push({ kind: "local", hdr: h, rcpts: list, file: BODY });
			}
			var nets = [["fido", D.NET_FIDO, D.NETMSG_LOCAL | D.NETMSG_KILLSENT], ["qwk", D.NET_QWK, D.NETMSG_LOCAL]];
			for (var ni = 0; ni < nets.length; ni++) {
				var kind = nets[ni][0];
				if (by[kind].length === 0)
					continue;
				list = [];
				for (i = 0; i < by[kind].length; i++) {
					var r = by[kind][i];
					list.push({ to: net_name(r), to_net_type: nets[ni][1],
						to_net_addr: r.addr.substr(r.addr.lastIndexOf("@") + 1), netattr: nets[ni][2] });
					list[i].dest = dest_key(list[i]);
				}
				var inet = Address.identity(ctx.user, kind, settings, sys.alias);
				h = header(false);
				h.from = inet.from;
				h.from_ext = inet.from_ext;
				h.ftn_charset = "UTF-8 4";
				h.netattr = D.NETMSG_LOCAL;
				groups.push({ kind: kind, hdr: h, rcpts: list, file: PLAIN });
			}
			var feedback = 0;
			for (i = 0; i < by.local.length; i++)
				if (by.local[i].usernum == 1)
					feedback = 1;
			var collect = [];
			for (i = 0; i < rcpts.length; i++)
				collect.push({ name: rcpts[i].given_name, addr: rcpts[i].typed });

			// The Sent copy's header lists every recipient
			var sent_hdr = header(true);
			sent_hdr.to = display_to(rcpts[0]);
			if (ls.sent_to.length > 0)
				sent_hdr.to_list = ls.sent_to.join(", ");
			if (ls.sent_cc.length > 0)
				sent_hdr.cc_list = ls.sent_cc.join(", ");
			if (ls.sent_bcc.length > 0)
				sent_hdr.field_list.push({ type: D.RFC822HEADER, data: "Bcc: " + ls.sent_bcc.join(", ") });
			if (by.internet.length > 0) {
				sent_hdr.from = groups[0].hdr.from;
				sent_hdr.from_net_type = D.NET_INTERNET;
				sent_hdr.from_net_addr = from.address;
			} else
				sent_hdr.from = Address.identity(ctx.user, "local", settings, sys.alias).from;

			rec.msgid = msgid;
			rec.groups = groups;
			rec.cost = chk.cost;
			rec.feedback = feedback;
			rec.emails = rcpts.length - feedback;
			rec.reply = draft.in_reply_to || "";
			rec.collect = collect;
			rec.files = files.length;
			if (warning)
				rec.warning = warning;
			rec.sent = null;
			phase(ctx, c, rec, "copy");
			hook("copy", null);
			beat(ctx, c, rec);
			try {
				rec.sent = ctx.store.save_sent(sent_hdr, c.dir + BODY);
				hook("copied", null);
			} catch (e) {
				if (!is_coded(e) || e.code !== "quota")
					throw e;
				rec.warning = rec.warning || "nocopy";
			}
			if (rec.sent !== null)
				beat(ctx, c, rec);
			var mb = open_shared(ctx);
			try {
				rec.after = mb.last_msg;
			} finally {
				mb.close();
			}
			phase(ctx, c, rec, "delivering");
		} catch (e) {
			if (is_stop(e))
				throw e;
			// Nothing delivered: undo the Sent copy, give the draft back
			if (rec.phase !== "claimed")
				remove_unsent(ctx, rec);
			try {
				owned(ctx, function () {
					Drafts.release(ctx, c.id, { token: c.token });
				});
			} catch (x) {
				if (is_stop(x))
					throw x;
				Send.log(LOG_ERR, "send: user " + ctx.usernum + ": draft not released: " + text_of(x));
			}
			throw e;
		}
		var result;
		try {
			save_groups(ctx, c, rec, null);
		} catch (e) {
			if (is_stop(e))
				throw e;
			return delivery_failed(ctx, c, rec, e);
		}
		hook("delivered", null);
		phase(ctx, c, rec, "delivered");
		result = complete(ctx, c, rec, false);
		Send.log(LOG_INFO, "send: user " + ctx.usernum + " sent mail: " + n.local + " local, "
			+ n.internet + " Internet, " + n.fido + " FidoNet, " + n.qwk + " QWK, " + body_bytes + " bytes"
			+ (files.length ? ", " + files.length + " attachments" : "")
			+ (result.warning ? ", " + result.warning : ""));
		return result;
	}

	function send(ctx, draft_id, key, opts)
	{
		if (typeof key !== "string" || !KEY_RE.test(key))
			throw error("invalid", "key");
		if (typeof draft_id !== "string" || !/^[0-9a-f]{16}$/.test(draft_id))
			throw error("notfound");
		var ix = index_get(ctx, key);
		if (ix !== null) {
			if (ix.draft !== draft_id)
				throw error("conflict", "key");
			return ix.result;
		}
		var c = claimed(ctx, draft_id);
		if (c !== null) {
			if (is_live(ctx, c))
				throw error("conflict", "sending");
			var out = recover_claim(ctx, take_over(ctx, c));
			if (out.result)
				return out.result;
		} else {
			// A send completed by another worker (a recovery that took the
			// claim over): the draft's outcome
			var done = index_by_draft(ctx, draft_id);
			if (done !== null)
				return done.result;
		}
		return fresh(ctx, draft_id, key, opts && opts.confirm_resend === true);
	}

	// Recovery of a stale claim, for Drafts.sweep's recover callback: the
	// outcome when the send was completed, null when the draft was given
	// back (or is not claimed); throws conflict "sending" for a live
	// claim, "unknown" or "partial" as recovery does
	function recover(ctx, draft_id)
	{
		var c = claimed(ctx, draft_id);
		if (c === null) {
			var done = index_by_draft(ctx, draft_id);
			return done !== null ? done.result : null;
		}
		if (is_live(ctx, c))
			throw error("conflict", "sending");
		var out = recover_claim(ctx, take_over(ctx, c));
		return out.result || null;
	}

	var Send = {
		KEY_RE: KEY_RE,
		LIVE: LIVE,
		dest_key: dest_key,	// for tests
		hook: null,
		log: function (level, text) {
			log(level, text);
		},
		send: function (ctx, draft_id, key, opts) {
			try {
				return send(ctx, draft_id, key, opts);
			} catch (e) {
				throw outgoing(e);
			}
		},
		// sent(ctx, draft_id): true when the completed-send index records
		// a send of this draft (for 24 hours, KEEP): a read of one small
		// file, no lock
		sent: function (ctx, draft_id) {
			return index_by_draft(ctx, draft_id) !== null;
		},
		recover: function (ctx, draft_id) {
			try {
				return recover(ctx, draft_id);
			} catch (e) {
				throw outgoing(e);
			}
		}
	};

	return Send;
})();

Send;
