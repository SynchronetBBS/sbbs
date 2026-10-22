/* SQLite persistence for embedded IRC services. GPL-2.0-or-later. */
"use strict";

function IRC_Services_Store(path) {
	if (typeof SQLite != "function") throw new Error("Embedded services require Synchronet's SQLite API");
	this.db = new SQLite(path);
	try {
		this.db.busy_timeout = 25;
		this.db.exec("PRAGMA foreign_keys=ON; PRAGMA locking_mode=EXCLUSIVE;");
		var version = this.db.query("PRAGMA user_version")[0].user_version;
		if (version != 0 && version != 1) throw new Error("Unsupported services schema");
		/* The exclusive connection owns the in-memory policy cache until
		   close. A second daemon must not act on a stale copy of this DB. */
		this.db.exec("BEGIN EXCLUSIVE;" +
			"CREATE TABLE IF NOT EXISTS accounts(id TEXT PRIMARY KEY, name TEXT NOT NULL, home TEXT NOT NULL, qwk TEXT NOT NULL, number INTEGER NOT NULL, created INTEGER NOT NULL);" +
			"CREATE TABLE IF NOT EXISTS nicks(name TEXT PRIMARY KEY, account TEXT NOT NULL REFERENCES accounts(id));" +
			"CREATE TABLE IF NOT EXISTS channels(name TEXT PRIMARY KEY, display TEXT NOT NULL, founder TEXT NOT NULL REFERENCES accounts(id), description TEXT NOT NULL, registered INTEGER NOT NULL, mlock TEXT NOT NULL, keeptopic INTEGER NOT NULL, topic TEXT NOT NULL);" +
			"CREATE TABLE IF NOT EXISTS access(channel TEXT NOT NULL REFERENCES channels(name) ON DELETE CASCADE, account TEXT NOT NULL REFERENCES accounts(id), level INTEGER NOT NULL CHECK(level IN (10,50,80)), PRIMARY KEY(channel,account));" +
			"PRAGMA user_version=1; COMMIT;");
		var data = this.data = {accounts: {}, nicks: {}, channels: {}};
		this.db.query("SELECT * FROM accounts").forEach(function(a) { data.accounts[a.id] = a; });
		this.db.query("SELECT * FROM nicks").forEach(function(n) { data.nicks[n.name] = n.account; });
		this.db.query("SELECT * FROM channels").forEach(function(c) {
			data.channels[c.name] = {name: c.display, founder: c.founder, description: c.description,
				registered: c.registered, mlock: JSON.parse(c.mlock), keeptopic: !!c.keeptopic, topic: c.topic, access: {}};
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
		Object.keys(old.nicks).forEach(function(k) { if (!next.nicks[k]) db.run("DELETE FROM nicks WHERE name=?", [k]); });
		Object.keys(next.nicks).forEach(function(k) {
			if (old.nicks[k] != next.nicks[k]) db.run("INSERT INTO nicks(name,account) VALUES(?,?) ON CONFLICT(name) DO UPDATE SET account=excluded.account", [k, next.nicks[k]]);
		});
		Object.keys(old.channels).forEach(function(k) { if (!next.channels[k]) db.run("DELETE FROM channels WHERE name=?", [k]); });
		Object.keys(next.channels).forEach(function(k) {
			var c = next.channels[k];
			if (JSON.stringify(c) == JSON.stringify(old.channels[k])) return;
			db.run("INSERT INTO channels(name,display,founder,description,registered,mlock,keeptopic,topic) VALUES(?,?,?,?,?,?,?,?) ON CONFLICT(name) DO UPDATE SET founder=excluded.founder,description=excluded.description,mlock=excluded.mlock,keeptopic=excluded.keeptopic,topic=excluded.topic",
				[k, c.name, c.founder, c.description, c.registered, JSON.stringify(c.mlock), c.keeptopic ? 1 : 0, c.topic]);
			db.run("DELETE FROM access WHERE channel=?", [k]);
			Object.keys(c.access).forEach(function(id) { db.run("INSERT INTO access(channel,account,level) VALUES(?,?,?)", [k, id, c.access[id]]); });
		});
	});
	this.data = next;
};
IRC_Services_Store.prototype.close = function() { this.db.close(); };
