// Send: crashes, failures part-way, repeats and recovery
// (webmail/lib/send.js).  A crash is modeled by Send.hook throwing
// {crash: true} at a named point, which Send passes through untouched.
// Each case runs on its own; every failure is reported at the end.

load(js.exec_dir + "send.inc");

var failures = [];
var cases = 0;
var LIVE = Send.LIVE;

function run_case(name, fn)
{
	cases++;
	try {
		fn();
	} catch (e) {
		failures.push(name + ": " + (e && e.message ? e.message : JSON.stringify(e)));
	} finally {
		Send.hook = null;
	}
}

// fn must die (a simulated crash passes through Send untouched)
function dies(fn)
{
	var caught = null;
	try {
		fn();
	} catch (e) {
		caught = e;
	}
	Kit.assert(caught !== null && caught.crash === true, "expected a simulated crash, got " + JSON.stringify(caught));
}

function live_copies(subject)
{
	var n = 0;
	var all = all_with_subject(subject);
	for (var i = 0; i < all.length; i++)
		if (!all[i].deleted)
			n++;
	return n;
}

// Live (not deleted) copies of 'subject' to a recipient (address or user number)
function live_to(subject, who)
{
	var n = 0;
	var all = all_with_subject(subject);
	for (var i = 0; i < all.length; i++)
		if (!all[i].deleted && (all[i].to_net_addr === who || String(all[i].to_ext) === who))
			n++;
	return n;
}

// Each recipient has exactly one live copy, or the call reported partial
// (and no recipient has two)
function served_once(subject, who, res, label)
{
	var tag = subject + (label ? " (" + label + ")" : "");
	for (var i = 0; i < who.length; i++) {
		var n = live_to(subject, who[i]);
		Kit.assert(n <= 1, tag + ": " + who[i] + " has " + n + " copies");
		if (res && res.ok === true)
			Kit.eq(n, 1, tag + ": ok, so " + who[i] + " has its copy (" + JSON.stringify(all_with_subject(subject)) + ")");
	}
	if (!(res && res.ok === true))
		Kit.eq([res && res.code, res && res.reason], ["conflict", "partial"], tag + ": else partial");
}

// Run fn with the n-th MsgBase.remove_msg() call made to fail
function with_failing_removal(n, fn)
{
	var Real = MsgBase;
	var calls = 0;
	MsgBase = function (a, b, c) {
		var m = new Real(a, b, c);
		var rr = m.remove_msg;
		m.remove_msg = function () {
			if (++calls == n)
				return false;
			return rr.apply(m, arguments);
		};
		return m;
	};
	try {
		fn();
	} finally {
		MsgBase = Real;
	}
}

function recover_cb(ctx)
{
	return { recover: function (id) {
		Send.recover(ctx, id);
	} };
}

