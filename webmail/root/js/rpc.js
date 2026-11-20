// Calls to the server (api.ssjs).
//
// rpc.call(method, args, {retry, alone, background, keepalive})
//                          a Promise of the method's result ({ok: true, ...});
//                          rejects with {error, message}.  Calls made in the
//                          same tick go to the server as one request, except
//                          a call with alone: true, which always goes in a
//                          request of its own (the poll: the server does not
//                          count a request holding only "changes" as
//                          activity), and calls with background: true (made
//                          by the app on its own, not for the user), which
//                          go in requests of their own marked background,
//                          so the server does not count them as activity
//                          either.  keepalive: true (implies alone) asks
//                          the browser to finish the request even when
//                          the page is closing (fetch's keepalive: the
//                          body must be under 64 KB)
// rpc.optimistic(apply, undo, method, args, {quiet, onUnsure})
//                          runs apply() at once and sends the call.  When
//                          the call fails as a whole, undo(failed) runs
//                          with every id of args.ids (an empty list when
//                          there is none) and the error is shown.  When it
//                          succeeds with per-id results (results: [{id, ok}
//                          or {id, error}], the write methods' shape),
//                          undo(failed, codes) runs with only the ids that
//                          failed (codes: a Map of their error codes) and
//                          a notice says how many (failureText; none
//                          with quiet: the caller says it).  onUnsure() is
//                          called when the server could not be reached for
//                          all or part of it, so it may have been done.  A
//                          Promise of the result, or of null after a
//                          failure of the whole call.
// rpc.failureText(results) the notice text for the failed ids of per-id
//                          results ("" when none failed): how many, and
//                          the reason when they all failed for the same one
// rpc.bulk(method, args, opts)
//                          rpc.call() for a method with an id list
//                          (CHUNKED: message.move, message.delete and
//                          message.undelete take 50 ids per call,
//                          message.set 200) given more ids than the server
//                          takes: the ids go in consecutive calls of at
//                          most that many,
//                          one after another (never at once), and the
//                          caller gets one result, {ok: true, results (in
//                          the order of args.ids), state (the last
//                          chunk's)}.  When a chunk fails as a whole, its
//                          ids are reported with that error and the ids
//                          after it as "failed", without being sent; when
//                          the first chunk fails, nothing was done and the
//                          Promise rejects with its error.  unsure: true in
//                          the result when a chunk could not reach the
//                          server.  Any other call
//                          goes to rpc.call() as it is.  rpc.optimistic
//                          sends through it.
// rpc.user                 the signed-in user's number, set by the app (0:
//                          none); a reply naming another user (a non-zero
//                          usernum that differs: someone else signed in in
//                          this browser since) calls hooks.user(number)
// rpc.state                the last state token the server sent
// rpc.stateOf(result)      the state token of the reply that carried result
// rpc.retryNow()           sends what waits for the connection at once
// rpc.reset()              forgets the state token and every waiting call
//                          (they are rejected), stops retrying, forgets the
//                          user (at sign-out)
//
// A rejection is {error, message}, plus unsure: true when the server could
// not be reached for a call that is not sent again (it may have been done),
// and the server's reason, rev, received, index, entry and count when the
// call's result has them.
// rpc.hooks                set by the app:
//   auth()                 a call was refused for want of a session
//   signedOut()            the browser's session is no longer valid
//   offline(on)            calls wait for the server / no longer
//   state(token)           a new state token arrived
//   user(number)           the server answered as another user
//
// When the server cannot be reached, it is not known whether a request
// that was sent got there.  A call may then be sent again only when doing
// it twice is harmless: retry defaults to true for the methods in
// READ_ONLY and to false for every other method (session.login and
// session.logout included).  The methods in WRITE change mail: the server
// refuses them in a background request, so call() rejects a write with
// background: true at once ("invalid") instead of sending it.  A call
// that may not be retried is rejected at once with "failed" and a text
// saying to check whether it went through; the others wait and are sent
// again after 2, 4 and 8 seconds, then every 15 seconds, until a request
// gets through.  At most one "changes" poll waits (a newer one takes the
// older one's place and answers both), and at most MAX_WAITING calls wait
// (beyond that the oldest is rejected).

import { t } from "./i18n.js";
import { notice } from "./notice.js";

