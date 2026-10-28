/* Embedded BBS services: identity and authenticated message transport.
 * GPL-2.0-or-later. No daemon globals are required by this module.
 */
"use strict";
load("ircd/exceptions.js");
load("ircd/services-help.js");

function IRC_services_key(name) { return "$" + String(name).toUpperCase(); }
function IRC_services_copy(value) { return JSON.parse(JSON.stringify(value)); }
function IRC_services_escape(value) {
	return String(value).replace(/ /g, ".").replace(/[^a-zA-Z0-9_.-]/g, function(c) {
		var n = c.charCodeAt(0);
		if (n > 255) throw new Error("Account name is not a byte string");
		return "%" + ("0" + n.toString(16).toUpperCase()).slice(-2);
	});
}
function IRC_services_unescape(value) {
	if (/%(?![0-9a-f]{2})/i.test(value)) throw new Error("Invalid escaped account name");
	return value.replace(/%([0-9a-f]{2})/ig, function(_, hex) { return String.fromCharCode(parseInt(hex, 16)); });
}
/* All operations except public help/info and logout carry authentication,
 * private account/mailbox data, or authorization changes. */
function IRC_services_requires_tls(service, text) {
	var verb = (String(text || "").trim().split(/\s+/)[0] || "HELP").toUpperCase();
	return verb != "HELP" && verb != "INFO" && !(service == "NickServ" && verb == "LOGOUT");
}

function IRC_services_identity(user, qwk, home) {
	var alias = user.alias || user.name;
	if (!alias || !/^[a-z0-9_-]{1,8}$/i.test(qwk) || !user.number || !user.stats.firston_date)
		throw new Error("Invalid BBS account identity");
	return {id: qwk.toUpperCase() + ":" + user.number + ":" + user.stats.firston_date,
		qwk: qwk.toUpperCase(), number: user.number, created: user.stats.firston_date,
		name: alias, home: home.toLowerCase(), username: IRC_services_escape(alias) + "+" + qwk.toUpperCase()};
}
function IRC_services_authenticate(account, password, qwk, home) {
	if (!password || /[\x00\r\n]/.test(password)) return null;
	var number = system.find_login_id ? system.find_login_id(account) : system.matchuser(account, false);
	if (!number) return null;
	var u = new User(number);
	/* Do not treat an existing name as evidence of successful authentication.
	   Guest/shared accounts cannot own permanent network registrations. */
	if ((u.settings & (USER_DELETED | USER_INACTIVE)) || (u.security.restrictions & UFLAG_G)
		|| !u.security.password || password.toUpperCase() != u.security.password.toUpperCase()) return null;
	return IRC_services_identity(u, qwk, home);
}
function IRC_services_random() {
	var c = new CryptContext(CryptContext.ALGO["HMAC-SHA2"]);
	c.generate_key(); c.encrypt("Synchronet IRC services nonce");
	return c.hashvalue.replace(/[\s\S]/g, function(b) { return ("0" + b.charCodeAt(0).toString(16)).slice(-2); });
}
function IRC_services_hmac(key, text) {
	var c = new CryptContext(CryptContext.ALGO["HMAC-SHA2"]);
	c.set_key(key); c.encrypt(text);
	return c.hashvalue.replace(/[\s\S]/g, function(b) { return ("0" + b.charCodeAt(0).toString(16)).slice(-2); });
}
function IRC_services_equal(a, b) {
	var diff = a.length ^ b.length;
	for (var i = 0; i < a.length; i++) diff |= a.charCodeAt(i) ^ b.charCodeAt(i % (b.length || 1));
	return diff === 0;
}
function IRC_services_redact(line) {
	return String(line).replace(/^((?::[^\s]+\s+)?(?:PASS|IDENTIFY|NS|NICKSERV|CS|CHANSERV|MS|MEMOSERV)\s+).*/i, "$1[redacted]")
		.replace(/((?:PRIVMSG|NOTICE)\s+(?:NickServ|ChanServ|MemoServ|BBSsvc_[^\s@,]+)(?:@[^\s,]+)?\s+:?).*/i, "$1[redacted]");
}

/* Each endpoint is a real, uniquely named IRC user introduced by its BBS.
 * The three-message challenge goes to that user's actual route in both
 * directions. It prevents an untrusted leaf behind an old relay from
 * impersonating another agent merely by forging its IRC source prefix.
 * Hubs, which can observe these routes, retain the existing network trust.
 */
