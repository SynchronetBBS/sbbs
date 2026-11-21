// Api: the web client's batched JSON calls, the trust boundary between a
// browser and a user's mail.
//
// var Api = load("api.js");
// var request = Api.parse_body(http_request.post_data);	// or null
// var reply = Api.dispatch(env, request);	// never throws
//
// Request text: Api.parse_body(bytes) is the one way a body becomes a
// request.  The body must be valid UTF-8 (no overlong forms, no encoded
// surrogates, nothing above U+10FFFF); it is decoded, parsed as JSON, and
// every string in the result, keys included, is encoded back to UTF-8
// bytes (surrogate pairs combined), so raw UTF-8 and \u escapes give the
// same byte strings.  A lone surrogate, nesting deeper than 32, invalid
// UTF-8 or invalid JSON give null, which dispatch answers with the
// top-level "invalid".
//
// request: { calls: [[method, args, call_id], ...], background, as }
//   at most 32 calls; each call an array of 1 to 3 elements.  args is an
//   object of named arguments (absent or null: none).  call_id is echoed
//   in the result when it is a number or a string of at most 40
//   characters, each 0x20 to 0x7E; else null.  background: true marks a
//   request the client made on its own, not for the user (see Session
//   below); it can only withhold refreshing the session, any other value
//   is ignored.  as: the user number the page making the request was
//   started for (a positive integer; absent: the page does not know one,
//   e.g. at sign-in).  A request holding a write method must carry it.
//
// reply:
//   { results: [[call_id, result], ...], state, signed_out, usernum }
//     one result per call, in order: {ok: true, ...method's fields} or
//     {ok: false, error: <code>, message: <text>}.  state is the mailbox's
//     change fingerprint (Store.state()) when a signed-in user's mailbox
//     was opened for this request, else "".  signed_out is true when the
//     request carried a session cookie but no value of it is a valid
//     session (expired, signed out elsewhere, password changed): the
//     calls needing a user still get "auth", and the client can say "you
//     were signed out" rather than show the wrong-password text.  It is
//     false when the request ends signed in (a session.login or webv4
//     sign-in in the same request).  usernum is the user the request ended
//     signed in as (0: none), in every reply, so a page started by another
//     user can tell.
//   { error: "invalid", message, results: [], state: "", usernum: 0 }
//     the top-level error, when request is not an object with a "calls"
//     array of at most 32 arrays of 1 to 3 elements, or "as" is present
//     and not a positive integer; no call runs.  "toolarge" likewise when
//     dispatch(env, request, {length}) is told the body was longer than
//     256 KB and the request is not exactly one draft.attach or draft.save
//     call within that method's allowance (body_allowance()).  Also
//     { error: "internal", ... } if the dispatcher itself fails.
//
// Error codes: notfound, forbidden, invalid, quota, toolarge, conflict,
// failed, expired, auth, https, stale (the mailbox library's codes and
// this file's) and internal.  message is the text for the code from the
// request's locale strings ("errors.<code>"; in the "https" text {name} is
// the request's product name), or a built-in English text
// if the locale files cannot be read.  A thrown object whose code is one of
// the known codes passes through with that code; anything else is
// "internal", logged with its file name and line.  No exception text,
// server path or stack ever reaches the reply.
//
// Gates, in this order; each one gives its error to EVERY call of the
// request and stops anything later (no method runs, no file is read):
//   1. a malformed request             top-level "invalid"
//   2. env.has_header is not true      "forbidden" (the request lacks the
//                                      X-Webmail: 1 header a cross-site
//                                      form cannot send)
//   3. env.secure is not true while config.require_tls
//                                      "https"
//   then the session cookie is checked (see Session below); if that throws,
//   every call is "internal".  Then, for the request as a whole:
//   3a. "as" present, a user signed in, and as is not that user
//                                      "stale" (the page was started for
//                                      another user: someone else signed
//                                      in in this browser since; the
//                                      client reloads)
//   3b. a user signed in, a write method in the request and no "as"
//                                      "invalid" (signed out, the calls
//                                      that need a user get "auth")
//   Then for each call:
//   4. unknown method                  "invalid"
//   5. the method needs a signed-in user and there is none
//                                      "auth"
//   6. a write method (one that changes mail) in a request with
//      background: true                "forbidden"
//   7. an argument check fails         "invalid"
//   8. the method runs.
//
// env, supplied by the caller (api.ssjs, or a test):
//   config         Config.load()
//   has_header     boolean: the X-Webmail: 1 request header was present
//   secure         boolean: Session.is_secure(...)
//   cookie         the "webmail" cookie values (an array; a string is one
//                  value), or null/undefined
//   accept_language  the Accept-Language request header, or ""
//   client         { ip, agent } recorded with a new session
//   webv4_user()   returns the user signed in to webv4
//                  (Webv4.session_user()) or 0; called at most once per
//                  request, only when config.webv4_login is true and
//                  only by session.get without a session and by
//                  session.logout, so a poll never reads the .web file
//   now            optional time_t (tests); time() otherwise
//   find_user(name)  the user number the server's login() would look the
//                  name up as, or 0 (system.find_login_id(name), which
//                  calls the same lookup login() does)
//   login(user, password)
//                  the password check: the user number, or 0.  The shim
//                  wraps the web server's login() so failed attempts are
//                  counted by the server.
//   get_user(usernum)  the User object, or null
//   userdata(usernum)  the UserData of that user's web client directory
//   make_ctx(usernum)
//                  { usernum, alias, name, user, store (an opened Store),
//                    userdata, config, close() }.  Called at most once per
//                  dispatch, on the first call that needs the mailbox;
//                  close() is called once, at the end, in a finally.
//                  Api.context_factory() builds one.
//   set_cookie(value, keep)
//                  called to set the session cookie (value "" clears it);
//                  the last call of a dispatch is the one that counts
//   data_dir       optional (tests): the data directory MailSig uses
//                  (system.data_dir otherwise)
//   sys            optional (tests): fields over the system values the
//                  settings use (see sys_of())
//   lib_ctx        optional (tests): fields added to the context the
//                  drafts, send and contacts libraries get (domains,
//                  resolvers, sys, now, user; see lib_ctx())
//
// Session: before the calls run, each cookie value (at most 8) is
// checked in turn with Session.parse -> env.userdata ->
// Session.check(ud, value, now, config, {touch, stamp}) -> the user record
// (Api.eligible); the first that passes is used.  stamp is a function
// Session calls only after the key matched a stored session; it loads the
// user record (env.get_user) and returns the digest of the user's
// password and password date (stamp_of), stored with every session made
// here, so changing the password in any way ends every web client
// session of that user, and a forged value never reads a user record.
// touch is false when every call of the request is "changes" (the
// background poll) or the request carries background: true (a refresh the
// client made on its own), so polling and what follows from it do not
// keep a session alive.  When values were sent but none passes, the
// cookie is cleared (set_cookie("")) and signed_out is set; a session
// whose user is no longer eligible is also destroyed.
// Api.session_user(env, touch) does the same for part.ssjs.
//
// Who may sign in (Api.eligible), for every sign-in path (password, webv4,
// and every later request): the user record exists, its number is at most
// 65535 (the mail index's limit), it is not deleted or inactive, is not a
// guest (User.is_guest, or the G restriction, which is how Synchronet
// marks a guest account: user_is_guest() in userdat.c), and meets
// config.access_requirements (User.compare_ars) when it is not empty.  A
// password sign-in also needs a stored password (the web server's login()
// accepts any password for an account without one).
// Before login() is called, the name is resolved as login() would
// resolve it (env.find_user); an account found that way and refused here
// never reaches login(), whose success has side effects (a logon recorded,
// the user record updated).  A name that does not resolve goes to login()
// and the account it signs in is checked afterwards.  Each refusal of a
// known account logs one notice "webmail: user #N refused: <reason>"; the
// reply is the same "auth" text in every case.
//
// Methods (Api.methods, a plain object that more methods are added to):
// each entry is { auth, ctx, write, args, check, run }.  auth: needs a
// signed-in user.  ctx (default: auth): needs the mailbox, so make_ctx is
// called first.  write: changes mail (refused in a background request).
// args: { <name>: { type: "string"|"int"|"boolean"|"list", required, max,
// min, test, of, unique } }; arguments not listed are invalid,
// "__proto__" included; strings are byte strings (no character above
// 0xFF) of at most max characters (default 1000); ints are finite whole
// numbers from min (default 0) to max; test(value) may refuse a string;
// a list is an array of min (default 1) to max (default 100) elements,
// each checked by the spec "of" (not a list), no element twice when
// unique.  check(args), when present, may refuse the checked arguments
// as a whole ("invalid", before the mailbox is opened).
// run(ctx, args, rq): ctx is the make_ctx() context or null;
// rq is this request's state ({ env, config, usernum, user, ... }); it
// returns an object whose fields go into the result beside ok: true.
//
//   session.get {no_webv4}
//                    -> { user ({number, alias, name, avatar} or null; alias
//                       and name as UTF-8, converted from CP437 when they
//                       are neither ASCII nor valid UTF-8; avatar see
//                       Avatars below), insecure,
//                       locale, strings (the locale strings, in the first
//                       session.get result of a request only), domains
//                       (Api.sending_domains of the signed-in user; [] when
//                       signed out), limits (client_limits(): max_attachment_bytes,
//                       max_text_part_bytes, max_drafts, max_draft_bytes,
//                       max_body_bytes, max_attachments, max_chunk,
//                       max_recipients, max_message_bytes, max_attach_total:
//                       the attachment bytes a message may carry, see
//                       attach_total()), poll_ms, powered_by (the system's
//                       version notice, "Synchronet BBS for Linux Version
//                       3.22": Api.powered_by(system.version_notice), white
//                       space runs made one space), product_name, brand,
//                       login_text, footer_links, fallback_url
//                       [, via: "webv4"] }
//                       With no web client session, config.webv4_login
//                       and env.webv4_user() an eligible user, it first
//                       signs that user in (a new session, keep false) and
//                       reports via "webv4".  It does not after a
//                       session.logout earlier in the same request, nor
//                       when no_webv4 is true (the browser signed out and
//                       asks not to be signed in again this way).
//                       brand, login_text and footer_links are sysop-
//                       supplied MARKUP (HTML); product_name and
//                       fallback_url are plain text; the client must treat
//                       them so.  No password, session key or setting with
//                       security weight (access_requirements, directory,
//                       tls_proxy_header, ...) is included.
//   session.login {user, password, keep}
//                    -> { user }.  user at most 64 characters and password
//                       at most 128, neither empty, else "auth" without
//                       asking login(); every refusal is "auth" with the
//                       same text.  A valid session the browser already
//                       had is destroyed.  Later calls of the request run
//                       as the new user.
//   session.logout   -> { webv4 }: true when the user is still signed in
//                       to webv4 (webv4_login on and env.webv4_user() is
//                       this user), so the next visit signs in again
//   folder.list      -> { folders, usage, purged, trash_days, inbox_days,
//                       drafts }:  drafts is the number of drafts
//                       (Drafts.count; 0 when it cannot be counted), beside
//                       the folders so the folder list itself is unchanged;
//                       when a draft is claimed by a send and the request
//                       is not a background one, first the recovery of
//                       draft.list (sweep_sends(): an interrupted send is
//                       completed, given back or returned maybe_sent; a
//                       failure is logged, the list still returned);
//                       purge_expired() (at most once a day; purged is the
//                       number of messages it removed, 0 when nothing ran,
//                       nothing was due or it failed), then folders() and
//                       usage(), one Store; trash_days is the number of
//                       days a message stays in Trash (the Store's option,
//                       system.max_trash_age by default; 0: until removed
//                       by hand).  inbox_days: the mail base's age limit
//                       in days (Store.inbox_max_age(): the mail base's
//                       max_age, which SCFG writes into the base when the
//                       setting is saved; 0: none, or it cannot be read).  The spam age
//                       limit is not reported: scripts cannot read it.
//   message.list {folder, offset, limit, search}
//                    -> { total, items } (Store.list, items as it gives
//                       them; in Sent a failed delivery carries notice:
//                       the failure notice's id while it exists)
//   message.get {id, mark_read, head, avatar}
//                    -> { message } (Store.get, plus, with avatar true,
//                       avatar: the sender's avatar, see Avatars below;
//                       without it no avatar field: computing one costs
//                       hundreds of file operations, so the client keeps
//                       each sender's for the page's life and asks only on
//                       a miss); with head true { item [, avatar] }: only
//                       the list item (Store.get with head), for marking a
//                       message read that the client already holds: no
//                       bodies or parts
//   changes {state}  -> { changed, state }: state is Store.state() now
//   message.original {id}
//                    -> { original: {rfc822, stored, external_files} }
//                       (Store.original); a read
//
// Write methods (write: true), each a thin call of the Store, refused in a
// request with background: true ("forbidden", gate 6).  ids is 1 to 200
// message ids (the pattern below), 1 to 50 for message.move, delete and
// undelete (each of those can move mail between the bases, which is
// costly), no id twice; anything else is "invalid" for the whole call
// before the mailbox is opened.  Each
// returns its per-id results in Store order and the state after it (also
// the reply's state):
//   results: [{id, ok: true, new_id [, folder: "archive"] [, expired:
//            true]} or {id, error: <code>}], new_id the id after the call
//            (null once removed); folder and expired as Store.restore()
//            gives them; a code other than the ones above is "failed"
//   message.set {ids, read, starred, spam}
//                    -> { results, state } (Store.set; at least one of the
//                       three; spam only for Inbox and Spam messages, so
//                       spam false is "Not spam": back to the Inbox)
//   message.move {ids, folder}       -> { results, state } (Store.move;
//                                       folder: inbox, archive or a "u:"
//                                       folder id, else "invalid")
//   message.delete {ids}             -> { results, state } (Store.remove)
//   message.undelete {ids}           -> { results, state } (Store.restore)
//   trash.empty                      -> { results, state }
//   folder.create {name}             -> { folder: {id, name}, state }
//   folder.rename {id, name}         -> { folder: {id, name}, results
//                                       (messages not retagged), state }
//   folder.delete {id}               -> { results (its messages, to Trash),
//                                       state }
//   (folder.rename / folder.delete take a "u:" folder id; a name argument
//   is at most 4096 bytes, so an overlong name reaches the Store and comes
//   back with reason "toolong".)  A failure of
//   the whole call keeps the error shape above; a refused folder name is
//   "invalid" with reason one of empty, invalid, toolong, reserved,
//   duplicate (the Store's; nothing else passes through).
//
// Drafts, sending, contacts and settings (webmail/lib/drafts.js, send.js,
// contacts.js, exec/load/mailsig.js; each loaded on first use).  None
// opens the mailbox except where stated; the writes (all but draft.get,
// draft.list, contact.list, contact.suggest and settings.get) need "as"
// and are refused in a background request like the others.  A library
// error keeps its code; its reason passes when it is one of PASS_REASONS
// (then the message is errors.<code>_<reason> when that text exists),
// with rev, received, index, entry and count (see coded_failure()).  A
// request body refused before it is parsed gets the top-level shape
// (large_gate()), and a cookie that is no session is cleared there too.
//   draft.create {mode, source_id}
//                    -> { draft, usage [, dropped_recipients] } (Drafts.create;
//                       mode new (no source_id), reply, replyall or forward
//                       (source_id: a message id; opens the mailbox);
//                       dropped_recipients, from a reply, the number of the
//                       source's recipients left out; usage: Drafts.usage,
//                       {drafts, bytes} the user's drafts take now)
//   draft.get {id}   -> { draft, usage }; draft.get and draft.save of a
//                       draft that was sent: notfound, reason "sent"; of
//                       one a send has claimed: notfound, reason "sending"
//                       (or "stalled" when the claim's heartbeat is
//                       Send.LIVE seconds old or more), with age (seconds)
//   draft.list       -> { drafts (Drafts.list rows, maybe_sent where set),
//                       swept, recovered (claims resolved: sweep's returned
//                       claims, and sends completed, given back or returned
//                       maybe_sent) }.  When the mailbox cannot be opened
//                       the recovery is skipped (logged) and the list is
//                       still returned.  First Drafts.sweep, then
//                       Send.recover for the stale sends it reports (claims
//                       whose heartbeat is Send.LIVE old; at most
//                       MAX_RECOVER per request, none started RECOVER_SECONDS
//                       after the first; opens the mailbox only then); in a
//                       background request neither, only the list.  Safe
//                       to repeat: what it removes or completes is done once.
//                       draft.list and folder.list are reads (no `as`, retried
//                       by rpc.js) that may nonetheless complete an
//                       interrupted send: save copies and charge credits.
//                       That is the user's own send finishing, and recovery
//                       is idempotent (the claim's record decides; a second
//                       run finds the send done), so a retry or a stale tab
//                       cannot deliver or charge twice.
//   draft.save {id, rev, to, cc, bcc, subject, body, from_domain, from_local,
//               priority}
//                    -> { draft } (Drafts.save; conflict "stale" carries rev)
//   draft.delete {id}  -> {}
//   draft.attach {id, upload, name, type, offset, total, b64}
//                    -> { attachment: {id, received, complete} }
//                       (conflict "offset" carries received)
//   draft.detach {id, attachment}  -> { draft }
//   draft.send {id, key, confirm_resend}
//                    -> { id (the Sent copy's, or null), message_id
//                       [, warning: nocopy, longlist or recovered] }
//                       (Send.send; opens the mailbox; key: 16 to 64 of
//                       A-Z a-z 0-9 _ -, new for each press of Send; the
//                       same key returns the recorded outcome; conflict
//                       reasons sending, unknown, partial, key, taken, busy,
//                       incomplete, stale, maybe_sent; forbidden recipient
//                       and invalid unknown carry index and entry)
//   contact.list     -> { contacts }
//   contact.set {id, name, addrs, note}  -> { contact } (addrs: 1 to 10)
//   contact.delete {id}  -> {}
//   contact.suggest {q, limit}  -> { suggestions } (the user's own
//                       contacts only, limit 1 to 50, default 10)
//   settings.get     -> { display_name, default_name (the name Internet
//                       mail carries when display_name is blank),
//                       from_domain, theme, images_always,
//                       from_domains (Address.domains_for: the domains the
//                       user may send from), from_address_local (the part
//                       of the address the system assigns the user before
//                       "@"; from_domain "" is the system's address),
//                       from_local (the chosen name before "@"; "" is
//                       from_address_local), from_addresses (the addresses
//                       offered as From, the default first:
//                       Address.addresses_for; from_local with from_domain
//                       is one of them or another address the mail server
//                       delivers to the user, Address.own_address),
//                       signature, signature_source, signature_enabled
//                       (MailSig.get, MailSig.enabled), forward: {allowed,
//                       enabled, address, keeps_copy [, reason: off or
//                       restricted]} }.  Stored in webmail.settings.json:
//                       display_name (Address.clean, 64 bytes),
//                       from_domain ("" or an allowed domain, lowercased),
//                       theme (system, light or dark), images_always (at
//                       most 200 Internet addresses, ASCII lowercased).
//                       Forwarding is the user record's: user.netmail and
//                       the USER_NETMAIL bit; allowed unless the system
//                       does not forward (SYS_FWDTONET) or the user has the
//                       forward-mail restriction ('F'); keeps_copy is the
//                       MM_FWDKEEP bit of msg_area.settings when sbbsdefs.js
//                       defines that name, else false.  The netmail address
//                       has other uses: the mail server exempts mail FROM
//                       any user's netmail address from DNSBL refusal,
//                       login.js can mail the password there, emailval.js
//                       sends its validation code there.
//   settings.get also returns blocked_tags: [{ text, pattern }], the
//                       user's blocked address tags (lib/blocktags.js);
//                       settings.set tag_block adds one (invalid "tag",
//                       toolarge "tags"), tag_unblock removes the entry
//                       that reads so
//   settings.set {display_name, from_domain, from_local, theme, images_always,
//                images_add, images_remove, forward_enabled,
//                forward_address, signature}
//                    -> what settings.get returns.  At least one;
//                       images_always (the whole list) not with images_add
//                       or images_remove (one address each, applied under
//                       the settings file's lock, so two tabs do not undo
//                       each other).  signature: text, "" (none on mail) or
//                       null (back to the general signature): MailSig.set.
//                       forward_address, and the stored address when
//                       forward_enabled true turns forwarding on: at most
//                       60 characters, a plain Internet address, not at
//                       one of the system's domains
//                       (system.inet_domain_list), one the system can send
//                       to (system.check_netmail_addr), not in email.can,
//                       not another user's when duplicates are not allowed
//                       (UQ_DUPNETMAIL); "" is refused (forward_enabled
//                       false turns forwarding off and keeps the address).
//                       Everything is checked before anything is written
//                       (the order is at settings_set()).
//
// Request body size: see body_check(); 256 KB for every request,
// more only for a request of exactly one draft.attach or draft.save call.
//
// Avatars: the base64 data of a Synchronet avatar (10 x 6 character
// cells, two bytes each: a CP437 character and a PC text attribute),
// from exec/load/avatar_lib.js, or null.  The user's own: read(number,
// alias).  A sender's: as the terminal's message header picks it (see
// sender_avatar()).  The library's own rules apply: its "avatars"
// options (enabled, auto_identicon: a generated identicon when there is
// none), a user's disabled flag, and is_valid().  env.avatar_lib, when
// present, is the library to use (null: none); otherwise it is loaded
// once per request.  Any failure gives null, never a failed call.
//
// Argument patterns: message ids /^[sp][1-9][0-9]{0,9}$/ (as Store checks
// them); folder ids one of inbox, starred, sent, spam, trash, archive,
// all, or "u:" and a tag of 1 to 200 bytes with no white space (space, tab,
// CR, LF, FF, VT) that does not start with "$" (any tag Store.folders()
// can list, foreign ones included).
//
// Sending domains: Api.sending_domains(usernum) is the ONE place the list
// of domains a user may send from comes from (Address.domains_for);
// session.get's domains (none when signed out) and settings.get's
// from_domains are that list.

