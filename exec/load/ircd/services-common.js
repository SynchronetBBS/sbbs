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
function IRC_services_memo_empty(result) {
	/* Older BBS helpers report only text; newer helpers also return a
	   structured mailbox status. Never classify an error as an empty inbox. */
	return !result.error && (result.pending === false
		|| (result.pending === undefined && result.text == "No telegrams waiting."));
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
		.replace(/((?:PRIVMSG|NOTICE)\s+(?:NickServ|ChanServ|MemoServ|FileServ(?:_[a-z0-9_-]+)?|BBSsvc_[^\s@,]+)(?:@[^\s,]+)?\s+:?).*/i, "$1[redacted]");
}

/* Whitelist diagnostics rather than logging RPC objects or free-form errors.
   In particular, text may be a memo and account/password may be credentials. */
function IRC_services_log_atom(value) {
	return typeof value == "string" ? value.replace(/[^\x21-\x7e]/g, "?").substr(0, 100) : "?";
}
function IRC_services_log_id(value) {
	return typeof value == "string" && /^(?:BBSsvc_[a-z0-9_]+:)?[0-9]{1,16}$/i.test(value) ? value : "?";
}
function IRC_services_log_error(error) {
	var text = typeof error == "string" ? error : "";
	if (/requires TLS/.test(text)) return "TLS required";
	if (/Invalid or obsolete login set/.test(text)) return "obsolete login set";
	if (/Invalid BBS login set/.test(text)) return "invalid login set";
	if (/Authentication changed/.test(text)) return "authentication changed";
	if (/login was revoked/.test(text)) return "BBS login revoked";
	if (/Invalid account or password|Authentication failed/.test(text)) return "authentication failed";
	if (/disabled/.test(text)) return "service disabled";
	if (/busy/.test(text)) return "busy";
	if (/timed out/.test(text)) return "timeout";
	if (/unavailable|no longer available/.test(text)) return "unavailable";
	if (/identify|Identify/.test(text)) return "identification required";
	return "error";
}
function IRC_services_log_rpc(m) {
	var types = ["files-capabilities","files-check","files-access","authenticate","local-auth","local-memo","identity","rename","memo-text","local-validate","check-session","local-resolve",
		"reply","reply-text","session","sync-logins","claim","logout","command","memo"];
	var type = m && types.indexOf(m.type) >= 0 ? m.type : "unknown", parts = ["operation=" + type];
	if (!m || typeof m != "object") return parts.join(" ");
	if (m.id !== undefined) parts.push("request=" + IRC_services_log_id(m.id));
	var actor = m.actor || (type == "files-access" ? m.subject : null);
	if (actor && typeof actor == "object") {
		parts.push("nick=" + IRC_services_log_atom(actor.nick), "connection=" + IRC_services_log_atom(actor.server));
		if (typeof actor.auth_revision == "number" && isFinite(actor.auth_revision)) parts.push("auth_revision=" + actor.auth_revision);
	}
	if (m.home) parts.push("BBS=" + IRC_services_log_atom(m.home));
	if (type == "command") {
		var verb = typeof m.text == "string" ? m.text.trim().split(/\s+/)[0].toUpperCase() : "";
		parts.push("service=" + (["NickServ","ChanServ","MemoServ"].indexOf(m.service) >= 0 ? m.service : "unknown"));
		parts.push("command=" + (/^(HELP|IDENTIFY|LOGOUT|REGISTER|GROUP|DROP|INFO|GHOST|ACCOUNTS|LINK|UNLINK|SET|ACCESS|EXCEPT|TOPIC|OP|DEOP|VOICE|DEVOICE|INVITE|UNBAN)$/.test(verb) ? verb : "unknown"));
	}
	if (type == "memo" || type == "local-memo") parts.push("command=" + (["SEND","READ","STATUS"].indexOf(m.operation) >= 0 ? m.operation : "unknown"));
	if (type == "identity" || type == "claim") parts.push("identity=" + (m.identity ? "set" : "cleared"));
	if (Array.isArray(m.logins)) parts.push("logins=" + m.logins.length);
	if (typeof m.revision == "number" && isFinite(m.revision)) parts.push("revision=" + m.revision);
	if (type == "reply") {
		parts.push("result=" + (m.error ? IRC_services_log_error(m.error) : "ok"));
		if (typeof m.valid == "boolean") parts.push("session_valid=" + m.valid);
		if (m.identity) parts.push("identity=set");
	}
	return parts.join(" ");
}

/* Each endpoint is a real, uniquely named IRC user introduced by its BBS.
 * The three-message challenge goes to that user's actual route in both
 * directions. It prevents an untrusted leaf behind an old relay from
 * impersonating another agent merely by forging its IRC source prefix.
 * Hubs, which can observe these routes, retain the existing network trust.
 */
