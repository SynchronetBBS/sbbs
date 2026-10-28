/* Embedded BBS NickServ/MemoServ and central nickname/ChanServ adapter.
 * GPL-2.0-or-later. Loaded by ircd.js; owns no listening socket.
 */
"use strict";
load("ircd/services-backend.js");
var IRC_Services = null;

function IRC_services_agent(servername) {
	var name = servername.toLowerCase();
	return "BBSsvc_" + name.split(".")[0].substr(0, 8) + "_" + md5_calc(name, true).substr(0, 8);
}
function IRC_services_tick() { this.tick(); }
function IRC_services_start() {
	if (IRC_Services) IRC_Services.stop();
	IRC_Services = new IRC_Services_Manager(EmbeddedServices_Config);
}
function IRC_Services_Manager(options) {
	this.config = IRC_services_copy(options);
	this.config.server = ServerName.toLowerCase();
	this.config.qwk = String(system.qwk_id || "").toUpperCase();
	this.config.database = options.database || system.data_dir + "ircd-services.sqlite";
	this.config.binds = server.interface_ip_addr_list || ["0.0.0.0", "::"];
	this.config.host = options.host.toLowerCase();
	this.jobs = {}; this.requests = {}; this.serial = 0; this.sequence = 0;
	this.usercache = {}; this.chancache = {}; this.peerobjects = {};
	this.attestations = {}; this.identities = {}; this.central = false; this.activating = false; this.localhost = false;
	this.nextdns = 0; this.nextsnapshot = 0; this.stopped = false;
	this.nextactivation = system.timer + 5;
	var self = this;
	this.transport = new IRC_Services_Transport(IRC_services_agent(ServerName),
		function(peer, text) {
			var target = self.peer(peer);
			if (target && self.agent) target.originatorout("PRIVMSG " + target.nick + " :" + text, self.agent);
		}, function(peer, message) { self.receive(peer, message); });
	this.queue = new IRC_Services_Backend(this.config, DNS_Resolver);
	if (options.enabled || options.nickserv || options.memoserv)
		this.interval = js.setInterval(IRC_services_tick, 250, this);
	if (options.nickserv || options.memoserv) this.ensure_agent();
}
IRC_Services_Manager.prototype.peer = function(name) {
	var u = Users[String(name).toUpperCase()];
	/* USER/PASS usernames are sanitized and NickServ account usernames
	   contain exactly one '+'. Only the daemon can introduce this marker,
	   including through older relays that do not reserve helper nicknames. */
	if (!u || u.server || u.uprefix != "bbs+ircd+agent" || IRC_services_agent(u.servername).toUpperCase() != u.nick.toUpperCase()) return null;
	if (u.local || (!Servers[u.servername.toLowerCase()] && u.servername.toLowerCase() != ServerName.toLowerCase())) return null;
	return u;
};
IRC_Services_Manager.prototype.central_peer = function() {
	if (this.central) return this.agent && this.agent.nick;
	var s = Servers[this.config.host];
	if (!s || !s.uline || !s.linkparent) return null;
	var name = IRC_services_agent(s.linkparent);
	return this.peer(name) ? name : null;
};
IRC_Services_Manager.prototype.send = function(peer, message) {
	if (!this.agent || !this.peer(peer)) throw new Error("Destination services are unavailable");
	if (peer.toUpperCase() == this.agent.nick.toUpperCase()) this.receive(this.agent.nick, message);
	else this.transport.write(peer, message);
};
IRC_Services_Manager.prototype.job = function(job, callback) {
	if (Object.keys(this.jobs).length >= 128 || this.queue.write_level >= 512) throw new Error("Services are busy");
	job.id = String(++this.serial);
	this.jobs[job.id] = {callback:callback, until:system.timer + 30};
	if (!this.queue.write(job)) { delete this.jobs[job.id]; throw new Error("Services are busy"); }
	return job.id;
};
IRC_Services_Manager.prototype.event = function(event) {
	return this.central && this.queue.write({type:"event", event:event});
};
IRC_Services_Manager.prototype.fake_user = function(nick, home, uline, handler) {
	if (Users[nick.toUpperCase()]) throw new Error("Service nickname already exists");
	var u = new IRC_User(Generate_ID());
	u.nick = nick; u.local = false; u.uline = uline; u.servername = home;
	u.parent = 0; u.uprefix = "services"; u.hostname = home; u.created = Epoch(); u.hops = 0; u.services_internal = true;
	if (nick == IRC_services_agent(home)) u.uprefix = "bbs+ircd+agent";
	u.ip = "0.0.0.0"; u.realname = "Synchronet embedded services"; u.mode = USERMODE_INVISIBLE;
	u.rawout = function() {}; u.ircout = function() {};
	u.originatorout = function(line, origin) {
		var cmd = IRC_parse(line);
		if (cmd.verb == "PRIVMSG" && cmd.params[1]) handler(origin, cmd.params[1]);
	};
	Users[nick.toUpperCase()] = u; IRC_bcast_nick(u);
	return u;
};
IRC_Services_Manager.prototype.ensure_agent = function() {
	if (this.agent && Users[this.agent.nick.toUpperCase()] === this.agent) return true;
	var self = this, name = IRC_services_agent(ServerName);
	if (Users[name.toUpperCase()]) return false;
	this.agent = this.fake_user(name, ServerName, false, function(origin, text) {
		if (self.peer(origin.nick)) { log(LOG_DEBUG, "Embedded services wire from " + origin.nick + " phase " + text.split(" ")[1]); self.transport.read(origin.nick, text); }
	});
	this.agent.services_agent = true;
	return true;
};
IRC_Services_Manager.prototype.start_central = function() {
	if (!this.config.enabled || this.central || this.activating || system.timer < (this.nextactivation || 0) || !this.localhost || !this.config.host || !ULine_Exists(this.config.host)) return;
	if (this.config.host == this.config.server) return;
	if (Servers[this.config.host] || Users.NICKSERV || Users.CHANSERV) return;
	for (var id in Unregistered) if (Unregistered[id].socket && Unregistered[id].socket.outbound) return;
	for (var i in Local_Servers) if (Local_Servers[i].bursting || !Local_Servers[i].admitted) return;
	if (!this.ensure_agent()) return;
	this.activating = this.queue.write({type:"activate"});
};
IRC_Services_Manager.prototype.introduce = function() {
	this.activating = false;
	if (!this.config.enabled || !this.localhost || Servers[this.config.host] || Users.NICKSERV || Users.CHANSERV) { this.queue.write({type:"deactivate"}); return; }
	var self = this, s = new IRC_Server();
	s.nick = this.config.host; s.id = s.nick; s.local = false; s.parent = 0; s.linkparent = ServerName;
	s.hops = 1; s.info = "Synchronet embedded BBS services"; s.uline = true; s.admitted = true;
	s.rawout = function() {}; s.quit = function() { self.stop_central(); };
	Servers[s.nick] = s; this.service_server = s;
	server_bcast_to_servers(":" + ServerName + " SERVER " + s.nick + " 2 :" + s.info);
	this.central = true; this.usercache = {}; this.chancache = {};
	IRC_linksec_internal(s);
	this.nickserv = this.fake_user("NickServ", s.nick, true, function(user, text) { self.command(user, "NickServ", text); });
	this.chanserv = this.fake_user("ChanServ", s.nick, true, function(user, text) { self.command(user, "ChanServ", text); });
	log(LOG_NOTICE, "Embedded IRC services active as " + s.nick);
	this.snapshot();
};
IRC_Services_Manager.prototype.owns_server = function(name) {
	return this.central && String(name).toLowerCase() == this.config.host && Servers[this.config.host] === this.service_server;
};
IRC_Services_Manager.prototype.stop_central = function(except) {
	this.central = false; this.activating = false;
	this.nextactivation = system.timer + 60;
	var bots = [this.nickserv, this.chanserv];
	for (var b = 0; b < bots.length; b++) if (bots[b] && Users[bots[b].nick.toUpperCase()] === bots[b]) {
		for (var key in Local_Servers) if (Local_Servers[key].admitted && Local_Servers[key] !== except)
			Local_Servers[key].rawout(":" + bots[b].nick + " QUIT :Services stopped");
		bots[b].quit("Services stopped", true);
	}
	this.nickserv = this.chanserv = null;
	if (this.service_server && Servers[this.config.host] === this.service_server) {
		for (key in Local_Servers) if (Local_Servers[key].admitted && Local_Servers[key] !== except)
			Local_Servers[key].rawout(":" + ServerName + " SQUIT " + this.config.host + " :Services stopped");
		delete Servers[this.config.host];
	}
	this.service_server = null; this.queue.write({type:"deactivate"});
	this.identities = {}; this.usercache = {}; this.chancache = {};
};
IRC_Services_Manager.prototype.stop = function() {
	if (this.stopped) return;
	this.stopped = true;
	if (this.interval !== undefined) js.clearInterval(this.interval);
	this.stop_central();
	if (this.agent && Users[this.agent.nick.toUpperCase()] === this.agent) this.agent.quit("Services stopped");
	this.queue.write({type:"stop"});
};
IRC_Services_Manager.prototype.ref = function(user) {
	if (!user.services_ref) user.services_ref = IRC_services_random();
	return user.services_ref;
};
IRC_Services_Manager.prototype.describe = function(user) {
	return {ref:this.ref(user), nick:user.nick, server:user.servername.toLowerCase(), created:user.services_created || user.created,
		rev:user.nick, username:user.uprefix, account:user.services_identity ? user.services_identity.id : null, auth_revision:user.services_auth_revision || 0,
		secure_path:this.config.require_tls ? IRC_tls_user_to_server(user, this.config.server) == "TLS" : undefined};
};

