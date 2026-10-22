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
	u.parent = 0; u.uprefix = "services"; u.hostname = home; u.created = Epoch(); u.hops = 0;
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
		rev:user.nick, username:user.uprefix, account:user.services_identity ? user.services_identity.id : null};
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
IRC_Services_Manager.prototype.notice = function(service, user, text) {
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
			key:c.arg[CHANMODE_KEY], limit:c.arg[CHANMODE_LIMIT], topic:c.topic, users:members};
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
						this.send(cp, {type:"session", actor:this.describe(sessionuser)});
					}
				}
			}
			if (cp && (this.peer(cp) !== this.lastauthority || now >= (this.nextclaim || 0))) {
				this.nextclaim = now + 30; this.lastauthority = this.peer(cp);
				for (var id in Local_Users) {
					var u = Local_Users[id];
					if (u.services_identity)
						this.send(cp, {type:"claim", actor:this.describe(u), identity:u.services_identity});
				}
			} else if (!cp) this.lastauthority = null;
		}
	} catch (e) { log(LOG_ERR, "Embedded IRC services callback failed: " + e); }
};
IRC_Services_Manager.prototype.refresh_local_identity = function(user, now) {
	var identity = user.services_identity;
	if (!identity || identity.home != this.config.server || now < (user.services_validate_at || 0)
		|| Object.keys(this.jobs).length >= 64) return;
	var self = this, actor = this.describe(user);
	user.services_validate_at = now + 30;
	this.job({type:"validate", identity:identity}, function(r) {
		var current = self.find(actor);
		if (!current || current.services_identity !== identity) return;
		/* A transient I/O failure may revoke a session, but cannot grant
		   access to a replacement or disabled BBS account. */
		var checked = r.identity || null;
		self.apply_identity(current, checked);
		var cp = self.central_peer();
		if (cp) self.send(cp, checked ? {type:"claim", actor:self.describe(current), identity:checked} : {type:"logout", actor:self.describe(current)});
	});
};
IRC_Services_Manager.prototype.request = function(peer, message, callback) {
	if (!peer) { callback({error:"Services are unavailable."}); return; }
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
	if (!this.event({type:"user", user:this.describe(user)}) || !this.event({type:"identify", ref:this.ref(user), identity:identity})) throw new Error("Services are busy");
	this.identities[this.ref(user)] = identity;
};
IRC_Services_Manager.prototype.intercept = function(user, target, text) {
	try { return this.intercept_command(user, target, text); }
	catch (error) {
		log(LOG_WARNING, "Embedded IRC services command failed: " + error);
		this.notice(/^MemoServ/i.test(target) ? "MemoServ" : "NickServ", user, "Services are busy or unavailable; please try again later.");
		return true;
	}
};
IRC_Services_Manager.prototype.intercept_command = function(user, target, text) {
	if (!user.local || user.server) return false;
	var match = /^(NickServ|MemoServ)(?:@([^,\s]+))?$/i.exec(target);
	if (!match) return false;
	var service = match[1].toLowerCase() == "nickserv" ? "NickServ" : "MemoServ";
	if (!(service == "NickServ" ? this.config.nickserv : this.config.memoserv)) return false;
	if (!this.ensure_agent()) { this.notice(service, user, "Local services are unavailable."); return true; }
	var self = this, actor = this.describe(user), a = text.trim().split(/\s+/), verb = (a.shift() || "HELP").toUpperCase();
	if (user.services_last_request && system.timer - user.services_last_request < 1) { this.notice(service, user, "Please wait before sending another services command."); return true; }
	user.services_last_request = system.timer;
	var home = (match[2] || ServerName).toLowerCase();
	if (service == "NickServ" && verb == "IDENTIFY") {
		if (a.length != 2) { this.notice(service, user, "Syntax: IDENTIFY <escaped-account> <password>"); return true; }
		var account;
		try { account = IRC_services_unescape(a[0]); } catch (_) { this.notice(service, user, "Invalid escaped account name."); return true; }
		var done = function(result) {
			var current = self.find(actor); if (!current) return;
			if (!result.identity || result.error) { self.notice(service, current, result.error || "Authentication failed."); return; }
			if (!self.identity_valid(result.identity, home)) { self.notice(service, current, "Invalid authentication response."); return; }
			self.apply_identity(current, result.identity);
			self.notice(service, current, "Identified as " + result.identity.username + ".");
			var cp = self.central_peer();
			if (home == self.config.server && cp) self.send(cp, {type:"claim", actor:self.describe(current), identity:result.identity});
			if (self.config.memoserv) self.memo(current, "STATUS", [], home);
		};
		if (home == this.config.server) this.job({type:"authenticate", account:account, password:a[1]}, done);
		else this.request(this.central_peer(), {type:"authenticate", actor:actor, home:home, account:account, password:a[1]}, done);
	} else if (service == "NickServ" && verb == "LOGOUT") {
		this.apply_identity(user, null);
		var cp = this.central_peer(); if (cp) this.send(cp, {type:"logout", actor:this.describe(user)});
		this.notice(service, user, "You are logged out.");
	} else if (service == "MemoServ") this.memo(user, verb, a, match[2] && home);
	else if (verb == "HELP") this.notice(service, user, "IDENTIFY <escaped-account> <password> | LOGOUT | REGISTER | DROP [nickname] | INFO [nickname] | GHOST <nickname>. Use NickServ@server to choose the BBS.");
	else this.request(this.central_peer(), {type:"command", actor:actor, service:"NickServ", text:text}, function(r) { if (r.error && self.find(actor)) self.notice(service, user, r.error); });
	return true;
};
IRC_Services_Manager.prototype.command = function(user, service, text) {
	if (!this.central || user.uline || user === this.agent) return;
	this.snapshot();
	if (!this.event({type:"user", user:this.describe(user)}) || !this.event({type:"command", actor:this.describe(user), service:service, text:text}))
		this.notice(service, user, "Services are busy; please try again.");
};
IRC_Services_Manager.prototype.memo = function(user, verb, args, home) {
	var self = this, actor = this.describe(user);
	if (verb == "HELP") { this.notice("MemoServ", user, "SEND <registered-nick|escaped-account+QWKID> <text> | READ | STATUS. READ consumes BBS telegrams; automatic checks do not."); return; }
	if (!user.services_identity) { this.notice("MemoServ", user, "Identify to your BBS account first."); return; }
	if (["SEND","READ","STATUS"].indexOf(verb) < 0 || (verb == "SEND" && args.length < 2)) { this.notice("MemoServ", user, "Invalid command. Use HELP."); return; }
	if (home && verb != "SEND" && home != user.services_identity.home) { this.notice("MemoServ", user, "You may read only your authenticated account's telegrams."); return; }
	var finish = function(r) { if (self.find(actor)) self.notice("MemoServ", user, r.error || r.text || "Telegram operation completed."); };
	if (verb != "SEND" && user.services_identity.home == this.config.server)
		this.job({type:"memo", operation:verb, identity:user.services_identity}, finish);
	else this.request(this.central_peer(), {type:"memo", actor:actor, operation:verb, target:args.shift(), text:args.join(" ")}, finish);
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
		if (m.type == "check-session") {
			var checked = this.find(m.actor);
			this.reply(peer, m.id, {valid:!!(checked && checked.local)});
		} else if (m.type == "memo-text") {
			var recipient = this.find(m.actor); if (recipient && recipient.local) this.notice("MemoServ", recipient, m.text);
		} else if (m.type == "local-resolve" && this.config.memoserv) {
			this.job({type:"local-resolve", account:m.account}, function(r) { self.reply(peer, m.id, r); });
		} else if (m.type == "local-auth" && this.config.nickserv) {
			var session = m.actor && Users[String(m.actor.nick).toUpperCase()];
			if (!session || session.servername.toLowerCase() != m.actor.server) { this.reply(peer, m.id, {error:"Session is no longer available."}); return; }
			this.job({type:"authenticate", account:m.account, password:m.password}, function(r) {
				if (r.identity && Users[session.nick.toUpperCase()] === session) self.attestations[m.actor.ref] = {actor:m.actor, identity:r.identity, user:session};
				self.reply(peer, m.id, r);
			});
		} else if (m.type == "local-validate") {
			var attestation = m.actor && this.attestations[m.actor.ref];
			var online = m.actor && Users[String(m.actor.nick).toUpperCase()];
			if (!attestation || !online || online !== attestation.user || online.servername.toLowerCase() != m.actor.server
				|| attestation.actor.server != m.actor.server || attestation.actor.created != m.actor.created) {
				this.reply(peer, m.id, {error:"Please identify to your home BBS again."}); return;
			}
			this.job({type:"validate", identity:attestation.identity}, function(r) { self.reply(peer, m.id, r); });
		} else if (m.type == "local-memo" && this.config.memoserv) {
			this.job({type:"memo", operation:m.operation, identity:m.identity, text:m.text}, function(r) { self.reply(peer, m.id, r); });
		} else if (m.type == "identity") {
			var changed = this.find(m.actor);
			if (!changed && m.actor) {
				var remote = Users[String(m.actor.nick).toUpperCase()];
				if (remote && !remote.local && remote.created == m.actor.created && remote.servername.toLowerCase() == m.actor.server) {
					remote.services_ref = m.actor.ref; changed = remote;
				}
			}
			if (changed && (!m.identity || this.identity_valid(m.identity, m.identity.home))) this.apply_identity(changed, m.identity);
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
	this.snapshot();
	if (m.type == "session") return;
	if (m.type == "claim") {
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
	} else if (m.type == "logout") { this.claim(user, null); this.publish_identity(user, null); }
	else if (m.type == "command") {
		if (m.service != "NickServ" && m.service != "ChanServ") return;
		this.command(user, m.service, m.text); if (m.id) this.reply(peer, m.id, {});
	} else if (m.type == "authenticate") {
		var homeagent = IRC_services_agent(String(m.home).toLowerCase()), actor = this.describe(user);
		this.request(this.peer(homeagent) && homeagent, {type:"local-auth", actor:actor, account:m.account, password:m.password}, function(r) {
			var current = self.find(actor);
			if (!current) return;
			if (r.identity && !r.error && self.identity_valid(r.identity, m.home)) {
				self.claim(current, r.identity); self.publish_identity(current, r.identity);
				self.reply(peer, m.id, {identity:r.identity});
			} else self.reply(peer, m.id, {error:r.error || "Authentication failed."});
		});
	} else if (m.type == "memo") {
		var identity = this.identities[this.ref(user)], actor = this.describe(user);
		if (!identity) { this.reply(peer, m.id, {error:"Identify to your BBS account first."}); return; }
		if (m.operation == "SEND") {
			this.resolve_memo(m.target, function(r) {
				if (!self.find(actor)) return;
				if (!r.identity) { self.reply(peer, m.id, {error:r.error || "Unknown destination account."}); return; }
				self.deliver_memo(peer, m.id, actor, r.identity, "SEND", identity.username + " (IRC " + actor.nick + "):\r\n" + String(m.text).replace(/[\x00-\x1f\x7f]/g, " ") + "\r\n");
			});
		} else if (m.operation == "READ" || m.operation == "STATUS") this.deliver_memo(peer, m.id, actor, identity, m.operation);
	}
};
IRC_Services_Manager.prototype.resolve_memo = function(target, callback) {
	var self = this;
	this.job({type:"resolve", target:target}, function(r) {
		if (r.identity || r.error) { callback(r); return; }
		var qualified = /^(.+)\+([a-z0-9_-]{1,8})$/i.exec(String(target)), candidates = [];
		if (!qualified) { callback({error:"Unknown destination account."}); return; }
		for (var key in Users) {
			var agent = self.peer(Users[key].nick);
			if (agent && agent.servername.split(".")[0].toUpperCase() == qualified[2].toUpperCase()) candidates.push(agent);
		}
		if (candidates.length != 1) { callback({error:"Destination BBS is unavailable or ambiguous."}); return; }
		self.request(candidates[0].nick, {type:"local-resolve", account:qualified[1]}, callback);
	});
};
IRC_Services_Manager.prototype.deliver_memo = function(peer, id, actor, identity, operation, text) {
	var self = this, name = IRC_services_agent(identity.home);
	this.request(this.peer(name) && name, {type:"local-memo", identity:identity, operation:operation, text:text}, function(r) {
		if (!self.find(actor)) return;
		/* Keep each RPC below the framing limit, including a large native
		   telegram spool. Partial replies are notices; the final reply ACKs. */
		var all = String(r.text || "");
		for (var i = 0; i < all.length; i += 8000) self.send(peer, {type:"memo-text", actor:actor, text:all.substr(i,8000)});
		self.reply(peer, id, {error:r.error, text:all ? "End of telegram response." : "Telegram operation completed."});
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
	var user = a.actor && this.find(a.actor), target = a.target && this.find(a.target);
	if (a.actor && (!user || a.actor.account != (user.services_identity ? user.services_identity.id : null))) return;
	if (a.target && (!target || a.target.account != (target.services_identity ? target.services_identity.id : null))) return;
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
	if (a.type == "mode") this.service_server.set_chanmode(c, [a.mode].concat(target ? [target.nick] : a.arg ? [a.arg] : []), 0);
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
