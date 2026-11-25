// Loaded in a fresh scope for each SSJS request.
"use strict";
var provider, db, issuer, libraryDir, webv4;
var sessionCookie = "__Host-sbbs_oidc", csrfCookie = "__Host-sbbs_oidc_form";
var registry = "main.", ownerNumber = 0;
var idle = 7200, lifetime = 86400;

function fail(code, status, description)
{
	throw {oidc:code, status:status || "400 Bad Request", description:description};
}

function json(value)
{
	http_reply.header["Content-Type"] = "application/json";
	write(JSON.stringify(value));
}

function method(allowed)
{
	if (allowed.indexOf(http_request.method) < 0) {
		http_reply.header.Allow = allowed.join(", ");
		fail("invalid_request", "405 Method Not Allowed");
	}
}

function formPost()
{
	if (http_request.query_string || !/^application\/x-www-form-urlencoded(?:\s*;|$)/i.test(http_request.header["content-type"] || ""))
		fail("invalid_request");
}

function param(name, optional)
{
	var q = http_request.query;
	if (!Object.prototype.hasOwnProperty.call(q, name)) {
		if (optional) return null;
		fail("invalid_request");
	}
	var a = q[name];
	if (!a || a.length !== 1 || !a[0] || a[0].length > 4096 || /[\x00-\x1f\x7f]/.test(a[0]))
		fail("invalid_request");
	return a[0];
}

function cookie(name)
{
	var a = http_request.cookie[name];
	if (a && a.length === 1 && /^[A-Za-z0-9_-]{43}$/.test(a[0])) return a[0];
	return "";
}

function setCookie(name, value)
{
	http_reply.header["Set-Cookie"] = name + "=" + value + "; Path=/; HttpOnly; Secure; SameSite=Lax" +
	    (value ? "" : "; Max-Age=0");
}

function validateForm()
{
	var csrf = cookie(csrfCookie);
	if (!csrf || !provider.equal(param("csrf"), csrf)) fail("invalid_request", "403 Forbidden");
}

function html(s)
{
	return String(s).replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/"/g,"&quot;");
}

function formToken()
{
	var csrf = cookie(csrfCookie) || provider.token();
	setCookie(csrfCookie, csrf);
	return csrf;
}

function page(title)
{
	http_reply.header["Content-Type"] = "text/html; charset=UTF-8";
	write('<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>' +
	    html(title) + '</title><main><h1>' + html(title) + '</h1>');
}

function passwordFields()
{
	write('<p><label>BBS username <input name="username" autocomplete="username" required></label></p>' +
	    '<p><label>Password <input type="password" name="password" autocomplete="current-password" required></label></p>');
}

function form(request, message, app, signedIn)
{
	var csrf = formToken();
	var title = app ? "Authorize " + app.name : request ? "Sign in to " + request.client_id : "Sign out of Synchronet";
	page(title);
	if (message) write('<p role="alert">' + html(message) + '</p>');
	write('<form method="post" action="/oauth/' + (request ? "auth.ssjs" : "logout.ssjs") +
	    '"><input type="hidden" name="csrf" value="' + csrf + '">');
	if (request) {
		Object.keys(request).forEach(function (key) {
			if (request[key] !== null) write('<input type="hidden" name="' + key + '" value="' + html(request[key]) + '">');
		});
		if (app) {
			var scopes = request.scope.split(" "), fields = ["your account identifier"];
			if (scopes.indexOf("profile") >= 0) fields.push("your alias and real name");
			if (scopes.indexOf("email") >= 0) fields.push("your email address");
			write('<p>Allow this application to receive ' + fields.join(", ") + '?</p><p>Callback: <code>' +
			    html(request.redirect_uri) + '</code></p>');
		}
		if (!signedIn) passwordFields();
	}
	if (app) write('<button name="decision" value="allow">Allow</button> <button name="decision" value="deny" formnovalidate>Deny</button>');
	else write('<button>' + (request ? "Sign in" : "Sign out") + '</button>');
	write('</form>');
	if (request) write('<p><a href="/oauth/register.ssjs">Register an application</a></p>');
	write('</main></html>');
}

// Only an authenticated owner's registration page can create this database.
// ATTACH lets grants, client changes and shared sessions commit together.
function useOwner(num, create)
{
	if (!(num > 0 && num <= system.lastuser) || !eligible(new User(num), "")) return false;
	var path = system.data_dir + format("user/%04u.apps.sqlite", num);
	if (!create && !file_exists(path)) return false;
	db.run("ATTACH DATABASE ? AS apps", [path]);
	function ownerMatches() {
		var initialized = db.query("SELECT 1 FROM apps.sqlite_master WHERE type='table' AND name='owner'").length;
		if (!initialized && !create) return false;
		var sub = provider.subject(num, !initialized);
		if (!initialized) {
			db.exec("CREATE TABLE apps.owner (sub TEXT NOT NULL)");
			provider.createRegistry(db, true);
			db.run("INSERT INTO apps.owner VALUES (?)", [sub]);
		}
		return !!sub && !!db.query("SELECT 1 FROM apps.owner WHERE sub=?", [sub]).length;
	}
	if (!(create ? db.transaction(ownerMatches) : ownerMatches())) return false;
	registry = "apps.";
	ownerNumber = num;
	return true;
}

