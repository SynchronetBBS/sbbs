// RFC 7033 discovery for local BBS accounts.
"use strict";

function oidcIssuer()
{
	var dbPath = system.ctrl_dir + "oidc.sqlite";
	if (!file_exists(dbPath))
		return "https://" + system.host_name;
	var db = new SQLite(dbPath);
	try {
		return db.query("SELECT issuer FROM settings")[0].issuer;
	} finally {
		db.close();
	}
}

// Synchronet exposes user data as bytes, either UTF-8 or legacy CP437.
function profileText(value)
{
	var bytes = String(value);
	if (!str_is_ascii(bytes) && !str_is_utf8(bytes))
		bytes = utf8_encode(bytes);
	return decodeURIComponent(escape(bytes));
}

function findAccount(local)
{
	var bytes = unescape(encodeURIComponent(local));
	var number = system.matchuser(bytes, false);
	if (!number && !str_is_ascii(bytes)) {
		var legacy = utf8_decode(bytes);
		if (utf8_encode(legacy) === bytes)
			number = system.matchuser(legacy, false);
	}
	return number;
}

function publicUrl(template, account, encodedAlias)
{
	if (typeof template !== "string")
		return;
	var url = template.replace(/\{(alias|number|number4)\}/g, function (_, key) {
		if (key === "alias") return encodedAlias;
		if (key === "number4") return format("%04d", account.number);
		return String(account.number);
	});
	if (/^https:\/\/[a-z0-9.-]+(?::[0-9]+)?\/[!-~]*$/i.test(url) && !/[{}\\]/.test(url))
		return url;
}

function serve()
{
	var query = http_request.query;
	http_reply.header["Access-Control-Allow-Origin"] = "*";
	function error(status) {
		http_reply.status = status;
		http_reply.header["Content-Type"] = "text/plain; charset=utf-8";
		if (http_request.method !== "HEAD")
			writeln(status);
	}
	if (http_request.method !== "GET" && http_request.method !== "HEAD") {
		http_reply.header.Allow = "GET, HEAD";
		return error("405 Method Not Allowed");
	}
	if (!query.resource || query.resource.length !== 1 || !/^[a-z][a-z0-9+.-]*:/i.test(query.resource[0]))
		return error("400 Bad Request");
	var resource = query.resource[0];
	if (!/^acct:/i.test(resource))
		return error("404 Not Found");
	var parts = /^acct:([^@]+)@([a-z0-9.-]+)$/i.exec(resource);
	if (!parts || /[^A-Za-z0-9._~!$&'()*+,;=% -]/.test(parts[1]))
		return error("400 Bad Request");
	var local;
	try {
		local = decodeURIComponent(parts[1]);
	} catch (_) {
		return error("400 Bad Request");
	}
	if (/[\x00-\x1f\x7f]/.test(local))
		return error("400 Bad Request");

	var issuer = oidcIssuer();
	var host = system.host_name.toLowerCase();
	var issuerHost = issuer.slice("https://".length).replace(/:\d+$/, "").toLowerCase();
	var requestedHost = parts[2].toLowerCase();
	if (requestedHost !== host && requestedHost !== issuerHost)
		return error("404 Not Found");
	var number = findAccount(local);
	if (!number)
		return error("404 Not Found");
	var account = new User(number);
	require("userdefs.js", "USER_DELETED");
	if (!account.number || (account.settings & (USER_DELETED | USER_INACTIVE)))
		return error("404 Not Found");

	// matchuser also accepts partial equivalents (e.g. punctuation differences).
	var accountAlias = profileText(account.alias);
	if (accountAlias.toLowerCase() !== local.toLowerCase())
		return error("404 Not Found");
	var alias = encodeURIComponent(accountAlias);
	var prefix = "acct:" + alias + "@";
	var doc = {subject: prefix + host, properties: {}, links: []};
	if (host !== issuerHost)
		doc.aliases = [prefix + issuerHost];
	var ns = "https://synchro.net/ns/finger/";
	var visibility = load("finger_options.js");
	function property(key, value) {
		if (value !== undefined && value !== null && value !== "")
			doc.properties[ns + key] = profileText(value);
	}
	property("handle", account.handle);
	property("email", account.email);
	if (visibility.include_real_name) property("real-name", account.name);
	if (visibility.include_location) property("location", account.location);
	if (visibility.include_gender) property("gender", account.gender);
	if (visibility.include_age && account.age) property("age", account.age);

	function link(rel, href, type) {
		if (!href || (query.rel && query.rel.indexOf(rel) < 0))
			return;
		var item = {rel: rel, href: href};
		if (type) item.type = type;
		doc.links.push(item);
	}
	link("http://openid.net/specs/connect/1.0/issuer", issuer);
	var web = load("modopts.js", "webfinger") || {};
	link("http://webfinger.net/rel/profile-page", publicUrl(web.profile_url, account, alias), "text/html");
	if (web.plan_url && file_exists(format("%suser/%04d.plan", system.data_dir, number)))
		link(ns + "plan", publicUrl(web.plan_url, account, alias), "text/plain");

	http_reply.status = "200 OK";
	http_reply.header["Content-Type"] = "application/jrd+json";
	if (http_request.method !== "HEAD")
		write(unescape(encodeURIComponent(JSON.stringify(doc))));
}

serve();
