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

function schema(db)
{
	db.transaction(function () {
		db.exec("CREATE TABLE sessions (hash TEXT PRIMARY KEY, num INTEGER NOT NULL, sub TEXT NOT NULL, stamp TEXT NOT NULL, webkey TEXT, created INTEGER NOT NULL, seen INTEGER NOT NULL)");
		createRegistry(db, false);
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

var curves = {
	ES256:{name:"P-256", width:32, hash:32},
	ES384:{name:"P-384", width:48, hash:48},
	ES512:{name:"P-521", width:66, hash:64}
};
var algorithms = ["ES256", "ES384", "ES512", "RS256"];

function keyAlgorithm(key)
{
	if (key.algo === CryptContext.ALGO.RSA && key.keysize >= 256) return "RS256";
	if (key.algo === CryptContext.ALGO.ECDSA) {
		for (var alg in curves)
			if (key.keysize === curves[alg].width) return alg;
	}
	throw new Error("Unsupported OIDC signing key");
}

function keyPassword()
{
	var f = new File(system.ctrl_dir + "main.ini");
	if (!f.open("r", true)) throw new Error("Cannot read main.ini");
	try {
		var pass = f.iniGetValue(null, "Password", "");
		if (!pass) throw new Error("Missing keyset password");
		return pass;
	} finally { f.close(); }
}

var keyPath = system.ctrl_dir + "oid.key";

// Host-labelled keys from the original provider retain their published key ID.
function readKeys(path, pass)
{
	var ks = new CryptKeyset(path, CryptKeyset.KEYOPT.READONLY);
	var keys = [];
	try {
		[system.host_name].concat(algorithms.map(function (alg) { return "OIDC " + alg; })).forEach(function (label, i) {
			var key;
			try {
				key = ks.get_private_key(label, pass);
			} catch (e) {
				// The binding reports CRYPT_ERROR_NOTFOUND in the exception text.
				if (/Error -43 calling cryptGetPrivateKey\(\)/.test(String(e))) return;
				throw e;
			}
			var signer = {key:key, alg:keyAlgorithm(key), kid:"1"};
			if (i) {
				if (signer.alg !== algorithms[i-1]) throw new Error("Wrong signing algorithm for " + label);
				signer.kid = keyThumbprint(signer);
			}
			keys.push(signer);
		});
	} finally { ks.close(); }
	return keys;
}

function missingKeys(keys, wanted)
{
	return wanted.filter(function (alg) {
		return !keys.some(function (key) { return key.alg === alg; });
	});
}

function generateKey(ks, pass, alg)
{
	var ec = curves[alg];
	var key = new CryptContext(ec ? CryptContext.ALGO.ECDSA : CryptContext.ALGO.RSA);
	key.keysize = ec ? ec.width : 256;
	key.label = "OIDC " + alg;
	key.generate_key();
	ks.add_private_key(key, pass);
}

function signingKeys(extra)
{
	var wanted = ["ES256", "RS256"];
	if (extra !== undefined) {
		if (algorithms.indexOf(extra) < 0) throw new Error("Unknown signing algorithm");
		if (wanted.indexOf(extra) < 0) wanted.push(extra);
	}
	var pass = keyPassword(), lock = keyPath + ".lock";
	var until = Date.now() + 30000;
	while (true) {
		if (!file_exists(lock) && file_exists(keyPath)) {
			var keys = readKeys(keyPath, pass);
			if (!missingKeys(keys, wanted).length) return keys;
		}
		if (file_mutex(lock, "OIDC key generation", 300)) break;
		if (Date.now() >= until) throw new Error("Signing keyset is busy");
		mswait(50);
	}
	var tmp;
	try {
		var exists = file_exists(keyPath);
		var current = exists ? readKeys(keyPath, pass) : [];
		var missing = missingKeys(current, wanted);
		if (!missing.length) return current;

		// Publish complete keysets by rename. Windows rename cannot replace an
		// existing file, so use cryptlib's native keyset update there.
		if (!exists || system.platform !== "Win32") {
			tmp = keyPath + "." + token() + ".tmp";
			if (exists && (!file_mutex(tmp, "", 0) || !file_chmod(tmp, 384) || !file_copy(keyPath, tmp)))
				throw new Error("Cannot copy signing keyset");
		}
		var path = tmp || keyPath;
		var ks = new CryptKeyset(path, exists ? CryptKeyset.KEYOPT.NONE : CryptKeyset.KEYOPT.CREATE);
		try {
			missing.forEach(function (alg) { generateKey(ks, pass, alg); });
		} finally { ks.close(); }
		var result = readKeys(path, pass);
		if (missingKeys(result, wanted).length) throw new Error("Incomplete signing keyset");
		if (tmp && !file_rename(tmp, keyPath)) throw new Error("Cannot publish signing keyset");
		return result;
	} finally {
		if (tmp && file_exists(tmp)) file_remove(tmp);
		file_remove(lock);
	}
}

function signingKey(alg)
{
	var keys = signingKeys();
	if (alg === undefined) return keys[0];
	for (var i = 0; i < keys.length; i++) {
		if (keys[i].alg === alg) return keys[i];
	}
	throw new Error("Signing key unavailable: " + alg);
}

function addKey(alg)
{
	if (algorithms.indexOf(alg) < 0) throw new Error("Unknown signing algorithm");
	return signingKeys(alg).filter(function (key) { return key.alg === alg; })[0];
}

function padUnsigned(bytes, width)
{
	if (!bytes.length || bytes.length > width) throw new Error("Invalid EC integer width");
	return new Array(width - bytes.length + 1).join("\x00") + bytes;
}

function jwk(signer)
{
	var pub = signer.key.public_key;
	var result = {use:"sig", alg:signer.alg, kid:signer.kid};
	if (signer.alg === "RS256") {
		result.kty = "RSA";
		result.n = pub.n;
		result.e = pub.e;
	} else {
		var curve = curves[signer.alg];
		result.kty = "EC";
		result.crv = curve.name;
		// CryptContext.public_key strips leading zero octets from coordinates.
		var coordinate = function (encoded) {
			var bytes = base64_decode(encoded.replace(/-/g, "+").replace(/_/g, "/"));
			return url64(base64_encode(padUnsigned(bytes, curve.width)));
		};
		result.x = coordinate(pub.x);
		result.y = coordinate(pub.y);
	}
	return result;
}

// RFC 7638 thumbprint. A regenerated key gets a distinct kid in client caches.
function keyThumbprint(signer)
{
	var k = jwk(signer);
	var fields = k.kty === "RSA" ? {e:k.e, kty:k.kty, n:k.n} : {crv:k.crv, kty:k.kty, x:k.x, y:k.y};
	return url64(sha256_calc(JSON.stringify(fields)));
}

// cryptCreateSignature returns a cryptlib SignerInfo. Its final OCTET STRING
// contains the DER ECDSA-Sig-Value. JOSE uses two fixed-width unsigned integers.
function derReader(bytes)
{
	var pos = 0;
	return {
		take:function (tag) {
			if (pos + 2 > bytes.length || bytes.charCodeAt(pos++) !== tag)
				throw new Error("Unexpected cryptlib signature field");
			var len = bytes.charCodeAt(pos++);
			if (len >= 128) {
				var count = len - 128;
				if (!count || count > 2 || pos + count > bytes.length) throw new Error("Invalid DER length");
				len = 0;
				while (count--) len = len * 256 + bytes.charCodeAt(pos++);
			}
			if (pos + len > bytes.length) throw new Error("Truncated cryptlib signature");
			pos += len;
			return bytes.slice(pos - len, pos);
		},
		end:function () { if (pos !== bytes.length) throw new Error("Unexpected trailing signature data"); }
	};
}

function ecSignature(input, signer)
{
	var curve = curves[signer.alg];
	var hash = new CryptContext(CryptContext.ALGO.SHA2);
	hash.blocksize = curve.hash;
	hash.encrypt(input);
	hash.encrypt("");
	var outer = derReader(hash.create_signature(signer.key));
	var info = derReader(outer.take(0x30));
	outer.end();
	info.take(0x02); // version
	info.take(0x80); // key identifier
	info.take(0x30); // digest algorithm
	info.take(0x30); // signature algorithm
	var octets = derReader(info.take(0x04));
	info.end();
	var pair = derReader(octets.take(0x30));
	octets.end();
	function integer() {
		var value = pair.take(0x02);
		if (value.charCodeAt(0) >= 128) throw new Error("Negative signature integer");
		if (value.length > 1 && value.charCodeAt(0) === 0) value = value.slice(1);
		return padUnsigned(value, curve.width);
	}
	var raw = integer() + integer();
	pair.end();
	return raw;
}

function sign(claims, signer)
{
	var header = {alg:signer.alg, kid:signer.kid, typ:"JWT"};
	var input = url64(base64_encode(JSON.stringify(header))) + "." +
	    url64(base64_encode(JSON.stringify(claims)));
	var signature;
	if (signer.alg === "RS256") {
		// RSASSA-PKCS1-v1_5 SHA-256 DigestInfo and modulus-sized signature.
		var info = "\x30\x31\x30\x0d\x06\x09\x60\x86\x48\x01\x65\x03\x04\x02\x01\x05\x00\x04\x20" +
		    base64_decode(sha256_calc(input));
		var block = "\x00\x01" + new Array(signer.key.keysize - info.length - 2).join("\xff") + "\x00" + info;
		signature = signer.key.decrypt(block);
	} else signature = ecSignature(input, signer);
	return input + "." + url64(base64_encode(signature));
}

// Older client registries have no signing_alg column and use the OIDC default.
// Called within the transaction creating the client, including for attached DBs.
function setSigningAlg(db, registry, id, alg)
{
	if (alg === "RS256") return;
	if (!db.query("PRAGMA " + registry + "table_info(clients)").some(function (c) { return c.name === "signing_alg"; }))
		db.exec("ALTER TABLE " + registry + "clients ADD COLUMN signing_alg TEXT");
	db.run("UPDATE " + registry + "clients SET signing_alg=? WHERE id=?", [alg,id]);
}

({token:token, url64:url64, digest:digest, equal:equal, openDatabase:openDatabase,
    schema:schema, dynamicClientId:dynamicClientId, createRegistry:createRegistry, validCallback:validCallback, subject:subject, signingKey:signingKey, signingKeys:signingKeys,
    addKey:addKey, setSigningAlg:setSigningAlg, jwk:jwk, sign:sign});