try {
	// The copy was delivered (and so deleted) before recovery
	run_case("delivered and deleted, recovered by a new key", function () {
		var K = context({ sys: { inet_cost: 10 }, user: { credits: 100 } });
		var id = new_draft(K, { to: "ext@example.org", subject: "c1a", body: "hello" });
		crash_at("delivered");
		dies(function () { Send.send(K, id, key()); });
		var copies = all_with_subject("c1a");
		Kit.eq(copies.length, 1, "one copy saved");
		mailserver_sends(copies[0].num);
		fails(function () { Send.send(K, id, key()); }, "conflict", "sending");	// a live claim
		K.clock = NOW + LIVE + 1;
		var r = Send.send(K, id, key());
		Kit.eq([r.ok, r.warning], [true, "recovered"], "completed, not sent again");
		Kit.eq(all_with_subject("c1a").length, 1, "still the one copy");
		Kit.eq(K.user.adjusted, [-10], "charged once");
		Kit.eq(K.user.sent, [[1, false]], "counted once");
		Kit.assert(!draft_exists(K, id), "draft finished");
		Kit.eq(sent_item(K, r.id).delivery.state, "sent", "Sent copy no longer $Unsent");
	});

	run_case("delivered and deleted, recovered by the same key, then replayed", function () {
		var K = context({ sys: { inet_cost: 10 }, user: { credits: 100 } });
		var id = new_draft(K, { to: "ext@example.org", subject: "c1b", body: "hello" });
		var k = key();
		crash_at("delivered");
		dies(function () { Send.send(K, id, k); });
		mailserver_sends(all_with_subject("c1b")[0].num);
		K.clock = NOW + LIVE + 1;
		var r = Send.send(K, id, k);
		Kit.eq(r.warning, "recovered", "recovered");
		Kit.eq(Send.send(K, id, k), r, "the same key replays");
		Kit.eq(all_with_subject("c1b").length, 1, "one copy");
		Kit.eq(K.user.adjusted, [-10], "charged once");
	});

	run_case("delivered and deleted, recovered by sweep", function () {
		var K = context({ sys: { inet_cost: 10 }, user: { credits: 100 } });
		var id = new_draft(K, { to: "ext@example.org", subject: "c1c", body: "hello" });
		crash_at("delivered");
		dies(function () { Send.send(K, id, key()); });
		mailserver_sends(all_with_subject("c1c")[0].num);
		K.clock = NOW + 3700;
		var sw = Drafts.sweep(K, K.clock, recover_cb(K));
		Kit.eq(sw.stale_sends, [id], "left to the send step");
		Kit.assert(!draft_exists(K, id), "not given back as a draft");
		Kit.eq(all_with_subject("c1c").length, 1, "one copy");
		Kit.eq(K.user.adjusted, [-10], "charged once");
		Kit.eq(Send.send(K, id, key()).ok, true, "a later send of the draft: the outcome");
		Kit.eq(all_with_subject("c1c").length, 1, "still one copy");
	});

	// Unique ids; one send never touches another's copies
	run_case("two sends in one second", function () {
		var A = context();
		var B = context();
		var ida = new_draft(A, { to: "ext@example.org, bob", subject: "c2A", body: "a" });
		var idb = new_draft(B, { to: "carl", subject: "c2B", body: "b" });
		var rb = null;
		Send.hook = function (point, kind) {
			if (point == "delivery" && kind == "internet") {
				Send.hook = null;
				rb = Send.send(B, idb, key());	// B runs to completion meanwhile
				fail_at("delivery", "local");
			}
		};
		fails(function () { Send.send(A, ida, key()); }, "failed");
		Kit.assert(rb !== null && rb.ok, "B sent");
		Kit.eq(live_copies("c2B"), 1, "B's copy untouched by A's cleanup");
		Kit.eq(live_copies("c2A"), 0, "A rolled back");
		Kit.assert(draft_exists(A, ida), "A's draft back");
		var r1 = Send.send(A, new_draft(A, { to: "bob", subject: "c2C", body: "c" }), key());
		var r2 = Send.send(B, new_draft(B, { to: "bob", subject: "c2D", body: "d" }), key());
		Kit.assert(r1.message_id !== r2.message_id, "distinct Message-IDs in one second");
	});

	run_case("a crashed send does not adopt another send's copies", function () {
		var A = context();
		var B = context();
		A.clock = B.clock = NOW + 100;
		var ida = new_draft(A, { to: "ext@example.org", subject: "c2E", body: "a" });
		var idb = new_draft(B, { to: "bob", subject: "c2F", body: "b" });
		var k = key();
		Send.hook = function (point) {
			if (point == "copy") {
				Send.hook = null;
				Send.send(B, idb, key());
				throw { crash: true };
			}
		};
		dies(function () { Send.send(A, ida, k); });
		A.clock = NOW + 100 + LIVE + 1;
		var r = Send.send(A, ida, k);
		Kit.eq(r.warning, undefined, "sent anew (nothing had been saved)");
		Kit.eq(live_copies("c2E"), 1, "A delivered once");
		Kit.eq(live_copies("c2F"), 1, "B untouched");
	});

	// A failure after a copy can no longer be recalled
	run_case("partial: the missing recipient completed", function () {
		var P = context();
		var id = new_draft(P, { to: "ext@example.org, bob", subject: "c3a", body: "x" });
		var k = key();
		Send.hook = function (point, kind) {
			if (point == "delivery" && kind == "local") {
				Send.hook = null;
				mailserver_sends(all_with_subject("c3a")[0].num);	// the Internet copy went out
				throw new Error("simulated failure of the local save");
			}
		};
		var r = Send.send(P, id, k);
		Kit.eq(r.ok, true, "completed");
		var all = all_with_subject("c3a");
		Kit.eq(all.length, 2, "one copy each: " + JSON.stringify(all));
		Kit.eq(Send.send(P, id, k), r, "replayed, nothing more");
		Kit.eq(all_with_subject("c3a").length, 2, "no duplicate");
	});

	run_case("partial: conflict, then a later recovery completes", function () {
		var P = context();
		var id = new_draft(P, { to: "ext@example.org, bob", subject: "c3b", body: "x" });
		var k = key();
		var marked = false;
		Send.hook = function (point, kind) {
			if (point == "delivery" && kind == "local") {
				if (!marked) {
					marked = true;
					mailserver_sends(all_with_subject("c3b")[0].num);
				}
				throw new Error("simulated failure of the local save");
			}
		};
		fails(function () { Send.send(P, id, k); }, "conflict", "partial");
		Send.hook = null;
		Kit.assert(!draft_exists(P, id), "the draft stays claimed");
		fails(function () { Send.send(P, id, key()); }, "conflict", "sending");
		P.clock = NOW + LIVE + 1;
		var r = Send.send(P, id, k);
		Kit.eq([r.ok, r.warning], [true, "recovered"], "completed");
		var all = all_with_subject("c3b");
		Kit.eq(all.length, 2, "one copy each: " + JSON.stringify(all));
	});

	// Delivering with no copy found: unknown
	run_case("unknown outcome: maybe_sent, resend only when confirmed", function () {
		var U = context();
		var id = new_draft(U, { to: "bob", subject: "c4", body: "x" });
		crash_at("delivery");
		dies(function () { Send.send(U, id, key()); });
		U.clock = NOW + LIVE + 1;
		fails(function () { Send.send(U, id, key()); }, "conflict", "unknown");
		var d = Drafts.get(U, id);
		Kit.eq(d.maybe_sent, U.clock, "draft back, marked maybe_sent");
		var items = U.store.list("sent", { limit: 100 }).items;
		var states = [];
		for (var i = 0; i < items.length; i++)
			if (items[i].subject === "c4")
				states.push(items[i].delivery.state);
		Kit.eq(states, ["unknown"], "the Sent copy shows the outcome is unknown");
		var row = Drafts.list(U).filter(function (x) { return x.id === id; })[0];
		Kit.eq(row.maybe_sent, U.clock, "list rows carry maybe_sent");
		fails(function () { Send.send(U, id, key()); }, "conflict", "maybe_sent");
		Kit.eq(live_copies("c4"), 0, "nothing sent");
		var r = Send.send(U, id, key(), { confirm_resend: true });
		Kit.eq(r.ok, true, "sent when confirmed");
		Kit.eq(live_copies("c4"), 1, "once");
		items = U.store.list("sent", { limit: 100 }).items;
		states = [];
		for (i = 0; i < items.length; i++)
			if (items[i].subject === "c4")
				states.push(items[i].delivery.state);
		Kit.eq(states, ["sent"], "the earlier unknown Sent copy is gone");
	});

	// Outcome recorded but not indexed / charging interrupted
	run_case("crash after done, before the index: same key, no second charge", function () {
		var C = context({ sys: { inet_cost: 10 }, user: { credits: 100 } });
		var id = new_draft(C, { to: "ext@example.org", subject: "c5a", body: "x" });
		var k = key();
		crash_at("done");
		dies(function () { Send.send(C, id, k); });
		C.clock = NOW + LIVE + 1;
		var r = Send.send(C, id, k);
		Kit.eq(r.ok, true, "the recorded outcome");
		Kit.eq(C.user.adjusted, [-10], "charged once");
		Kit.eq(C.user.sent, [[1, false]], "counted once");
		Kit.eq(all_with_subject("c5a").length, 1, "one copy");
	});

	run_case("index write fails: the claim stays, a same-key retry charges nothing", function () {
		var C = context({ sys: { inet_cost: 10 }, user: { credits: 100 } });
		var id = new_draft(C, { to: "ext@example.org", subject: "c5b", body: "x" });
		var k = key();
		var real = C.userdata.update;
		C.userdata.update = function (name) {
			if (name == "sends") {
				C.userdata.update = real;
				throw new Error("simulated write failure");
			}
			return real.apply(C.userdata, arguments);
		};
		var r = Send.send(C, id, k);
		Kit.eq(r.ok, true, "sent");
		fails(function () { Send.send(C, id, k); }, "conflict", "sending");
		C.clock = NOW + LIVE + 1;
		Kit.eq(Send.send(C, id, k).message_id, r.message_id, "the same outcome");
		Kit.eq(C.user.adjusted, [-10], "charged once");
		Kit.eq(C.user.sent, [[1, false]], "counted once");
		Kit.eq(Send.send(C, id, k).message_id, r.message_id, "indexed now");
	});

	run_case("crash after charging was recorded: never charged twice", function () {
		var C = context({ sys: { inet_cost: 10 }, user: { credits: 100 } });
		var id = new_draft(C, { to: "ext@example.org", subject: "c5c", body: "x" });
		crash_at("charging");
		dies(function () { Send.send(C, id, key()); });
		C.clock = NOW + LIVE + 1;
		Kit.eq(Send.send(C, id, key()).ok, true, "completed");
		Kit.eq(C.user.adjusted, [], "the charge is lost, not repeated");
		Kit.eq(all_with_subject("c5c").length, 1, "one copy");
	});

	// The record lives with the claim: no age or count limit
	run_case("the record outlives 24 hours and 50 newer sends", function () {
		var R = context();
		var id = new_draft(R, { to: "bob", subject: "c6a", body: "x" });
		crash_at("delivered");
		dies(function () { Send.send(R, id, key()); });
		for (var i = 0; i < 55; i++)
			Send.send(R, new_draft(R, { to: "carl", subject: "c6 filler", body: "x" }), key());
		R.clock = NOW + 25 * 3600;
		Drafts.sweep(R, R.clock);	// no recover callback: left alone
		Kit.assert(!draft_exists(R, id), "not given back as a draft");
		var r = Send.send(R, id, key());
		Kit.eq(r.warning, "recovered", "completed");
		Kit.eq(all_with_subject("c6a").length, 1, "one copy, the next day too");
	});

	// A key belongs to one draft
	run_case("a key replayed with another draft id", function () {
		var X = context();
		var k = key();
		Send.send(X, new_draft(X, { to: "bob", subject: "c7", body: "x" }), k);
		var other = new_draft(X, { to: "bob", subject: "c7b", body: "x" });
		fails(function () { Send.send(X, other, k); }, "conflict", "key");
		Kit.eq(live_copies("c7b"), 0, "nothing sent");
	});

	// A crash before anything was saved: the draft comes back, sent once
	run_case("crash before the Sent copy", function () {
		var Y = context();
		var id = new_draft(Y, { to: "bob", subject: "c8", body: "x" });
		var k = key();
		crash_at("copy");
		dies(function () { Send.send(Y, id, k); });
		Y.clock = NOW + 3700;
		Kit.eq(Drafts.sweep(Y, Y.clock).recovered, 0, "a claim past 'claimed' is not renamed back");
		var r = Send.send(Y, id, k);
		Kit.eq(r.ok, true, "sent");
		Kit.eq(live_copies("c8"), 1, "once");
		var items = Y.store.list("sent", { limit: 100 }).items;
		var n = 0;
		for (var i = 0; i < items.length; i++)
			if (items[i].subject === "c8")
				n++;
		Kit.eq(n, 1, "one Sent copy");
	});

	run_case("an orphan Sent copy (id never recorded) is found and removed", function () {
		var Z = context();
		var id = new_draft(Z, { to: "bob", subject: "c9", body: "x" });
		crash_at("copied");	// dies right after the Sent copy was saved
		dies(function () { Send.send(Z, id, key()); });
		Z.clock = NOW + LIVE + 1;
		var r = Send.send(Z, id, key());
		var items = Z.store.list("sent", { limit: 100 }).items;
		var n = 0;
		for (var i = 0; i < items.length; i++)
			if (items[i].subject === "c9")
				n++;
		Kit.eq(n, 1, "only the new Sent copy");
		Kit.eq(live_copies("c9"), 1, "sent once");
	});

	// The stored form of an address is not predicted
	run_case("non-canonical FidoNet forms, crash after the saves", function () {
		var F = context();
		var id = new_draft(F, { to: "John <John@1:103/705.0>, Ann <Ann@01:103/706>", subject: "n1a", body: "x" });
		crash_at("delivered");
		dies(function () { Send.send(F, id, key()); });
		F.clock = NOW + LIVE + 1;
		Kit.eq(Send.send(F, id, key()).warning, "recovered", "recovered");
		Kit.eq(all_with_subject("n1a").length, 2, "one copy each: " + JSON.stringify(all_with_subject("n1a")));
	});

	run_case("non-canonical FidoNet forms, crash right after a save, before its record", function () {
		var F = context();
		var id = new_draft(F, { to: "John <John@1:103/705.0>, Ann <Ann@01:103/706>, bob", subject: "n1b",
			body: "x" });
		crash_at("saved", "fido");
		dies(function () { Send.send(F, id, key()); });
		F.clock = NOW + LIVE + 1;
		Kit.eq(Send.send(F, id, key()).ok, true, "completed");
		Kit.eq(all_with_subject("n1b").length, 3, "one copy each: " + JSON.stringify(all_with_subject("n1b")));
	});

	run_case("a mixed-case Internet domain, crash after the saves", function () {
		var F = context();
		var id = new_draft(F, { to: "Ext <Ext@Example.ORG>, bob", subject: "n1c", body: "x" });
		crash_at("delivered");
		dies(function () { Send.send(F, id, key()); });
		F.clock = NOW + LIVE + 1;
		Send.send(F, id, key());
		Kit.eq(all_with_subject("n1c").length, 2, "one copy each");
	});

	run_case("destination keys compare in the stored form", function () {
		var q1 = Send.dest_key({ to: "Bob", to_net_type: D.NET_QWK, to_net_addr: "HUB/VERT" });
		var q2 = Send.dest_key({ to: "bob", to_net_type: D.NET_QWK, to_net_addr: "vert" });
		Kit.eq(q1, q2, "QWK: route and case");
		var f1 = Send.dest_key({ to: "Ann", to_net_type: D.NET_FIDO, to_net_addr: "01:0103/706.0" });
		var f2 = Send.dest_key({ to: "ann", to_net_type: D.NET_FIDO, to_net_addr: "1:103/706" });
		Kit.eq(f1, f2, "FidoNet: numbers");
		var i1 = Send.dest_key({ to: "x", to_net_type: D.NET_INTERNET, to_net_addr: "<Ext@Example.ORG>" });
		var i2 = Send.dest_key({ to: "y", to_net_type: D.NET_INTERNET, to_net_addr: "ext@example.org" });
		Kit.eq(i1, i2, "Internet: case and brackets");
		Kit.eq(Send.dest_key({ to_ext: "0042" }), Send.dest_key({ to_ext: "42" }), "local: number");
	});

	// One owner per claim
	run_case("two recoveries overlap, the second dies after charging", function () {
		var K = context({ sys: { inet_cost: 10 }, user: { credits: 100 } });
		var K2 = context({ sys: { inet_cost: 10 }, user: { credits: 100 } });
		var id = new_draft(K, { to: "ext@example.org", cc: "bob", subject: "q2b", body: "x" });
		crash_at("delivery", "local");
		dies(function () { Send.send(K, id, key()); });
		K.clock = K2.clock = NOW + LIVE + 100;
		var inner = null;
		Send.hook = function (p, k) {
			if (p == "delivery" && k == "local") {
				Send.hook = function (p2) {
					if (p2 == "charging") {
						Send.hook = null;
						throw { crash: true };
					}
				};
				try {
					inner = Send.recover(K2, id);
				} catch (e) {
					inner = e;
				}
			}
		};
		// The second cannot take the claim over, so the crash meant for it
		// hits the first, right after it recorded "charging"
		var r = null;
		try {
			r = Send.recover(K, id);
		} catch (e) {
			r = e;
		}
		Send.hook = null;
		Kit.eq([inner && inner.code, inner && inner.reason], ["conflict", "sending"], "the second waits");
		Kit.eq(r && r.crash, true, "the first died after recording charging");
		Kit.eq(all_with_subject("q2b").length, 2, "one copy each");
		K.clock = K2.clock = NOW + 3 * LIVE;
		Kit.eq(Send.recover(K2, id).ok, true, "a later recovery completes");
		Kit.eq(all_with_subject("q2b").length, 2, "still one copy each");
		Kit.eq(K.user.adjusted.concat(K2.user.adjusted), [], "never charged twice (this charge is lost)");
	});

	run_case("a stalled send overtaken by a recovery that dies after charging", function () {
		var L = context({ sys: { inet_cost: 10 }, user: { credits: 100 } });
		var L2 = context({ sys: { inet_cost: 10 }, user: { credits: 100 } });
		L.clock = L2.clock = NOW + 30000;
		var id = new_draft(L, { to: "ext@example.org", cc: "bob", subject: "q3c", body: "x" });
		var kl = key();
		var inner = null;
		Send.hook = function (p, k) {
			if (p == "delivery" && k == "local") {
				L2.clock = NOW + 30000 + LIVE + 1;
				Send.hook = function (p2) {
					if (p2 == "charging") {
						Send.hook = null;
						throw { crash: true };
					}
				};
				try {
					inner = Send.send(L2, id, key());
				} catch (e) {
					inner = e;
				}
			}
		};
		fails(function () { Send.send(L, id, kl); }, "conflict", "sending");
		Send.hook = null;
		Kit.eq(inner && inner.crash, true, "the overtaking recovery died after charging");
		Kit.eq(all_with_subject("q3c").length, 2, "one copy each: " + JSON.stringify(all_with_subject("q3c")));
		// the recovery died after recording "charging": that charge is lost
		Kit.eq(L.user.adjusted.concat(L2.user.adjusted), [], "not charged by the stalled send");
		L.clock = NOW + 30000 + 3 * LIVE;
		var r = Send.send(L, id, kl);
		Kit.eq(r.ok, true, "the original sender gets the outcome");
		Kit.eq(L.user.adjusted.concat(L2.user.adjusted), [], "never charged twice");
		Kit.eq(Send.send(L, id, key()).message_id, r.message_id, "another key: the draft's outcome");
		Kit.eq(all_with_subject("q3c").length, 2, "nothing more");
	});

	run_case("two recoveries at charging: charged once", function () {
		var K = context({ sys: { inet_cost: 10 }, user: { credits: 100 } });
		var K2 = context({ sys: { inet_cost: 10 }, user: { credits: 100 } });
		var id = new_draft(K, { to: "ext@example.org", subject: "n2c", body: "x" });
		crash_at("delivered");
		dies(function () { Send.send(K, id, key()); });
		K.clock = K2.clock = NOW + LIVE + 1;
		var inner = null;
		Send.hook = function (p) {
			if (p == "charging") {
				Send.hook = null;
				try {
					inner = Send.recover(K2, id);
				} catch (e) {
					inner = e;
				}
			}
		};
		var r = Send.recover(K, id);
		Kit.eq([inner && inner.code, inner && inner.reason], ["conflict", "sending"], "the second waits");
		Kit.eq(K.user.adjusted.concat(K2.user.adjusted), [-10], "charged once");
		Kit.eq(Send.recover(K2, id).message_id, r.message_id, "a later recovery reports the outcome");
	});

	run_case("takeover only after the window", function () {
		var K = context();
		K.clock = NOW + 50000;
		var id = new_draft(K, { to: "bob", subject: "n2d", body: "x" });
		crash_at("delivered");
		dies(function () { Send.send(K, id, key()); });
		K.clock = NOW + 50000 + LIVE - 1;
		fails(function () { Send.send(K, id, key()); }, "conflict", "sending");
		K.clock = NOW + 50000 + LIVE + 1;
		Kit.eq(Send.send(K, id, key()).warning, "recovered", "taken over");
		Kit.eq(all_with_subject("n2d").length, 1, "once");
	});

	// Copies this send recalled itself are never evidence of delivery
	run_case("the second removal fails: every recipient served once, or partial", function () {
		var K = context();
		var id = new_draft(K, { to: "x1@example.org, x2@example.org", cc: "bob", subject: "q6", body: "x" });
		var res;
		with_failing_removal(2, function () {
			fail_at("delivery", "local");
			try {
				res = Send.send(K, id, key());
			} catch (e) {
				res = e;
			}
		});
		served_once("q6", ["x1@example.org", "x2@example.org", String(BOB)], res);
	});

	run_case("a crash between the recall record and the delete", function () {
		var K = context();
		var id = new_draft(K, { to: "x1@example.org", cc: "bob", subject: "q6b", body: "x" });
		var k = key();
		Send.hook = function (p, kind) {
			if (p == "delivery" && kind == "local")
				throw new Error("simulated failure of the local save");
			if (p == "recalling") {
				Send.hook = null;
				throw { crash: true };
			}
		};
		dies(function () { Send.send(K, id, k); });
		K.clock = NOW + LIVE + 1;
		var res;
		try {
			res = Send.send(K, id, k);
		} catch (e) {
			res = e;
		}
		served_once("q6b", ["x1@example.org", String(BOB)], res);
	});

	run_case("a crash after some deletes", function () {
		var points = ["removed", "recalled"];
		for (var pi = 0; pi < points.length; pi++) {
			var K = context();
			var subj = "q6c" + pi;
			var id = new_draft(K, { to: "x1@example.org, x2@example.org", cc: "bob", subject: subj, body: "x" });
			var k = key();
			var point = points[pi];
			Send.hook = function (p, kind) {
				if (p == "delivery" && kind == "local")
					throw new Error("simulated failure of the local save");
				if (p == point) {
					Send.hook = null;
					throw { crash: true };
				}
			};
			dies(function () { Send.send(K, id, k); });
			K.clock = NOW + LIVE + 1;
			var res;
			try {
				res = Send.send(K, id, k);
			} catch (e) {
				res = e;
			}
			served_once(subj, ["x1@example.org", "x2@example.org", String(BOB)], res, point);
		}
	});

	// The mail server takes a copy in the instant between the rollback's
	// check and its removal: that copy went out, so it is not recalled
	// and its recipient gets no second one
	run_case("a copy sent in the instant before its removal is not recalled", function () {
		var flags = [D.NETMSG_SENT, D.NETMSG_INTRANSIT];
		for (var fi = 0; fi < flags.length; fi++) {
			var K = context();
			var subj = "q7" + fi;
			var id = new_draft(K, { to: "x1@example.org", cc: "bob", subject: subj, body: "x" });
			var flag = flags[fi];
			Send.hook = function (p, kind) {
				if (p == "delivery" && kind == "local")
					throw new Error("simulated failure of the local save");
				if (p == "removed") {
					Send.hook = null;
					var mb = Kit.email_base(W.S.shared_path);
					try {
						var all = all_with_subject(subj);
						for (var i = 0; i < all.length; i++) {
							if (all[i].to_net_addr !== "x1@example.org")
								continue;
							var h = mb.get_msg_header(false, all[i].num, false);
							h.netattr |= flag;
							Kit.assert(mb.put_msg_header(false, all[i].num, h), "mark taken");
						}
					} finally {
						mb.close();
					}
				}
			};
			var res;
			try {
				res = Send.send(K, id, key());
			} catch (e) {
				res = e;
			}
			Kit.eq(all_with_subject(subj).filter(function (c) {
				return c.to_net_addr === "x1@example.org";
			}).length, 1, subj + ": x1 was sent one copy, never a second: " + JSON.stringify(res));
			Kit.assert(res && res.ok === true, subj + ": completed: " + JSON.stringify(res));
			Kit.eq(live_to(subj, String(BOB)), 1, subj + ": bob served once");
		}
	});

	run_case("recalled copies do not count toward the copies-vs-recipients rule", function () {
		var K = context();
		var id = new_draft(K, { to: "x1@example.org, x2@example.org", cc: "bob", subject: "q6d", body: "x" });
		var k = key();
		Send.hook = function (p, kind) {
			if (p == "delivery" && kind == "local")
				throw new Error("simulated failure of the local save");
			if (p == "recalled") {
				Send.hook = null;
				throw { crash: true };
			}
		};
		dies(function () { Send.send(K, id, k); });
		// a copy with this send's id that matches no recipient
		var x2 = all_with_subject("q6d").filter(function (c) { return c.to_net_addr === "x2@example.org"; })[0];
		W.save({ to: "Carl", to_ext: String(CARL), from: "alice", from_ext: String(ALICE), subject: "q6d",
			id: x2.id });
		K.clock = NOW + LIVE + 1;
		var res;
		try {
			res = Send.send(K, id, k);
		} catch (e) {
			res = e;
		}
		Kit.assert(!(res && res.ok === true && live_to("q6d", "x1@example.org") === 0),
			"never ok with x1 unserved: " + JSON.stringify(res));
		Kit.assert(!(res && res.ok === true && live_to("q6d", String(BOB)) === 0),
			"never ok with bob unserved: " + JSON.stringify(res));
		if (res.ok !== true)
			Kit.eq([res.code, res.reason], ["conflict", "partial"], "partial");
	});
} finally {
	Send.hook = null;
	for (var ci = 0; ci < contexts.length; ci++)
		contexts[ci].close();
	W.cleanup();
}
Kit.assert(cases >= 15, "cases run: " + cases);
if (argv.indexOf("-v") >= 0)
	print(cases + " cases, " + failures.length + " failed");
if (failures.length)
	throw new Error(failures.length + " case(s) failed:\n" + failures.join("\n"));
