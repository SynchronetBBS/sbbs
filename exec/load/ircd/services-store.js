/* SQLite persistence for embedded IRC services. GPL-2.0-or-later. */
"use strict";

function IRC_Services_Store(path) {
	if (typeof SQLite != "function") throw new Error("Embedded services require Synchronet's SQLite API");
	this.db = new SQLite(path);
	try {
		this.db.busy_timeout = 25;
		this.db.exec("PRAGMA foreign_keys=ON; PRAGMA locking_mode=EXCLUSIVE;");
		var version = this.db.query("PRAGMA user_version")[0].user_version;
		if (version != 0 && version != 3) throw new Error("Unsupported services schema");
		/* The exclusive connection owns the in-memory policy cache until
		   close. A second daemon must not act on a stale copy of this DB. */
		this.db.exec("BEGIN EXCLUSIVE;" +
			"CREATE TABLE IF NOT EXISTS accounts(id TEXT PRIMARY KEY, name TEXT NOT NULL, home TEXT NOT NULL, qwk TEXT NOT NULL, number INTEGER NOT NULL, created INTEGER NOT NULL);" +
			"CREATE TABLE IF NOT EXISTS identities(id TEXT PRIMARY KEY);" +
			"CREATE TABLE IF NOT EXISTS memberships(account TEXT PRIMARY KEY REFERENCES accounts(id), identity TEXT NOT NULL REFERENCES identities(id));" +
			"CREATE TABLE IF NOT EXISTS nicks(name TEXT PRIMARY KEY, account TEXT NOT NULL REFERENCES identities(id), memo_account TEXT NOT NULL REFERENCES accounts(id), last_used INTEGER NOT NULL);" +
			"CREATE TABLE IF NOT EXISTS channels(name TEXT PRIMARY KEY, display TEXT NOT NULL, founder TEXT NOT NULL REFERENCES identities(id), description TEXT NOT NULL, registered INTEGER NOT NULL, last_used INTEGER NOT NULL, mlock TEXT NOT NULL, keeptopic INTEGER NOT NULL, topic TEXT NOT NULL);" +
			"CREATE TABLE IF NOT EXISTS access(channel TEXT NOT NULL REFERENCES channels(name) ON DELETE CASCADE, account TEXT NOT NULL REFERENCES identities(id), level INTEGER NOT NULL CHECK(level IN (10,50,80)), PRIMARY KEY(channel,account));" +
			"PRAGMA user_version=3; COMMIT;");
		var data = this.data = {accounts: {}, groups: {}, memberships: {}, nicks: {}, memo_accounts: {}, nick_last_used: {}, channels: {}};
		this.db.query("SELECT * FROM accounts").forEach(function(a) { a.username = IRC_services_escape(a.name) + "+" + a.qwk; data.accounts[a.id] = a; });
		this.db.query("SELECT * FROM identities").forEach(function(g) { data.groups[g.id] = true; });
		this.db.query("SELECT * FROM memberships").forEach(function(m) { data.memberships[m.account] = m.identity; });
		this.db.query("SELECT * FROM nicks").forEach(function(n) { data.nicks[n.name] = n.account; data.memo_accounts[n.name] = n.memo_account; data.nick_last_used[n.name] = n.last_used; });
		this.db.query("SELECT * FROM channels").forEach(function(c) {
			data.channels[c.name] = {name: c.display, founder: c.founder, description: c.description,
				registered: c.registered, last_used: c.last_used, mlock: JSON.parse(c.mlock), keeptopic: !!c.keeptopic, topic: c.topic, access: {}};
		});
		this.db.query("SELECT * FROM access").forEach(function(a) { data.channels[a.channel].access[a.account] = a.level; });
	} catch (e) { this.db.close(); throw e; }
}
IRC_Services_Store.prototype.save = function(next) {
	var db = this.db, old = this.data;
	db.transaction(function() {
		Object.keys(next.accounts).forEach(function(id) {
			var a = next.accounts[id];
			if (JSON.stringify(a) == JSON.stringify(old.accounts[id])) return;
			db.run("INSERT INTO accounts(id,name,home,qwk,number,created) VALUES(?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET name=excluded.name,home=excluded.home,qwk=excluded.qwk,number=excluded.number,created=excluded.created",
				[id, a.name, a.home, a.qwk, a.number, a.created]);
		});
		Object.keys(next.groups).forEach(function(id) { if (!old.groups[id]) db.run("INSERT INTO identities(id) VALUES(?)", [id]); });
		Object.keys(next.memberships).forEach(function(id) {
			if (next.memberships[id] != old.memberships[id]) db.run("INSERT INTO memberships(account,identity) VALUES(?,?) ON CONFLICT(account) DO UPDATE SET identity=excluded.identity", [id,next.memberships[id]]);
		});
		Object.keys(old.nicks).forEach(function(k) { if (!next.nicks[k]) db.run("DELETE FROM nicks WHERE name=?", [k]); });
		Object.keys(next.nicks).forEach(function(k) {
			if (old.nicks[k] != next.nicks[k] || old.nick_last_used[k] != next.nick_last_used[k] || old.memo_accounts[k] != next.memo_accounts[k])
				db.run("INSERT INTO nicks(name,account,memo_account,last_used) VALUES(?,?,?,?) ON CONFLICT(name) DO UPDATE SET account=excluded.account,memo_account=excluded.memo_account,last_used=excluded.last_used", [k, next.nicks[k], next.memo_accounts[k], next.nick_last_used[k]]);
		});
		Object.keys(old.channels).forEach(function(k) { if (!next.channels[k]) db.run("DELETE FROM channels WHERE name=?", [k]); });
		Object.keys(next.channels).forEach(function(k) {
			var c = next.channels[k];
			if (JSON.stringify(c) == JSON.stringify(old.channels[k])) return;
			db.run("INSERT INTO channels(name,display,founder,description,registered,last_used,mlock,keeptopic,topic) VALUES(?,?,?,?,?,?,?,?,?) ON CONFLICT(name) DO UPDATE SET founder=excluded.founder,description=excluded.description,last_used=excluded.last_used,mlock=excluded.mlock,keeptopic=excluded.keeptopic,topic=excluded.topic",
				[k, c.name, c.founder, c.description, c.registered, c.last_used, JSON.stringify(c.mlock), c.keeptopic ? 1 : 0, c.topic]);
			db.run("DELETE FROM access WHERE channel=?", [k]);
			Object.keys(c.access).forEach(function(id) { db.run("INSERT INTO access(channel,account,level) VALUES(?,?,?)", [k, id, c.access[id]]); });
		});
		Object.keys(old.groups).forEach(function(id) { if (!next.groups[id]) db.run("DELETE FROM identities WHERE id=?", [id]); });
	});
	this.data = next;
};
/* Activity updates never rewrite registrations or their access lists. The
   policy engine batches these writes and retains dirty timestamps on failure. */
IRC_Services_Store.prototype.update_usage = function(nicks, channels) {
	var db = this.db, data = this.data;
	if (!Object.keys(nicks).length && !Object.keys(channels).length) return;
	db.transaction(function() {
		Object.keys(nicks).forEach(function(k) { db.run("UPDATE nicks SET last_used=? WHERE name=?", [nicks[k], k]); });
		Object.keys(channels).forEach(function(k) { db.run("UPDATE channels SET last_used=? WHERE name=?", [channels[k], k]); });
	});
	Object.keys(nicks).forEach(function(k) { data.nick_last_used[k] = nicks[k]; });
	Object.keys(channels).forEach(function(k) { data.channels[k].last_used = channels[k]; });
};
IRC_Services_Store.prototype.close = function() { this.db.close(); };
