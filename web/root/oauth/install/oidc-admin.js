// Run with jsexec from a terminal.
"use strict";
if (typeof http_request !== "undefined") throw new Error("Local command only");
var P = load({}, js.exec_dir + "../provider.js");
var args = argv.slice();
var op = args.shift();
var usage = "init | add-key ES256|ES384|ES512|RS256 | add ID [--ars ARS] [--alg ALG] CALLBACK... | list | rotate ID | disable ID | remove ID";
function run()
{
	if (op === "init") {
		if (args.length) throw new Error(usage);
		if (file_exists(system.ctrl_dir + "oidc.sqlite")) throw new Error("Database already exists");
		P.signingKeys();
	}
	var db = P.openDatabase(op === "init");
	try {
		if (op === "init") { P.schema(db); return; }
		if (op === "add-key" && args.length === 1) { P.addKey(args[0]); return; }
		if (op === "list" && !args.length) {
			db.query("SELECT id, enabled, ars, uri FROM clients JOIN callbacks ON client=id ORDER BY id,uri").forEach(function (row) {
				print(JSON.stringify(row));
			});
			return;
		}
		var id = args.shift();
		if (!id || (!/^[A-Za-z0-9._-]{1,128}$/.test(id) && !(op !== "add" && P.dynamicClientId(id)))) throw new Error(usage);
		var secret;
		db.transaction(function () {
			if (op === "add") {
				var ars = "", alg = "RS256";
				if (args[0] === "--ars") {
					args.shift();
					ars = args.shift();
				}
				if (args[0] === "--alg") {
					args.shift();
					alg = args.shift();
				}
				if (!P.signingKeys().some(function (key) { return key.alg === alg; }))
					throw new Error("Signing algorithm unavailable");
				if (!args.length || typeof ars !== "string") throw new Error(usage);
				secret = P.token();
				db.run("INSERT INTO clients(id, secret, ars) VALUES(?,?,?)", [id,P.digest(secret),ars]);
				P.setSigningAlg(db, "main.", id, alg);
				args.forEach(function (uri) {
					if (!P.validCallback(uri))
						throw new Error("Expected an exact HTTPS callback URI without a fragment");
					db.run("INSERT INTO callbacks VALUES(?,?)", [id,uri]);
				});
			} else {
				if (args.length) throw new Error(usage);
				if (op === "rotate") {
					secret = P.token();
					db.run("UPDATE clients SET secret=? WHERE id=?", [P.digest(secret),id]);
				} else if (op === "disable") db.run("UPDATE clients SET enabled=0 WHERE id=?", [id]);
				else if (op === "remove") db.run("DELETE FROM clients WHERE id=?", [id]);
				else throw new Error(usage);
				if (!db.changes) throw new Error("Unknown client");
			}
		});
		if (secret) print(secret);
	} finally { db.close(); }
}
try { run(); } catch (e) { alert(String(e)); exit(1); }