IRC_Services_Manager.prototype.tls_error = function(user, required, servers) {
	if (!required) return "";
	var status = user ? IRC_tls_user_to_server(user, this.config.server) : "UNKNOWN";
	(servers || []).forEach(function(name) {
		status = IRC_tls_combine(status, name ? IRC_tls_server_path(ServerName, name).transport : "UNKNOWN");
	});
	return status == "TLS" ? "" : "This command requires TLS on your client connection and every server link to services; the full path is "
		+ (status == "PLAIN" ? "unencrypted" : "unknown") + ".";
};
IRC_Services_Manager.prototype.find = function(actor) {
	if (!actor) return null;
	var user = Users[String(actor.nick).toUpperCase()];
	var prior = user && user.services_previous_session;
	return user && user.nick == actor.nick && user.servername.toLowerCase() == actor.server
		&& ((this.ref(user) == actor.ref && (user.services_created || user.created) == actor.created)
			|| (prior && prior.ref == actor.ref && prior.created == actor.created)) ? user : null;
};
IRC_Services_Manager.prototype.bind_actor = function(peer, actor) {
	var source = this.peer(peer), u = actor && Users[String(actor.nick).toUpperCase()];
	if (!source || !u || u.uline || u.services_agent || actor.server != source.servername.toLowerCase()
		|| u.servername.toLowerCase() != actor.server || u.services_ref != actor.ref
		|| (u.services_created || u.created) != actor.created || !/^[0-9a-f]{64}$/.test(actor.ref)) return null;
	return u;
};
IRC_Services_Manager.prototype.notice = function(service, user, text, home) {
	/* Proxied replies are delivered by the connection server, where the
	   recipient is still local. Compare BBS names, not user.local. Central
	   registration notices omit home and keep their network-wide meaning. */
	home = String(home || "").toLowerCase();
	if (home && /^[a-z0-9_.:-]+$/.test(home) && home != String(user.servername || ServerName).toLowerCase())
		text = "[" + service + "@" + home + "] " + text
			+ "\nTo reply to this BBS, use /msg " + service + "@" + home + " <command>.";
	text = String(text).replace(/\x01./g, "").replace(/[\x00-\x08\x0b-\x1f\x7f]/g, "");
	var bot = service == "ChanServ" ? this.chanserv : this.nickserv;
	text.split(/\r?\n/).forEach(function(line) {
		for (var i = 0; i < Math.max(1, line.length); i += 300) {
			var body = line.substr(i, 300);
			if (user.local) user.rawout(":" + service + "!services@" + ServerName + " NOTICE " + user.nick + " :" + body);
			else if (bot) user.originatorout("NOTICE " + user.nick + " :" + body, bot);
		}
	});
};
IRC_Services_Manager.prototype.snapshot = function() {
	if (!this.central) return;
	var self = this, users = {}, chans = {};
	for (var k in Users) {
		var u = Users[k], d = this.describe(u), sig = JSON.stringify(d);
		users[d.ref] = true;
		if (this.usercache[d.ref] != sig && this.event({type:"user", user:d})) this.usercache[d.ref] = sig;
	}
	Object.keys(this.usercache).forEach(function(ref) { if (!users[ref] && self.event({type:"quit", ref:ref})) { delete self.usercache[ref]; delete self.identities[ref]; } });
	for (k in Channels) {
		var c = Channels[k];
		if (c.nam[0] != "#" || c.pending_burst) continue;
		if (!c.services_ref) c.services_ref = String(++this.sequence);
		var members = [];
		for (var id in c.users) members.push({ref:this.ref(c.users[id]), op:!!c.modelist[CHANMODE_OP][id], voice:!!c.modelist[CHANMODE_VOICE][id]});
		var channel = {ref:c.services_ref, name:c.nam, flags:c.chanmode(false).split(" ")[0],
			key:c.arg[CHANMODE_KEY], limit:c.arg[CHANMODE_LIMIT], topic:c.topic, users:members,
			exceptions:Object.keys(c.modelist[CHANMODE_EXCEPT]).map(function(i) { return c.modelist[CHANMODE_EXCEPT][i]; })};
		sig = JSON.stringify(channel); chans[channel.ref] = true;
		if (this.chancache[channel.ref] != sig && this.event({type:"channel", channel:channel})) this.chancache[channel.ref] = sig;
	}
	Object.keys(this.chancache).forEach(function(ref) { if (!chans[ref] && self.event({type:"channel_gone", ref:ref})) delete self.chancache[ref]; });
};
IRC_Services_Manager.prototype.tick = function() {
	if (this.stopped) return;
	var self = this, now = system.timer;
	try {
		this.queue.drain();
		if (this.config.enabled && this.config.host && now >= this.nextdns) { this.nextdns = now + 300; this.queue.write({type:"dns"}); }
		for (var i = 0; i < 100 && this.queue.read_level; i++) {
			var event = this.queue.read();
			if (!event) continue;
			if (event.type == "dns") { this.localhost = event.local; if (!event.local && this.central) this.stop_central(); }
			else if (event.type == "ready") this.introduce();
			else if (event.type == "action") this.action(event.action);
			else if (event.type == "result" || event.type == "failure") {
				var job = this.jobs[event.id]; delete this.jobs[event.id];
				if (job) job.callback(event);
				else if (event.operation == "activate") { this.activating = false; this.nextactivation = now + 60; }
			}
		}
		Object.keys(this.jobs).forEach(function(id) {
			if (self.jobs[id].until < now) { var j = self.jobs[id]; delete self.jobs[id]; j.callback({error:"Services request timed out."}); }
		});
		Object.keys(this.requests).forEach(function(id) {
			if (self.requests[id].until < now) { var r = self.requests[id]; delete self.requests[id]; r.callback({error:"Destination services are unavailable."}); }
		});
		for (var name in this.transport.peers) {
			var peer = this.peer(name.substr(1));
			if (!peer || (this.peerobjects[name] && this.peerobjects[name] !== peer)) this.transport.drop(name.substr(1));
			this.peerobjects[name] = peer;
		}
		Object.keys(this.attestations).forEach(function(ref) {
			var a = self.attestations[ref];
			if (Users[a.user.nick.toUpperCase()] !== a.user) delete self.attestations[ref];
		});
		if ((this.config.nickserv || this.config.memoserv || (this.config.enabled && this.localhost)) && !this.ensure_agent()) return;
		this.start_central();
		if (now >= this.nextsnapshot) {
			this.nextsnapshot = now + 1; this.snapshot(); this.event({type:"tick"});
			/* Reassert authenticated local sessions after central reconnects.
			   No foreign BBS identity is asserted by its connection server. */
			var cp = this.central_peer();
			if (cp) {
				var authority = this.peer(cp);
				for (var sessionid in Local_Users) {
					var sessionuser = Local_Users[sessionid];
					this.refresh_local_identity(sessionuser, now);
					if (sessionuser.services_central !== authority) {
						sessionuser.services_central = authority;
						sessionuser.services_multi = false; sessionuser.services_memo_routing = false;
						(function(u) {
							self.request(cp, {type:"session", actor:self.describe(u), multi_login:1}, function(r) {
								if (!self.find(self.describe(u)) || u.services_central !== authority) return;
								u.services_multi = r.multi_login == 1; u.services_memo_routing = r.memo_routing == 1;
								if (!u.services_multi && self.logins(u).length > 1) self.set_logins(u, u.services_identity ? [u.services_identity] : [], (u.services_auth_revision || 0) + 1);
								self.sync_session(u);
							});
						})(sessionuser);
					}
				}
			}
			if (cp && (this.peer(cp) !== this.lastauthority || now >= (this.nextclaim || 0))) {
				this.nextclaim = now + 30; this.lastauthority = this.peer(cp);
				for (var id in Local_Users) {
					var u = Local_Users[id];
					if (u.services_identity) this.sync_session(u);
				}
			} else if (!cp) this.lastauthority = null;
		}
	} catch (e) { log(LOG_ERR, "Embedded IRC services callback failed: " + e); }
};
/* Login collections exist only on the connection server and central authority.
   An identity broadcast changes presentation, never grants another BBS login. */
