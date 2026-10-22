/* Embedded BBS services backend. GPL-2.0-or-later.
 * Run one queued operation between socket callbacks. The current embedding
 * ends load(true) workers when the parent enters its callback loop, so a
 * persistent background worker is unsafe here. DNS remains asynchronous;
 * SQLite lock waits are short and BBS files use the native locking APIs.
 */
"use strict";
load("ircd/services-store.js");
load("ircd/services-core.js");
function IRC_Services_Backend(config, resolver) {
	this.config = config; this.resolver = resolver;
	this.store = null; this.engine = null; this.inputs = []; this.outputs = []; this.closed = false;
	this.__defineGetter__("read_level", function() { return this.outputs.length; });
	this.__defineGetter__("write_level", function() { return this.inputs.length; });
}
IRC_Services_Backend.prototype.output = function(event) { if (!this.closed) this.outputs.push(event); };
IRC_Services_Backend.prototype.read = function() { return this.outputs.shift(); };
IRC_Services_Backend.prototype.write = function(job) {
	if (job.type == "stop") {
		this.closed = true; this.inputs = []; this.outputs = [];
		if (this.store) this.store.close(); this.store = this.engine = null; return true;
	}
	if (this.closed || this.inputs.length >= 512) return false;
	if (job.type == "dns") { this.resolve_host(); return true; }
	this.inputs.push(job); return true;
};
function IRC_services_dns_reply(ips) {
	this.output({type:"dns", local:IRC_services_host_is_local(ips || [], this.config.binds)});
}
IRC_Services_Backend.prototype.resolve_host = function() {
	var host = this.config.host;
	if (!host) { IRC_services_dns_reply.call(this, []); return; }
	if (/^[0-9.]+$/.test(host) || /^[0-9a-f:]+$/i.test(host)) IRC_services_dns_reply.call(this, [host]);
	else this.resolver.resolve(host, IRC_services_dns_reply, this);
};
IRC_Services_Backend.prototype.process_one = function() {
	if (this.closed || !this.inputs.length) return;
	var job = this.inputs.shift(), self = this;
	try {
			if (job.type == "activate") {
				if (!this.engine) {
					this.store = new IRC_Services_Store(this.config.database);
					this.engine = new IRC_Services_Engine(this.store, this.config, function(action) { self.output({type:"action", action:action}); });
					this.output({type:"ready"}); this.engine.event({type:"boot"});
				}
			} else if (job.type == "deactivate") {
				this.engine = null; if (this.store) this.store.close(); this.store = null;
			} else if (job.type == "event") { if (this.engine) this.engine.event(job.event); }
			else if (job.type == "validate") {
				var checked = this.memo_account(job.identity);
				this.output({type:"result", id:job.id, identity:IRC_services_identity(checked, this.config.qwk, this.config.server)});
			}
			else if (job.type == "resolve") {
				var found = this.engine && this.engine.resolve_account(job.target);
				this.output({type:"result", id:job.id, identity:found ? this.store.data.accounts[found] : null});
			}
			else if (job.type == "local-resolve") {
				var number = system.matchuser(IRC_services_unescape(job.account), false);
				var recipient = number ? new User(number) : null;
				if (recipient && ((recipient.settings & (USER_DELETED | USER_INACTIVE)) || (recipient.security.restrictions & UFLAG_G))) recipient = null;
				this.output({type:"result", id:job.id, identity:recipient ? IRC_services_identity(recipient, this.config.qwk, this.config.server) : null});
			}
			else if (job.type == "authenticate") {
				var identity = IRC_services_authenticate(job.account, job.password, this.config.qwk, this.config.server);
				delete job.password;
				if (identity) {
					var authenticated_user = new User(identity.number);
					authenticated_user.connection = "IRC"; authenticated_user.logontime = time();
				}
				this.output({type:"result", id:job.id, identity:identity, error:identity ? "" : "Invalid account or password."});
			} else if (job.type == "memo") {
				this.memo_account(job.identity);
				if (job.operation == "READ") this.output({type:"result", id:job.id, text:system.get_telegram(job.identity.number) || "No telegrams waiting."});
				else if (job.operation == "STATUS") this.output({type:"result", id:job.id, text:this.memo_pending(job.identity) ? "You have telegrams waiting. Use READ to receive them." : "No telegrams waiting."});
				else if (job.operation == "SEND") {
					if (!system.put_telegram(job.identity.number, job.text)) throw new Error("Cannot store telegram");
					this.output({type:"result", id:job.id, text:"Telegram delivered."});
				}
			}
	} catch (error) {
		log(LOG_ERR, "Embedded IRC services " + job.type + " failed: " + error);
		this.output({type:"failure", id:job.id, operation:job.type, error:"Service operation failed; please try again later."});
	}
};

function IRC_services_host_is_local(ips, binds) {
	if (typeof ips == "string") ips = [ips];
	for (var i = 0; i < ips.length; i++) {
		var ip = ips[i], v6 = ip.indexOf(":") >= 0;
		if (ip == "0.0.0.0" || ip == "::") continue;
		/* Bind the resolved address to an ephemeral port to verify that it
		   is actually local. A wildcard binding alone proves nothing. */
		var probe = new Socket(SOCK_STREAM, "IRCd services ownership", v6);
		try {
			if (!probe.bind(0, ip)) continue;
			var normalized = probe.local_ip_address;
			for (var j = 0; j < binds.length; j++) {
				if (binds[j] == (v6 ? "::" : "0.0.0.0") || binds[j] == ip || binds[j] == normalized) return true;
				var other = new Socket(SOCK_STREAM, "IRCd services binding", binds[j].indexOf(":") >= 0);
				try { if (other.bind(0, binds[j]) && other.local_ip_address == normalized) return true; }
				finally { other.close(); }
			}
		} finally { probe.close(); }
	}
	return false;
}

IRC_Services_Backend.prototype.memo_account = function(identity) {
	if (!identity || identity.home != this.config.server || identity.qwk != this.config.qwk) throw new Error("Wrong home BBS");
	var u = new User(identity.number);
	if ((u.settings & (USER_DELETED | USER_INACTIVE)) || (u.security.restrictions & UFLAG_G) || u.stats.firston_date != identity.created) throw new Error("Account no longer exists");
	return u;
};
IRC_Services_Backend.prototype.memo_pending = function(identity) {
	this.memo_account(identity);
	var f = new File(system.data_dir + "msgs/" + format("%04u", identity.number) + ".msg");
	/* Never use shareable=true here: the native writers/readers use nopen
	   locks, not an unrelated lock file or an unlocked fopen. */
	if (!f.exists) return false;
	if (!f.open("rb", false)) throw new Error("Telegram file is busy");
	try { return f.length > 0; } finally { f.close(); }
};

IRC_Services_Backend.prototype.drain = function() {
	var start = system.timer;
	for (var i = 0; i < 128 && this.inputs.length; i++) {
		var event = this.inputs[0].type == "event";
		this.process_one();
		if (!event || system.timer - start >= 0.005) break;
	}
};