const ENDPOINT = "api.ssjs";	// relative to the page, so the app works at any base path
const MAX_CALLS = 32;		// per request, as the server allows
const MAX_WAITING = 100;
const BACKOFF = [2000, 4000, 8000, 15000];
const UNREACHABLE = [502, 503, 504];	// a proxy in front could not reach the server

// The methods that are safe to repeat: sending one twice has the same
// effect as sending it once (folder.list may purge expired Trash,
// message.get may mark a message read, session.get may sign a webv4 user
// in; draft.list may remove stale empty drafts; draft.list and
// folder.list may complete an interrupted send, saving copies and
// charging credits, which the server's recovery does only once however
// often it runs; again, each changes nothing more).  The ONE list: every
// method not named here is not retried by default.
const READ_ONLY = ["session.get", "folder.list", "message.list", "message.get", "message.original", "changes",
	"draft.get", "draft.list", "contact.list", "contact.suggest", "settings.get"];

// The methods that change mail, drafts, contacts or settings (never
// retried, never in a background request)
const WRITE = ["message.set", "message.move", "message.delete", "message.undelete", "trash.empty",
	"folder.create", "folder.rename", "folder.delete", "draft.create", "draft.save", "draft.delete",
	"draft.attach", "draft.detach", "draft.send", "contact.set", "contact.delete", "settings.set"];

// The fields of a failed call's result copied onto the rejection
const ERROR_FIELDS = ["rev", "received", "index", "entry", "count"];

// The methods with an id list, and how many ids the server takes per call
const CHUNKED = { "message.move": 50, "message.delete": 50, "message.undelete": 50, "message.set": 200 };

const states = new WeakMap();	// result -> its reply's state token
let queue = [];
let scheduled = false;
let nextId = 1;
let waiting = [];
let offline = false;
let attempt = 0;
let timer = 0;

export const rpc = {
	state: "",
	user: 0,
	hooks: { auth: null, signedOut: null, offline: null, state: null, user: null },
	call,
	stateOf: (r) => (r && typeof r === "object" ? states.get(r) : undefined),
	optimistic,
	bulk,
	failureText,
	retryNow,
	reset
};

function hook(name, ...args) {
	const f = rpc.hooks[name];
	if (typeof f === "function")
		f(...args);
}

function failure(code, message) {
	return { error: code, message: message || t("errors." + code) };
}

function notReached() {
	return Object.assign(failure("failed", t("errors.unreachable")), { unsure: true });
}

function call(method, args, opts) {
	if (!opts || typeof opts !== "object")
		opts = {};
	const retry = typeof opts.retry === "boolean" ? opts.retry : READ_ONLY.includes(method);
	const keepalive = opts.keepalive === true;
	const alone = opts.alone === true || keepalive;
	const background = opts.background === true;
	if (background && WRITE.includes(method))
		return Promise.reject(failure("invalid"));
	return new Promise((resolve, reject) => {
		queue.push({ method, args: args === undefined ? null : args, id: nextId++, retry, alone, background, keepalive,
			resolve, reject });
		if (!scheduled) {
			scheduled = true;
			queueMicrotask(flush);
		}
	});
}

function flush() {
	scheduled = false;
	const calls = queue;
	queue = [];
	if (offline) {
		wait(calls);	// not sent yet, so any of them may go later
		return;
	}
	sendAll(calls);
}

// A call marked alone goes by itself; background calls never share a
// request with the user's calls
function sendAll(calls) {
	const user = [];
	const bg = [];
	for (const c of calls) {
		if (c.alone)
			send([c]);
		else
			(c.background ? bg : user).push(c);
	}
	for (const group of [user, bg]) {
		for (let i = 0; i < group.length; i += MAX_CALLS)
			send(group.slice(i, i + MAX_CALLS));
	}
}

// Add calls to the waiting list: one "changes" poll at most, MAX_WAITING
// calls at most
function wait(calls) {
	for (const c of calls) {
		if (c.method === "changes") {
			const i = waiting.findIndex((w) => w.method === "changes");
			if (i >= 0) {
				const old = waiting.splice(i, 1)[0];
				const resolve = c.resolve;
				const reject = c.reject;
				c.resolve = (r) => {
					old.resolve(r);
					resolve(r);
				};
				c.reject = (e) => {
					old.reject(e);
					reject(e);
				};
			}
		}
		waiting.push(c);
	}
	while (waiting.length > MAX_WAITING)
		waiting.shift().reject(notReached());
}