IRC_Services_Manager.prototype.logins = function(user) {
	if (!user.services_logins) {
		user.services_logins = {}; user.services_login_order = [];
		if (user.services_identity) { user.services_logins[user.services_identity.home] = user.services_identity; user.services_login_order.push(user.services_identity.home); }
	}
	return user.services_login_order.map(function(home) { return user.services_logins[home]; });
};
IRC_Services_Manager.prototype.set_logins = function(user, identities, revision) {
	user.services_logins = {}; user.services_login_order = [];
	identities.forEach(function(a) { user.services_logins[a.home] = a; user.services_login_order.push(a.home); });
	user.services_auth_revision = revision;
	this.apply_identity(user, identities.length ? identities[identities.length - 1] : null);
};
IRC_Services_Manager.prototype.add_login = function(user, identity, multi) {
	var logins = multi ? this.logins(user).filter(function(a) { return a.home != identity.home; }) : [];
	if (logins.length >= 64) throw new Error("Too many retained BBS logins");
	logins.push(identity); this.set_logins(user, logins, (user.services_auth_revision || 0) + 1);
};
IRC_Services_Manager.prototype.remove_login = function(user, home, all) {
	this.set_logins(user, all ? [] : this.logins(user).filter(function(a) { return a.home != home; }), (user.services_auth_revision || 0) + 1);
};
IRC_Services_Manager.prototype.sync_session = function(user, callback) {
	var self = this, cp = this.central_peer(), actor = this.describe(user);
	callback = callback || function() {};
	if (!cp) { callback({error:"Services are unavailable."}); return; }
	if (!user.services_multi) {
		this.send(cp, user.services_identity ? {type:"claim", actor:actor, identity:user.services_identity} : {type:"logout", actor:actor}); callback({}); return;
	}
	/* Periodic refresh, automatic memo checks and user commands can overlap.
	   Share one proof round for the same session revision instead of having
	   equally valid checks supersede one another at the authority. */
	var pending = user.services_sync;
	if (pending && pending.actor.auth_revision == actor.auth_revision && pending.actor.nick == actor.nick) { pending.callbacks.push(callback); return; }
	pending = user.services_sync = {actor:actor, callbacks:[callback]};
	function finish(result) { pending.callbacks.forEach(function(cb) { cb(result); }); }
	this.request(cp, {type:"sync-logins", actor:actor, logins:this.logins(user)}, function(r) {
		if (user.services_sync === pending) delete user.services_sync;
		var current = self.find(actor);
		if (!current || ((current.services_auth_revision || 0) != actor.auth_revision && !(self.central && r.revision == current.services_auth_revision))) { finish({error:"Authentication changed; please retry."}); return; }
		if (Array.isArray(r.logins) && r.revision >= actor.auth_revision) self.set_logins(current, r.logins, r.revision);
		if (!r.error && r.multi_login !== 1 && self.logins(current).length > 1) {
			self.set_logins(current, current.services_identity ? [current.services_identity] : [], (current.services_auth_revision || 0) + 1);
			self.notice("NickServ", current, "A BBS helper lacks multi-login support; retaining only the active BBS login.");
			self.sync_session(current, finish); return;
		}
		finish(r);
	});
};
IRC_Services_Manager.prototype.refresh_local_identity = function(user, now) {
	if (now < (user.services_validate_at || 0) || Object.keys(this.jobs).length >= 64) return;
	var self = this, actor = this.describe(user), local = this.logins(user).filter(function(a) { return a.home == self.config.server; })[0];
	if (!local) return;
	user.services_validate_at = now + 30;
	this.job({type:"validate", identity:local}, function(r) {
		var current = self.find(actor);
		if (!current || current.services_logins[local.home] !== local) return;
		if (!r.identity) self.remove_login(current, local.home);
		else {
			current.services_logins[local.home] = r.identity;
			if (current.services_identity === local) self.apply_identity(current, r.identity);
		}
		self.sync_session(current);
	});
};
IRC_Services_Manager.prototype.request = function(peer, message, callback) {
	if (!peer) { callback({error:"Services are unavailable."}); return; }
	if (this.central && this.config.require_tls && ["local-auth", "local-memo", "local-resolve", "local-validate"].indexOf(message.type) >= 0) {
		var destination = this.peer(peer), error = this.tls_error(this.find(message.actor), true, [destination && destination.servername]);
		if (error) { callback({error:error}); return; }
		var self = this, finish = callback;
		callback = function(result) {
			var error = self.tls_error(self.find(message.actor), true, [destination && destination.servername]);
			finish(error ? {error:error} : result);
		};
	}
	if (Object.keys(this.requests).length >= 128) { callback({error:"Services are busy."}); return; }
	var id = this.agent.nick + ":" + (++this.serial);
	message.id = id; this.requests[id] = {peer:peer.toUpperCase(), callback:callback, until:system.timer + 25};
	try { this.send(peer, message); }
	catch (_) { delete this.requests[id]; callback({error:"Destination services are unavailable."}); }
};
IRC_Services_Manager.prototype.reply = function(peer, id, result) {
	var text = result.text;
	if (text && text.length > 8000) {
		for (var i = 0; i < text.length; i += 8000) this.send(peer, {type:"reply-text", id:id, text:text.substr(i,8000)});
		delete result.text;
	}
	result.type = "reply"; result.id = id; this.send(peer, result);
};
IRC_Services_Manager.prototype.identity_valid = function(identity, home) {
	return identity && typeof home == "string" && identity.home == home.toLowerCase() && identity.qwk == home.split(".")[0].toUpperCase()
		&& identity.id == identity.qwk + ":" + identity.number + ":" + identity.created
		&& identity.number > 0 && identity.number % 1 == 0 && identity.created > 0 && identity.created % 1 == 0
		&& typeof identity.name == "string" && identity.name.length <= 25
		&& identity.username == IRC_services_escape(identity.name) + "+" + identity.qwk;
};
IRC_Services_Manager.prototype.apply_identity = function(user, identity) {
	var old = user.nuh;
	if (!user.services_anonymous) user.services_anonymous = "~" + parse_username(user.uprefix.replace(/^~/, ""));
	user.services_identity = identity;
	user.uprefix = identity ? identity.username : user.services_anonymous;
	delete user.bbs_usernum;
	if (identity && identity.home == this.config.server) user.bbs_usernum = identity.number;
	if (old == user.nuh) return;
	/* Existing clients cache user@host. Refresh only their presentation,
	   never network membership or channel timestamps. */
	for (var id in Local_Users) {
		var viewer = Local_Users[id];
		if (viewer === user) continue;
		var shared = [];
		for (var k in user.channels) if (viewer.channels[k]) shared.push(user.channels[k]);
		if (!shared.length) continue;
		viewer.rawout(":" + old + " QUIT :Account identity changed");
		shared.forEach(function(chan) {
			viewer.rawout(":" + user.nuh + " JOIN :" + chan.nam);
			var modes = (chan.modelist[CHANMODE_OP][user.id] ? "o" : "") + (chan.modelist[CHANMODE_VOICE][user.id] ? "v" : "");
			if (modes) viewer.rawout(":" + ServerName + " MODE " + chan.nam + " +" + modes + " " + modes.split("").map(function() { return user.nick; }).join(" "));
		});
	}
};
IRC_Services_Manager.prototype.claim = function(user, identity) {
	this.snapshot();
	if (!this.event({type:"user", user:this.describe(user)}) || !this.event({type:"identify", ref:this.ref(user), identity:identity, logins:user.services_verified_logins, revision:user.services_auth_revision || 0, multi:!!user.services_verified_multi})) throw new Error("Services are busy");
	this.identities[this.ref(user)] = identity;
};
IRC_Services_Manager.prototype.intercept = function(user, target, text) {
	try { return this.intercept_command(user, target, text); }
	catch (error) {
		log(LOG_WARNING, "Embedded IRC services command failed: " + error);
		var service = /^MemoServ/i.test(target) ? "MemoServ" : "NickServ";
		var home = String(target).split("@")[1] || (service == "MemoServ" && user.services_identity && user.services_identity.home);
		this.notice(service, user, "Services are busy or unavailable; please try again later.", home);
		return true;
	}
};
IRC_Services_Manager.prototype.intercept_command = function(user, target, text) {
	if (!user.local || user.server) return false;
	var match = /^(NickServ|MemoServ)(?:@([^,\s]+))?$/i.exec(target);
	if (!match) return false;
	var service = match[1].toLowerCase() == "nickserv" ? "NickServ" : "MemoServ";
	if (!(service == "NickServ" ? this.config.nickserv : this.config.memoserv)) return false;
	var home = (match[2] || (service == "MemoServ" && user.services_identity && user.services_identity.home) || ServerName).toLowerCase();
	if (!this.ensure_agent()) { this.notice(service, user, "Local services are unavailable.", home); return true; }
	var self = this, actor = this.describe(user), a = text.trim().split(/\s+/), verb = (a.shift() || "HELP").toUpperCase();
	var via = [], central = this.config.bbs_require_tls && this.peer(this.central_peer());
	if (!(service == "NickServ" && verb == "IDENTIFY" && home == this.config.server)
		&& !(service == "MemoServ" && verb != "SEND" && !user.services_memo_routing && home == this.config.server)) via.push(central && central.servername);
	if (home != this.config.server) via.push(home);
	var tls_error = IRC_services_requires_tls(service, text) && this.tls_error(user, this.config.bbs_require_tls, via);
	if (tls_error) { this.notice(service, user, tls_error, home); return true; }
	if (user.services_last_request && system.timer - user.services_last_request < 1) { this.notice(service, user, "Please wait before sending another services command.", home); return true; }
	user.services_last_request = system.timer;
	if (service == "NickServ" && verb == "IDENTIFY") {
		if (a.length != 2) { this.notice(service, user, "Syntax: IDENTIFY <escaped-account> <password>", home); return true; }
		var account;
		try { account = IRC_services_unescape(a[0]); } catch (_) { this.notice(service, user, "Invalid escaped account name.", home); return true; }
		var attempt = user.services_auth_attempt = (user.services_auth_attempt || 0) + 1;
		var done = function(result) {
			var current = self.find(actor); if (!current || current.services_auth_attempt != attempt || (current.services_auth_revision || 0) != actor.auth_revision) return;
			var error = self.tls_error(current, self.config.bbs_require_tls, via);
			if (error) { self.notice(service, current, error, home); return; }
			if (!result.identity || result.error) { self.notice(service, current, result.error || "Authentication failed.", home); return; }
			if (!self.identity_valid(result.identity, home)) { self.notice(service, current, "Invalid authentication response.", home); return; }
			self.add_login(current, result.identity, current.services_multi && (home == self.config.server || result.multi_login == 1));
			self.notice(service, current, "Identified as " + result.identity.username + ".", home);
			var cp = self.central_peer();
			if (cp) self.sync_session(current);
			if (self.config.memoserv) self.memo(current, "STATUS", []);
		};
		if (home == this.config.server) this.job({type:"authenticate", account:account, password:a[1]}, done);
		else this.request(this.central_peer(), {type:"authenticate", actor:actor, home:home, account:account, password:a[1]}, done);
	} else if (service == "NickServ" && verb == "LOGOUT") {
		if (a.length > 1 || (a.length && a[0].toUpperCase() != "ALL")) { this.notice(service, user, "Syntax: LOGOUT [ALL]", home); return true; }
		this.remove_login(user, home, a.length == 1);
		this.sync_session(user);
		this.notice(service, user, a.length ? "All BBS logins removed." : "Logged out of " + home + ".", home);
	} else if (service == "MemoServ") this.memo(user, verb, a, match[2] && home);
	else if (verb == "HELP") this.notice(service, user, "IDENTIFY <escaped-account> <password> | LOGOUT [ALL] | REGISTER | DROP [nickname] | INFO [nickname] | GHOST <nickname> | ACCOUNTS | LINK <account+QWKID> <account+QWKID> | LINK CONFIRM <code> | UNLINK <account+QWKID> | SET <nick> MEMOACCOUNT <account+QWKID>. Use /msg NickServ@" + home + " <command> for this BBS.", home);
	else {
		var forward = function(verified) {
			if (verified.error) { if (self.find(actor)) self.notice(service, user, verified.error, home); return; }
			self.request(self.central_peer(), {type:"command", actor:self.describe(user), service:"NickServ", text:text}, function(r) { if (r.error && self.find(actor)) self.notice(service, user, r.error, home); });
		};
		if (IRC_services_requires_tls(service, text)) this.sync_session(user, forward);
		else forward({});
	}
	return true;
};
IRC_Services_Manager.prototype.command = function(user, service, text) {
	if (!this.central || user.uline || user === this.agent) return;
	var error = IRC_services_requires_tls(service, text) && this.tls_error(user, this.config.require_tls);
	if (error) { this.notice(service, user, error); return; }
	this.snapshot();
	if (!this.event({type:"user", user:this.describe(user)}) || !this.event({type:"command", actor:this.describe(user), service:service, text:text}))
		this.notice(service, user, "Services are busy; please try again.");
};
IRC_Services_Manager.prototype.memo = function(user, verb, args, home) {
	var self = this, actor = this.describe(user), destination_home = home || "";
	home = home || (user.services_identity && user.services_identity.home) || this.config.server;
	var identity = this.logins(user).filter(function(a) { return a.home == home; })[0], group_mailboxes = verb != "SEND" && user.services_memo_routing;
	if (verb == "HELP") { this.notice("MemoServ", user, "SEND <registered-nick|escaped-account+QWKID> <text> | READ | STATUS. READ/STATUS check your linked identity's mailboxes; MemoServ@server selects that BBS. SEND to a nick uses its configured destination. READ consumes telegrams; automatic checks do not. Use /msg MemoServ@" + home + " <command> for this BBS.", home); return; }
	var central = this.config.bbs_require_tls && this.peer(this.central_peer()), via = home == this.config.server && !group_mailboxes && verb != "SEND" ? [] : [central && central.servername, home];
	var error = this.tls_error(user, this.config.bbs_require_tls, via);
	if (error) { this.notice("MemoServ", user, error, home); return; }
	if (!user.services_identity) { this.notice("MemoServ", user, "Identify to your BBS account first with /msg NickServ@" + home + " IDENTIFY <escaped-account> <password>.", home); return; }
	if (["SEND","READ","STATUS"].indexOf(verb) < 0 || (verb == "SEND" && args.length < 2)) { this.notice("MemoServ", user, "Invalid command. Use HELP.", home); return; }
	if (verb == "SEND" && destination_home && !user.services_memo_routing) { this.notice("MemoServ", user, "Server-specific delivery requires updated central services; use unqualified MemoServ SEND <account+QWKID> <text> instead.", home); return; }
	if (verb != "SEND" && !group_mailboxes && !identity) { this.notice("MemoServ", user, "You may read only your authenticated account's telegrams. Use /msg MemoServ@" + user.services_identity.home + " " + verb + ".", user.services_identity.home); return; }
	var finish = function(r) { if (self.find(actor) && (user.services_auth_revision || 0) == actor.auth_revision) self.notice("MemoServ", user, self.tls_error(user, self.config.bbs_require_tls, via) || r.error || r.text || "Telegram operation completed.", r.group_mailboxes ? undefined : (r.home || home)); };
	if (verb != "SEND" && !group_mailboxes && identity.home == this.config.server)
		this.job({type:"memo", operation:verb, identity:identity}, finish);
	else this.sync_session(user, function(r) {
		if (r.error) { finish(r); return; }
		self.request(self.central_peer(), {type:"memo", actor:self.describe(user), operation:verb, home:home, destination_home:destination_home, group_mailboxes:!!group_mailboxes, read_home:destination_home, target:args.shift(), text:args.join(" ")}, finish);
	});
};
IRC_Services_Manager.prototype.receive = function(peer, m) {
	try { this.receive_message(peer, m); }
	catch (error) {
		log(LOG_WARNING, "Embedded IRC services request failed: " + error);
		if (m && typeof m.id == "string" && m.id.length <= 128 && this.peer(peer)) {
			try { this.reply(peer, m.id, {error:"Services are busy or unavailable; please try again later."}); } catch (_) {}
		}
	}
};
IRC_Services_Manager.prototype.receive_message = function(peer, m) {
	if (!m || typeof m != "object" || !this.peer(peer)) return;
	log(LOG_DEBUG, "Embedded services RPC from " + peer + " type " + m.type);
	var self = this, source = this.peer(peer), cp = this.central_peer();
	if (m.type == "reply-text") {
		var partial = this.requests[m.id];
		if (partial && partial.peer == peer.toUpperCase()) {
			if (!partial.parts) partial.parts = [];
			partial.parts.push(String(m.text));
		} return;
	}
	if (m.type == "reply") {
		var request = this.requests[m.id];
		if (request && request.peer == peer.toUpperCase()) { delete this.requests[m.id]; if (request.parts) m.text = request.parts.join("") + (m.text || ""); request.callback(m); }
		return;
	}
	if (m.type == "local-auth" || m.type == "local-memo" || m.type == "identity" || m.type == "rename" || m.type == "memo-text" || m.type == "local-validate" || m.type == "check-session" || m.type == "local-resolve") {
		if (!cp || peer.toUpperCase() != cp.toUpperCase()) return;
		var local_reply = function(r) { self.reply(peer, m.id, r); };
		if (this.config.bbs_require_tls && ["local-auth", "local-memo", "local-resolve", "local-validate"].indexOf(m.type) >= 0) {
			var tls_user = m.actor && Users[String(m.actor.nick).toUpperCase()];
			var error = this.tls_error(tls_user && tls_user.servername.toLowerCase() == m.actor.server ? tls_user : null, true, [source.servername]);
			if (error) { this.reply(peer, m.id, {error:error}); return; }
			local_reply = function(r) {
				var error = self.tls_error(Users[tls_user.nick.toUpperCase()] === tls_user ? tls_user : null, true, [source.servername]);
				self.reply(peer, m.id, error ? {error:error} : r);
			};
		}
		if (m.type == "check-session") {
			var checked = this.find(m.actor);
			this.reply(peer, m.id, {valid:!!(checked && checked.local)});
		} else if (m.type == "memo-text") {
			var recipient = this.find(m.actor);
			if (recipient && recipient.local && (m.actor.auth_revision === undefined || m.actor.auth_revision == (recipient.services_auth_revision || 0))) {
				var error = this.tls_error(recipient, this.config.bbs_require_tls, [source.servername, m.home]);
				if (error) { this.notice("MemoServ", recipient, error, m.home); return; }
				var pending_memo = this.requests[m.request_id];
				if (pending_memo && pending_memo.peer == peer.toUpperCase()) pending_memo.until = system.timer + 30;
				this.notice("MemoServ", recipient, m.text, m.home || (recipient.services_identity && recipient.services_identity.home));
			}
		} else if (m.type == "local-resolve" && this.config.memoserv) {
			this.job({type:"local-resolve", account:m.account}, local_reply);
		} else if (m.type == "local-auth" && this.config.nickserv) {
			var session = m.actor && Users[String(m.actor.nick).toUpperCase()];
			if (!session || session.servername.toLowerCase() != m.actor.server) { this.reply(peer, m.id, {error:"Session is no longer available."}); return; }
			this.job({type:"authenticate", account:m.account, password:m.password}, function(r) {
				if (r.identity && Users[session.nick.toUpperCase()] === session) self.attestations[m.actor.ref] = {actor:m.actor, identity:r.identity, user:session};
				r.multi_login = 1; local_reply(r);
			});
		} else if (m.type == "local-validate") {
			var attestation = m.actor && this.attestations[m.actor.ref];
			var online = m.actor && Users[String(m.actor.nick).toUpperCase()], proven;
			if (online && online.local && this.find(m.actor) === online) {
				proven = this.logins(online).filter(function(a) { return a.home == self.config.server && (!m.identity || a.id == m.identity.id); })[0];
			} else if (attestation && online && online === attestation.user && online.servername.toLowerCase() == m.actor.server
				&& attestation.actor.server == m.actor.server && attestation.actor.created == m.actor.created
				&& (!m.identity || attestation.identity.id == m.identity.id)) proven = attestation.identity;
			if (!proven) { this.reply(peer, m.id, {error:"Please identify to your home BBS again."}); return; }
			this.job({type:"validate", identity:proven}, function(r) { r.multi_login = 1; local_reply(r); });
		} else if (m.type == "local-memo" && this.config.memoserv) {
			this.job({type:"memo", operation:m.operation, identity:m.identity, text:m.text}, local_reply);
		} else if (m.type == "identity") {
			var changed = this.find(m.actor);
			if (!changed && m.actor) {
				var remote = Users[String(m.actor.nick).toUpperCase()];
				if (remote && !remote.local && remote.created == m.actor.created && remote.servername.toLowerCase() == m.actor.server) {
					remote.services_ref = m.actor.ref; changed = remote;
				}
			}
			if (changed && (!changed.local || m.actor.auth_revision === undefined || m.actor.auth_revision == (changed.services_auth_revision || 0))
				&& (!m.identity || this.identity_valid(m.identity, m.identity.home))) this.apply_identity(changed, m.identity);
		} else if (m.type == "rename") {
			var target = this.find(m.actor);
			if (target && target.local) target.work("NICK " + this.guest());
		} else if (m.id) this.reply(peer, m.id, {error:"Requested BBS service is disabled."});
		return;
	}
	if (!this.central) return;
	var user = this.bind_actor(peer, m.actor);
	if (!user) {
		var claimed = m.actor && Users[String(m.actor.nick).toUpperCase()];
		if (!claimed || claimed.uline || claimed.servername.toLowerCase() != source.servername.toLowerCase()
			|| m.actor.server != source.servername.toLowerCase() || !/^[0-9a-f]{64}$/.test(m.actor.ref)) return;
		/* Legacy relays can replace NICK timestamps. Ask the actual
		   connection server to confirm its random session cookie instead
		   of trusting a wall-clock timestamp or a delayed nickname match. */
		this.request(peer, {type:"check-session", actor:m.actor}, function(r) {
			if (!r.valid || Users[String(m.actor.nick).toUpperCase()] !== claimed) return;
			/* Actions already queued for this same object remain valid while
			   its provisional central cookie is replaced by the proven one.
			   Keep just that one predecessor on the object, never by nick. */
			if (claimed.services_ref != m.actor.ref) claimed.services_previous_session = self.describe(claimed);
			claimed.services_ref = m.actor.ref; claimed.services_created = m.actor.created;
			self.receive(peer, m);
		}); return;
	}
	var restricted = m.type == "authenticate" || m.type == "memo" || (m.type == "claim" && m.identity)
		|| (m.type == "sync-logins" && m.logins && m.logins.length);
	var error = restricted && this.tls_error(user, this.config.require_tls);
	if (error) { if (m.id) this.reply(peer, m.id, {error:error}); return; }
	this.snapshot();
	if (m.type == "session") {
		user.services_peer_multi = m.multi_login == 1;
		if (m.id) this.reply(peer, m.id, {multi_login:1, memo_routing:1}); return;
	}
	if (m.type == "sync-logins") { this.verify_logins(peer, user, m); return; }
	if (m.type == "claim") {
		if (user.services_peer_multi) return;
		user.services_verified_logins = undefined; user.services_verified_multi = false;
		if (this.identity_valid(m.identity, source.servername)) {
			this.claim(user, m.identity); this.publish_identity(user, m.identity);
		} else if (m.identity && m.identity.home) {
			var home = m.identity.home, actor = this.describe(user), homepeer = IRC_services_agent(home);
			this.request(this.peer(homepeer) && homepeer, {type:"local-validate", actor:actor}, function(r) {
				var current = self.find(actor); if (!current) return;
				if (r.identity && self.identity_valid(r.identity, home)) {
					self.claim(current, r.identity); self.publish_identity(current, r.identity);
				} else { self.claim(current, null); self.publish_identity(current, null, true); }
			});
		}
	} else if (m.type == "logout") { if (user.services_peer_multi) return; user.services_verified_logins = undefined; user.services_verified_multi = false; this.claim(user, null); this.publish_identity(user, null); }
	else if (m.type == "command") {
		if (m.service != "NickServ" && m.service != "ChanServ") return;
		if (IRC_services_requires_tls(m.service, m.text) && user.services_peer_multi && (m.actor.auth_revision != user.services_auth_revision || !user.services_verified_logins)) { this.reply(peer, m.id, {error:"Authentication changed; please retry."}); return; }
		this.command(user, m.service, m.text); if (m.id) this.reply(peer, m.id, {});
	} else if (m.type == "authenticate") {
		var homeagent = IRC_services_agent(String(m.home).toLowerCase()), actor = this.describe(user);
		this.request(this.peer(homeagent) && homeagent, {type:"local-auth", actor:actor, account:m.account, password:m.password}, function(r) {
			var current = self.find(actor);
			if (!current || (current.services_auth_revision || 0) != actor.auth_revision) return;
			if (r.identity && !r.error && self.identity_valid(r.identity, m.home)) {
				if (!user.services_peer_multi) { self.claim(current, r.identity); self.publish_identity(current, r.identity); }
				self.reply(peer, m.id, {identity:r.identity, multi_login:r.multi_login});
			} else self.reply(peer, m.id, {error:r.error || "Authentication failed."});
		});
	} else if (m.type == "memo") {
		var identity = m.operation == "SEND" || m.group_mailboxes ? this.identities[this.ref(user)] : user.services_verified_logins ? user.services_verified_logins.filter(function(a) { return a.home == (m.home || (user.services_identity && user.services_identity.home)); })[0] : this.identities[this.ref(user)], actor = this.describe(user);
		if (!identity) { this.reply(peer, m.id, {error:"Identify to your BBS account first."}); return; }
		if (user.services_peer_multi && m.actor.auth_revision != user.services_auth_revision) { this.reply(peer, m.id, {error:"Authentication changed; please retry."}); return; }
		if (m.operation == "SEND") {
			this.resolve_memo(m.target, function(r) {
				if (!self.find(actor) || (user.services_auth_revision || 0) != actor.auth_revision) return;
				if (!r.identity) { self.reply(peer, m.id, {error:r.error || "Unknown destination account."}); return; }
				self.deliver_memo(peer, m.id, actor, r.identity, "SEND", identity.username + " (IRC " + actor.nick + "):\r\n" + String(m.text).replace(/[\x00-\x1f\x7f]/g, " ") + "\r\n");
			}, m.destination_home, actor);
		} else if (m.operation == "READ" || m.operation == "STATUS") {
			if (!m.group_mailboxes) this.deliver_memo(peer, m.id, actor, identity, m.operation);
			else this.job({type:"memo-read", actor:actor, home:m.read_home}, function(r) {
				if (!self.find(actor) || (user.services_auth_revision || 0) != actor.auth_revision) return;
				if (!r.identities) { self.reply(peer, m.id, {error:r.error || "No authenticated mailbox is available.", home:r.home}); return; }
				self.read_memos(peer, m.id, actor, r.identities, m.operation);
			});
		}
	}
};
/* The connection server chooses retained logins; each BBS independently
   proves its exact account. Delayed validations cannot overwrite newer auth. */