function IRC_Services_Transport(name, send, receive, clock, diagnostic) {
	this.name = name; this.send = send; this.receive = receive;
	this.clock = clock || function() { return system.timer; };
	this.peers = {}; this.pending = {}; this.serial = 0;
	this.diagnostic = diagnostic || function() {};
}
IRC_Services_Transport.prototype.open = function(peer) {
	var key = IRC_services_key(peer), p = this.peers[key];
	if (p && this.clock() - p.time < 20) return;
	p = this.peers[key] = {nonce: IRC_services_random(), time: this.clock(), out: 0, inseq: 0};
	this.diagnostic(peer, "starting authentication");
	this.send(peer, "SVC1 H " + p.nonce);
};
IRC_Services_Transport.prototype.write = function(peer, payload) {
	var key = IRC_services_key(peer), p = this.peers[key];
	if (!p || !p.key) {
		if (!this.pending[key]) this.pending[key] = [];
		if (this.pending[key].length >= 32) throw new Error("Service queue is full");
		this.pending[key].push({payload: payload, time: this.clock()});
		this.diagnostic(peer, "queued for authentication", payload); this.open(peer); return;
	}
	var text = base64_encode(JSON.stringify(payload)), id = String(++this.serial);
	if (text.length > 65536) throw new Error("Service message exceeds 48 KiB");
	var chunks = Math.max(1, Math.ceil(text.length / 160));
	this.diagnostic(peer, "sending message=" + id + " frames=" + chunks, payload);
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
	var self = this;
	function reject(reason) { self.diagnostic(peer, "ignored frame: " + reason); }
	if (line.length > 400) { reject("oversized frame"); return; }
	var a = line.split(" "), key = IRC_services_key(peer), p = this.peers[key], now = this.clock();
	if (a[0] != "SVC1") { reject("unknown protocol"); return; }
	if (a[1] == "H" && p && p.key) { reject("authentication already established"); return; }
	if (a[1] == "H" && a.length == 3 && /^[0-9a-f]{64}$/.test(a[2])) {
		/* A simultaneous open has one deterministic initiator. */
		if (p && !p.key && p.nonce && this.name.toUpperCase() < peer.toUpperCase() && now - p.time < 20) { this.diagnostic(peer, "simultaneous authentication; retaining our challenge"); return; }
		p = this.peers[key] = {remote: a[2], nonce: IRC_services_random(), time: now, out: 0, inseq: 0};
		this.diagnostic(peer, "received authentication challenge; sending response");
		this.send(peer, "SVC1 R " + p.remote + " " + p.nonce); return;
	}
	if (!p) { reject("no authentication session"); return; }
	if (a[1] == "R" && !p.remote && !p.key && a.length == 4 && a[2] == p.nonce
		&& /^[0-9a-f]{64}$/.test(a[3]) && now - p.time < 20) {
		p.key = IRC_services_hmac(p.nonce, a[3]);
		this.diagnostic(peer, "peer authenticated; sending acknowledgement");
		this.send(peer, "SVC1 A " + IRC_services_hmac(p.key, "accept " + peer));
		this.flush(peer); return;
	}
	if (a[1] == "A" && p.remote && !p.key && a.length == 3 && now - p.time < 20) {
		var secret = IRC_services_hmac(p.remote, p.nonce);
		if (!IRC_services_equal(a[2], IRC_services_hmac(secret, "accept " + this.name))) { reject("invalid authentication acknowledgement"); return; }
		p.key = secret; this.diagnostic(peer, "peer authenticated; acknowledgement accepted"); this.flush(peer); return;
	}
	if (a[1] != "D" || !p.key || a.length != 8) { reject(!p.key ? "authentication incomplete" : "malformed or unexpected frame"); return; }
	var body = a.slice(3).join(" "), seq = Number(a[3]), part = Number(a[5]), count = Number(a[6]);
	if (!IRC_services_equal(a[2], IRC_services_hmac(p.key, body))) { reject("invalid message authentication"); return; }
	if (seq != p.inseq + 1) { reject("unexpected sequence; expected=" + (p.inseq + 1)); return; }
	if (!/^[0-9]+$/.test(a[4]) || part % 1 || count % 1 || count < 1 || count > 410 || part < 0 || part >= count) { reject("invalid fragment metadata"); return; }
	p.inseq = seq;
	if (!part) p.message = {id: a[4], parts: [], count: count, time: now};
	var m = p.message;
	if (!m || m.id != a[4] || part != m.parts.length || count != m.count || now - m.time > 20) { reject("missing, out-of-order or expired message fragments"); return; }
	m.parts.push(a[7]);
	if (m.parts.length == count) {
		delete p.message;
		try { var payload = JSON.parse(base64_decode(m.parts.join(""))); }
		catch (_) { reject("invalid encoded RPC message"); return; }
		this.receive(peer, payload, {id:IRC_services_log_id(m.id), frames:m.count});
	}
};
IRC_Services_Transport.prototype.drop = function(peer) {
	delete this.peers[IRC_services_key(peer)]; delete this.pending[IRC_services_key(peer)];
};