// The request body: the calls, background when they are, and the user this
// page is for ("as": the server refuses the whole request when another user
// is signed in, and a write without it)
function request(batch) {
	const body = { calls: batch.map((c) => [c.method, c.args, c.id]) };
	if (batch[0].background)
		body.background = true;
	if (rpc.user > 0)
		body.as = rpc.user;
	return body;
}

function post(batch) {
	return fetch(ENDPOINT, {
		method: "POST",
		credentials: "same-origin",
		cache: "no-store",
		headers: { "Content-Type": "application/json", "X-Webmail": "1" },
		keepalive: batch.length === 1 && batch[0].keepalive === true,
		body: JSON.stringify(request(batch))
	});
}

async function send(batch) {
	let resp;
	try {
		resp = await post(batch);
	} catch (e) {
		// A browser that reuses a connection the server has just closed
		// fails the request without sending it (Safari does not send a
		// POST again by itself).  Requests that are safe to repeat get one
		// more try at once, on a new connection, before the server counts
		// as out of reach.
		if (!batch.every((c) => c.retry)) {
			unreachable(batch);
			return;
		}
		try {
			resp = await post(batch);
		} catch (e2) {
			unreachable(batch);
			return;
		}
	}
	if (UNREACHABLE.includes(resp.status)) {
		unreachable(batch);
		return;
	}
	reachable();

	let body = null;
	try {
		body = await resp.json();
	} catch (e) {
		body = null;
	}
	if (!resp.ok || body === null || typeof body !== "object") {
		const code = resp.status === 413 ? "toolarge" : "internal";
		for (const c of batch)
			c.reject(failure(code));
		return;
	}
	if (typeof body.state === "string" && body.state !== "" && body.state !== rpc.state) {
		rpc.state = body.state;
		hook("state", body.state);
	}
	// A sign-in or sign-out in the batch changes the session itself
	const session = batch.some((c) => c.method === "session.login" || c.method === "session.logout");
	// Another user than the one this page shows (0: the reply does not say)
	if (!session && rpc.user > 0 && typeof body.usernum === "number" && body.usernum > 0 && body.usernum !== rpc.user) {
		for (const c of batch)
			c.reject(failure("auth"));
		hook("user", body.usernum);
		return;
	}
	if (body.signed_out === true && !session)
		hook("signedOut");
	// Refused as another user's page (see request()): reload as the user
	// signed in now
	const stale = body.error === "stale" || (Array.isArray(body.results)
		&& body.results.some((r) => Array.isArray(r) && r[1] && r[1].error === "stale"));
	if (stale && !session) {
		for (const c of batch)
			c.reject(failure("stale"));
		hook("user", body.usernum);
		return;
	}
	if (typeof body.error === "string") {
		for (const c of batch)
			c.reject(failure(body.error, body.message));
		// A top-level "auth" (a large request refused before it was read)
		// is handled as a per-call one
		if (body.error === "auth" && !session && body.signed_out !== true)
			hook("auth");
		return;
	}
	const results = new Map();
	if (Array.isArray(body.results)) {
		for (const r of body.results) {
			if (Array.isArray(r) && r.length === 2)
				results.set(r[0], r[1]);
		}
	}
	let auth = false;
	for (const c of batch) {
		const r = results.get(c.id);
		if (r && r.ok === true) {
			if (typeof body.state === "string")
				states.set(r, body.state);
			c.resolve(r);
			continue;
		}
		const err = r && typeof r.error === "string" ? failure(r.error, r.message) : failure("internal");
		if (r && typeof r.reason === "string")
			err.reason = r.reason;	// why it was refused (a fixed word)
		// The numbers and the entry the server passes with some errors
		// (a draft's rev, an upload's received bytes, a recipient's index
		// and entry, a limit's count)
		for (const k of ERROR_FIELDS) {
			if (r && (typeof r[k] === "number" || (k === "entry" && typeof r[k] === "string")))
				err[k] = r[k];
		}
		if (err.error === "auth" && !session)
			auth = true;
		c.reject(err);
	}
	if (auth && body.signed_out !== true)
		hook("auth");
}