function IRC_Services_Transport(name, send, receive, clock) {
	this.name = name; this.send = send; this.receive = receive;
	this.clock = clock || function() { return system.timer; };
	this.peers = {}; this.pending = {}; this.serial = 0;
}
IRC_Services_Transport.prototype.open = function(peer) {
	var key = IRC_services_key(peer), p = this.peers[key];
	if (p && this.clock() - p.time < 20) return;
	p = this.peers[key] = {nonce: IRC_services_random(), time: this.clock(), out: 0, inseq: 0};
	this.send(peer, "SVC1 H " + p.nonce);
};
IRC_Services_Transport.prototype.write = function(peer, payload) {
	var key = IRC_services_key(peer), p = this.peers[key];
	if (!p || !p.key) {
		if (!this.pending[key]) this.pending[key] = [];
		if (this.pending[key].length >= 32) throw new Error("Service queue is full");
		this.pending[key].push({payload: payload, time: this.clock()}); this.open(peer); return;
	}
	var text = base64_encode(JSON.stringify(payload)), id = String(++this.serial);
	if (text.length > 65536) throw new Error("Service message exceeds 48 KiB");
	var chunks = Math.max(1, Math.ceil(text.length / 160));
	for (var i = 0; i < chunks; i++) {
		var body = (++p.out) + " " + id + " " + i + " " + chunks + " " + text.substr(i * 160, 160);
		this.send(peer, "SVC1 D " + IRC_services_hmac(p.key, body) + " " + body);
	}
};
IRC_Services_Transport.prototype.flush = function(peer) {
	var key = IRC_services_key(peer), queued = this.pending[key] || [];
	delete this.pending[key];
	for (var i = 0; i < queued.length; i++)
		if (this.clock() - queued[i].time < 20) this.write(peer, queued[i].payload);
};
IRC_Services_Transport.prototype.read = function(peer, line) {
	if (line.length > 400) return;
	var a = line.split(" "), key = IRC_services_key(peer), p = this.peers[key], now = this.clock();
	if (a[0] != "SVC1") return;
	if (a[1] == "H" && p && p.key) return;
	if (a[1] == "H" && a.length == 3 && /^[0-9a-f]{64}$/.test(a[2])) {
		/* A simultaneous open has one deterministic initiator. */
		if (p && !p.key && p.nonce && this.name.toUpperCase() < peer.toUpperCase() && now - p.time < 20) return;
		p = this.peers[key] = {remote: a[2], nonce: IRC_services_random(), time: now, out: 0, inseq: 0};
		this.send(peer, "SVC1 R " + p.remote + " " + p.nonce); return;
	}
	if (!p) return;
	if (a[1] == "R" && !p.remote && !p.key && a.length == 4 && a[2] == p.nonce
		&& /^[0-9a-f]{64}$/.test(a[3]) && now - p.time < 20) {
		p.key = IRC_services_hmac(p.nonce, a[3]);
		this.send(peer, "SVC1 A " + IRC_services_hmac(p.key, "accept " + peer));
		this.flush(peer); return;
	}
	if (a[1] == "A" && p.remote && !p.key && a.length == 3 && now - p.time < 20) {
		var secret = IRC_services_hmac(p.remote, p.nonce);
		if (!IRC_services_equal(a[2], IRC_services_hmac(secret, "accept " + this.name))) return;
		p.key = secret; this.flush(peer); return;
	}
	if (a[1] != "D" || !p.key || a.length != 8) return;
	var body = a.slice(3).join(" "), seq = Number(a[3]), part = Number(a[5]), count = Number(a[6]);
	if (!IRC_services_equal(a[2], IRC_services_hmac(p.key, body)) || seq != p.inseq + 1
		|| !/^[0-9]+$/.test(a[4]) || part % 1 || count % 1 || count < 1 || count > 410 || part < 0 || part >= count) return;
	p.inseq = seq;
	if (!part) p.message = {id: a[4], parts: [], count: count, time: now};
	var m = p.message;
	if (!m || m.id != a[4] || part != m.parts.length || count != m.count || now - m.time > 20) return;
	m.parts.push(a[7]);
	if (m.parts.length == count) {
		delete p.message;
		try { var payload = JSON.parse(base64_decode(m.parts.join(""))); }
		catch (_) { return; }
		this.receive(peer, payload);
	}
};
IRC_Services_Transport.prototype.drop = function(peer) {
	delete this.peers[IRC_services_key(peer)]; delete this.pending[IRC_services_key(peer)];
};
