/* Embedded NickServ/ChanServ policy and durable registration storage.
 * No IRC sockets or daemon globals are used here. GPL-2.0-or-later.
 */
"use strict";

function IRC_Services_Engine(store, config, emit) {
	this.store = store; this.config = config; this.emit = emit;
	this.users = {}; this.channels = {}; this.sessions = {}; this.deadlines = {};
	this.nick_usage = {}; this.channel_usage = {};
	/* Allow the initial network snapshot and account attestations to arrive
	   before expiring anything. Maintenance and writes run once per minute. */
	this.next_maintenance = system.timer + 60;
}

IRC_Services_Engine.prototype.now = function() { return Date.now(); };
IRC_Services_Engine.prototype.note_nick = function(user, now) {
	var key = IRC_services_key(user.nick), owner = this.store.data.nicks[key];
	if (owner && this.sessions[user.ref] == owner)
		this.nick_usage[key] = Math.max(now, this.nick_usage[key] || 0, this.store.data.nick_last_used[key]);
};
IRC_Services_Engine.prototype.note_channel = function(channel, now) {
	if (!channel) return;
	var key = IRC_services_key(channel.name), record = this.store.data.channels[key], self = this;
	if (record && channel.users.some(function(member) {
		var user = self.users[member.ref];
		return user && self.level(user, channel) > 0;
	})) this.channel_usage[key] = Math.max(now, this.channel_usage[key] || 0, record.last_used);
};
IRC_Services_Engine.prototype.note_user = function(ref, now) {
	var user = this.users[ref];
	if (!user || !this.sessions[ref]) return;
	this.note_nick(user, now);
	for (var k in this.channels) {
		var channel = this.channels[k];
		if (this.member(channel, ref) && this.level(user, channel) > 0) this.note_channel(channel, now);
	}
};
IRC_Services_Engine.prototype.flush_usage = function() {
	var nicks = {}, channels = {}, data = this.store.data, self = this;
	Object.keys(this.nick_usage).forEach(function(k) {
		if (data.nicks[k] && self.nick_usage[k] > data.nick_last_used[k]) nicks[k] = self.nick_usage[k];
	});
	Object.keys(this.channel_usage).forEach(function(k) {
		if (data.channels[k] && self.channel_usage[k] > data.channels[k].last_used) channels[k] = self.channel_usage[k];
	});
	this.store.update_usage(nicks, channels);
	this.nick_usage = {}; this.channel_usage = {};
};
IRC_Services_Engine.prototype.checkpoint = function() {
	var now = this.now();
	for (var ref in this.users) this.note_nick(this.users[ref], now);
	for (var k in this.channels) this.note_channel(this.channels[k], now);
	this.flush_usage();
};
IRC_Services_Engine.prototype.expire = function(now) {
	var data = this.store.data, self = this;
	var nicks = Object.keys(data.nicks).filter(function(k) {
		return self.config.nick_expire_days > 0 && now - data.nick_last_used[k] >= self.config.nick_expire_days * 86400000;
	});
	var channels = Object.keys(data.channels).filter(function(k) {
		return self.config.channel_expire_days > 0 && now - data.channels[k].last_used >= self.config.channel_expire_days * 86400000;
	});
	if (!nicks.length && !channels.length) return;
	var parted = channels.map(function(k) { return data.channels[k].name; });
	this.commit(function(next) {
		nicks.forEach(function(k) { delete next.nicks[k]; delete next.nick_last_used[k]; });
		channels.forEach(function(k) { delete next.channels[k]; });
	});
	parted.forEach(function(name) { self.emit({type:"part", channel:name}); });
	log(LOG_INFO, format("Embedded services expired %u nickname and %u channel registrations", nicks.length, channels.length));
};
IRC_Services_Engine.prototype.notice = function(service, user, text) {
	this.emit({type: "notice", service: service, actor: user, text: text});
};
IRC_Services_Engine.prototype.account = function(name) { return this.store.data.nicks[IRC_services_key(name)]; };
IRC_Services_Engine.prototype.level = function(user, channel) {
	var account = this.sessions[user.ref], record = this.store.data.channels[IRC_services_key(channel.name)];
	if (!record || !account) return 0;
	return record.founder == account ? 100 : (record.access[account] || 0);
};
IRC_Services_Engine.prototype.commit = function(change) {
	var next = IRC_services_copy(this.store.data); change(next); this.store.save(next);
};
IRC_Services_Engine.prototype.member = function(channel, ref) {
	for (var i = 0; i < channel.users.length; i++) if (channel.users[i].ref == ref) return channel.users[i];
	return null;
};
IRC_Services_Engine.prototype.by_nick = function(nick) {
	for (var ref in this.users) if (IRC_services_key(this.users[ref].nick) == IRC_services_key(nick)) return this.users[ref];
	return null;
};
IRC_Services_Engine.prototype.enforce = function(channel) {
	var record = this.store.data.channels[IRC_services_key(channel.name)], self = this;
	if (!record) return;
	this.emit({type: "join", channel: channel.name});
	channel.users.forEach(function(member) {
		var user = self.users[member.ref], level = user && self.level(user, channel);
		if (level >= 50 && !member.op) self.emit({type: "mode", channel: channel, target: user, mode: "+o"});
		else if (level >= 10 && !member.voice && !member.op) self.emit({type: "mode", channel: channel, target: user, mode: "+v"});
	});
	var lock = record.mlock, current = channel.flags, minus = "", plus = "";
	for (var i = 0; i < lock.off.length; i++) if (current.indexOf(lock.off[i]) >= 0) minus += lock.off[i];
	for (i = 0; i < lock.on.length; i++) if (current.indexOf(lock.on[i]) < 0) plus += lock.on[i];
	if (minus) this.emit({type: "mode", channel: channel, mode: "-" + minus});
	if (plus) this.emit({type: "mode", channel: channel, mode: "+" + plus});
	if (lock.key !== undefined && channel.key != lock.key)
		this.emit({type: "mode", channel: channel, mode: lock.key ? "+k" : "-k", arg: lock.key || "*"});
	if (lock.limit !== undefined && channel.limit != lock.limit)
		this.emit({type: "mode", channel: channel, mode: lock.limit ? "+l" : "-l", arg: lock.limit ? String(lock.limit) : ""});
};
IRC_Services_Engine.prototype.event = function(event) {
	var self = this, ref, old, record;
	if (event.type == "reset") {
		this.checkpoint();
		this.users = {}; this.channels = {}; this.sessions = {}; this.deadlines = {};
		return;
	}
	if (event.type == "user") {
		this.note_user(event.user.ref, this.now());
		old = this.users[event.user.ref]; this.users[event.user.ref] = event.user;
		this.note_user(event.user.ref, this.now());
		var owner = this.account(event.user.nick);
		if ((!old || old.nick != event.user.nick) && owner && this.sessions[event.user.ref] != owner)
			{
			this.notice("NickServ", event.user, "This nickname is registered. Identify to your BBS account within 60 seconds or change nicknames.");
			this.deadlines[event.user.ref] = {nick: event.user.nick, at: system.timer + 60};
		}
		return;
	}
	if (event.type == "quit") { this.note_user(event.ref, this.now()); delete this.users[event.ref]; delete this.sessions[event.ref]; delete this.deadlines[event.ref]; return; }
	if (event.type == "identify") {
		var identified = this.users[event.ref], identity = event.identity;
		if (!identified) return;
		this.note_user(event.ref, this.now());
		if (identity) {
			if (JSON.stringify(this.store.data.accounts[identity.id]) != JSON.stringify(identity))
				this.commit(function(next) { next.accounts[identity.id] = identity; });
			this.sessions[event.ref] = identity.id;
		} else delete this.sessions[event.ref];
		this.note_user(event.ref, this.now());
		if (this.account(identified.nick) && this.account(identified.nick) != this.sessions[event.ref]) {
			/* Periodic session revalidation must not renew an impostor's
			   grace period indefinitely. */
			if (!this.deadlines[event.ref] || this.deadlines[event.ref].nick != identified.nick)
				this.deadlines[event.ref] = {nick:identified.nick, at:system.timer + 60};
		} else delete this.deadlines[event.ref];
		Object.keys(this.channels).forEach(function(k) { self.enforce(self.channels[k]); });
		return;
	}
	if (event.type == "tick") {
		if (system.timer >= this.next_maintenance) {
			this.checkpoint(); this.expire(this.now());
			this.next_maintenance = system.timer + 60;
		}
		Object.keys(this.deadlines).forEach(function(ref) {
			var d = self.deadlines[ref], u = self.users[ref], owner = u && self.account(u.nick);
			if (!u || u.nick != d.nick || !owner || owner == self.sessions[ref]) { delete self.deadlines[ref]; return; }
			if (system.timer >= d.at) { delete self.deadlines[ref]; self.emit({type:"rename", target:u}); }
		}); return;
	}
	if (event.type == "channel_gone") { this.note_channel(this.channels[event.ref], this.now()); delete this.channels[event.ref]; return; }
	if (event.type == "channel") {
		this.note_channel(this.channels[event.channel.ref], this.now());
		old = this.channels[event.channel.ref]; this.channels[event.channel.ref] = event.channel;
		this.note_channel(event.channel, this.now());
		record = this.store.data.channels[IRC_services_key(event.channel.name)];
		if (record && record.keeptopic && old && old.topic != event.channel.topic) {
			this.commit(function(data) { data.channels[IRC_services_key(event.channel.name)].topic = event.channel.topic; });
		} else if (record && record.keeptopic && !old && record.topic != event.channel.topic) {
			this.emit({type: "topic", channel: event.channel, text: record.topic});
		}
		this.enforce(event.channel);
		return;
	}
	if (event.type == "boot") {
		Object.keys(this.store.data.channels).forEach(function(key) { self.emit({type: "join", channel: self.store.data.channels[key].name}); });
		return;
	}
	if (event.type == "command") {
		var user = this.users[event.actor.ref];
		if (!user || user.rev != event.actor.rev || user.nick != event.actor.nick) return;
		try { this.command(event.service, user, event.text); }
		catch (e) { this.notice(event.service, user, e.services_user ? e.message : "Services storage failed; please try again later."); if (!e.services_user) log(LOG_ERR, "Embedded services command failed: " + e); }
	}
};

