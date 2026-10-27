/* Embedded NickServ/ChanServ policy and durable registration storage.
 * No IRC sockets or daemon globals are used here. GPL-2.0-or-later.
 */
"use strict";

function IRC_Services_Engine(store, config, emit) {
	this.store = store; this.config = config;
	var self = this;
	this.emit = function(action) {
		if (action.actor) action.actor_groups = self.group_signature(action.actor);
		if (action.target) action.target_groups = self.group_signature(action.target);
		emit(action);
	};
	this.users = {}; this.channels = {}; this.sessions = {}; this.deadlines = {};
	this.nick_usage = {}; this.channel_usage = {}; this.links = {};
	/* Allow the initial network snapshot and account attestations to arrive
	   before expiring anything. Maintenance and writes run once per minute. */
	this.next_maintenance = system.timer + 60;
}

IRC_Services_Engine.prototype.now = function() { return Date.now(); };
IRC_Services_Engine.prototype.note_nick = function(user, now) {
	var key = IRC_services_key(user.nick), owner = this.store.data.nicks[key];
	if (owner && this.owns(user, owner))
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
		nicks.forEach(function(k) { delete next.nicks[k]; delete next.nick_last_used[k]; delete next.memo_accounts[k]; });
		channels.forEach(function(k) { delete next.channels[k]; });
	});
	parted.forEach(function(name) { self.emit({type:"part", channel:name}); });
	log(LOG_INFO, format("Embedded services expired %u nickname and %u channel registrations", nicks.length, channels.length));
};
IRC_Services_Engine.prototype.notice = function(service, user, text) {
	this.emit({type: "notice", service: service, actor: user, text: text});
};
IRC_Services_Engine.prototype.account = function(name) { return this.store.data.nicks[IRC_services_key(name)]; };
/* Session account IDs are proofs; persistent group IDs are ownership. */
IRC_Services_Engine.prototype.groups = function(user) {
	var session = this.sessions[user.ref], data = this.store.data;
	return session ? session.accounts.map(function(id) { return data.memberships[id]; }) : [];
};
IRC_Services_Engine.prototype.group_signature = function(user) { return JSON.stringify(this.groups(user).sort()); };
IRC_Services_Engine.prototype.owns = function(user, group) { return !!group && this.groups(user).indexOf(group) >= 0; };
IRC_Services_Engine.prototype.active_group = function(user) {
	var session = this.sessions[user.ref]; return session && this.store.data.memberships[session.active];
};
IRC_Services_Engine.prototype.group_name = function(group) {
	var data = this.store.data;
	/* A network identity has no privileged or primary BBS account. */
	return Object.keys(data.nicks).filter(function(k) { return data.nicks[k] == group; }).map(function(k) { return k.substr(1); }).join(", ") || "network identity " + group.substr(0, 12);
};
IRC_Services_Engine.prototype.record_level = function(user, record) {
	if (!record) return 0;
	var groups = this.groups(user), level = 0;
	groups.forEach(function(id) { level = Math.max(level, record.founder == id ? 100 : record.access[id] || 0); });
	return level;
};
IRC_Services_Engine.prototype.level = function(user, channel) { return this.record_level(user, this.store.data.channels[IRC_services_key(channel.name)]); };
IRC_Services_Engine.prototype.authorization_changed = function() {
	var self = this;
	Object.keys(this.users).forEach(function(ref) {
		var u = self.users[ref], owner = self.account(u.nick);
		if (!owner || self.owns(u, owner)) delete self.deadlines[ref];
		else if (!self.deadlines[ref] || self.deadlines[ref].nick != u.nick) self.deadlines[ref] = {nick:u.nick, at:system.timer + 60};
	});
	Object.keys(this.channels).forEach(function(k) { self.enforce(self.channels[k]); });
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
		this.users = {}; this.channels = {}; this.sessions = {}; this.deadlines = {}; this.links = {};
		return;
	}
	if (event.type == "user") {
		this.note_user(event.user.ref, this.now());
		old = this.users[event.user.ref]; this.users[event.user.ref] = event.user;
		this.note_user(event.user.ref, this.now());
		var owner = this.account(event.user.nick);
		if ((!old || old.nick != event.user.nick) && owner && !this.owns(event.user, owner))
			{
			this.notice("NickServ", event.user, "This nickname is registered. Identify to your BBS account within 60 seconds or change nicknames.");
			this.deadlines[event.user.ref] = {nick: event.user.nick, at: system.timer + 60};
		}
		return;
	}
	if (event.type == "quit") { this.note_user(event.ref, this.now()); delete this.users[event.ref]; delete this.sessions[event.ref]; delete this.deadlines[event.ref]; delete this.links[event.ref]; return; }
	if (event.type == "identify") {
		var identified = this.users[event.ref], identity = event.identity;
		if (!identified) return;
		this.note_user(event.ref, this.now());
		var identities = event.logins || (identity ? [identity] : []), changed = false;
		identities.forEach(function(a) { if (JSON.stringify(self.store.data.accounts[a.id]) != JSON.stringify(a) || !self.store.data.memberships[a.id]) changed = true; });
		if (changed) this.commit(function(next) {
			identities.forEach(function(a) {
				next.accounts[a.id] = a;
				if (!next.memberships[a.id]) { var group = IRC_services_random(); next.groups[group] = true; next.memberships[a.id] = group; }
			});
		});
		if (identities.length) this.sessions[event.ref] = {accounts:identities.map(function(a) { return a.id; }), active:identity ? identity.id : identities[identities.length - 1].id,
			revision:event.revision || 0, multi:!!event.multi, verified_at:system.timer};
		else delete this.sessions[event.ref];
		this.note_user(event.ref, this.now());
		this.authorization_changed();
		return;
	}
	if (event.type == "tick") {
		if (system.timer >= this.next_maintenance) {
			this.checkpoint(); this.expire(this.now());
			this.next_maintenance = system.timer + 60;
		}
		Object.keys(this.links).forEach(function(ref) { if (self.links[ref].until < system.timer) delete self.links[ref]; });
		Object.keys(this.deadlines).forEach(function(ref) {
			var d = self.deadlines[ref], u = self.users[ref], owner = u && self.account(u.nick);
			if (!u || u.nick != d.nick || !owner || self.owns(u, owner)) { delete self.deadlines[ref]; return; }
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
	var self = this, data = this.store.data, key = IRC_services_key(user.nick), session = this.sessions[user.ref], account = this.active_group(user);
	function reply(text) { self.notice("NickServ", user, text); }
	if (verb == "HELP") { reply("REGISTER | DROP [nickname] | INFO [nickname] | GHOST <nickname> | ACCOUNTS | LINK <account+QWKID> <account+QWKID> | LINK CONFIRM <code> | UNLINK <account+QWKID> | SET <nick> MEMOACCOUNT <account+QWKID>. IDENTIFY using NickServ@bbs-server; LOGOUT [ALL]."); return; }
	if (verb == "INFO") {
		var info_key = IRC_services_key(args[0] || user.nick), owner = data.nicks[info_key];
		if (!owner) throw IRC_services_error("That nickname is not registered.");
		reply("Registered network identity: " + this.group_name(owner) + ". Last used: " + new Date(Math.max(data.nick_last_used[info_key], this.nick_usage[info_key] || 0)).toISOString() + "."); return;
	}
	if (!account) throw IRC_services_error("Identify to your BBS account first.");
	if (verb == "ACCOUNTS") {
		reply("Verified BBS logins: " + session.accounts.map(function(id) { var a = data.accounts[id]; return a.username + "@" + a.home + (id == session.active ? " (active)" : ""); }).join(", "));
		var seen = {};
		this.groups(user).forEach(function(group) { if (!seen[group]) { seen[group] = true; self.describe_group(group, reply); } });
	} else if (verb == "LINK") this.link(user, args, reply);
	else if (verb == "UNLINK") {
		var id = args.length == 1 && this.resolve_bbs_account(args[0]), group = id && data.memberships[id];
		if (!group || !this.owns(user, group)) throw IRC_services_error("Syntax: UNLINK <account+QWKID> (must belong to your identity)");
		if (Object.keys(data.memberships).filter(function(k) { return data.memberships[k] == group; }).length < 2) throw IRC_services_error("Cannot unlink the final BBS account.");
		var destinations = Object.keys(data.memo_accounts).filter(function(k) { return data.memo_accounts[k] == id; });
		if (destinations.length) throw IRC_services_error("Retarget MEMOACCOUNT before unlinking: " + destinations.map(function(k) { return k.substr(1); }).join(", "));
		this.checkpoint();
		this.commit(function(next) { var fresh = IRC_services_random(); next.groups[fresh] = true; next.memberships[id] = fresh; });
		this.authorization_changed(); reply("BBS account unlinked into an empty identity. Existing registrations remain with the other members.");
	} else if (verb == "SET") {
		key = IRC_services_key(args[0]);
		var destination = this.resolve_bbs_account(args[2]);
		if (args.length != 3 || String(args[1]).toUpperCase() != "MEMOACCOUNT" || !this.owns(user, data.nicks[key]) || !destination || data.memberships[destination] != data.nicks[key])
			throw IRC_services_error("Syntax: SET <nickname> MEMOACCOUNT <account+QWKID> (destination must belong to that nickname's identity)");
		this.commit(function(next) { next.memo_accounts[key] = destination; }); reply("Telegram destination updated.");
	} else if (verb == "REGISTER" || verb == "GROUP") {
		if (args.length) throw IRC_services_error("Syntax: REGISTER (claims your current nickname)");
		if (data.nicks[key]) throw IRC_services_error("That nickname is already registered.");
		if (this.nick_count([account]) >= this.max_nicks()) throw IRC_services_error("A network identity may register at most " + this.max_nicks() + " nicknames.");
		var now = this.now();
		this.commit(function(next) { next.nicks[key] = account; next.memo_accounts[key] = session.active; next.nick_last_used[key] = now; });
		delete this.nick_usage[key]; reply("Nickname registered to your network identity.");
	} else if (verb == "DROP") {
		key = IRC_services_key(args[0] || user.nick);
		if (!this.owns(user, data.nicks[key])) throw IRC_services_error("That nickname belongs to another account.");
		this.commit(function(next) { delete next.nicks[key]; delete next.nick_last_used[key]; delete next.memo_accounts[key]; });
		delete this.nick_usage[key]; reply("Nickname registration dropped.");
	} else if (verb == "GHOST") {
		if (args.length != 1 || !this.owns(user, this.account(args[0]))) throw IRC_services_error("Syntax: GHOST <nickname> (must belong to your account)");
		var target = this.by_nick(args[0]);
		if (!target || target.ref == user.ref) throw IRC_services_error("No other session is using that nickname.");
		this.emit({type:"kill", actor:user, target:target}); reply("Ghost session removal requested.");
	} else throw IRC_services_error("Unknown command. Use HELP.");
};
IRC_Services_Engine.prototype.max_nicks = function() { return this.config.max_nicks || 10; };
IRC_Services_Engine.prototype.nick_count = function(groups) {
	var data = this.store.data; return Object.keys(data.nicks).filter(function(k) { return groups.indexOf(data.nicks[k]) >= 0; }).length;
};
IRC_Services_Engine.prototype.describe_group = function(group, reply) {
	var data = this.store.data;
	reply("Identity " + this.group_name(group) + ": " + Object.keys(data.memberships).filter(function(id) { return data.memberships[id] == group; }).map(function(id) { return data.accounts[id].username + "@" + data.accounts[id].home; }).join(", "));
	reply("Nickname telegram destinations: " + (Object.keys(data.nicks).filter(function(k) { return data.nicks[k] == group; }).map(function(k) { return k.substr(1) + " -> " + data.accounts[data.memo_accounts[k]].username; }).join(", ") || "none"));
	var rights = [];
	Object.keys(data.channels).sort().forEach(function(k) { var c = data.channels[k], level = c.founder == group ? 100 : c.access[group]; if (level) rights.push(c.name + " " + (level == 100 ? "founder" : level)); });
	reply("Channel rights: " + (rights.join(", ") || "none"));
};
IRC_Services_Engine.prototype.link_state = function(groups) {
	var data = this.store.data;
	return JSON.stringify({members:Object.keys(data.memberships).sort().filter(function(id) { return groups.indexOf(data.memberships[id]) >= 0; }).map(function(id) { return [id,data.memberships[id]]; }),
		nicks:Object.keys(data.nicks).sort().filter(function(k) { return groups.indexOf(data.nicks[k]) >= 0; }).map(function(k) { return [k,data.nicks[k],data.memo_accounts[k]]; }),
		rights:Object.keys(data.channels).sort().map(function(k) { var c = data.channels[k]; return [k,c.founder,groups.map(function(g) { return c.access[g] || 0; })]; }).filter(function(c) { return groups.indexOf(c[1]) >= 0 || c[2].some(function(v) { return v; }); })});
};
IRC_Services_Engine.prototype.link = function(user, args, reply) {
	var self = this, data = this.store.data, session = this.sessions[user.ref];
	if (!session.multi || system.timer - session.verified_at > 30) throw IRC_services_error("LINK requires updated services on the connection server and both BBSes; identify again if needed.");
	if (String(args[0]).toUpperCase() == "CONFIRM") {
		var pending = this.links[user.ref]; delete this.links[user.ref];
		if (args.length != 2 || !pending || pending.code != args[1] || system.timer > pending.until || pending.revision != session.revision
			|| pending.accounts.some(function(id) { return session.accounts.indexOf(id) < 0; }) || pending.state != this.link_state(pending.groups))
			throw IRC_services_error("Link confirmation expired or identity state changed; request a new LINK preview.");
		if (this.nick_count(pending.groups) > this.max_nicks()) throw IRC_services_error("Merge exceeds MaxNicks=" + this.max_nicks() + "; drop registrations before linking.");
		/* Preserve usage, destinations and settings; only ownership keys change. */
		this.checkpoint();
		var keep = pending.groups[0], remove = pending.groups[1];
		this.commit(function(next) {
			Object.keys(next.memberships).forEach(function(id) { if (next.memberships[id] == remove) next.memberships[id] = keep; });
			Object.keys(next.nicks).forEach(function(k) { if (next.nicks[k] == remove) next.nicks[k] = keep; });
			Object.keys(next.channels).forEach(function(k) {
				var c = next.channels[k]; if (c.founder == remove) c.founder = keep;
				if (c.founder == keep) { delete c.access[keep]; delete c.access[remove]; }
				else if (c.access[remove]) { c.access[keep] = Math.max(c.access[keep] || 0,c.access[remove]); delete c.access[remove]; }
			});
			delete next.groups[remove];
		});
		this.authorization_changed(); reply("Network identities linked. Every linked BBS account has equal control of the combined registrations and shared telegram access."); return;
	}
	var first = args.length == 2 && this.resolve_bbs_account(args[0]), second = args.length == 2 && this.resolve_bbs_account(args[1]);
	if (!first || !second || session.accounts.indexOf(first) < 0 || session.accounts.indexOf(second) < 0) throw IRC_services_error("LINK requires both exact account+QWKID logins verified in this IRC session.");
	var groups = [data.memberships[first],data.memberships[second]];
	if (groups[0] == groups[1]) throw IRC_services_error("Those accounts already share an identity.");
	if (this.nick_count(groups) > this.max_nicks()) throw IRC_services_error("Merge exceeds MaxNicks=" + this.max_nicks() + "; drop registrations before linking.");
	groups.forEach(function(g) { self.describe_group(g, reply); });
	var code = IRC_services_random().substr(0, 24);
	this.links[user.ref] = {code:code, until:system.timer + 300, revision:session.revision, accounts:[first,second], groups:groups, state:this.link_state(groups)};
	reply("Linking gives every listed account control of ALL listed registrations and access to ALL linked BBS telegram mailboxes. Confirm within 5 minutes: LINK CONFIRM " + code);
};

function IRC_services_error(message, home) { var e = new Error(message); e.services_user = true; e.services_home = home; return e; }

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
	var account = this.active_group(user), member = channel && this.member(channel, user.ref), level = channel ? this.level(user, channel) : 0;
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
	if (!member && (!this.record_level(user, record))
		&& (!channel || /[sp]/.test(channel.flags))) throw IRC_services_error("That channel is unavailable.");
	if (verb == "INFO") { reply(record.name + ": " + record.description + " Founder: " + this.group_name(record.founder)
		+ ". Last used: " + new Date(Math.max(record.last_used, this.channel_usage[key] || 0)).toISOString() + "."); return; }
	if (!this.record_level(user, record)) throw IRC_services_error("Identify to an account with access to that channel.");
	if (!channel) level = this.record_level(user, record);
	if (verb == "DROP") {
		if (level < 100) throw IRC_services_error("Only the founder can drop the channel.");
		this.commit(function(next) { delete next.channels[key]; });
		delete this.channel_usage[key];
		this.emit({type: "part", channel: record.name, actor: user}); reply("Channel registration dropped."); return;
	}
	if (verb == "ACCESS") {
		var action = (args.shift() || "").toUpperCase();
		if (action == "LIST") {
			reply("Founder: " + this.group_name(record.founder));
			Object.keys(record.access).forEach(function(id) { reply(self.group_name(id) + " " + record.access[id]); });
			reply("End of access list."); return;
		}
		var target_account = this.resolve_group(args[0]), roles = {VOICE: 10, OP: 50, AOP: 50, SOP: 80}, grant = roles[(args[1] || "").toUpperCase()];
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

/* Explicit BBS aliases resolve accounts; nicknames resolve network groups.
   Memo destinations never use a group ID as a mailbox identity. */
IRC_Services_Engine.prototype.resolve_bbs_account = function(name) {
	for (var id in this.store.data.accounts) {
		var a = this.store.data.accounts[id];
		if ((IRC_services_escape(a.name) + "+" + a.qwk).toUpperCase() == String(name).toUpperCase()) return id;
	}
	return null;
};
IRC_Services_Engine.prototype.resolve_group = function(name) { return this.account(name) || this.store.data.memberships[this.resolve_bbs_account(name)] || null; };
IRC_Services_Engine.prototype.resolve_memo = function(name, home) {
	var data = this.store.data, key = IRC_services_key(name), group = data.nicks[key];
	if (group) {
		if (!home) return data.memo_accounts[key];
		var members = Object.keys(data.memberships).filter(function(id) { return data.memberships[id] == group && data.accounts[id].home == home; });
		if (!members.length) throw IRC_services_error("That nickname has no linked account on " + home + ".");
		if (members.length > 1) throw IRC_services_error("That nickname has multiple linked accounts on " + home + "; address an explicit account+QWKID.");
		return members[0];
	}
	var id = this.resolve_bbs_account(name);
	if (id && home && data.accounts[id].home != home) throw IRC_services_error("The destination account belongs to " + data.accounts[id].home + ", not " + home + ".");
	return id;
};
/* A verified member grants mailbox access to its whole network identity.
   Resolve membership for every read so unlinking also revokes this authority. */
IRC_Services_Engine.prototype.can_read_memo = function(actor, id) {
	var user = this.users[actor.ref], session = this.sessions[actor.ref];
	return !!(user && session && user.nick == actor.nick && user.rev == actor.rev && session.revision == (actor.auth_revision || 0)
		&& this.owns(user, this.store.data.memberships[id]));
};
IRC_Services_Engine.prototype.memo_read = function(actor, home) {
	var user = this.users[actor.ref], session = this.sessions[actor.ref], data = this.store.data;
	if (!user || !session || user.nick != actor.nick || user.rev != actor.rev || session.revision != (actor.auth_revision || 0))
		throw IRC_services_error("Authentication changed; please retry.");
	var key = IRC_services_key(user.nick), owner = this.owns(user, data.nicks[key]) && data.nicks[key];
	var groups = home ? this.groups(user) : [owner || this.active_group(user)];
	var ids = Object.keys(data.memberships).filter(function(id) { return groups.indexOf(data.memberships[id]) >= 0 && (!home || data.accounts[id].home == home); }).sort();
	if (!ids.length) throw IRC_services_error("No account in your authenticated network identities belongs to " + home + ".", home);
	/* The configured notification mailbox is checked first; direct telegrams
	   on other member BBSes remain accessible through the same service. */
	var preferred = owner && data.memo_accounts[key], index = ids.indexOf(preferred);
	if (index > 0) { ids.splice(index,1); ids.unshift(preferred); }
	return ids;
};