function selectClient(id)
{
	var m = /^user:([1-9][0-9]{0,8}):[A-Za-z0-9_-]{43}$/.exec(id);
	return !m || useOwner(Number(m[1]), false);
}

function dynamicRegistrationLimit()
{
	var options = load("modopts.js", "oidc") || {};
	if (options.dynamic_registrations_per_hour === undefined) return 60;
	var limit = Number(options.dynamic_registrations_per_hour);
	return isFinite(limit) && limit > 0 ? Math.floor(limit) : 0;
}

function registered(id)
{
	if (provider.dynamicClientId(id)) {
		// The metadata table is created by the first successful registration.
		if (!db.query("SELECT 1 FROM main.clients WHERE id=? AND enabled=1", [id]).length) return null;
		return db.query("SELECT c.*, d.name, d.scope FROM main.clients c JOIN main.dynamic_clients d ON d.id=c.id WHERE c.id=? AND c.enabled=1", [id])[0];
	}
	return db.query("SELECT * FROM " + registry + "clients WHERE id=? AND enabled=1", [id])[0];
}

function eligible(u, ars)
{
	require("userdefs.js","USER_DELETED");
	return u && u.number > 0 && !u.is_guest && u.security.password &&
	    !(u.settings & (USER_DELETED | USER_INACTIVE)) &&
	    !(u.security.restrictions & UFLAG_G) && (!ars || u.compare_ars(ars));
}

function web()
{
	if (!webv4) {
		var path = libraryDir + "webv4.js";
		if (!file_exists(path)) path = libraryDir + "../../../webmail/lib/webv4.js";
		webv4 = load({}, path);
	}
	return webv4;
}

function webIdentity()
{
	var values = http_request.cookie.synchronet || [];
	for (var i = 0; i < values.length && i < 16; i++) {
		var value = values[i];
		var num = web().session_user({synchronet:[value]}, system.data_dir);
		if (num) return {num:num, key:provider.digest(value.split(",")[1])};
	}
	return null;
}

function stamp(u, secret)
{
	return provider.digest(secret + "\n" + u.security.password_date + "\n" + u.security.password);
}

function validSession()
{
	var raw = cookie(sessionCookie);
	if (!raw) return null;
	var s = db.query("SELECT * FROM sessions WHERE hash=? AND created>? AND seen>?",
	    [provider.digest(raw),time()-lifetime,time()-idle])[0];
	if (!s) return null;
	var u = new User(s.num);
	if (!eligible(u, "") || s.sub !== provider.subject(s.num, false) || !provider.equal(s.stamp, stamp(u, raw))) return null;
	if (s.webkey) {
		var source = webIdentity();
		if (!source || source.num !== s.num || !provider.equal(source.key, s.webkey)) return null;
	}
	return s;
}

// Called inside the transaction that accepts the login.
function saveSession(u, sub, source, session, now)
{
	db.run("DELETE FROM sessions WHERE seen<=? OR created<=?", [now-idle,now-lifetime]);
	if (session) {
		db.run("UPDATE sessions SET seen=? WHERE hash=?", [now,session.hash]);
		return null;
	}
	var raw = provider.token();
	db.run("DELETE FROM sessions WHERE hash=?", [provider.digest(cookie(sessionCookie))]);
	db.run("INSERT INTO sessions VALUES(?,?,?,?,?,?,?)",
	    [provider.digest(raw),u.number,sub,stamp(u,raw),source ? source.key : null,now,now]);
	return raw;
}

// An enabled server supplies this object even for anonymous requests (#1398).
function serverSession()
{
	var session = http_request.browser_session;
	if (session === undefined) return undefined;
	if (!session || typeof session.request_login !== "function" || typeof session.logout !== "function")
		throw new Error("Invalid server browser-session interface");
	var t = session.auth_time;
	if (t !== null && (typeof t !== "number" || !(t > 0 && t <= time()) || Math.floor(t) !== t))
		throw new Error("Invalid server authentication time");
	return session;
}

function serve(dir, handler)
{
	libraryDir = dir;
	http_reply.header["Cache-Control"] = "no-store";
	http_reply.header.Pragma = "no-cache";
	http_reply.header["Referrer-Policy"] = "no-referrer";
	http_reply.header["Content-Security-Policy"] = "default-src 'none'; form-action 'self'; frame-ancestors 'none'; base-uri 'none'";
	try {
		provider = load({}, dir + "provider.js");
		db = provider.openDatabase(false);
		issuer = db.query("SELECT issuer FROM settings")[0].issuer;
		if (String(client.protocol).toLowerCase() !== "https" || "https://" + http_request.host.toLowerCase() !== issuer)
			fail("invalid_request");
		handler(this);
	} catch (e) {
		if (!e.oidc) log(LOG_ERR,"OIDC " + handler.name + " failed at " + (e.fileName || "") + " line " + (e.lineNumber || 0));
		http_reply.status = e.status || "500 Internal Server Error";
		json({error:e.oidc || "server_error", error_description:e.oidc ? e.description : undefined});
	} finally {
		if (db) db.close();
	}
}

this;