IRC_Services_Engine.prototype.command = function(service, user, text) {
	var args = text.trim().split(/\s+/), verb = (args.shift() || "HELP").toUpperCase();
	if (args.some(function(a) { return /[\x00-\x1f\x7f]/.test(a); })) throw IRC_services_error("Control characters are not allowed.");
	if (service == "NickServ") this.nick_command(user, verb, args);
	else this.channel_command(user, verb, args);
};

IRC_Services_Engine.prototype.nick_command = function(user, verb, args) {
	var self = this, data = this.store.data, key = IRC_services_key(user.nick), account = this.sessions[user.ref];
	function reply(text) { self.notice("NickServ", user, text); }
	if (verb == "HELP") { reply("REGISTER | DROP [nickname] | INFO [nickname] | GHOST <nickname>. Identify using NickServ@your-bbs-server IDENTIFY <escaped-account> <password>."); return; }
	if (verb == "INFO") {
		var info_key = IRC_services_key(args[0] || user.nick), owner = data.nicks[info_key];
		if (!owner) throw IRC_services_error("That nickname is not registered.");
		var a = data.accounts[owner]; reply("Registered to " + IRC_services_escape(a.name) + "+" + a.qwk
			+ ". Last used: " + new Date(Math.max(data.nick_last_used[info_key], this.nick_usage[info_key] || 0)).toISOString() + "."); return;
	}
	if (!account) throw IRC_services_error("Identify to your BBS account first.");
	if (verb == "REGISTER" || verb == "GROUP") {
		if (args.length) throw IRC_services_error("Syntax: REGISTER (claims your current nickname)");
		if (data.nicks[key]) throw IRC_services_error("That nickname is already registered.");
		var count = Object.keys(data.nicks).filter(function(k) { return data.nicks[k] == account; }).length;
		if (count >= 20) throw IRC_services_error("An account may register at most 20 nicknames.");
		var now = this.now();
		this.commit(function(next) { next.nicks[key] = account; next.nick_last_used[key] = now; });
		delete this.nick_usage[key]; reply("Nickname registered to your BBS account.");
	} else if (verb == "DROP") {
		key = IRC_services_key(args[0] || user.nick);
		if (data.nicks[key] != account) throw IRC_services_error("That nickname belongs to another account.");
		this.commit(function(next) { delete next.nicks[key]; delete next.nick_last_used[key]; });
		delete this.nick_usage[key]; reply("Nickname registration dropped.");
	} else if (verb == "GHOST") {
		if (args.length != 1 || this.account(args[0]) != account) throw IRC_services_error("Syntax: GHOST <nickname> (must belong to your account)");
		var target = this.by_nick(args[0]);
		if (!target || target.ref == user.ref) throw IRC_services_error("No other session is using that nickname.");
		this.emit({type:"kill", actor:user, target:target}); reply("Ghost session removal requested.");
	} else throw IRC_services_error("Unknown command. Use HELP.");
};