IRC_Services_Manager.prototype.verify_logins = function(peer, user, m) {
	var self = this, actor = m.actor, revision = actor.auth_revision, list = m.logins, seen = {}, multi = true;
	if (!user.services_peer_multi || !Array.isArray(list) || list.length > 64 || typeof revision != "number" || !isFinite(revision) || revision < 0 || revision % 1
		|| revision < (user.services_auth_revision || 0) || revision < (user.services_pending_revision || 0)) { this.reply(peer, m.id, {error:"Invalid or obsolete login set."}); return; }
	for (var i = 0; i < list.length; i++) {
		var a = list[i];
		if (!a || !this.identity_valid(a, a.home) || seen[a.home]) { this.reply(peer, m.id, {error:"Invalid BBS login set."}); return; }
		seen[a.home] = true;
	}
	user.services_pending_revision = revision;
	var nonce = user.services_sync_nonce = {}, remaining = list.length, checked = [], finished = false, errors = [];
	function complete() {
		if (finished || remaining) return;
		finished = true;
		if (self.find(actor) !== user || user.services_sync_nonce !== nonce || (user.local && (user.services_auth_revision || 0) != revision)) { self.reply(peer, m.id, {error:"Authentication changed; please retry."}); return; }
		checked = checked.filter(function(a) { return !!a; });
		user.services_auth_revision = revision + (errors.length ? 1 : 0);
		user.services_verified_logins = checked;
		user.services_verified_multi = multi;
		if (user.local) self.set_logins(user, checked, user.services_auth_revision);
		else self.apply_identity(user, checked.length ? checked[checked.length - 1] : null);
		self.claim(user, user.services_identity); self.publish_identity(user, user.services_identity, true);
		self.reply(peer, m.id, {error:errors.length ? "A BBS login was revoked: " + errors.join("; ") : undefined, logins:checked, revision:user.services_auth_revision, multi_login:multi ? 1 : 0});
	}
	list.forEach(function(a, index) {
		var name = IRC_services_agent(a.home);
		self.request(self.peer(name) && name, {type:"local-validate", actor:actor, identity:a}, function(r) {
			if (finished) return;
			if (!r.identity || r.identity.id != a.id || !self.identity_valid(r.identity,a.home)) { errors.push(a.home + ": " + (r.error || "Identify again.")); remaining--; complete(); return; }
			if (r.multi_login != 1) multi = false;
			checked[index] = r.identity; remaining--; complete();
		});
	});
	complete();
};
IRC_Services_Manager.prototype.resolve_memo = function(target, callback, home, actor) {
	var self = this;
	this.job({type:"resolve", target:target, home:home}, function(r) {
		if (r.identity || r.error) { callback(r); return; }
		var qualified = /^(.+)\+([a-z0-9_-]{1,8})$/i.exec(String(target)), candidates = [];
		if (!qualified) { callback({error:"Unknown destination account."}); return; }
		if (home && home.split(".")[0].toUpperCase() != qualified[2].toUpperCase()) { callback({error:"The destination account does not belong to " + home + "."}); return; }
		for (var key in Users) {
			var agent = self.peer(Users[key].nick);
			if (agent && (!home || agent.servername.toLowerCase() == home) && agent.servername.split(".")[0].toUpperCase() == qualified[2].toUpperCase()) candidates.push(agent);
		}
		if (candidates.length != 1) { callback({error:"Destination BBS is unavailable or ambiguous."}); return; }
		self.request(candidates[0].nick, {type:"local-resolve", actor:actor, account:qualified[1]}, callback);
	});
};
/* Stream one mailbox at a time, retaining useful results if another BBS is
   unavailable. Recheck membership before dispatch and before exposing text. */
