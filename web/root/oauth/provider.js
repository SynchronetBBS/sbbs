// Shared by the SSJS endpoints and the offline administration command.
"use strict";

function token()
{
	var mac = new CryptContext(CryptContext.ALGO["HMAC-SHA2"]);
	mac.generate_key();
	mac.encrypt("Synchronet OIDC");
	mac.encrypt("");
	if (mac.hashvalue.length !== 32)
		throw new Error("Random key generation failed");
	return url64(base64_encode(mac.hashvalue));
}

function url64(s)
{
	return s.replace(/=/g, "").replace(/\+/g, "-").replace(/\//g, "_");
}

function digest(s)
{
	return sha256_calc(s, true);
}

function equal(a, b)
{
	if (typeof a !== "string" || typeof b !== "string") return false;
	var diff = a.length ^ b.length;
	for (var i = 0; i < b.length; i++)
		diff |= a.charCodeAt(i) ^ b.charCodeAt(i);
	return diff === 0;
}

function openDatabase(create)
{
	var path = system.ctrl_dir + "oidc.sqlite";
	if (!create && !file_exists(path)) throw new Error("Initialize the OIDC provider first");
	var db = new SQLite(path);
	db.exec("PRAGMA foreign_keys=ON");
	return db;
}

// SQLite index and foreign-key table names are local to their database.
function createRegistry(db, owned)
{
	var schema = owned ? "apps." : "main.";
	db.exec("CREATE TABLE " + schema + "clients (id TEXT PRIMARY KEY, secret TEXT NOT NULL, ars TEXT NOT NULL, enabled INTEGER NOT NULL DEFAULT 1" +
	    (owned ? ", name TEXT NOT NULL UNIQUE" : "") + ");" +
	    "CREATE TABLE " + schema + "callbacks (client TEXT REFERENCES clients(id) ON DELETE CASCADE, uri TEXT NOT NULL, PRIMARY KEY(client,uri));" +
	    "CREATE TABLE " + schema + "grants (hash TEXT PRIMARY KEY, kind TEXT NOT NULL, client TEXT REFERENCES clients(id) ON DELETE CASCADE, uri TEXT NOT NULL, challenge TEXT NOT NULL, scope TEXT NOT NULL, nonce TEXT, num INTEGER NOT NULL, sub TEXT NOT NULL, auth_time INTEGER NOT NULL, expires INTEGER NOT NULL);" +
	    "CREATE INDEX " + schema + "grant_expiry ON grants(expires)");
}

function schema(db, issuer)
{
	db.transaction(function () {
		db.exec("CREATE TABLE settings (issuer TEXT NOT NULL);" +
		    "CREATE TABLE sessions (hash TEXT PRIMARY KEY, num INTEGER NOT NULL, sub TEXT NOT NULL, stamp TEXT NOT NULL, webkey TEXT, created INTEGER NOT NULL, seen INTEGER NOT NULL)");
		createRegistry(db, false);
		db.run("INSERT INTO settings VALUES (?)", [issuer]);
	});
}

// A tilde keeps these IDs outside the sysop and member registration namespaces.
function dynamicClientId(id)
{
	return /^dyn~[A-Za-z0-9_-]{43}$/.test(id);
}

function validCallback(uri)
{
	return typeof uri === "string" && uri.length <= 4096 &&
	    /^https:\/\/[A-Za-z0-9.-]+(:[0-9]+)?(\/[^\s#\\]*)?$/.test(uri) && !/[\x00-\x20\x7f]/.test(uri);
}

// The flat ####.oidc file is removed by newuserdat() when a slot is reused.
// Allocate subjects inside the issuing transaction to serialize first use.
function subject(num, create)
{
	var path = system.data_dir + format("user/%04u.oidc", num);
	if (create && !file_exists(path)) {
		var id = token();
		if (!file_mutex(path, id, 0)) throw new Error("Could not create subject");
	}
	var f = new File(path);
	if (!f.open("rb", true)) {
		if (file_exists(path)) throw new Error("Cannot read subject");
		return null;
	}
	try {
		var sub = f.read();
		if (!/^[A-Za-z0-9_-]{43}$/.test(sub)) throw new Error("Invalid subject file");
		return sub;
	} finally {
		f.close();
	}
}

function signingKey(create)
{
	var path = system.ctrl_dir + "oid.key";
	var f = new File(system.ctrl_dir + "main.ini");
	if (!f.open("r", true)) throw new Error("Cannot read main.ini");
	var pass;
	try { pass = f.iniGetValue(null, "Password", ""); } finally { f.close(); }
	if (!pass) throw new Error("Missing keyset password");
	var fresh = create && !file_exists(path);
	var ks = new CryptKeyset(path, fresh ? CryptKeyset.KEYOPT.CREATE : CryptKeyset.KEYOPT.READONLY);
	try {
		var rsa;
		if (fresh) {
			rsa = new CryptContext(CryptContext.ALGO.RSA);
			rsa.keysize = 256;
			rsa.label = system.host_name;
			rsa.generate_key();
			ks.add_private_key(rsa, pass);
		} else {
			rsa = ks.get_private_key(system.host_name, pass);
		}
		if (rsa.algo !== CryptContext.ALGO.RSA || rsa.keysize < 256)
			throw new Error("OIDC requires a 2048-bit or larger RSA key");
		return rsa;
	} finally {
		ks.close();
	}
}

function jwk(rsa)
{
	var pub = rsa.public_key;
	return {kty:"RSA", use:"sig", alg:"RS256", kid:"1", n:pub.n, e:pub.e};
}

function sign(claims, rsa)
{
	var header = {alg:"RS256", kid:"1", typ:"JWT"};
	var input = url64(base64_encode(JSON.stringify(header))) + "." +
	    url64(base64_encode(JSON.stringify(claims)));
	// RSASSA-PKCS1-v1_5 SHA-256 DigestInfo and modulus-sized signature.
	var info = "\x30\x31\x30\x0d\x06\x09\x60\x86\x48\x01\x65\x03\x04\x02\x01\x05\x00\x04\x20" +
	    base64_decode(sha256_calc(input));
	var block = "\x00\x01" + new Array(rsa.keysize - info.length - 2).join("\xff") + "\x00" + info;
	return input + "." + url64(base64_encode(rsa.decrypt(block)));
}

({token:token, url64:url64, digest:digest, equal:equal, openDatabase:openDatabase,
    schema:schema, dynamicClientId:dynamicClientId, createRegistry:createRegistry, validCallback:validCallback, subject:subject, signingKey:signingKey, jwk:jwk, sign:sign});