// The request may or may not have reached the server
function unreachable(batch) {
	const again = [];
	for (const c of batch) {
		if (c.retry)
			again.push(c);
		else
			c.reject(notReached());
	}
	wait(again);
	if (waiting.length === 0)
		return;
	if (!offline) {
		offline = true;
		attempt = 0;
		hook("offline", true);
	}
	if (!timer) {
		timer = setTimeout(resend, BACKOFF[Math.min(attempt, BACKOFF.length - 1)]);
		attempt++;
	}
}

function reachable() {
	if (!offline)
		return;
	offline = false;
	attempt = 0;
	clearTimeout(timer);
	timer = 0;
	hook("offline", false);
	const calls = waiting;
	waiting = [];
	sendAll(calls);
}

function resend() {
	timer = 0;
	const calls = waiting;
	waiting = [];
	sendAll(calls);
}

function retryNow() {
	if (!offline)
		return;
	clearTimeout(timer);
	timer = 0;
	resend();
}

function reset() {
	clearTimeout(timer);
	timer = 0;
	attempt = 0;
	const dropped = waiting;
	waiting = [];
	rpc.state = "";
	rpc.user = 0;
	if (offline) {
		offline = false;
		hook("offline", false);
	}
	for (const c of dropped)
		c.reject(failure("failed"));
}

window.addEventListener("online", retryNow);

async function bulk(method, args, opts) {
	const ids = args && Array.isArray(args.ids) ? args.ids : null;
	const size = Object.prototype.hasOwnProperty.call(CHUNKED, method) ? CHUNKED[method] : 0;
	if (size === 0 || ids === null || ids.length <= size)
		return call(method, args, opts);
	const merged = { ok: true, results: [], state: "" };
	for (let at = 0; at < ids.length; at += size) {
		const part = ids.slice(at, at + size);
		let r;
		try {
			r = await call(method, { ...args, ids: part }, opts);
		} catch (err) {
			if (at === 0)
				throw err;	// nothing was done
			const code = err && typeof err.error === "string" ? err.error : "failed";
			if (err && err.unsure)
				merged.unsure = true;
			for (const id of part)
				merged.results.push({ id, error: code });
			for (const id of ids.slice(at + size))
				merged.results.push({ id, error: "failed" });
			break;
		}
		if (Array.isArray(r.results))
			merged.results.push(...r.results);
		const st = states.get(r);
		if (typeof st === "string")
			merged.state = st;
		else if (typeof r.state === "string")
			merged.state = r.state;
	}
	states.set(merged, merged.state);
	return merged;
}

// The ids a call was about (args.ids), for undoing all of them
function idsOf(args) {
	return args && Array.isArray(args.ids) ? args.ids.slice() : [];
}

function failureText(results) {
	const failed = Array.isArray(results) ? results.filter((x) => x && x.ok !== true) : [];
	if (failed.length === 0)
		return "";
	const code = failed[0].error;
	const same = typeof code === "string" && failed.every((x) => x.error === code);
	if (results.length === 1)	// one message: not "of the selected"
		return t("errors." + (typeof code === "string" ? code : "failed"));
	if (!same)
		return t("errors.partial", { count: failed.length });
	return t("errors.partial_reason", { count: failed.length, reason: t("errors." + code) });
}

function optimistic(apply, undo, method, args, opts) {
	const o = opts && typeof opts === "object" ? opts : {};
	const unsure = () => {
		if (typeof o.onUnsure === "function")
			o.onUnsure();
	};
	apply();
	return bulk(method, args).then((r) => {
		if (r && r.unsure)
			unsure();
		const bad = r && Array.isArray(r.results) ? r.results.filter((x) => x && x.ok !== true) : [];
		const failed = bad.map((x) => x.id);
		if (failed.length > 0) {
			try {
				undo(failed, new Map(bad.map((x) => [x.id, x.error])));
			} finally {
				if (o.quiet !== true)
					notice(failureText(r.results));
			}
		}
		return r;
	}, (err) => {
		if (err && err.unsure)
			unsure();
		try {
			undo(idsOf(args));
		} finally {
			notice(err && err.message ? err.message : t("errors.failed"));
		}
		return null;
	});
}