IRC_Services_Manager.prototype.read_memos = function(peer, id, actor, identities, operation) {
	var self = this, index = 0, failed = 0;
	function authorized(identity) {
		var user = self.find(actor);
		return user && !self.tls_error(user, self.config.require_tls) && (user.services_auth_revision || 0) == actor.auth_revision && self.queue.engine && self.queue.engine.can_read_memo(actor, identity.id);
	}
	function output(identity, text) {
		for (var i = 0; i < text.length; i += 8000) self.send(peer, {type:"memo-text", request_id:id, actor:actor, text:text.substr(i,8000), home:identity.home});
	}
	function next() {
		while (index < identities.length) {
			var identity = identities[index++];
			if (!authorized(identity)) { self.reply(peer, id, {error:"Authentication or account group changed; please retry.", group_mailboxes:true}); return; }
			output(identity, "Telegram mailbox " + identity.username + "@" + identity.home + ":");
			var agent = IRC_services_agent(identity.home);
			if (!self.peer(agent)) { failed++; output(identity, "Destination BBS services are unavailable."); continue; }
			self.request(agent, {type:"local-memo", actor:actor, identity:identity, operation:operation}, function(r) {
				if (!authorized(identity)) { self.reply(peer, id, {error:"Authentication or account group changed; please retry.", group_mailboxes:true}); return; }
				if (r.error) failed++;
				output(identity, String(r.error || r.text || "Telegram operation completed."));
				next();
			});
			return;
		}
		self.reply(peer, id, {text:"Checked " + index + " telegram mailbox" + (index == 1 ? "" : "es") + (failed ? " (" + failed + " failed)" : "") + ".", group_mailboxes:true});
	}
	next();
};
IRC_Services_Manager.prototype.deliver_memo = function(peer, id, actor, identity, operation, text) {
	var self = this, name = IRC_services_agent(identity.home);
	this.request(this.peer(name) && name, {type:"local-memo", actor:actor, identity:identity, operation:operation, text:text}, function(r) {
		var current = self.find(actor);
		if (!current || (actor.auth_revision !== undefined && (current.services_auth_revision || 0) != actor.auth_revision)) return;
		/* Keep each RPC below the framing limit, including a large native
		   telegram spool. Partial replies are notices; the final reply ACKs. */
		var all = String(r.text || "");
		for (var i = 0; i < all.length; i += 8000) self.send(peer, {type:"memo-text", actor:actor, text:all.substr(i,8000), home:identity.home});
		self.reply(peer, id, {error:r.error, text:all ? "End of telegram response." : "Telegram operation completed.", home:identity.home});
	});
};
IRC_Services_Manager.prototype.publish_identity = function(user, identity, include_connection) {
	var actor = this.describe(user);
	this.apply_identity(user, identity);
	for (var key in Users) {
		var u = Users[key];
		if (this.peer(u.nick) && u !== this.agent && (include_connection || u.servername.toLowerCase() != user.servername.toLowerCase())) this.send(u.nick, {type:"identity", actor:actor, identity:identity});
	}
};
IRC_Services_Manager.prototype.guest = function() {
	var nick;
	do { nick = "Guest" + IRC_services_random().substr(0, 12); } while (Users[nick.toUpperCase()]);
	return nick;
};
IRC_Services_Manager.prototype.action = function(a) {
	if (!this.central) return;
	/* A membership change need not change the active BBS login. Check the
	   ownership behind queued actions as well as their authentication revision. */
	var engine = this.queue.engine;
	if (a.type != "notice" && engine && ((a.actor && a.actor_groups !== undefined && a.actor_groups != engine.group_signature(a.actor))
		|| (a.target && a.target_groups !== undefined && a.target_groups != engine.group_signature(a.target)))) return;
	var user = a.actor && this.find(a.actor), target = a.target && this.find(a.target);
	if (a.actor && (!user || (a.actor.auth_revision !== undefined && a.actor.auth_revision != (user.services_auth_revision || 0)) || a.actor.account != (user.services_identity ? user.services_identity.id : null))) return;
	if (a.actor && a.require_tls && this.tls_error(user, this.config.require_tls)) return;
	if (a.target && (!target || (a.target.auth_revision !== undefined && a.target.auth_revision != (target.services_auth_revision || 0)) || a.target.account != (target.services_identity ? target.services_identity.id : null))) return;
	if (a.type == "notice") { this.notice(a.service, user, a.text); return; }
	if (a.type == "join") {
		this.chanserv.do_join(a.channel);
		var joined = Channels[a.channel.toUpperCase()];
		if (joined && !joined.modelist[CHANMODE_OP][this.chanserv.id]) this.service_server.set_chanmode(joined, ["+o", "ChanServ"], 0);
		return;
	}
	if (a.type == "kill") {
		server_bcast_to_servers(":" + this.config.host + " KILL " + target.nick + " :Nickname reclaimed by its BBS account owner");
		target.quit("Nickname reclaimed by its BBS account owner", true); return;
	}
	if (a.type == "rename") {
		if (target.local) target.work("NICK " + this.guest());
		else {
			var agent = IRC_services_agent(target.servername);
			if (this.peer(agent)) this.send(agent, {type:"rename", actor:this.describe(target)});
			else {
				/* Old daemons have no safe forced-rename command. Reclaim via
				   standard KILL, rather than impersonating a user's NICK. */
				server_bcast_to_servers(":" + this.config.host + " KILL " + target.nick + " :Registered nickname; identify using an updated IRC server");
				target.quit("Registered nickname; connect through an updated IRC server", true);
			}
		} return;
	}
	if (a.type == "part") { this.chanserv.do_part(a.channel); return; }
	var c = a.channel && Channels[a.channel.name.toUpperCase()];
	if (!c || c.services_ref != a.channel.ref) return;
	if (a.type == "exception") {
		var entry = IRC_exception_parse(a.mask);
		if (!entry) return;
		var policy = engine && engine.store.data.channels[IRC_services_key(c.nam)];
		var saved = policy && policy.exceptions && policy.exceptions[IRC_services_key(entry.value)];
		/* Delayed restoration must not revive a deleted grant, nor may a
		   queued DEL erase a grant that has since been added again. */
		if ((a.add && !saved) || (!a.add && saved)) return;
		if (a.add && !c.is_str_member_of_chanmode_list(CHANMODE_EXCEPT, entry.value)
			&& c.count_modelist(CHANMODE_EXCEPT) >= IRC_MAX_EXCEPTIONS) {
			if (!c.services_exception_full) log(LOG_WARNING, "ChanServ cannot restore managed exceptions for " + c.nam + ": exception list is full");
			c.services_exception_full = true; return;
		}
		delete c.services_exception_full;
		this.service_server.set_chanmode(c, [a.add ? "+e" : "-e", entry.value], 0);
	} else if (a.type == "mode") this.service_server.set_chanmode(c, [a.mode].concat(target ? [target.nick] : a.arg ? [a.arg] : []), 0);
	else if (a.type == "topic") {
		c.topic = a.text; c.topictime = Math.max(Epoch(), c.topictime + 1); c.topicchangedby = "ChanServ";
		this.service_server.bcast_to_channel(c, "TOPIC " + c.nam + " :" + c.topic, false);
		IRC_bcast_topic(this.service_server, c);
	} else if (a.type == "invite") {
		target.invited = c.nam; target.originatorout("INVITE " + target.nick + " :" + c.nam, this.chanserv);
	} else if (a.type == "unban") {
		for (var i = c.modelist[CHANMODE_BAN].length - 1; i >= 0; i--)
			if (wildmatch(target.nuh, c.modelist[CHANMODE_BAN][i])) this.service_server.set_chanmode(c, ["-b", c.modelist[CHANMODE_BAN][i]], 0);
	}
};