"use strict";

var Api = (function () {
	var HERE = (function () {
		var f = String(new Error().fileName || "");
		var i = Math.max(f.lastIndexOf('/'), f.lastIndexOf('\\'));
		return i >= 0 ? f.substr(0, i + 1) : "";
	})();

	function sibling(name)
	{
		var path = HERE + name;
		if (HERE === "" || !file_exists(path))
			path = name;
		return load({}, path);
	}

	var Session = sibling("session.js");
	// locale.js on first use: a request that needs no text (the poll, a
	// call that succeeds) does not load it
	var locale_lib = null;
	function Locale()
	{
		if (locale_lib === null)
			locale_lib = sibling("locale.js");
		return locale_lib;
	}
	// The user record bits used here, as userdefs.js defines them (pinned
	// to it by the api.js test): not loaded for every request
	var UD = { USER_DELETED: 1 << 0, USER_INACTIVE: 1 << 6, USER_NETMAIL: 1 << 12,
		UFLAG_G: 1 << 6, UREST_FORWARD_MAIL: 1 << 5 };
	var MAX_COOKIES = 8;		// cookie values tried per request
	var MAX_DEPTH = 32;		// nesting of a request body

	var MAX_CALLS = 32;
	var MAX_CALL_ID = 40;
	var MAX_STRING = 1000;
	// A user name or password argument: longer is "invalid"; from 65 (user)
	// or 129 (password) up to this it is "auth" without asking login()
	var MAX_CREDENTIAL = 8192;
	var MAX_USER = 64;
	var MAX_PASSWORD = 128;
	var MAX_TAG = 200;
	var MAX_IDS = 200;		// message ids per write call
	// ... per move, delete and undelete: a move between the bases costs
	// some 230 file operations under the user's summary lock
	var MAX_MOVE_IDS = 50;
	var MAX_NAME = 4096;		// a folder name argument: longer ones reach
					// the mailbox (and come back "toolong")
	var MOVE_TARGETS = ["inbox", "archive"];
	// The reasons a folder name is refused (the mailbox library's)
	var REASONS = ["empty", "invalid", "toolong", "reserved", "duplicate"];
	var MSG_ID_RE = /^[sp][1-9][0-9]{0,9}$/;
	var METHOD_RE = /^[a-z][a-z0-9_]{0,31}(\.[a-z][a-z0-9_]{0,31}){0,3}$/;
	var WS_RE = /[ \t\r\n\f\v]/;
	// Arrays, not objects: this engine's hasOwnProperty() is true for
	// "__proto__" on every object
	var FOLDERS = ["inbox", "starred", "sent", "spam", "trash", "archive", "all"];
	var CODES = ["notfound", "forbidden", "invalid", "quota", "toolarge", "conflict", "failed",
		"expired", "auth", "https", "stale"];
	// The reasons a call's error may carry (the libraries'; anything else is
	// dropped): the folder-name ones above, then drafts, sending, contacts
	// and settings.  A reason with a text of its own (errors.<code>_<reason>
	// in the locale, REASON_FALLBACK below) gets it as the message.
	var PASS_REASONS = REASONS.concat([
		// Drafts
		"stale", "offset", "busy", "incomplete", "sending", "stalled", "exists", "unreadable", "drafts", "sent",
		"bytes", "total", "count", "beyond", "maybe_sent", "chunk", "upload", "mode", "to", "cc",
		"bcc", "subject", "body", "from_domain", "from_local", "priority", "rev",
		"attachment", "fields", "send", "token", "source_id",
		// Address (parse_list, resolve, from_for)
		"syntax", "control", "utf8", "unbalanced", "group", "route", "kind",
		// Send
		"key", "unknown", "partial", "taken", "norecipients", "toomany", "qwk", "fido", "fidoname",
		"recipient", "attachments", "credits", "domain",
		// Contacts
		"name", "addrs", "note", "id", "contacts",
		// Settings
		"display_name", "theme", "images", "signature", "tag", "tags", "forward_off", "forward_restricted",
		"forward_address", "forward_toolong", "forward_own", "forward_empty", "forward_blocked",
		"forward_duplicate", "forward_unsupported"]);
	var REASON_FALLBACK = {
		conflict_stale: "This draft was changed in another window.",
		conflict_offset: "Part of the file was not received. The upload continues from where it stopped.",
		conflict_busy: "The server is busy with your drafts. Please try again in a moment.",
		conflict_sending: "This message is being sent. Please wait a moment.",
		conflict_unknown: "It is not known whether this message was sent. Check your Sent folder before sending it again.",
		conflict_partial: "This message was sent to some of its recipients only. The rest are tried again when you next open a folder, ten minutes from now or later.",
		conflict_key: "This send request was already used for another message. Please try again.",
		conflict_taken: "This message is being sent. Please wait a moment.",
		conflict_incomplete: "An attachment has not finished uploading.",
		conflict_maybe_sent: "This message may have been sent already. Check your Sent folder, then confirm to send it again.",
		notfound_unreadable: "This draft cannot be read.",
		notfound_sent: "This message has already been sent.",
		notfound_sending: "This message is being sent.",
		notfound_stalled: "Sending this message did not finish. Look in your Sent folder, then open Drafts.",
		quota_drafts: "You have as many drafts as allowed. Send or delete some first.",
		quota_bytes: "Your drafts take all the space allowed. Delete some drafts or attachments first.",
		toolarge_total: "The attachments of this message are larger than allowed.",
		toolarge_count: "This message has as many attachments as allowed.",
		toolarge_body: "The message text is longer than allowed.",
		toolarge_contacts: "Your address book is full.",
		toolarge_images: "The list of senders whose images are always shown is full.",
		toolarge_tags: "The list of blocked tags is full.",
		invalid_norecipients: "Add at least one recipient.",
		invalid_syntax: "One of the addresses is not written correctly.",
		invalid_control: "One of the addresses contains a character that is not allowed.",
		invalid_utf8: "One of the addresses contains text that cannot be read.",
		invalid_unbalanced: "One of the addresses has a quote, bracket or parenthesis that is not closed.",
		invalid_group: "Address groups cannot be used. Enter the addresses one by one.",
		invalid_route: "Routed addresses cannot be used.",
		invalid_toolong: "That is too long.",
		invalid_unknown: "One of the recipients is not a known user or address.",
		invalid_domain: "You cannot send from that address.",
		invalid_key: "The send request was not valid. Please try again.",
		invalid_toomany: "A message can have at most {count} recipients.",
		invalid_toomany_one: "A message can have only one recipient.",
		invalid_fidoname: "A FidoNet recipient needs a name, as in Name@1:2/3.",
		invalid_attachments: "Attachments cannot be sent to FidoNet or QWK addresses.",
		invalid_addrs: "One of the addresses is not valid.",
		invalid_from_domain: "You cannot send from that address.",
		invalid_from_local: "You cannot send from that address.",
		invalid_images: "That is not a valid e-mail address.",
		invalid_tag: "A tag can have letters, digits and . _ + = - only, 64 at most.",
		invalid_signature: "The signature is too long or not valid.",
		invalid_forward_address: "Mail can only be forwarded to an Internet e-mail address.",
		invalid_forward_toolong: "The forwarding address can be at most 60 characters long.",
		invalid_forward_own: "Mail cannot be forwarded to an address on this system.",
		invalid_forward_empty: "Enter the address to forward your mail to.",
		invalid_forward_blocked: "That forwarding address is not allowed.",
		invalid_forward_duplicate: "That forwarding address is in use by another user.",
		invalid_forward_unsupported: "This system cannot forward mail to that address.",
		forbidden_qwk: "Mail cannot be sent to that QWK address.",
		forbidden_fido: "This system does not send FidoNet mail.",
		forbidden_recipient: "You are not allowed to send mail to one of the recipients.",
		forbidden_credits: "You do not have enough credits to send this message.",
		forbidden_domain: "You cannot send from that address.",
		forbidden_forward_off: "This system does not forward mail.",
		forbidden_forward_restricted: "You are not allowed to forward your mail."
	};
	// Only when the locale files cannot be read (the same as en_us.ini)
	var FALLBACK = {
		notfound: "That item could not be found. It may have been deleted.",
		forbidden: "You are not allowed to do that.",
		invalid: "Some of the information is not valid. Please check it and try again.",
		quota: "Your mailbox is full. Delete some messages to make room.",
		toolarge: "That is too large to send or save.",
		conflict: "Someone or something else changed this first. Please reload and try again.",
		failed: "That did not work. Please try again.",
		expired: "That message is too old to go back to the Inbox.",
		auth: "The user name or password is not correct.",
		https: "{name} needs a secure connection. Please use the https address of this site.",
		stale: "Someone else signed in in this browser. The page is loading again.",
		internal: "Something went wrong on the server. Please try again in a moment."
	};
	var BRAND_KEYS = ["product_name", "brand", "login_text", "footer_links", "fallback_url"];

	function own(obj, key)
	{
		return Object.prototype.hasOwnProperty.call(obj, key);
	}

	function ascii_lower(s)
	{
		return s.replace(/[A-Z]+/g, function (m) {
			return m.toLowerCase();
		});
	}

	function now_of(env)
	{
		return typeof env.now === "number" && isFinite(env.now) ? env.now : time();
	}

	function describe(e)
	{
		var text;
		try {
			text = String(e);
			if (e !== null && typeof e === "object") {
				if (e.code !== undefined && !(e instanceof Error))
					text = "code " + e.code + ": " + e.message;
				if (e.fileName)
					text += " (" + e.fileName + ":" + e.lineNumber + ")";
			}
		} catch (x) {
			text = "(unprintable exception)";
		}
		return text;
	}

	function is_coded(e)
	{
		return e !== null && typeof e === "object" && !(e instanceof Error)
			&& typeof e.code === "string" && CODES.indexOf(e.code) >= 0;
	}

	// ---- Who may use the web client ----

	// Why u may not use the web client, or null if it may.  need_password:
	// also refuse an account without a stored password (password sign-in)
	function refusal(u, config, need_password)
	{
		try {
			if (u === null || typeof u !== "object" || typeof u.number !== "number" || !(u.number >= 1))
				return "no such user";
			// The mail index holds user numbers in 16 bits (the mailbox
			// library refuses a higher one)
			if (u.number > 0xFFFF)
				return "user number above 65535";
			if (u.settings & UD.USER_DELETED)
				return "deleted";
			if (u.settings & UD.USER_INACTIVE)
				return "inactive";
			if (u.is_guest === true || !u.security || (u.security.restrictions & UD.UFLAG_G))
				return "guest";
			var ars = config && typeof config.access_requirements === "string"
				? config.access_requirements : "";
			if (ars !== "" && u.compare_ars(ars) !== true)
				return "access requirements";
			if (need_password && (typeof u.security.password !== "string" || u.security.password === ""))
				return "no password";
			return null;
		} catch (e) {
			Api.log(LOG_ERR, "webmail api: eligibility check failed: " + describe(e));
			return "check failed";
		}
	}

	function eligible(u, config)
	{
		return refusal(u, config, false) === null;
	}

	// The session stamp of a user: the hex SHA-256 digest of the password
	// and the password date ("<password>\n<date>").  Most ways to change a
	// password (the User object's setter, the terminal server, the sysop's
	// user editor) leave the date alone, so the password itself must be in
	// it; only the digest is stored.  Any change ends every web client
	// session made before it.
	function stamp_of(u)
	{
		var pw = "";
		var date = "";
		if (u !== null && typeof u === "object" && u.security) {
			if (typeof u.security.password === "string")
				pw = u.security.password;
			if (typeof u.security.password_date === "number")
				date = String(u.security.password_date);
		}
		return sha256_calc(pw + "\n" + date, /* hex: */ true);
	}

	function user_of(env, usernum)
	{
		var u = env.get_user(usernum);
		if (u === null || typeof u !== "object" || u.number !== usernum)
			return null;
		return u;
	}

	// The cookie values of env.cookie (a string or an array of strings)
	function cookie_values(c)
	{
		var list = c instanceof Array ? c : [c];
		var out = [];
		for (var i = 0; i < list.length && out.length < MAX_COOKIES; i++)
			if (typeof list[i] === "string" && list[i] !== "")
				out.push(list[i]);
		return out;
	}

	// The signed-in user from env.cookie (a browser can send several values
	// of the cookie, for different paths: the first that is a valid session
	// of an eligible user wins): {usernum, user, cookie, clear}; clear is
	// true when values were sent but none is valid (the cookie should be
	// cleared and the user told they were signed out).  Throws what Session
	// throws.
	function session_user(env, touch)
	{
		var out = { usernum: 0, user: null, cookie: null, clear: false };
		var values = cookie_values(env.cookie);
		if (values.length === 0)
			return out;
		var now = now_of(env);
		for (var i = 0; i < values.length; i++) {
			var value = values[i];
			var p = Session.parse(value);
			if (p === null)
				continue;
			var ud = env.userdata(p.usernum);
			// The user record is loaded only once the key has matched a
			// stored session (Session calls the stamp function then), so a
			// forged value costs one sessions-file read and no user read
			var u = null;
			var s = Session.check(ud, value, now, env.config, { touch: touch !== false,
				stamp: (function (n) {
					return function () {
						u = user_of(env, n);
						return stamp_of(u);
					};
				})(p.usernum) });
			if (s === null)
				continue;
			if (!eligible(u, env.config)) {
				try {
					Session.destroy(ud, value, now, env.config);
				} catch (e) {
					Api.log(LOG_WARNING, "webmail api: user " + s.usernum
						+ ": removing the session of an ineligible user: " + describe(e));
				}
				continue;
			}
			out.usernum = s.usernum;
			out.user = u;
			out.cookie = value;
			return out;
		}
		out.clear = true;
		return out;
	}

	// ---- Locale ----

	function locale_dirs(config)
	{
		var d = config && typeof config.directory === "string" ? config.directory : "";
		return { dir: d + "lib/", mods: d + "mods/" };
	}

	function locale_name(rq)
	{
		if (rq.available === null) {
			var dirs = locale_dirs(rq.config);
			try {
				rq.available = Locale().available(dirs.dir, dirs.mods);
			} catch (e) {
				Api.log(LOG_ERR, "webmail api: listing locales: " + describe(e));
				rq.available = [];
			}
		}
		var lang = rq.user && typeof rq.user.lang === "string" ? rq.user.lang : undefined;
		return Locale().pick(lang, rq.env.accept_language, rq.config.locale, rq.available);
	}

	// The strings of the request's locale, or null if they cannot be read
	function strings(rq)
	{
		var name = locale_name(rq);
		if (!own(rq.loaded, name)) {
			var dirs = locale_dirs(rq.config);
			try {
				rq.loaded[name] = Locale().load(dirs.dir, dirs.mods, name);
			} catch (e) {
				Api.log(LOG_ERR, "webmail api: loading locale " + name + ": " + describe(e));
				rq.loaded[name] = null;
			}
		}
		return rq.loaded[name];
	}

	function text(rq, code)
	{
		var s = null;
		try {
			s = strings(rq);
		} catch (e) {
			Api.log(LOG_ERR, "webmail api: locale: " + describe(e));
		}
		var key = "errors." + code;
		var t;
		if (s !== null && typeof s === "object" && own(s, key) && typeof s[key] === "string")
			t = s[key];
		else
			t = CODES.indexOf(code) >= 0 ? FALLBACK[code] : FALLBACK.internal;
		// The product name, as the shell page writes it in its own texts
		// (taken literally)
		if (code === "https") {
			var name = rq.config && typeof rq.config.product_name === "string"
				&& rq.config.product_name !== "" ? rq.config.product_name : "WebMail";
			t = t.split("{name}").join(name);
		}
		return t;
	}

	function failure(rq, code)
	{
		return { ok: false, error: code, message: text(rq, code) };
	}

	// The text for a reason of a code (errors.<code>_<reason>, the "_one"
	// form when count is 1), or null when the reason has none; {count} is
	// replaced by count
	function reason_text(rq, code, reason, count)
	{
		var key = code + "_" + reason;
		if (typeof count !== "number")
			count = null;
		else if (count === 1 && (own(REASON_FALLBACK, key + "_one")))
			key += "_one";
		var s = null;
		try {
			s = strings(rq);
		} catch (e) {
			Api.log(LOG_ERR, "webmail api: locale: " + describe(e));
		}
		var t = null;
		if (s !== null && typeof s === "object" && own(s, "errors." + key) && typeof s["errors." + key] === "string")
			t = s["errors." + key];
		else if (own(REASON_FALLBACK, key))
			t = REASON_FALLBACK[key];
		if (t !== null && count !== null)
			t = t.split("{count}").join(String(count));
		return t;
	}

	// The error reply for a thrown coded error: the code's text, or the
	// reason's when it has one; a whitelisted reason, and the numbers rev
	// (a draft's current revision), received (bytes of an upload so far)
	// and index (a recipient's position), and entry (that recipient as the
	// user typed it, cleaned by the library: at most 64 bytes, no control
	// characters); nothing else of the error
	//
	// count, for "too many recipients" (invalid, toomany): the limit that
	// refused, the address parser's MAX_ENTRIES (its error carries the
	// entry's index) or config.max_recipients (the send step's, which has
	// no index; 0 there means no limit, so it never refuses).
	// A folder method's reason keeps the code's text (the folder dialogs
	// have texts of their own).
	function coded_failure(rq, e, method)
	{
		var f = failure(rq, e.code);
		if (typeof e.reason === "string" && PASS_REASONS.indexOf(e.reason) >= 0) {
			f.reason = e.reason;
			if (e.code === "invalid" && e.reason === "toomany") {
				var max = rq.config.max_recipients;
				f.count = typeof e.index === "number" || !(typeof max === "number" && max > 0)
					? lib("address").MAX_ENTRIES : max;
			}
			var t = typeof method === "string" && method.substr(0, 7) === "folder."
				? null : reason_text(rq, e.code, e.reason, f.count);
			if (t !== null)
				f.message = t;
		}
		var nums = ["rev", "received", "index", "age"];
		for (var i = 0; i < nums.length; i++) {
			var v = e[nums[i]];
			if (typeof v === "number" && isFinite(v) && Math.floor(v) === v && v >= 0)
				f[nums[i]] = v;
		}
		if (f.index !== undefined && typeof e.entry === "string" && e.entry.length <= 64
			&& !/[\x00-\x1f\x7f]/.test(e.entry) && !BYTES_ONLY.test(e.entry))
			f.entry = e.entry;
		return f;
	}

	// ---- Arguments ----

	var BYTES_ONLY = /[^\x00-\xff]/;

	function check_value(d, v)
	{
		if (v === null && d.nullable === true)
			return true;
		switch (d.type) {
			case "string":
				return typeof v === "string"
					&& v.length <= (d.max === undefined ? MAX_STRING : d.max)
					&& !BYTES_ONLY.test(v)
					&& (d.test === undefined || d.test(v) === true);
			case "int":
				return typeof v === "number" && isFinite(v) && Math.floor(v) === v
					&& v >= (d.min === undefined ? 0 : d.min)
					&& v <= (d.max === undefined ? 0x7fffffff : d.max);
			case "boolean":
				return typeof v === "boolean";
			case "list":
				return check_list(d, v);
		}
		return false;
	}

	// A list: an array of min (default 1) to max (default 100) elements,
	// each passing d.of; unique: no element twice (strings and numbers)
	function check_list(d, v)
	{
		if (!(v instanceof Array) || v.length < (d.min === undefined ? 1 : d.min)
			|| v.length > (d.max === undefined ? 100 : d.max))
			return false;
		var seen = {};
		for (var i = 0; i < v.length; i++) {
			if (d.of.type === "list" || !check_value(d.of, v[i]))
				return false;
			if (d.unique) {
				// a prefix: no key is "__proto__"
				var key = "k" + typeof v[i] + ":" + v[i];
				if (own(seen, key))
					return false;
				seen[key] = true;
			}
		}
		return true;
	}

	// The checked arguments (a new object), or null if they are invalid
	function check_args(spec, args)
	{
		if (args === undefined || args === null)
			args = {};
		if (typeof args !== "object" || args instanceof Array)
			return null;
		var k;
		// "__proto__" first: this engine's hasOwnProperty() is true for it
		// on every object, so the test below would let it through
		if (Object.prototype.hasOwnProperty.call(args, "__proto__")
			&& Object.keys(args).indexOf("__proto__") >= 0)
			return null;
		for (k in args) {
			if (k === "__proto__" || (own(args, k) && !own(spec, k)))
				return null;
		}
		var out = {};
		for (k in spec) {
			if (!own(spec, k))
				continue;
			if (!own(args, k)) {
				if (spec[k].required)
					return null;
				continue;
			}
			if (!check_value(spec[k], args[k]))
				return null;
			out[k] = args[k];
		}
		return out;
	}

	function valid_folder(v)
	{
		if (FOLDERS.indexOf(v) >= 0)
			return true;
		if (v.substr(0, 2) !== "u:")
			return false;
		var tag = v.substr(2);
		return tag.length >= 1 && tag.length <= MAX_TAG && tag.charAt(0) !== '$' && !WS_RE.test(tag);
	}

	function valid_msg_id(v)
	{
		return MSG_ID_RE.test(v);
	}

	function valid_user_folder(v)
	{
		return v.substr(0, 2) === "u:" && valid_folder(v);
	}

	// Where message.move can put mail: the Inbox, Archive, a user folder
	function valid_move_target(v)
	{
		return MOVE_TARGETS.indexOf(v) >= 0 || valid_user_folder(v);
	}

	// ---- Sessions ----

	function set_cookie(rq, value, keep)
	{
		if (typeof rq.env.set_cookie === "function")
			rq.env.set_cookie(value, keep === true);
	}

	// A user record string as UTF-8: kept when ASCII or valid UTF-8, else
	// converted from CP437
	function utf8_of(v)
	{
		var s = v === undefined || v === null ? "" : String(v);
		if (str_is_ascii(s) || str_is_utf8(s))
			return s;
		return utf8_encode(s);
	}

	function user_info(rq)
	{
		if (rq.user === null)
			return null;
		return { number: rq.user.number, alias: utf8_of(rq.user.alias), name: utf8_of(rq.user.name),
			avatar: avatar_of(rq, rq.user.number, rq.user.alias, "", 0) };
	}

	// ---- Avatars (exec/load/avatar_lib.js) ----

	var MSG_ANONYMOUS = 1 << 5;	// smbdefs.js
	var NET_FIDO = 2;		// smbdefs.js
	var NET_QWK = 4;
	var QWK_ID = /^[A-Za-z][A-Za-z0-9_-]{1,7}$/;
	var BASE64 = /^[A-Za-z0-9+\/]+={0,2}$/;

	// The avatar library for this request (env.avatar_lib when given,
	// null for none), or null when it cannot be loaded
	function avatar_lib(rq)
	{
		if (rq.avatar_lib !== undefined)
			return rq.avatar_lib;
		var lib = null;
		if (rq.env.avatar_lib !== undefined)
			lib = rq.env.avatar_lib;
		else {
			try {
				lib = load({}, "avatar_lib.js");
			} catch (e) {
				Api.log(LOG_WARNING, "webmail api: loading avatar_lib.js: " + describe(e));
				lib = null;
			}
		}
		rq.avatar_lib = lib;
		return lib;
	}

	// The base64 avatar data that lib.read() gives for these arguments
	// (as the terminal's message header passes them, see
	// sender_avatar()), or null: avatars turned off, none, turned off by
	// its owner, or not a valid avatar.  Never throws.
	function avatar_of(rq, usernum, name, netaddr, bbsid)
	{
		var lib = avatar_lib(rq);
		if (lib === null || typeof lib !== "object")
			return null;
		try {
			if (lib.options && lib.options.enabled === false)
				return null;
			var obj = lib.read(usernum, name, netaddr, bbsid);
			if (!lib.is_enabled(obj) || typeof obj.data !== "string" || !BASE64.test(obj.data))
				return null;
			return lib.is_valid(base64_decode(obj.data)) ? obj.data : null;
		} catch (e) {
			Api.log(LOG_WARNING, "webmail api: user " + rq.usernum + ": avatar: " + describe(e));
			return null;
		}
	}

	// The sender's avatar for message id, chosen as the terminal's message
	// header does (exec/showmsgavatar.js): none for an anonymous message,
	// else avatar_lib.draw(from_ext || ftn_bbsid, from, from_net), which
	// reads (that, from, from_net, that).  The header is the sender's to
	// write, so what it may steer is checked first: from_ext only as a
	// number; ftn_bbsid only as a QWK ID (a letter, then 1 to 7 letters,
	// digits, "_" or "-"); and a sender that is not FidoNet or QWK is
	// looked up by name and address only, as an Internet sender: no user
	// number or BBS ID, and an address that can name no network file (no
	// "/" or "\", and an "@").
	function sender_avatar(ctx, rq, id)
	{
		try {
			var h = ctx.store.resolve(id).header;
			var from = h.from;	// a non-NULL field first (lazy fields)
			if (h.attr & MSG_ANONYMOUS)
				return null;
			var ext = h.from_ext ? String(h.from_ext) : "";
			if (!/^[0-9]{1,10}$/.test(ext))
				ext = "";
			var bbsid = h.ftn_bbsid ? String(h.ftn_bbsid) : "";
			if (!QWK_ID.test(bbsid))
				bbsid = "";
			var type = h.from_net_type || 0;
			var net = type ? String(h.from_net_addr || "") : "";
			var first = ext !== "" ? ext : bbsid;
			if (type && type !== NET_FIDO && type !== NET_QWK) {
				first = "";
				net = net.replace(/[\/\\]/g, "_");
				if (net.indexOf("@") < 0)
					net += "@";
			}
			return avatar_of(rq, first, typeof from === "string" ? from : "", net, first);
		} catch (e) {
			Api.log(LOG_WARNING, "webmail api: user " + rq.usernum + ": sender avatar: " + describe(e));
			return null;
		}
	}

	// Sign usernum in: a new session and its cookie; the session the
	// browser had (if any) is removed
	function start_session(rq, usernum, u, keep)
	{
		var env = rq.env;
		var now = now_of(env);
		var value = Session.create(env.userdata(usernum), usernum, env.client || null, keep === true,
			now, rq.config, stamp_of(u));
		if (rq.usernum > 0 && typeof rq.cookie === "string") {
			try {
				Session.destroy(env.userdata(rq.usernum), rq.cookie, now, rq.config);
			} catch (e) {
				Api.log(LOG_WARNING, "webmail api: user " + rq.usernum
					+ ": removing the replaced session: " + describe(e));
			}
		}
		rq.usernum = usernum;
		rq.user = u;
		rq.cookie = value;
		set_cookie(rq, value, keep);
	}

	// The user signed in to webv4 (env.webv4_user(), called at most once per
	// request and only when webv4_login is on), or 0
	function webv4_user(rq)
	{
		if (rq.config.webv4_login !== true || typeof rq.env.webv4_user !== "function")
			return 0;
		if (rq.webv4 === null) {
			var n = rq.env.webv4_user();
			rq.webv4 = typeof n === "number" && n >= 1 && Math.floor(n) === n ? n : 0;
		}
		return rq.webv4;
	}

	// Not after a session.logout in the same request, and not when the
	// call says the browser signed out here (no_webv4)
	function webv4_sign_in(rq, args)
	{
		if (rq.usernum !== 0 || rq.config.webv4_login !== true || rq.logged_out
			|| args.no_webv4 === true)
			return;
		var n = webv4_user(rq);
		if (n === 0)
			return;
		var u = user_of(rq.env, n);
		if (!eligible(u, rq.config))
			return;
		start_session(rq, n, u, false);
		rq.via = "webv4";
	}

	// ---- Methods ----

	function auth_error()
	{
		return { code: "auth", message: "sign-in refused" };
	}

	// The "powered by" text: a version notice with runs of white space
	// (ASCII only: these are byte strings) made one space
	function powered_by(notice)
	{
		return typeof notice === "string"
			? notice.replace(/[ \t\r\n\f\v]+/g, " ").replace(/^ | $/g, "") : "";
	}

	// THE single source of the domains user usernum may send from:
	// Address.domains_for (system.inet_domain_list, filtered by
	// User.can_send_from_domain).  session.get, settings.get and
	// settings.set use it through from_domains().
	function sending_domains(usernum)
	{
		return lib("address").domains_for(usernum);
	}

	function session_get(ctx, args, rq)
	{
		webv4_sign_in(rq, args);
		var s = strings(rq);
		if (s === null)
			throw new Error("the locale strings cannot be read");
		var config = rq.config;
		var out = { user: user_info(rq), insecure: rq.env.secure !== true, locale: locale_name(rq) };
		if (rq.via !== null)
			out.via = rq.via;
		if (!rq.strings_sent) {
			out.strings = s;
			rq.strings_sent = true;
		}
		out.domains = rq.usernum > 0 ? from_domains(rq) : [];
		out.limits = client_limits(config);
		out.poll_ms = config.poll_interval * 1000;
		out.powered_by = powered_by(system.version_notice);
		for (var i = 0; i < BRAND_KEYS.length; i++) {
			var v = config[BRAND_KEYS[i]];
			out[BRAND_KEYS[i]] = typeof v === "string" ? v : "";
		}
		return out;
	}

	function refuse_unless_eligible(rq, n, u)
	{
		var why = refusal(u, rq.config, true);
		if (why !== null) {
			Api.log(LOG_NOTICE, "webmail: user #" + n + " refused: " + why);
			throw auth_error();
		}
	}

	function session_login(ctx, args, rq)
	{
		var name = args.user;
		var password = args.password;
		if (name === "" || password === "" || name.length > MAX_USER || password.length > MAX_PASSWORD)
			throw auth_error();
		// An account the library would refuse never reaches login(), which
		// records a logon when it succeeds
		var found = typeof rq.env.find_user === "function" ? rq.env.find_user(name) : 0;
		if (typeof found === "number" && found >= 1)
			refuse_unless_eligible(rq, found, user_of(rq.env, found));
		var n = rq.env.login(name, password);
		if (typeof n !== "number" || !(n >= 1) || Math.floor(n) !== n)
			throw auth_error();
		var u = user_of(rq.env, n);
		refuse_unless_eligible(rq, n, u);
		start_session(rq, n, u, args.keep === true);
		return { user: user_info(rq) };
	}

	function session_logout(ctx, args, rq)
	{
		var n = rq.usernum;
		if (typeof rq.cookie === "string")
			Session.destroy(rq.env.userdata(n), rq.cookie, now_of(rq.env), rq.config);
		rq.usernum = 0;
		rq.user = null;
		rq.cookie = null;
		rq.logged_out = true;
		set_cookie(rq, "", false);
		return { webv4: n !== 0 && webv4_user(rq) === n };
	}

	function folder_list(ctx, args, rq)
	{
		var purged = 0;
		try {
			var res = ctx.store.purge_expired();
			if (res instanceof Array) {
				for (var i = 0; i < res.length; i++)
					if (res[i] && res[i].ok)
						purged++;
			}
		} catch (e) {
			Api.log(LOG_WARNING, "webmail api: user " + rq.usernum + ": purge_expired: " + describe(e));
		}
		var drafts = drafts_count(rq);
		// A claimed draft whose send record (else the claim) was last
		// written SEND_LIVE seconds ago or more: an interrupted send, which
		// is completed or given back (sweep_sends(); not in a background
		// request; a failure is logged and the list still returned).  A
		// send under way rewrites its record more often than that, so it
		// costs no sweep and no drafts lock here.
		if (drafts.stale > 0 && !rq.background) {
			try {
				var r = sweep_sends(rq);
				if (r.swept > 0 || r.recovered > 0)
					drafts = drafts_count(rq);
			} catch (e) {
				Api.log(LOG_WARNING, "webmail api: user " + rq.usernum + ": recovering sends: " + describe(e));
			}
		}
		return { folders: ctx.store.folders(), usage: ctx.store.usage(), purged: purged,
			trash_days: ctx.store.options.trash_days, inbox_days: inbox_days(ctx, rq),
			drafts: drafts.count };
	}

	// The number of the user's drafts, for display: the directories in
	// the user's drafts directory named by a draft id (Drafts' layout: a
	// claimed one, "<id>.sending", is not counted, but is in claims), read
	// with no lock and no write (Drafts.count would take the drafts lock,
	// which writes a file).  An unreadable draft.json still counts here.
	// {count, claims, stale}: stale counts the claims whose send.json (else
	// the claimed directory) is dated SEND_LIVE seconds ago or more, by the
	// file dates alone (no lock, nothing parsed); 0s when it cannot be
	// counted (logged), so the folder list never fails for it.
	function drafts_count(rq)
	{
		var out = { count: 0, claims: 0, stale: 0 };
		// the drafts library's clock (lib_ctx(): env.lib_ctx.now in tests)
		var lc = rq.env.lib_ctx;
		var old = (lc && typeof lc.now === "function" ? lc.now() : now_of(rq.env)) - SEND_LIVE;
		try {
			var root = rq.env.userdata(rq.usernum).dir + "drafts/";
			if (!file_isdir(root))
				return out;
			var list = directory(root + "*") || [];	// marked: a directory ends with a slash
			for (var i = 0; i < list.length; i++) {
				var p = String(list[i]);
				var c = p.charAt(p.length - 1);
				if (c !== "/" && c !== "\\")
					continue;	// not a directory
				p = p.substr(0, p.length - 1);
				var name = p.substr(Math.max(p.lastIndexOf("/"), p.lastIndexOf("\\")) + 1);
				if (DRAFT_ID_RE.test(name))
					out.count++;
				else if (name.length === 24 && name.substr(16) === ".sending" && DRAFT_ID_RE.test(name.substr(0, 16))) {
					out.claims++;
					var rec = p + "/send.json";
					var t = file_exists(rec) ? file_date(rec) : file_date(p);
					if (t <= old)
						out.stale++;
				}
			}
			return out;
		} catch (e) {
			Api.log(LOG_WARNING, "webmail api: user " + rq.usernum + ": counting drafts: " + describe(e));
			return { count: 0, claims: 0, stale: 0 };
		}
	}

	// The mail base's age limit in days (0: none): the max_age of the
	// opened shared base, which SCFG writes into the base's status when the
	// setting is saved.  0 when it cannot be read.
	function inbox_days(ctx, rq)
	{
		try {
			var n = ctx.store.inbox_max_age();
			return typeof n === "number" && n > 0 && Math.floor(n) === n ? n : 0;
		} catch (e) {
			Api.log(LOG_WARNING, "webmail api: user " + rq.usernum + ": inbox age: " + describe(e));
			return 0;
		}
	}

	function message_list(ctx, args, rq)
	{
		var opts = {};
		if (args.offset !== undefined)
			opts.offset = args.offset;
		if (args.limit !== undefined)
			opts.limit = args.limit;
		if (args.search !== undefined)
			opts.search = args.search;
		var list = ctx.store.list(args.folder, opts);
		return { total: list.total, items: list.items };
	}

	// The tag of the address a message was delivered to (name#tag@host),
	// lowercased, when the mail server delivers that address to this user
	// and the tag is one settings.set tag_block takes; else "".  The
	// reader offers to block it.  (A blocked tag is no longer delivered,
	// so it is not reported.)
	function delivered_tag(rq, addr)
	{
		try {
			if (typeof addr !== "string")
				return "";
			var at = addr.lastIndexOf("@");
			var local = at > 0 ? addr.substr(0, at) : "";
			var hash = local.indexOf("#");
			if (hash < 1)
				return "";
			var tag = local.substr(hash + 1);
			if (!lib("blocktags").valid(tag))
				return "";
			var extra = rq.env.lib_ctx;
			var who = extra && typeof extra.deliver_user === "function" ? extra.deliver_user(addr)
				: lib("address").system_resolvers({}).own.deliver_user(addr);
			return who === rq.usernum ? ascii_lower(tag) : "";
		} catch (e) {
			return "";
		}
	}

	function message_get(ctx, args, rq)
	{
		if (args.head === true) {
			var out = { item: ctx.store.get(args.id, { mark_read: args.mark_read === true, head: true }) };
			if (args.avatar === true)
				out.avatar = sender_avatar(ctx, rq, args.id);
			return out;
		}
		var msg = ctx.store.get(args.id, { mark_read: args.mark_read === true });
		if (msg && msg.headers)
			msg.headers.to_tag = delivered_tag(rq, msg.headers.to_addr);
		if (args.avatar === true)
			msg.avatar = sender_avatar(ctx, rq, args.id);
		return { message: msg };
	}

	// ---- Writes ----

	// The Store's per-id results in the reply's shape: {id, ok: true,
	// new_id [, folder, expired]} or {id, error} (a code the API does not
	// know is "failed"); nothing else the Store put in them
	function per_id(list)
	{
		var out = [];
		if (!(list instanceof Array))
			return out;
		for (var i = 0; i < list.length; i++) {
			var r = list[i];
			if (r === null || typeof r !== "object")
				continue;
			if (r.ok === true) {
				var o = { id: r.id, ok: true, new_id: r.new_id === undefined ? r.id : r.new_id };
				if (r.folder === "archive")
					o.folder = r.folder;
				if (r.expired === true)
					o.expired = true;
				out.push(o);
			} else
				out.push({ id: r.id, error: typeof r.error === "string" && CODES.indexOf(r.error) >= 0
					? r.error : "failed" });
		}
		return out;
	}

	// The state after a write: also the reply's state (computed once)
	function after_write(ctx, rq, out)
	{
		var s = ctx.store.state();
		rq.state = s;
		rq.state_fresh = true;
		out.state = s;
		return out;
	}

	function message_set(ctx, args, rq)
	{
		var ch = {};
		var keys = ["read", "starred", "spam"];
		for (var i = 0; i < keys.length; i++)
			if (args[keys[i]] !== undefined)
				ch[keys[i]] = args[keys[i]];
		return after_write(ctx, rq, { results: per_id(ctx.store.set(args.ids, ch)) });
	}

	// At least one of read, starred and spam
	function set_check(args)
	{
		return args.read !== undefined || args.starred !== undefined || args.spam !== undefined;
	}

	function message_move(ctx, args, rq)
	{
		return after_write(ctx, rq, { results: per_id(ctx.store.move(args.ids, args.folder)) });
	}

	function message_delete(ctx, args, rq)
	{
		return after_write(ctx, rq, { results: per_id(ctx.store.remove(args.ids)) });
	}

	function message_undelete(ctx, args, rq)
	{
		return after_write(ctx, rq, { results: per_id(ctx.store.restore(args.ids)) });
	}

	function trash_empty(ctx, args, rq)
	{
		return after_write(ctx, rq, { results: per_id(ctx.store.empty_trash()) });
	}

	function folder_of(f)
	{
		return { id: f.id, name: f.name };
	}

	function folder_create(ctx, args, rq)
	{
		return after_write(ctx, rq, { folder: folder_of(ctx.store.folder_create(args.name)) });
	}

	function folder_rename(ctx, args, rq)
	{
		var f = ctx.store.folder_rename(args.id, args.name);
		return after_write(ctx, rq, { folder: folder_of(f), results: per_id(f.errors) });
	}

	function folder_delete(ctx, args, rq)
	{
		return after_write(ctx, rq, { results: per_id(ctx.store.folder_delete(args.id)) });
	}

	function message_original(ctx, args, rq)
	{
		var o = ctx.store.original(args.id);
		return { original: { rfc822: o.rfc822, stored: o.stored, external_files: o.external_files } };
	}

	function changes(ctx, args, rq)
	{
		var s = ctx.store.state();
		rq.state = s;
		rq.state_fresh = true;
		return { changed: s !== args.state, state: s };
	}

	// ---- Drafts, sending, contacts and settings ----

	// The libraries these methods use, each loaded on first use (once per
	// load of this file): drafts, send, contacts and address from beside
	// this file, mailsig and sbbsdefs from exec/load/ of this install (as
	// userdata.js finds mailbox_files.js), else through load()'s search
	var LIBS = {};
	var LIB_FILES = { drafts: "drafts.js", send: "send.js", contacts: "contacts.js",
		address: "address.js", config: "config.js", blocktags: "blocktags.js" };
	var LOAD_FILES = { mailsig: "mailsig.js", sbbsdefs: "sbbsdefs.js" };

	function lib(name)
	{
		if (!own(LIBS, name)) {
			if (own(LIB_FILES, name))
				LIBS[name] = sibling(LIB_FILES[name]);
			else {
				var path = HERE + "../../exec/load/" + LOAD_FILES[name];
				if (HERE === "" || !file_exists(path))
					path = LOAD_FILES[name];
				LIBS[name] = load({}, path);
			}
		}
		return LIBS[name];
	}

	// The context the drafts, send and contacts libraries take: the
	// user, the user's UserData and the settings; with need_store also
	// the opened mailbox (make_ctx, once per request).  env.lib_ctx (tests
	// only) is an object whose fields are added (domains, resolvers, sys,
	// now, user, ...).
	function lib_ctx(rq, need_store)
	{
		var base = need_store ? get_ctx(rq) : null;
		if (base !== null)
			rq.state_fresh = false;
		var u = base !== null && base.user ? base.user : rq.user;
		var c = { usernum: rq.usernum, alias: u.alias, name: u.name, user: u,
			userdata: base !== null && base.userdata ? base.userdata : rq.env.userdata(rq.usernum),
			config: rq.config };
		if (base !== null)
			c.store = base.store;
		var extra = rq.env.lib_ctx;
		if (extra !== null && typeof extra === "object") {
			for (var k in extra)
				if (own(extra, k))
					c[k] = extra[k];
		}
		return c;
	}

	// The limits the client works with, sent once in session.get: the
	// attachment, text, draft and message limits of the settings, and the
	// drafts library's fixed ones (Drafts.limits; ATTACH_CHUNK and
	// MAX_ATTACHMENTS are pinned to the library's by a test, so session.get
	// need not load it)
	function client_limits(config)
	{
		return { max_attachment_bytes: config.max_attachment_bytes,
			max_text_part_bytes: config.max_text_part_bytes, max_drafts: config.max_drafts,
			max_draft_bytes: config.max_draft_bytes, max_body_bytes: config.max_body_bytes,
			max_attachments: MAX_ATTACHMENTS, max_chunk: ATTACH_CHUNK,
			max_recipients: config.max_recipients, max_message_bytes: config.max_message_bytes,
			max_attach_total: attach_total(config) };
	}

	// The most attachment bytes a message may carry: max_attachment_bytes,
	// and what fits max_message_bytes with no text (Config's
	// attachment_allowance: MimeWrite.estimate()'s arithmetic); 0 = no limit
	function attach_total(config)
	{
		var a = config.max_attachment_bytes > 0 ? config.max_attachment_bytes : 0;
		var m = config.max_message_bytes > 0 ? lib("config").attachment_allowance(config.max_message_bytes) : 0;
		return a > 0 && m > 0 ? Math.min(a, m) : Math.max(a, m);
	}

	// The From domains the user may use: ctx.domains when the context has
	// them (tests), else Address.domains_for(usernum)
	function from_domains(rq)
	{
		var extra = rq.env.lib_ctx;
		if (extra && extra.domains instanceof Array)
			return extra.domains;
		return sending_domains(rq.usernum);
	}

	// Every address the user may send from, the default first: ctx.addresses
	// when the context has them (tests), else Address.addresses_for() over
	// from_domains()
	function from_addresses(rq)
	{
		var extra = rq.env.lib_ctx;
		if (extra && extra.addresses instanceof Array)
			return extra.addresses;
		return lib("address").addresses_for(rq.usernum, { domains: from_domains(rq) });
	}

	// For Address.from_pick: may the user send from an address that is not
	// in from_addresses() (a name alias.cfg points at them)
	function own_check(rq)
	{
		return function (addr) {
			var extra = rq.env.lib_ctx;
			if (extra && typeof extra.own_address === "function")
				return extra.own_address(addr) === true;
			return lib("address").own_address(rq.usernum, addr, { domains: from_domains(rq) });
		};
	}

	// ---- Request body sizes ----

	// Every request may be BASE_BODY bytes; a request of exactly one
	// draft.attach or one draft.save call may be larger, up to what that
	// call can need (body_allowance), never more than HOST_MAX_POST: the
	// web server gives a script no post_data at all for a longer body
	// (MAX_POST_LEN in src/sbbs3/websrvr.cpp, 4 MiB; it then spools the
	// body to a file).  The web server reads the whole body before the
	// script runs, so the size cannot stop the reading, only the work.
	var BASE_BODY = 262144;
	var HOST_MAX_POST = 4 * 1048576;
	var ATTACH_CHUNK = 1500000;	// Drafts.MAX_CHUNK: decoded bytes per draft.attach
	var MAX_ATTACHMENTS = 100;	// Drafts.MAX_ATTACHMENTS
	var MAX_ADDR_FIELD = 16384;	// Drafts.MAX_ADDR_FIELD: to, cc, bcc bytes
	var MAX_SUBJECT_IN = 4096;	// a draft subject as received (stored: 255)
	var MAX_UPLOAD_NAME = 1024;	// draft.attach name as received (stored: 128)
	var MAX_UPLOAD_TYPE = 255;
	var BODY_OVERHEAD = 16384;	// the request's other parts: method, ids, as, numbers
	var LARGE_METHODS = ["draft.attach", "draft.save"];
	var LARGE_HEAD_RE = /^[ \t\r\n]*\{[ \t\r\n]*"calls"[ \t\r\n]*:[ \t\r\n]*\[[ \t\r\n]*\[[ \t\r\n]*"(draft\.attach|draft\.save)"[ \t\r\n]*,/;
	var AS_TAIL_RE = /,[ \t\r\n]*"as"[ \t\r\n]*:[ \t\r\n]*([0-9]{1,10})[ \t\r\n]*\}[ \t\r\n]*$/;

	// The largest body a request of this one method may have.
	//   draft.attach: the chunk's base64 (4 characters per 3 bytes), the
	//     name and type in the worst JSON form (a \u escape, 6 characters,
	//     per byte), plus BODY_OVERHEAD: 2,024,058 bytes.
	//   draft.save: the text fields twice over (JSON writes a quote, a
	//     backslash and a line break as two characters; every other control
	//     character is refused, and other text goes as UTF-8, a byte per
	//     byte; that is how a browser's JSON.stringify writes text, the
	//     client this allowance is for): max_body_bytes, three address fields, the subject as
	//     received, a domain and a priority; plus BODY_OVERHEAD: 2,220,546
	//     bytes with the default max_body_bytes (1 MiB).
	// Never less than BASE_BODY, never more than HOST_MAX_POST.
	function body_allowance(config, method)
	{
		var n = BASE_BODY;
		if (method === "draft.attach")
			n = 4 * Math.ceil(ATTACH_CHUNK / 3) + 6 * (MAX_UPLOAD_NAME + MAX_UPLOAD_TYPE) + BODY_OVERHEAD;
		else if (method === "draft.save") {
			var body = config && typeof config.max_body_bytes === "number" ? config.max_body_bytes : 0;
			n = 2 * (body + 3 * MAX_ADDR_FIELD + MAX_SUBJECT_IN + 253 + 4) + BODY_OVERHEAD;
		}
		return Math.min(Math.max(n, BASE_BODY), HOST_MAX_POST);
	}

	// The largest body any request may have
	function max_body(config)
	{
		var n = BASE_BODY;
		for (var i = 0; i < LARGE_METHODS.length; i++)
			n = Math.max(n, body_allowance(config, LARGE_METHODS[i]));
		return n;
	}

	// The size rule BEFORE the body is parsed (api.ssjs): declared is the
	// Content-Length header (a number, NaN when absent), body the post data
	// (a string, or not one when the web server kept a long body to
	// itself).  Returns { ok: false } (refuse: 413) or { ok: true, method }:
	// method null for a body of at most BASE_BODY, else the one large
	// method the body starts with ({"calls":[["draft.attach", ... as the
	// client writes it; a request written in another order is held to
	// BASE_BODY).  Nothing is copied but the body's first 256 characters
	// (and its last 64, see large_gate()).
	function body_check(config, declared, body)
	{
		var limit = max_body(config);
		if (typeof declared === "number" && declared > limit)
			return { ok: false };
		if (typeof body !== "string")
			return { ok: !(declared > 0), method: null };
		if (body.length > limit)
			return { ok: false };
		if (body.length <= BASE_BODY)
			return { ok: true, method: null };
		var m = LARGE_HEAD_RE.exec(body.substr(0, 256));
		if (m === null || body.length > body_allowance(config, m[1]))
			return { ok: false };
		return { ok: true, method: m[1] };
	}

	// The gates for a large body, BEFORE it is parsed (api.ssjs, after
	// body_check() gave a method): the X-Webmail header, HTTPS, a signed-in
	// user (the session cookie, checked without refreshing the session),
	// and "as" when the body ends with it (the client writes it last) and
	// names another user.  null to go on; else the reply (the top-level
	// error shape: forbidden, https, auth or stale), and the body is never
	// parsed.  dispatch() checks everything again on the parsed request.
	function large_gate(env, body)
	{
		var rq = new_request(env);
		var gate = null;
		var usernum = 0;
		var signed_out = false;
		try {
			if (env.has_header !== true)
				gate = "forbidden";
			else if (env.secure !== true && rq.config.require_tls !== false)
				gate = "https";
			else {
				var s = session_user(env, false);
				usernum = s.usernum;
				signed_out = s.clear;
				// as dispatch does: a cookie that is no session is cleared
				if (s.clear && typeof env.set_cookie === "function")
					env.set_cookie("", false);
				if (usernum === 0)
					gate = "auth";
				else if (typeof body === "string") {
					var m = AS_TAIL_RE.exec(body.substr(Math.max(0, body.length - 64)));
					if (m !== null && parseInt(m[1], 10) !== usernum)
						gate = "stale";
				}
			}
		} catch (e) {
			Api.log(LOG_ERR, "webmail api: checking a large request: " + describe(e));
			gate = "internal";
		}
		if (gate === null)
			return null;
		return { error: gate, message: text(rq, gate), results: [], state: "", signed_out: signed_out,
			usernum: gate === "stale" ? usernum : 0 };
	}

	// After parsing: a body over BASE_BODY must hold exactly one call, of
	// a large method whose allowance covers it
	function body_fits(config, length, calls)
	{
		if (typeof length !== "number" || length <= BASE_BODY)
			return true;
		return calls.length === 1 && typeof calls[0][0] === "string"
			&& LARGE_METHODS.indexOf(calls[0][0]) >= 0 && length <= body_allowance(config, calls[0][0]);
	}

	// ---- Drafts ----

	var DRAFT_ID_RE = /^[0-9a-f]{16}$/;
	var ATT_ID_RE = /^a[1-9][0-9]{0,8}$/;
	var UPLOAD_RE = /^[A-Za-z0-9_-]{8,64}$/;
	var SEND_KEY_RE = /^[A-Za-z0-9_-]{16,64}$/;	// Send.KEY_RE
	var CONTACT_ID_RE = /^c[1-9][0-9]{0,8}$/;
	var MODES = ["new", "reply", "replyall", "forward"];
	var PRIORITIES = ["", "high", "low"];
	var WARNINGS = ["nocopy", "longlist", "recovered"];
	var MAX_RECOVER = 2;		// stale sends draft.list completes per request
	var SEND_LIVE = 600;		// Send.LIVE (pinned by a test): folder.list does not load send.js
	var RECOVER_SECONDS = 5;	// ... and no new one after this long

	function valid_draft_id(v)
	{
		return DRAFT_ID_RE.test(v);
	}

	function valid_mode(v)
	{
		return MODES.indexOf(v) >= 0;
	}

	// mode "new" takes no source; the others need one
	function create_check(args)
	{
		return args.mode === "new" ? args.source_id === undefined : args.source_id !== undefined;
	}

	// The reply's draft, without dropped_recipients (which goes beside it)
	function draft_create(ctx, args, rq)
	{
		var opts = { mode: args.mode };
		if (args.source_id !== undefined)
			opts.source_id = args.source_id;
		var c = lib_ctx(rq, args.mode !== "new");
		var d = lib("drafts").create(c, opts);
		var out = { usage: lib("drafts").usage(c) };
		if (d !== null && typeof d === "object" && d.dropped_recipients !== undefined) {
			if (typeof d.dropped_recipients === "number")
				out.dropped_recipients = d.dropped_recipients;
			delete d.dropped_recipients;
		}
		out.draft = d;
		return out;
	}

	// A draft that is not found because a send has claimed it (another
	// window is sending it: reason "sending", with age, the seconds since
	// the claim's heartbeat; "stalled" when that is Send.LIVE or more: the
	// send did not finish, and a non-background draft.list completes or
	// gives it back) or because it was sent
	// (Send's index of completed sends names it: reason "sent"), so the
	// page says so and does not save the text again as if it were lost
	function why_gone(c, id, e)
	{
		if (is_coded(e) && e.code === "notfound" && e.reason === undefined) {
			try {
				var age = lib("drafts").claim_age(c, id);
				if (age !== null) {
					e.reason = age >= lib("send").LIVE ? "stalled" : "sending";
					e.age = age;
				} else if (lib("send").sent(c, id))
					e.reason = "sent";
			} catch (x) {
				// the index cannot be read: plain notfound
			}
		}
		return e;
	}

	function draft_get(ctx, args, rq)
	{
		var c = lib_ctx(rq, false);
		try {
			return { draft: lib("drafts").get(c, args.id), usage: lib("drafts").usage(c) };
		} catch (e) {
			throw why_gone(c, args.id, e);
		}
	}

	// Drafts.sweep, then the stale sends it reports (claims whose heartbeat
	// is Send.LIVE old: an interrupted send) are completed, given back or
	// returned marked maybe_sent by Send.recover, after the drafts lock is
	// released: at most MAX_RECOVER per request, none started
	// RECOVER_SECONDS after the first, starting at a rotating place so a
	// claim that keeps failing does not hold the others back.  The rest
	// wait for a later request.  Run by a draft.list and a folder.list that
	// are not background requests (a request the client made on its own
	// changes nothing), so an open mailbox finishes an interrupted send at
	// the next folder or Drafts visit.  Recovery is idempotent: repeating
	// it (rpc.js retries these reads) completes nothing twice.
	// Returns {swept, recovered}.
	function sweep_sends(rq)
	{
		var c = lib_ctx(rq, false);
		var out = lib("drafts").sweep(c, undefined, { live: lib("send").LIVE });
		var swept = (out.drafts || 0) + (out.files || 0);
		var recovered = out.recovered || 0;
		var stale = out.stale_sends instanceof Array ? out.stale_sends : [];
		var sc = null;
		if (stale.length > 0) {
			// The mailbox, for the recovery only: when it cannot be
			// opened the list is still returned (the sends wait)
			try {
				sc = lib_ctx(rq, true);
			} catch (e) {
				Api.log(LOG_WARNING, "webmail api: user " + rq.usernum
					+ ": no mailbox to recover sends: " + describe(e));
			}
		}
		if (sc !== null) {
			var start = time() % stale.length;
			var began = time();
			for (var i = 0; i < stale.length && i < MAX_RECOVER; i++) {
				if (i > 0 && time() - began >= RECOVER_SECONDS)
					break;
				var id = stale[(start + i) % stale.length];
				// Counted when the claim was resolved: the send
				// completed (its outcome), given back (null) or
				// returned marked maybe_sent (conflict "unknown");
				// not when it is still claimed (sending, partial, ...)
				try {
					lib("send").recover(sc, id);
					recovered++;
				} catch (e) {
					if (is_coded(e) && e.code === "conflict" && e.reason === "unknown")
						recovered++;
					else
						Api.log(LOG_WARNING, "webmail api: user " + rq.usernum + ": recovering send "
							+ id + ": " + describe(e));
				}
			}
		}
		return { swept: swept, recovered: recovered };
	}

	// sweep_sends() (not in a background request), then the list (rows
	// carry maybe_sent)
	function draft_list(ctx, args, rq)
	{
		var r = rq.background ? { swept: 0, recovered: 0 } : sweep_sends(rq);
		return { drafts: lib("drafts").list(lib_ctx(rq, false)), swept: r.swept, recovered: r.recovered };
	}

	function draft_save(ctx, args, rq)
	{
		var fields = {};
		for (var k in args)
			if (own(args, k) && k !== "id")
				fields[k] = args[k];
		var c = lib_ctx(rq, false);
		try {
			return { draft: lib("drafts").save(c, args.id, fields) };
		} catch (e) {
			throw why_gone(c, args.id, e);
		}
	}

	function draft_delete(ctx, args, rq)
	{
		lib("drafts").remove(lib_ctx(rq, false), args.id);
		return {};
	}

	function draft_attach(ctx, args, rq)
	{
		var r = lib("drafts").attach(lib_ctx(rq, false), args.id, { upload: args.upload,
			name: args.name, type: args.type === undefined ? "" : args.type, offset: args.offset,
			total: args.total, b64: args.b64 });
		return { attachment: { id: r.id, received: r.received, complete: r.complete === true } };
	}

	function draft_detach(ctx, args, rq)
	{
		return { draft: lib("drafts").detach(lib_ctx(rq, false), args.id, args.attachment) };
	}

	// Send.send's outcome: {id, message_id [, warning]}; its errors with
	// their fixed reasons (coded_failure)
	function draft_send(ctx, args, rq)
	{
		var c = lib_ctx(rq, true);
		if (c.settings === undefined)
			c.settings = settings_of(c.userdata);
		var r = lib("send").send(c, args.id, args.key,
			{ confirm_resend: args.confirm_resend === true });
		var out = { id: typeof r.id === "string" ? r.id : null,
			message_id: typeof r.message_id === "string" ? r.message_id : "" };
		if (typeof r.warning === "string" && WARNINGS.indexOf(r.warning) >= 0)
			out.warning = r.warning;
		return out;
	}

	// ---- Contacts ----

	function contact_list(ctx, args, rq)
	{
		return { contacts: lib("contacts").list(rq.env.userdata(rq.usernum)) };
	}

	function contact_set(ctx, args, rq)
	{
		var f = { addrs: args.addrs };
		if (args.id !== undefined)
			f.id = args.id;
		if (args.name !== undefined)
			f.name = args.name;
		if (args.note !== undefined)
			f.note = args.note;
		return { contact: lib("contacts").set(rq.env.userdata(rq.usernum), f) };
	}

	function contact_delete(ctx, args, rq)
	{
		lib("contacts").remove(rq.env.userdata(rq.usernum), args.id);
		return {};
	}

	// The user's own contacts only (Contacts.suggest never looks up the
	// system's users)
	function contact_suggest(ctx, args, rq)
	{
		return { suggestions: lib("contacts").suggest(rq.env.userdata(rq.usernum), args.q,
			args.limit === undefined ? 10 : args.limit) };
	}

	// ---- Settings ----

	var SETTINGS = "settings";	// webmail.settings.json
	var THEMES = ["system", "light", "dark"];
	var MAX_IMAGES = 200;
	var MAX_DISPLAY_NAME = 64;
	var MAX_SIGNATURE_IN = 16384;	// bytes of the argument
	var MAX_SIGNATURE_CHARS = 2000;	// MailSig's MAX_LENGTH (a test pins it)
	var LEN_NETMAIL = 60;		// sbbsdefs.js: the user record's field
	var NET_INTERNET = 5;		// smbdefs.js

	// The stored settings, each field checked (a bad or missing one gets
	// its default): {display_name, from_domain, theme, images_always}
	function settings_of(ud)
	{
		var s = null;
		try {
			s = ud.read(SETTINGS, null);
		} catch (e) {
			s = null;
		}
		if (s === null || typeof s !== "object" || s instanceof Array)
			s = {};
		var out = { display_name: "", from_domain: "", from_local: "", theme: "system", images_always: [] };
		if (typeof s.display_name === "string")
			out.display_name = s.display_name;
		if (typeof s.from_domain === "string")
			out.from_domain = s.from_domain;
		if (typeof s.from_local === "string")
			out.from_local = s.from_local;
		if (typeof s.theme === "string" && THEMES.indexOf(s.theme) >= 0)
			out.theme = s.theme;
		if (s.images_always instanceof Array) {
			for (var i = 0; i < s.images_always.length && out.images_always.length < MAX_IMAGES; i++)
				if (typeof s.images_always[i] === "string")
					out.images_always.push(s.images_always[i]);
		}
		return out;
	}

	// The system values the settings use: env.sys's fields (tests) over
	// the system's own
	function sys_of(rq)
	{
		if (rq.sys)
			return rq.sys;
		var defs = lib("sbbsdefs");
		var s = {
			settings: system.settings,
			newuser_questions: system.newuser_questions,
			inet_domain_list: system.inet_domain_list,
			msg_settings: msg_area.settings,
			MM_FWDKEEP: defs.MM_FWDKEEP,
			inet_alias: (msg_area.inet_netmail_settings & lib("address").NMAIL_ALIAS) != 0,
			findstr: function (list, str) {
				return system.findstr(list, str);
			},
			trashcan: function (base, str) {
				return system.trashcan(base, str);
			},
			matchuserdata: function (field, data) {
				return system.matchuserdata(field, data);
			},
			check_netmail_addr: function (a) {
				return system.check_netmail_addr(a);
			},
			netaddr_type: function (a) {
				return netaddr_type(a);
			}
		};
		var extra = rq.env.sys;
		if (extra !== null && typeof extra === "object") {
			for (var k in extra)
				if (own(extra, k))
					s[k] = extra[k];
		}
		s.defs = defs;
		rq.sys = s;
		return s;
	}

	// A plain Internet e-mail address by the address library's rules: one
	// entry, no display name, a plain host name, and netaddr_type()
	// Internet
	function internet_address(sys, a)
	{
		try {
			var Address = lib("address");
			var l = Address.parse_list(a);
			if (l.length !== 1 || l[0].addr !== a || l[0].name !== "")
				return false;
			Address.format("", a);
			return sys.netaddr_type(a) === NET_INTERNET;
		} catch (e) {
			return false;
		}
	}

	// Forwarding: allowed unless the system does not forward mail
	// (SYS_FWDTONET off: reason "off") or the user has the forward-mail
	// restriction ('F': "restricted")
	function forward_refusal(rq, sys)
	{
		if (!(sys.settings & sys.defs.SYS_FWDTONET))
			return "off";
		var u = rq.user;
		if (u.security && (u.security.restrictions & UD.UREST_FORWARD_MAIL))
			return "restricted";
		return null;
	}

	function forward_info(rq, sys)
	{
		var u = rq.user;
		var why = forward_refusal(rq, sys);
		var bit = sys.MM_FWDKEEP;
		var out = { allowed: why === null, enabled: (u.settings & UD.USER_NETMAIL) !== 0,
			address: utf8_of(u.netmail),
			keeps_copy: typeof bit === "number" && bit !== 0 && (sys.msg_settings & bit) !== 0 };
		if (why !== null)
			out.reason = why;
		return out;
	}

	// The user's blocked address tags: [{ text, pattern }] (BlockTags.list);
	// [] when the file cannot be read (logged)
	function blocked_tags(rq)
	{
		var BlockTags = lib("blocktags");
		try {
			return BlockTags.list(BlockTags.path(rq.env.data_dir || system.data_dir, rq.usernum)).map(function (e) {
				return { text: utf8_of(e.text), pattern: e.pattern };
			});
		} catch (e) {
			log(LOG_WARNING, "webmail: user #" + rq.usernum + " blocked tags: " + e);
			return [];
		}
	}

	function settings_get(ctx, args, rq)
	{
		var ud = rq.env.userdata(rq.usernum);
		var s = settings_of(ud);
		var domains = from_domains(rq);
		var addresses = from_addresses(rq);
		var out = { display_name: s.display_name, from_domain: s.from_domain, from_local: s.from_local,
			theme: s.theme, images_always: s.images_always, from_domains: domains,
			from_addresses: addresses, from_address_local: "" };
		// A stored domain the user may no longer use: the system address
		if (out.from_domain !== "" && domains.indexOf(out.from_domain) < 0)
			out.from_domain = "";
		// A stored name that is no longer one of the user's addresses at
		// that domain: the default name
		if (out.from_local !== ""
			&& lib("address").from_pick(out.from_local, out.from_domain, addresses, own_check(rq)) === null)
			out.from_local = "";
		// The blocked address tags (data/user/NNNN.smtpblock), as stored
		out.blocked_tags = blocked_tags(rq);
		// The name Internet mail carries when display_name is blank: the
		// alias or the real name, as the system is set up (what send uses)
		out.default_name = utf8_of(lib("address").identity(rq.user, "internet", null,
			{ inet_alias: sys_of(rq).inet_alias === true, fido_alias: false }).from);
		var email = utf8_of(rq.user.email);
		var at = email.lastIndexOf("@");
		if (at > 0)
			out.from_address_local = email.substr(0, at);
		var MailSig = lib("mailsig");
		var sig = MailSig.get(rq.usernum, rq.env.data_dir);
		out.signature = sig.text;
		out.signature_source = sig.source;
		out.signature_enabled = MailSig.enabled() === true;
		out.forward = forward_info(rq, sys_of(rq));
		return out;
	}

	function invalid(reason)
	{
		return { code: "invalid", message: "invalid setting", reason: reason };
	}

	// images_always, images_add, images_remove: an address, ASCII
	// lowercased, or invalid
	function image_sender(sys, a)
	{
		if (!internet_address(sys, a))
			throw invalid("images");
		return ascii_lower(a);
	}

	// Whether address may be forwarded to; throws invalid with the reason
	// when not.  The same rules for a new address and for the stored one
	// when forwarding is turned on: a plain Internet address of at most 60
	// characters; not at one of the system's own domains (the mail server
	// would not forward there: it would loop; system.inet_domain_list
	// starts with the system's address); one the system can send to
	// (system.check_netmail_addr, the terminal's netmail_supported());
	// not in email.can; with duplicates not allowed (UQ_DUPNETMAIL), not
	// another user's (as the terminal's prompt, get_netmail in
	// user_info_prompts.js)
	function check_forward_address(rq, sys, address)
	{
		if (address === "")
			throw invalid("forward_empty");
		if (address.length > LEN_NETMAIL)
			throw invalid("forward_toolong");
		if (!internet_address(sys, address))
			throw invalid("forward_address");
		var domain = address.substr(address.lastIndexOf("@") + 1);
		if (sys.findstr(sys.inet_domain_list, domain) === true)
			throw invalid("forward_own");
		if (sys.check_netmail_addr(address) !== true)
			throw invalid("forward_unsupported");
		if (sys.trashcan("email", address) === true)
			throw invalid("forward_blocked");
		if (sys.newuser_questions & sys.defs.UQ_DUPNETMAIL) {
			var other = sys.matchuserdata(sys.defs.U_NETMAIL, address);
			if (typeof other === "number" && other > 0 && other !== rq.usernum)
				throw invalid("forward_duplicate");
		}
	}

	// The forwarding change, checked: {address, enabled} or null for none.
	// An empty forward_address is refused (forward_empty): the terminal
	// never stores one, and the address has other uses (see the header);
	// forward_enabled false turns forwarding off and keeps the address.
	function forward_change(rq, sys, args)
	{
		if (args.forward_enabled === undefined && args.forward_address === undefined)
			return null;
		var why = forward_refusal(rq, sys);
		if (why !== null)
			throw { code: "forbidden", message: "forwarding not allowed", reason: "forward_" + why };
		var u = rq.user;
		var address = args.forward_address !== undefined ? args.forward_address : String(u.netmail || "");
		var enabled = args.forward_enabled !== undefined ? args.forward_enabled
			: (u.settings & UD.USER_NETMAIL) !== 0;
		if (args.forward_address !== undefined || args.forward_enabled === true)
			check_forward_address(rq, sys, address);
		return { address: address, enabled: enabled };
	}

	// What MailSig.set checks, checked first (so a refused signature is
	// known before anything is written): null, or well-formed UTF-8 of at
	// most 2000 characters (bytes that are not continuation bytes)
	function signature_ok(text)
	{
		if (text === null)
			return true;
		if (!str_is_ascii(text) && !str_is_utf8(text))
			return false;
		return text.replace(/[\x80-\xbf]/g, "").length <= MAX_SIGNATURE_CHARS;
	}

	// At least one setting; images_always (the whole list) not together
	// with images_add or images_remove
	function settings_check(args)
	{
		if (Object.keys(args).length === 0)
			return false;
		return args.images_always === undefined
			|| (args.images_add === undefined && args.images_remove === undefined);
	}

	// Order (settings.set):
	//   1. every argument is checked: names, domains, addresses, the
	//      signature (signature_ok: what MailSig.set refuses), forwarding;
	//      a refusal writes nothing
	//   2. the settings file, one locked read-modify-write, where the
	//      200-sender cap of images_add is decided (toolarge: nothing
	//      written); two tabs' images_add and images_remove both apply
	//   3. the signature (MailSig.set); it can then fail only to write
	//      ("failed"), leaving step 2's change
	//   4. the user record's forwarding fields, through the User object as
	//      the terminal changes them (user.netmail, then the USER_NETMAIL
	//      bit of user.settings); steps 2 and 3 stay if it fails
	// Returns what settings.get returns.
	function settings_set(ctx, args, rq)
	{
		var sys = sys_of(rq);
		var Address = lib("address");
		var change = {};
		if (args.display_name !== undefined)
			change.display_name = Address.clean(args.display_name, MAX_DISPLAY_NAME);
		if (args.from_domain !== undefined) {
			var d = ascii_lower(args.from_domain);
			if (d !== "" && from_domains(rq).indexOf(d) < 0)
				throw invalid("from_domain");
			change.from_domain = d;
		}
		if (args.from_local !== undefined) {
			// with the domain of this call, else the stored one
			var fd = change.from_domain !== undefined ? change.from_domain
				: settings_of(rq.env.userdata(rq.usernum)).from_domain;
			if (args.from_local !== ""
				&& Address.from_pick(args.from_local, fd, from_addresses(rq), own_check(rq)) === null)
				throw invalid("from_local");
			change.from_local = args.from_local;
		} else if (change.from_domain !== undefined)
			change.from_local = "";	// another domain alone: its default name
		if (args.theme !== undefined)
			change.theme = args.theme;
		var images = null;
		var i;
		if (args.images_always !== undefined) {
			images = [];
			for (i = 0; i < args.images_always.length; i++) {
				var a = image_sender(sys, args.images_always[i]);
				if (images.indexOf(a) < 0)
					images.push(a);
			}
		}
		var add = args.images_add !== undefined ? image_sender(sys, args.images_add) : null;
		var remove = args.images_remove !== undefined ? image_sender(sys, args.images_remove) : null;
		if (args.signature !== undefined && !signature_ok(args.signature))
			throw invalid("signature");
		var BlockTags = lib("blocktags");
		if (args.tag_block !== undefined && !BlockTags.valid(args.tag_block))
			throw invalid("tag");
		var fwd = forward_change(rq, sys, args);
		var ud = rq.env.userdata(rq.usernum);
		// The blocked tags file, under the settings lock (two requests of
		// one user); tag_unblock names an entry as settings.get listed it
		if (args.tag_block !== undefined || args.tag_unblock !== undefined) {
			ud.locked(SETTINGS, function () {
				var path = BlockTags.path(rq.env.data_dir || system.data_dir, rq.usernum);
				if (args.tag_unblock !== undefined) {
					var gone = null;
					BlockTags.list(path).forEach(function (e) {
						if (gone === null && utf8_of(e.text) === args.tag_unblock)
							gone = e.text;
					});
					if (gone !== null)
						BlockTags.unblock(path, gone);
				}
				if (args.tag_block !== undefined)
					BlockTags.block(path, args.tag_block);
			});
		}
		if (images !== null || add !== null || remove !== null || Object.keys(change).length > 0) {
			ud.update(SETTINGS, {}, function (stored) {
				if (stored === null || typeof stored !== "object" || stored instanceof Array)
					stored = {};
				var cur = settings_of({ read: function () {
					return stored;
				} });
				for (var k in change)
					if (own(change, k))
						cur[k] = change[k];
				var list = images !== null ? images : cur.images_always;
				if (remove !== null && list.indexOf(remove) >= 0)
					list.splice(list.indexOf(remove), 1);
				if (add !== null && list.indexOf(add) < 0) {
					if (list.length >= MAX_IMAGES)
						throw { code: "toolarge", message: "too many senders", reason: "images" };
					list.push(add);
				}
				cur.images_always = list;
				cur.v = 1;
				return cur;
			});
		}
		if (args.signature !== undefined) {
			try {
				lib("mailsig").set(rq.usernum, args.signature, rq.env.data_dir);
			} catch (e) {
				if (e !== null && typeof e === "object" && e.code === "invalid")
					throw invalid("signature");	// not reached: signature_ok
				throw e;
			}
		}
		if (fwd !== null) {
			var u = rq.user;
			if (String(u.netmail || "") !== fwd.address)
				u.netmail = fwd.address;
			if (fwd.enabled)
				u.settings |= UD.USER_NETMAIL;
			else
				u.settings &= ~UD.USER_NETMAIL;
		}
		return settings_get(ctx, {}, rq);
	}

	var DRAFT_ID = { type: "string", required: true, max: 16, test: valid_draft_id };
	var EMAIL_ARG = { type: "string", max: 254 };

	// Message ids for the write calls: 1 to MAX_IDS, no duplicates
	var IDS ={ type: "list", required: true, min: 1, max: MAX_IDS, unique: true,
		of: { type: "string", max: 11, test: valid_msg_id } };
	var MOVE_IDS = { type: "list", required: true, min: 1, max: MAX_MOVE_IDS, unique: true,
		of: IDS.of };

	var methods = {
		"session.get": { auth: false, args: {
			no_webv4: { type: "boolean" }
		}, run: session_get },
		"session.login": { auth: false, args: {
			user: { type: "string", required: true, max: MAX_CREDENTIAL },
			password: { type: "string", required: true, max: MAX_CREDENTIAL },
			keep: { type: "boolean" }
		}, run: session_login },
		"session.logout": { auth: true, ctx: false, args: {}, run: session_logout },
		"folder.list": { auth: true, args: {}, run: folder_list },
		"message.list": { auth: true, args: {
			folder: { type: "string", required: true, max: MAX_TAG + 2, test: valid_folder },
			offset: { type: "int", min: 0, max: 10000000 },
			limit: { type: "int", min: 1, max: 100 },
			search: { type: "string" }
		}, run: message_list },
		"message.get": { auth: true, args: {
			id: { type: "string", required: true, max: 11, test: valid_msg_id },
			mark_read: { type: "boolean" },
			head: { type: "boolean" },
			avatar: { type: "boolean" }
		}, run: message_get },
		"changes": { auth: true, args: {
			state: { type: "string", required: true }
		}, run: changes },
		"message.set": { auth: true, write: true, args: {
			ids: IDS,
			read: { type: "boolean" },
			starred: { type: "boolean" },
			spam: { type: "boolean" }
		}, check: set_check, run: message_set },
		"message.move": { auth: true, write: true, args: {
			ids: MOVE_IDS,
			folder: { type: "string", required: true, max: MAX_TAG + 2, test: valid_move_target }
		}, run: message_move },
		"message.delete": { auth: true, write: true, args: { ids: MOVE_IDS }, run: message_delete },
		"message.undelete": { auth: true, write: true, args: { ids: MOVE_IDS }, run: message_undelete },
		"trash.empty": { auth: true, write: true, args: {}, run: trash_empty },
		"folder.create": { auth: true, write: true, args: {
			name: { type: "string", required: true, max: MAX_NAME }
		}, run: folder_create },
		"folder.rename": { auth: true, write: true, args: {
			id: { type: "string", required: true, max: MAX_TAG + 2, test: valid_user_folder },
			name: { type: "string", required: true, max: MAX_NAME }
		}, run: folder_rename },
		"folder.delete": { auth: true, write: true, args: {
			id: { type: "string", required: true, max: MAX_TAG + 2, test: valid_user_folder }
		}, run: folder_delete },
		"message.original": { auth: true, args: {
			id: { type: "string", required: true, max: 11, test: valid_msg_id }
		}, run: message_original },
		// Drafts, sending, contacts and settings: no mailbox is opened
		// unless the call needs one (lib_ctx)
		"draft.create": { auth: true, ctx: false, write: true, args: {
			mode: { type: "string", required: true, max: 8, test: valid_mode },
			source_id: { type: "string", max: 11, test: valid_msg_id }
		}, check: create_check, run: draft_create },
		"draft.get": { auth: true, ctx: false, args: { id: DRAFT_ID }, run: draft_get },
		"draft.list": { auth: true, ctx: false, args: {}, run: draft_list },
		"draft.save": { auth: true, ctx: false, write: true, args: {
			id: DRAFT_ID,
			rev: { type: "int", required: true, min: 0 },
			to: { type: "string", max: MAX_ADDR_FIELD },
			cc: { type: "string", max: MAX_ADDR_FIELD },
			bcc: { type: "string", max: MAX_ADDR_FIELD },
			subject: { type: "string", max: MAX_SUBJECT_IN },
			// the body's limit is max_body_bytes, which the library checks
			// (toolarge, reason "body"); the request size bounds it here
			body: { type: "string", max: HOST_MAX_POST },
			from_domain: { type: "string", max: 253 },
			from_local: { type: "string", max: 64 },
			priority: { type: "string", max: 4, test: function (v) {
				return PRIORITIES.indexOf(v) >= 0;
			} }
		}, run: draft_save },
		"draft.delete": { auth: true, ctx: false, write: true, args: { id: DRAFT_ID }, run: draft_delete },
		"draft.attach": { auth: true, ctx: false, write: true, args: {
			id: DRAFT_ID,
			upload: { type: "string", required: true, max: 64, test: function (v) {
				return UPLOAD_RE.test(v);
			} },
			name: { type: "string", required: true, max: MAX_UPLOAD_NAME },
			type: { type: "string", max: MAX_UPLOAD_TYPE },
			offset: { type: "int", required: true, min: 0 },
			total: { type: "int", required: true, min: 0 },
			b64: { type: "string", required: true, max: 4 * Math.ceil(ATTACH_CHUNK / 3) }
		}, run: draft_attach },
		"draft.detach": { auth: true, ctx: false, write: true, args: {
			id: DRAFT_ID,
			attachment: { type: "string", required: true, max: 10, test: function (v) {
				return ATT_ID_RE.test(v);
			} }
		}, run: draft_detach },
		"draft.send": { auth: true, ctx: false, write: true, args: {
			id: DRAFT_ID,
			key: { type: "string", required: true, max: 64, test: function (v) {
				return SEND_KEY_RE.test(v);
			} },
			confirm_resend: { type: "boolean" }
		}, run: draft_send },
		"contact.list": { auth: true, ctx: false, args: {}, run: contact_list },
		"contact.set": { auth: true, ctx: false, write: true, args: {
			id: { type: "string", max: 10, test: function (v) {
				return CONTACT_ID_RE.test(v);
			} },
			name: { type: "string", max: 1024 },
			addrs: { type: "list", required: true, min: 1, max: 10, of: { type: "string", max: 1024 } },
			note: { type: "string", max: 4096 }
		}, run: contact_set },
		"contact.delete": { auth: true, ctx: false, write: true, args: {
			id: { type: "string", required: true, max: 10, test: function (v) {
				return CONTACT_ID_RE.test(v);
			} }
		}, run: contact_delete },
		"contact.suggest": { auth: true, ctx: false, args: {
			q: { type: "string", required: true, max: 254 },
			limit: { type: "int", min: 1, max: 50 }
		}, run: contact_suggest },
		"settings.get": { auth: true, ctx: false, args: {}, run: settings_get },
		"settings.set": { auth: true, ctx: false, write: true, args: {
			display_name: { type: "string", max: 1024 },
			from_domain: { type: "string", max: 253 },
			from_local: { type: "string", max: 64 },
			theme: { type: "string", max: 6, test: function (v) {
				return THEMES.indexOf(v) >= 0;
			} },
			images_always: { type: "list", min: 0, max: MAX_IMAGES, of: EMAIL_ARG },
			images_add: EMAIL_ARG,
			images_remove: EMAIL_ARG,
			tag_block: { type: "string", max: 254 },
			tag_unblock: { type: "string", max: 1000 },
			forward_enabled: { type: "boolean" },
			forward_address: { type: "string", max: 254 },
			signature: { type: "string", max: MAX_SIGNATURE_IN, nullable: true }
		}, check: settings_check, run: settings_set }
	};

	// ---- The request body ----

	// A string of UTF-16 code units as UTF-8 bytes (pairs combined); throws
	// URIError for a lone surrogate
	function to_utf8(s)
	{
		if (!/[^\x00-\x7f]/.test(s))
			return s;
		return unescape(encodeURIComponent(s));
	}

	// v with every string (keys too) converted by to_utf8; throws for a
	// lone surrogate or nesting deeper than MAX_DEPTH
	function convert(v, depth)
	{
		if (typeof v === "string")
			return to_utf8(v);
		if (v === null || typeof v !== "object")
			return v;
		if (depth >= MAX_DEPTH)
			throw new RangeError("nested too deep");
		var i;
		if (v instanceof Array) {
			var a = [];
			for (i = 0; i < v.length; i++)
				a.push(convert(v[i], depth + 1));
			return a;
		}
		var o = {};
		var keys = Object.keys(v);
		for (i = 0; i < keys.length; i++) {
			// defineProperty: assigning "__proto__" would set the prototype
			// instead of making a property (check_args then refuses it)
			Object.defineProperty(o, to_utf8(keys[i]), { value: convert(v[keys[i]], depth + 1),
				enumerable: true, writable: true, configurable: true });
		}
		return o;
	}

	// The request from a POST body (bytes, as the web server gives them), or
	// null if it is not valid UTF-8 or not JSON.  Text arrives two ways,
	// as raw UTF-8 and as \u escapes; both end up the same: the bytes are
	// decoded to UTF-16, parsed, and every string (keys too) is encoded
	// back to UTF-8 bytes, which is what the libraries work with.  A lone
	// surrogate (an escape that is half a pair) is refused.
	function parse_body(text)
	{
		if (typeof text !== "string" || text === "")
			return null;
		try {
			if (/[\x80-\xff]/.test(text)) {
				// str_is_utf8 refuses encoded surrogates (which this engine's
				// decodeURIComponent accepts) and overlong forms;
				// decodeURIComponent refuses what is above U+10FFFF
				if (/[^\x00-\xff]/.test(text) || !str_is_utf8(text))
					return null;
				text = decodeURIComponent(escape(text));
			}
			return convert(JSON.parse(text), 0);
		} catch (e) {
			return null;
		}
	}

	// ---- The dispatcher ----

	function well_formed(request)
	{
		if (request === null || typeof request !== "object" || request instanceof Array)
			return false;
		var calls = request.calls;
		if (!(calls instanceof Array) || calls.length > MAX_CALLS)
			return false;
		for (var i = 0; i < calls.length; i++) {
			if (!(calls[i] instanceof Array) || calls[i].length < 1 || calls[i].length > 3)
				return false;
		}
		if (request.as !== undefined && !valid_as(request.as))
			return false;
		return true;
	}

	function valid_as(v)
	{
		return typeof v === "number" && v >= 1 && v <= 0x7fffffff && Math.floor(v) === v;
	}

	// The request holds a call of a write method
	function has_write(calls)
	{
		for (var i = 0; i < calls.length; i++) {
			var name = calls[i][0];
			if (typeof name === "string" && own(methods, name) && methods[name].write === true)
				return true;
		}
		return false;
	}

	function call_id(v)
	{
		if (typeof v === "string" && v.length <= MAX_CALL_ID && /^[\x20-\x7e]*$/.test(v))
			return v;
		if (typeof v === "number" && isFinite(v))
			return v;
		return null;
	}

	function get_ctx(rq)
	{
		if (!rq.ctx_tried) {
			rq.ctx_tried = true;
			try {
				rq.ctx = rq.env.make_ctx(rq.usernum);
				if (rq.ctx === null || typeof rq.ctx !== "object" || !rq.ctx.store)
					throw new Error("make_ctx returned no store");
			} catch (e) {
				rq.ctx_error = e;
			}
		}
		if (rq.ctx_error !== null)
			throw rq.ctx_error;
		if (rq.ctx.usernum !== rq.usernum)
			throw auth_error();	// the user changed within this request
		return rq.ctx;
	}

	function run_call(rq, call)
	{
		var name = call[0];
		// The name pattern also keeps out "__proto__", which this engine's
		// hasOwnProperty() reports as an own property of every object
		if (typeof name !== "string" || !METHOD_RE.test(name) || !own(methods, name))
			return failure(rq, "invalid");
		var m = methods[name];
		if (m === null || typeof m !== "object" || typeof m.run !== "function")
			return failure(rq, "invalid");
		if (m.auth && rq.usernum === 0)
			return failure(rq, "auth");
		// A request the client made on its own never changes mail
		if (m.write && rq.background)
			return failure(rq, "forbidden");
		var args = check_args(m.args || {}, call.length > 1 ? call[1] : undefined);
		if (args === null || (typeof m.check === "function" && m.check(args) !== true))
			return failure(rq, "invalid");
		try {
			var ctx = null;
			if (m.auth && m.ctx !== false) {
				ctx = get_ctx(rq);
				rq.state_fresh = false;
			}
			var out = m.run(ctx, args, rq);
			var result = { ok: true };
			if (out !== null && typeof out === "object") {
				for (var k in out) {
					if (own(out, k) && k !== "ok")
						result[k] = out[k];
				}
			}
			return result;
		} catch (e) {
			if (is_coded(e)) {
				if (e.code === "failed" || e.code === "conflict")
					Api.log(LOG_WARNING, "webmail api: user " + rq.usernum + ": " + name + ": "
						+ describe(e));
				return coded_failure(rq, e, name);
			}
			Api.log(LOG_ERR, "webmail api: user " + rq.usernum + ": " + name + ": " + describe(e));
			return failure(rq, "internal");
		}
	}

	function new_request(env)
	{
		return { env: env, config: env && env.config ? env.config : {}, usernum: 0, user: null,
			cookie: null, via: null, webv4: null, signed_out: false, logged_out: false, background: false,
			ctx: null, ctx_tried: false, ctx_error: null,
			available: null, loaded: {}, strings_sent: false, state: null, state_fresh: false,
			avatar_lib: undefined, sys: null };
	}

	function only_changes(calls)
	{
		if (calls.length === 0)
			return false;
		for (var i = 0; i < calls.length; i++)
			if (calls[i][0] !== "changes")
				return false;
		return true;
	}

	function dispatch(env, request, meta)
	{
		var rq = null;
		try {
			rq = new_request(env);
			if (!well_formed(request))
				return { error: "invalid", message: text(rq, "invalid"), results: [], state: "", usernum: 0 };
			if (meta && !body_fits(rq.config, meta.length, request.calls))
				return { error: "toolarge", message: text(rq, "toolarge"), results: [], state: "", usernum: 0 };
			rq.background = request.background === true;
			var calls = request.calls;
			var results = [];
			var i;
			var gate = null;
			if (env.has_header !== true)
				gate = "forbidden";
			else if (env.secure !== true && rq.config.require_tls !== false)
				gate = "https";
			if (gate === null) {
				try {
					var s = session_user(env, request.background !== true && !only_changes(calls));
					rq.usernum = s.usernum;
					rq.user = s.user;
					rq.cookie = s.cookie;
					if (s.clear) {
						set_cookie(rq, "", false);
						rq.signed_out = true;
					}
				} catch (e) {
					Api.log(LOG_ERR, "webmail api: checking the session: " + describe(e));
					gate = "internal";
				}
			}
			if (gate === null) {
				if (request.as !== undefined && rq.usernum !== 0 && request.as !== rq.usernum)
					gate = "stale";
				else if (request.as === undefined && rq.usernum !== 0 && has_write(calls))
					gate = "invalid";
			}
			for (i = 0; i < calls.length; i++) {
				var id = call_id(calls[i].length > 2 ? calls[i][2] : null);
				results.push([id, gate !== null ? failure(rq, gate) : run_call(rq, calls[i])]);
			}
			var state = "";
			if (rq.ctx !== null && rq.ctx_error === null && rq.usernum !== 0
				&& rq.ctx.usernum === rq.usernum) {
				if (rq.state_fresh)
					state = rq.state;
				else {
					try {
						state = rq.ctx.store.state();
					} catch (e) {
						Api.log(LOG_WARNING, "webmail api: user " + rq.usernum + ": state: " + describe(e));
					}
				}
			}
			// signed_out only when the request did not end signed in (a
			// sign-in later in the same request cancels it)
			return { results: results, state: state, signed_out: rq.signed_out && rq.usernum === 0,
				usernum: rq.usernum };
		} catch (e) {
			Api.log(LOG_ERR, "webmail api: dispatch: " + describe(e));
			var msg = FALLBACK.internal;
			try {
				msg = text(rq || new_request({}), "internal");
			} catch (x) {
			}
			return { error: "internal", message: msg, results: [], state: "", usernum: 0 };
		} finally {
			if (rq !== null && rq.ctx !== null) {
				try {
					if (typeof rq.ctx.close === "function")
						rq.ctx.close();
					else if (rq.ctx.store && typeof rq.ctx.store.close === "function")
						rq.ctx.store.close();
				} catch (e) {
					Api.log(LOG_WARNING, "webmail api: closing the mailbox: " + describe(e));
				}
			}
		}
	}

	// ---- Helpers for the shims ----

	// A make_ctx for env: opts { config, get_user(usernum), userdata(usernum),
	// Store (the constructor) or load_store() (returns it; called once, on
	// the first make_ctx, so a request that opens no mailbox never loads
	// the mailbox library), shared_path (tests; default the "mail" base),
	// data_dir (tests; default system.data_dir) }
	function context_factory(opts)
	{
		var Store = opts.Store || null;
		return function (usernum) {
			var u = user_of(opts, usernum);
			if (u === null)
				throw auth_error();
			if (Store === null)
				Store = opts.load_store();
			var store = new Store({ usernum: usernum, alias: u.alias, name: u.name,
				shared_path: opts.shared_path, data_dir: opts.data_dir,
				options: { max_text_part_bytes: opts.config.max_text_part_bytes } });
			store.open();
			return { usernum: usernum, alias: u.alias, name: u.name, user: u, store: store,
				userdata: opts.userdata(usernum), config: opts.config,
				close: function () {
					store.close();
				} };
		};
	}

	// The user's web client directory
	function userdata_dir(data_dir, usernum)
	{
		return data_dir + format("user/%04u/webmail/", usernum);
	}

	// The app's base path for the cookie, from the request's virtual path
	// ("/mail/api.ssjs" -> "/mail/"); "/" when it is not usable
	function base_path(vpath)
	{
		if (typeof vpath !== "string")
			return "/";
		var i = vpath.lastIndexOf('/');
		if (i < 0)
			return "/";
		var p = vpath.substr(0, i + 1);
		if (p.length > 256 || !/^\/(?:[\x21-\x3a\x3c-\x7e]*\/)?$/.test(p))
			return "/";
		return p;
	}

	var Api = {
		UD: UD,		// for the test that pins it to userdefs.js
		// The libraries this one loaded, for callers that need them too
		Session: Session,
		methods: methods,
		dispatch: dispatch,
		parse_body: parse_body,
		session_user: session_user,
		eligible: eligible,
		sending_domains: sending_domains,
		powered_by: powered_by,
		context_factory: context_factory,
		body_check: body_check,
		body_allowance: body_allowance,
		max_body: max_body,
		large_gate: large_gate,
		client_limits: client_limits,
		lib: lib,
		BASE_BODY: BASE_BODY,
		HOST_MAX_POST: HOST_MAX_POST,
		ATTACH_CHUNK: ATTACH_CHUNK,
		MAX_ATTACHMENTS: MAX_ATTACHMENTS,
		MAX_ADDR_FIELD: MAX_ADDR_FIELD,
		SEND_LIVE: SEND_LIVE,
		userdata_dir: userdata_dir,
		base_path: base_path,
		log: function (level, text) {
			log(level, text);
		}
	};
	// The locale library, loaded when first asked for
	Object.defineProperty(Api, "Locale", { get: Locale, enumerable: true });

	return Api;
})();

Api;