function IRC_services_error(message) { var e = new Error(message); e.services_user = true; return e; }

function IRC_services_mlock(args) {
	var modes = args.shift(), lock = {on: "", off: ""}, add = true;
	if (!modes || !/^[+-][imnpstkl+-]+$/.test(modes)) throw IRC_services_error("MLOCK accepts +/-imnpstkl; use MLOCK NONE to clear it.");
	for (var i = 0; i < modes.length; i++) {
		var c = modes[i];
		if (c == "+" || c == "-") { add = c == "+"; continue; }
		if (c == "k") {
			lock.key = add ? args.shift() : "";
			if (lock.key === undefined || (add && (!lock.key.length || lock.key.length > 64))) throw IRC_services_error("MLOCK +k requires a key of at most 64 characters.");
		} else if (c == "l") {
			lock.limit = add ? Number(args.shift()) : 0;
			if (add && (!isFinite(lock.limit) || lock.limit < 1 || lock.limit > 99999 || lock.limit % 1)) throw IRC_services_error("MLOCK +l requires a limit from 1 to 99999.");
		} else {
			lock.on = lock.on.replace(c, ""); lock.off = lock.off.replace(c, "");
			lock[add ? "on" : "off"] += c;
		}
	}
	if (args.length || (lock.on.indexOf("s") >= 0 && lock.on.indexOf("p") >= 0)) throw IRC_services_error("Invalid or conflicting MLOCK parameters.");
	return lock;
}

