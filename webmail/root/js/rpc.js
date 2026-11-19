// Calls to the server (api.ssjs).
//
// rpc.call(method, args, {retry, alone, background})
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
//                          either.
// rpc.optimistic(apply, undo, method, args)
//                          runs apply() at once and sends the call; on
//                          failure runs undo() and shows the error.  A
//                          Promise of the result, or of null after a failure.
// rpc.state                the last state token the server sent
// rpc.stateOf(result)      the state token of the reply that carried result
// rpc.retryNow()           sends what waits for the connection at once
// rpc.reset()              forgets the state token and every waiting call
//                          (they are rejected), stops retrying (at sign-out)
// rpc.hooks                set by the app:
//   auth()                 a call was refused for want of a session
//   signedOut()            the browser's session is no longer valid
//   offline(on)            calls wait for the server / no longer
//   state(token)           a new state token arrived
//
// When the server cannot be reached, it is not known whether a request
// that was sent got there.  A call may then be sent again only when doing
// it twice is harmless: retry defaults to true for the methods in
// READ_ONLY and to false for every other method (session.login and
// session.logout included).  A call that may not be retried is rejected
// at once with "failed" and a text saying to check whether it went
// through; the others wait and are sent again after 2, 4 and 8 seconds,
// then every 15 seconds, until a request gets through.  At most one
// "changes" poll waits (a newer one takes the older one's place and
// answers both), and at most MAX_WAITING calls wait (beyond that the
// oldest is rejected).

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
// in; again, each changes nothing more).  The ONE list: every method not
// named here is not retried by default.
const READ_ONLY = ["session.get", "folder.list", "message.list", "message.get", "changes"];

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
	hooks: { auth: null, signedOut: null, offline: null, state: null },
	call,
	stateOf: (r) => (r && typeof r === "object" ? states.get(r) : undefined),
	optimistic,
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
	return failure("failed", t("errors.unreachable"));
}

function call(method, args, opts) {
	if (!opts || typeof opts !== "object")
		opts = {};
	const retry = typeof opts.retry === "boolean" ? opts.retry : READ_ONLY.includes(method);
	const alone = opts.alone === true;
	const background = opts.background === true;
	return new Promise((resolve, reject) => {
		queue.push({ method, args: args === undefined ? null : args, id: nextId++, retry, alone, background,
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

async function send(batch) {
	let resp;
	try {
		resp = await fetch(ENDPOINT, {
			method: "POST",
			credentials: "same-origin",
			cache: "no-store",
			headers: { "Content-Type": "application/json", "X-Webmail": "1" },
			body: JSON.stringify(batch[0].background
				? { calls: batch.map((c) => [c.method, c.args, c.id]), background: true }
				: { calls: batch.map((c) => [c.method, c.args, c.id]) })
		});
	} catch (e) {
		unreachable(batch);
		return;
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
	if (body.signed_out === true && !session)
		hook("signedOut");
	if (typeof body.error === "string") {
		for (const c of batch)
			c.reject(failure(body.error, body.message));
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
	if (offline) {
		offline = false;
		hook("offline", false);
	}
	for (const c of dropped)
		c.reject(failure("failed"));
}

window.addEventListener("online", retryNow);

function optimistic(apply, undo, method, args) {
	apply();
	return call(method, args).catch((err) => {
		try {
			undo();
		} finally {
			notice(err && err.message ? err.message : t("errors.failed"));
		}
		return null;
	});
}
