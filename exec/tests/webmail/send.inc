// Shared by send.js and send_recovery.js (not a test itself: the runner
// only runs *.js files): the scratch world, a fake sender, fake address
// lookups and fake system values for Send (webmail/lib/send.js).
// A test loads it with:  load(js.exec_dir + "send.inc");

load(js.exec_dir + "kit.js");
var Send = Kit.wlib("send");
var Drafts = Kit.wlib("drafts");
var Contacts = Kit.wlib("contacts");

var W = Kit.api_world();
var D = W.D;
var logged = [];
Send.log = function (level, text) {
	logged.push(text);
};
Drafts.log = function (level, text) {
	logged.push(text);
};

var ALICE = W.ALICE;
var BOB = W.BOB;
var CARL = 60005;
W.add_user(CARL, { alias: "carl" });
var BY_ALIAS = { alice: ALICE, bob: BOB, carl: CARL };
var NAMES = {};
NAMES[ALICE] = "alice";
NAMES[BOB] = "bob";
NAMES[CARL] = "carl";
var DOMAINS = ["vert.example", "family.example"];
var NOW = time();
var keys = 0;

var make = W.Api.context_factory({ config: W.cfg,
	get_user: function (n) {
		return W.USERS.hasOwnProperty(n) ? W.USERS[n] : null;
	},
	userdata: function (n) {
		return new W.UserData(W.S.user_dir(n) + "webmail/", W.JsonFiles);
	},
	Store: W.Store, shared_path: W.S.shared_path, data_dir: W.S.data_dir });
var contexts = [];

function lc(s)
{
	return String(s).replace(/[A-Z]/g, function (c) {
		return String.fromCharCode(c.charCodeAt(0) + 32);
	});
}

function fake_user(o)
{
	o = o || {};
	var u = {
		number: ALICE, alias: "alice", name: "Alice Smith", email: "alice@vert.example",
		is_sysop: o.is_sysop === true,
		security: { restrictions: 0, exemptions: o.exemptions || 0, credits: o.credits || 0,
			free_credits: o.free_credits || 0 },
		adjusted: [], sent: [],
		can_send_mail: function (net, usernum, count) {
			u.asked = (u.asked || 0) + 1;
			return !(o.deny && o.deny.indexOf(net) >= 0);
		},
		adjust_credits: function (n) {
			if (o.adjust_throws)
				throw new Error("simulated adjust failure");
			u.adjusted.push(n);
			return true;
		},
		sent_email: function (n, feedback) {
			u.sent.push([n, !!feedback]);
			return true;
		}
	};
	return u;
}

function resolvers()
{
	return {
		matchuser: function (t) {
			return BY_ALIAS[lc(t)] || 0;
		},
		netaddr_type: netaddr_type,
		own: {
			is_own_domain: function (h) {
				return DOMAINS.indexOf(lc(h)) >= 0;
			},
			local_user: function (lp) {
				return BY_ALIAS[lc(lp)] || 0;
			},
			username: function (n) {
				return NAMES[n] || "";
			}
		}
	};
}

// A context for Alice; o: user (fake_user options), config, sys, settings,
// domains
function context(o)
{
	o = o || {};
	var ctx = make(ALICE);
	contexts.push(ctx);
	var cfg = {};
	for (var k in W.cfg)
		cfg[k] = W.cfg[k];
	for (k in (o.config || {}))
		cfg[k] = o.config[k];
	ctx.config = cfg;
	ctx.domains = o.domains || DOMAINS;
	ctx.clock = NOW;
	ctx.now = function () {
		return ctx.clock;
	};
	ctx.user = fake_user(o.user);
	ctx.resolvers = resolvers();
	ctx.settings = o.settings || {};
	var sys = { inet_addr: "vert.example", fido_addrs: ["1:103/705"], inet_cost: 0, fido_cost: 0,
		inet_alias: true, fido_alias: true,
		user_email: function (n) {
			return NAMES[n] + "@vert.example";
		},
		check_netmail_addr: function (a) {
			return false;	// no QWK route
		},
		mailbox_full: function (n) {
			return false;
		} };
	for (k in (o.sys || {}))
		sys[k] = o.sys[k];
	ctx.sys = sys;
	return ctx;
}

function bytes(n, seed)
{
	var a = [];
	for (var i = 0; i < n; i++)
		a.push(String.fromCharCode((i * 13 + (seed || 0) + (i >> 10)) & 0xFF));
	return a.join("");
}

function read_file(path)
{
	var f = new File(path);
	Kit.assert(f.open("rb"), "open " + path);
	var t = f.read();
	f.close();
	return t;
}

// A saved draft; atts: [{name, type, data}]
function new_draft(ctx, fields, atts, mode, source)
{
	var d = Drafts.create(ctx, { mode: mode || "new", source_id: source });
	var f = { rev: d.rev };
	for (var k in fields)
		f[k] = fields[k];
	d = Drafts.save(ctx, d.id, f);
	for (var i = 0; i < (atts || []).length; i++) {
		var a = atts[i];
		var up = "upload" + i + "abcdef";
		var off = 0;
		do {
			var part = a.data.substr(off, Drafts.MAX_CHUNK);
			Drafts.attach(ctx, d.id, { upload: up, name: a.name, type: a.type, offset: off,
				total: a.data.length, b64: base64_encode(part) || "" });
			off += part.length;
		} while (off < a.data.length);
	}
	return d.id;
}

function key()
{
	return "testkey-" + (keys++) + "-abcdefghij";
}