IRC_Services_Engine.prototype.channel_command = function(user, verb, args) {
	var self = this, data = this.store.data, name = args.shift(), key = IRC_services_key(name), record = data.channels[key], channel;
	function reply(text) { self.notice("ChanServ", user, text); }
	if (verb == "HELP") { reply("REGISTER #channel <description> | INFO #channel | DROP #channel | ACCESS #channel ADD|DEL|LIST ... | OP|DEOP|VOICE|DEVOICE|INVITE|UNBAN #channel [nickname] | TOPIC #channel <text> | SET #channel MLOCK|KEEPTOPIC ..."); return; }
	if (!name || !/^#[^\s,\x00-\x1f]{1,99}$/.test(name)) throw IRC_services_error("Specify a valid #channel. Use HELP for command syntax.");
	for (var ref in this.channels) if (IRC_services_key(this.channels[ref].name) == key) channel = this.channels[ref];
	var account = this.sessions[user.ref], member = channel && this.member(channel, user.ref), level = channel ? this.level(user, channel) : 0;
	if (verb == "REGISTER") {
		if (record) throw IRC_services_error("That channel is already registered.");
		if (!account || !member || !member.op) throw IRC_services_error("Identify to NickServ and obtain channel ops before registering it.");
		if (!args.length) throw IRC_services_error("Syntax: REGISTER #channel <description>");
		if (Object.keys(data.channels).length >= 50000) throw IRC_services_error("Registration limit reached.");
		var now = this.now();
		this.commit(function(next) { next.channels[key] = {name: channel.name, founder: account, description: args.join(" ").substr(0, 200),
			registered: now, last_used: now, access: {}, mlock: {on: "", off: ""}, keeptopic: false, topic: channel.topic}; });
		delete this.channel_usage[key];
		this.enforce(channel); reply("Channel registered to your account."); return;
	}
	if (!record) throw IRC_services_error("That channel is not registered.");
	/* INFO must not reveal a hidden channel to outsiders. */
	if (!member && (!account || (record.founder != account && !record.access[account]))
		&& (!channel || /[sp]/.test(channel.flags))) throw IRC_services_error("That channel is unavailable.");
	if (verb == "INFO") { reply(record.name + ": " + record.description + " Founder: " + data.accounts[record.founder].name
		+ ". Last used: " + new Date(Math.max(record.last_used, this.channel_usage[key] || 0)).toISOString() + "."); return; }
	if (!account || (record.founder != account && !record.access[account])) throw IRC_services_error("Identify to an account with access to that channel.");
	if (!channel) level = record.founder == account ? 100 : record.access[account];
	if (verb == "DROP") {
		if (level < 100) throw IRC_services_error("Only the founder can drop the channel.");
		this.commit(function(next) { delete next.channels[key]; });
		delete this.channel_usage[key];
		this.emit({type: "part", channel: record.name, actor: user}); reply("Channel registration dropped."); return;
	}
	if (verb == "ACCESS") {
		var action = (args.shift() || "").toUpperCase();
		if (action == "LIST") {
			reply("Founder: " + data.accounts[record.founder].name);
			Object.keys(record.access).forEach(function(id) { reply(data.accounts[id].name + " " + record.access[id]); });
			reply("End of access list."); return;
		}
		var target_account = this.resolve_account(args[0]), roles = {VOICE: 10, OP: 50, AOP: 50, SOP: 80}, grant = roles[(args[1] || "").toUpperCase()];
		if (level < 80 || !target_account || target_account == record.founder || (record.access[target_account] || 0) >= level
			|| (action != "ADD" && action != "DEL") || (action == "ADD" && (!grant || grant >= level)))
			throw IRC_services_error("Syntax: ACCESS #channel ADD <registered-nick> VOICE|OP|SOP, DEL <registered-nick>, or LIST (SOP/founder required to edit)");
		this.commit(function(next) { if (action == "ADD") next.channels[key].access[target_account] = grant; else delete next.channels[key].access[target_account]; });
		if (channel) this.enforce(channel); reply("Access list updated."); return;
	}
	if (verb == "SET") {
		if (level < 100) throw IRC_services_error("Only the founder can change channel settings.");
		var setting = (args.shift() || "").toUpperCase(), lock, enabled;
		if (setting == "MLOCK") lock = args.length == 1 && args[0].toUpperCase() == "NONE" ? {on:"",off:""} : IRC_services_mlock(args);
		else if (setting == "KEEPTOPIC" && /^(ON|OFF)$/i.test(args[0] || "")) enabled = args[0].toUpperCase() == "ON";
		else throw IRC_services_error("Syntax: SET #channel MLOCK <modes> [key/limit] | MLOCK NONE | KEEPTOPIC ON|OFF");
		this.commit(function(next) { if (lock) next.channels[key].mlock = lock; else { next.channels[key].keeptopic = enabled; if (channel) next.channels[key].topic = channel.topic; } });
		if (channel) this.enforce(channel); reply("Channel settings updated."); return;
	}
	if (!channel || level < 50) throw IRC_services_error("Channel operator access is required.");
	if (verb == "TOPIC") {
		var topic = args.join(" ").substr(0, 307);
		this.commit(function(next) { next.channels[key].topic = topic; });
		this.emit({type: "topic", channel: channel, actor: user, text: topic}); reply("Topic change requested."); return;
	}
	var target = this.by_nick(args[0] || user.nick), modes = {OP:"+o",DEOP:"-o",VOICE:"+v",DEVOICE:"-v"};
	if (!target) throw IRC_services_error("That nickname is not online.");
	if (verb == "INVITE") this.emit({type: "invite", channel: channel, actor: user, target: target});
	else if (verb == "UNBAN") this.emit({type: "unban", channel: channel, actor: user, target: target});
	else if (modes[verb]) {
		if (!this.member(channel, target.ref)) throw IRC_services_error("That nickname is not in the channel.");
		this.emit({type: "mode", channel: channel, actor: user, target: target, mode: modes[verb]});
	} else throw IRC_services_error("Unknown command. Use HELP.");
	reply("Channel action requested.");
};

IRC_Services_Engine.prototype.resolve_account = function(name) {
	var id = this.account(name);
	if (id) return id;
	for (id in this.store.data.accounts) {
		var a = this.store.data.accounts[id];
		if ((IRC_services_escape(a.name) + "+" + a.qwk).toUpperCase() == String(name).toUpperCase()) return id;
	}
	return null;
};