// Every live message in the scratch shared base: {num, h} (fields expanded)
function shared()
{
	var mb = Kit.email_base(W.S.shared_path);
	var out = [];
	try {
		var idx = mb.get_index() || [];
		for (var i = 0; i < idx.length; i++) {
			if (idx[i].attr & D.MSG_DELETE)
				continue;
			var h = mb.get_msg_header(false, idx[i].number);
			if (h === null)
				continue;
			out.push({ num: idx[i].number, h: {
				to: h.to, to_ext: h.to_ext, to_net_type: h.to_net_type, to_net_addr: h.to_net_addr,
				from: h.from, from_ext: h.from_ext, from_net_type: h.from_net_type,
				from_net_addr: h.from_net_addr, reverse_path: h.reverse_path, to_list: h.to_list,
				cc_list: h.cc_list, id: h.id, reply_id: h.reply_id, priority: h.priority,
				netattr: h.netattr, subject: h.subject, ftn_charset: h.ftn_charset,
				content_type: h.content_type, field_list: h.field_list, auxattr: h.auxattr } });
		}
	} finally {
		mb.close();
	}
	return out;
}

function with_id(list, id)
{
	var out = [];
	for (var i = 0; i < list.length; i++)
		if (list[i].h.id === id)
			out.push(list[i]);
	return out;
}

function remove_shared(num)
{
	var mb = Kit.email_base(W.S.shared_path);
	try {
		Kit.assert(mb.remove_msg(false, num), "remove #" + num);
	} finally {
		mb.close();
	}
}

function sent_items(ctx)
{
	return ctx.store.list("sent").items;
}

function sent_item(ctx, id)
{
	var items = ctx.store.list("sent", { limit: 100 }).items;
	for (var i = 0; i < items.length; i++)
		if (items[i].id === id)
			return items[i];
	return null;
}

function store_for(usernum)
{
	var st = new W.Store({ usernum: usernum, shared_path: W.S.shared_path, data_dir: W.S.data_dir });
	st.open();
	contexts.push({ close: function () { st.close(); } });
	return st;
}

function draft_exists(ctx, id)
{
	try {
		Drafts.get(ctx, id);
		return true;
	} catch (e) {
		if (e.code !== "notfound")
			throw e;
		return false;
	}
}

function fails(fn, code, reason)
{
	var caught = null;
	try {
		fn();
	} catch (e) {
		caught = e;
	}
	Kit.assert(caught !== null, "expected " + code + (reason ? "/" + reason : ""));
	Kit.eq(caught.code, code, "code (" + JSON.stringify(caught) + ")");
	if (reason !== undefined)
		Kit.eq(caught.reason, reason, "reason");
	Kit.assert(typeof caught.message === "string" && !/[\/\\]/.test(caught.message), "fixed message");
	return caught;
}

// Nothing saved: no new shared message and no new Sent copy
function nothing_saved(ctx, fn, code, reason)
{
	var before = shared().length;
	var sent = ctx.store.list("sent").total;
	var e = fails(fn, code, reason);
	Kit.eq(shared().length, before, "no delivery copy saved");
	Kit.eq(ctx.store.list("sent").total, sent, "no Sent copy saved");
	return e;
}

function strip_end(s)
{
	return s.replace(/(\r\n)+$/, "");
}

// Send.hook that simulates the process dying at 'point' (kind: only for
// that kind of delivery group); passes = how many such points to let by
function crash_at(point, kind, passes)
{
	var left = passes || 0;
	Send.hook = function (p, k) {
		if (p === point && (kind === undefined || k === kind)) {
			if (left-- > 0)
				return;
			Send.hook = null;
			throw { crash: true };
		}
	};
}

// Send.hook that makes a save (an exception, not a crash) fail at 'point'
function fail_at(point, kind, times)
{
	var left = times === undefined ? 1 : times;
	Send.hook = function (p, k) {
		if (p === point && (kind === undefined || k === kind) && left-- > 0)
			throw new Error("simulated failure at " + point);
	};
}

// What the mail server does once it has sent a copy marked kill-when-sent
// (mailsrvr.cpp: NETMSG_SENT, then MSG_DELETE)
function mailserver_sends(num)
{
	var mb = Kit.email_base(W.S.shared_path);
	try {
		var h = mb.get_msg_header(false, num, false);
		h.netattr |= D.NETMSG_SENT;
		h.attr |= D.MSG_DELETE;
		Kit.assert(mb.put_msg_header(false, num, h), "mark #" + num + " sent and deleted");
	} finally {
		mb.close();
	}
}

// Every record (deleted ones included) with Message-ID id: [{num, h, deleted}]
function all_with_id(id)
{
	var mb = Kit.email_base(W.S.shared_path);
	var out = [];
	try {
		var idx = mb.get_index() || [];
		for (var i = 0; i < idx.length; i++) {
			var h = mb.get_msg_header(false, idx[i].number, false);
			if (h && h.id === id)
				out.push({ num: idx[i].number, to: h.to, to_ext: h.to_ext, to_net_addr: h.to_net_addr,
					deleted: (h.attr & D.MSG_DELETE) != 0 });
		}
	} finally {
		mb.close();
	}
	return out;
}

// Every record (deleted ones included) with this subject
function all_with_subject(subject)
{
	var mb = Kit.email_base(W.S.shared_path);
	var out = [];
	try {
		var idx = mb.get_index() || [];
		for (var i = 0; i < idx.length; i++) {
			var h = mb.get_msg_header(false, idx[i].number, false);
			if (h && h.subject === subject)
				out.push({ num: idx[i].number, id: h.id, to_ext: h.to_ext, to_net_addr: h.to_net_addr,
					deleted: (h.attr & D.MSG_DELETE) != 0 });
		}
	} finally {
		mb.close();
	}
	return out;
}

