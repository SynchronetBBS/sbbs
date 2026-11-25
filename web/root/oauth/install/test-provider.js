// jsexec install/test-provider.js /new/private/test-directory
"use strict";
var dir = js.exec_dir + "../";
var tmp = argv[0] + "/";
if (argc !== 1 || !mkdir(tmp)) throw new Error("Supply a new scratch directory");
mkdir(tmp + "user");
var site = {ctrl_dir:tmp,data_dir:tmp,host_name:"bbs.example",lastuser:3};
var P = load({system:site}, dir+"provider.js");
var issuer = "https://bbs.example", callback = "https://app.example/callback";
var f = new File(tmp+"main.ini");
if (!f.open("w")) throw new Error("Cannot create test main.ini");
f.writeln("Password=fixture"); f.close();
var key = P.signingKey("ES256");
var db = P.openDatabase(true), secret = P.token();
P.schema(db,issuer);
db.run("INSERT INTO clients VALUES('app',?,'ALLOW',1)",[P.digest(secret)]);
db.run("INSERT INTO callbacks VALUES('app',?)",[callback]);
var allowed = true, now = time(), serverSession;
var account = {number:2, security:{password:"fixture",password_date:1,restrictions:0},settings:0,
    alias:"Test Name",name:"Real Name",netmail:"test@example.org",compare_ars:function () { return allowed; }};
function assert(ok,msg) { if (!ok) throw new Error(msg); }
var registrationOptions = {};
function request(endpoint, method, params, cookies, headers, bodyData) {
	var reply = {status:"200 OK",header:{}}, body = "", q = {};
	Object.keys(params || {}).forEach(function (k) { q[k] = [params[k]]; });
	var scope = {http_request:{host:"bbs.example",method:method,cookie:cookies || {},query:q,post_data:bodyData,
	    header:headers || {"content-type":"application/x-www-form-urlencoded"}},http_reply:reply,
	    client:{protocol:"HTTPS"},js:{exec_dir:dir},system:site,User:function () { return account; },user:account,
	    login:function (name,pw) {return name === account.alias && pw === "fixture";},
	    write:function (s) { body += s; },time:function () { return now; },
	    USER_DELETED:1,USER_INACTIVE:64,UFLAG_G:64,
	    load:function (o,path) {
		    if (o === "modopts.js" && path === "oidc") return registrationOptions;
		    if (path === dir+"provider.js") return P;
		    if (path === dir+"request.js") {
			    Object.keys(scope).forEach(function (k) { o[k] = scope[k]; });
		    }
		    return load(o,path);
	    }};
	if (serverSession !== undefined) scope.http_request.browser_session = serverSession;
	load(scope,dir+endpoint+".ssjs");
	return {status:reply.status,header:reply.header,body:body};
}
var keys = JSON.parse(request("jwks","GET").body).keys;
assert(keys.length === 2 && keys[0].x === P.jwk(key).x && keys[0].crv === "P-256" && keys[1].alg === "RS256", "published signing keys");
var verifier = P.token();
var req = {response_type:"code", scope:"openid profile email",client_id:"app",redirect_uri:callback,
    code_challenge_method:"S256",code_challenge:P.url64(sha256_calc(verifier)),nonce:"abc",state:"state"};
function exchange(code, ver) {
	return request("token","POST",{grant_type:"authorization_code",code:code,client_id:"app",client_secret:secret,
	    redirect_uri:callback,code_verifier:ver});
}
function gotCode(r) {
	assert(r.status === "302 Found" && r.header.Location.indexOf(callback+"?") === 0,"missing callback: "+r.status+" "+r.body);
	var m = /[?&]code=([^&]+)/.exec(r.header.Location);
	assert(m,"missing code: "+r.header.Location);
	return m[1];
}
var first = request("auth","GET",req);
assert(first.body.indexOf("BBS username") >= 0,"sign-in form: "+first.body);
var csrf = first.header["Set-Cookie"].split(/[=;]/)[1], cookies = {"__Host-sbbs_oidc_form":[csrf]};
req.username=account.alias; req.password="fixture"; req.csrf=csrf;
assert(request("auth","POST",req).status === "403 Forbidden","csrf");
var post = request("auth","POST",req,cookies);
delete req.username; delete req.password; delete req.csrf;
var code = gotCode(post);
cookies["__Host-sbbs_oidc"]=[post.header["Set-Cookie"].split(/[=;]/)[1]];
assert(JSON.parse(exchange(code,P.token()).body).error === "invalid_grant","PKCE mismatch");
var tokens = JSON.parse(exchange(code,verifier).body);
assert(tokens.id_token && tokens.access_token,"redemption: "+JSON.stringify(tokens));
assert(JSON.parse(exchange(code,verifier).body).error === "invalid_grant","replay");
function info() { return JSON.parse(request("userinfo","GET",{},null,{authorization:"Bearer "+tokens.access_token}).body); }
var sub = info().sub;
assert(JSON.parse(base64_decode(tokens.id_token.split(".")[0])).alg === "RS256", "legacy client signature");
assert(sub && info().email_verified === false,"userinfo");
account.alias = "New Alias";
assert(info().sub === sub && info().preferred_username === "New Alias","rename");
allowed=false;
assert(request("auth","GET",req,cookies).header.Location.indexOf("access_denied")>0,"ARS");
allowed=true;
var expiring=gotCode(request("auth","GET",req,cookies));
now+=601;
assert(JSON.parse(exchange(expiring,verifier).body).error === "invalid_grant","code expiry");
now+=3600;
assert(info().error === "invalid_token","access expiry");
assert(request("logout","POST",{csrf:csrf},cookies).status === "200 OK","logout");
assert(request("auth","GET",req,cookies).body.indexOf("BBS username")>=0,"session revoked");
var v4file = new File(tmp+"user/0002.web");
if (!v4file.open("w")) throw new Error("Cannot create webv4 session fixture");
v4file.writeln("key=abcdef");v4file.close();
var v4={synchronet:["2,abcdef"]};
var handoff=request("auth","GET",req,v4);
gotCode(handoff);
v4["__Host-sbbs_oidc"]=[handoff.header["Set-Cookie"].split(/[=;]/)[1]];

// A future server session takes precedence, even when both legacy cookies
// identify a real user. Keep an eligible ambient user in the anonymous case.
var loginRequests = [], logouts = 0;
serverSession = {
	auth_time: null,
	request_login: function (path, force) { loginRequests.push({path:path, force:force}); },
	logout: function () { logouts++; this.auth_time = null; }
};
var sessionsBefore = db.query("SELECT hash,seen FROM sessions ORDER BY hash");
req.prompt = "none";
assert(request("auth","GET",req,v4).header.Location === callback+"?error=login_required&state=state",
    "anonymous server session silently reused a legacy login");
assert(loginRequests.length === 0,"prompt=none initiated sign-in");
delete req.prompt;
req.state = "state & /next";
req.nonce = "nonce+value";
request("auth","GET",req,v4);
assert(loginRequests.length === 1 && loginRequests[0].force === false,"interactive server sign-in");
function resumed(path) {
	var fields = {}, prefix = "/oauth/auth.ssjs?";
	assert(path.indexOf(prefix) === 0,"server login must resume locally");
	path.slice(prefix.length).split("&").forEach(function (pair) {
		var i = pair.indexOf("=");
		fields[pair.slice(0,i)] = decodeURIComponent(pair.slice(i+1));
	});
	return fields;
}
var back = resumed(loginRequests[0].path);
assert(Object.keys(req).length === Object.keys(back).length &&
    Object.keys(req).every(function (k) { return req[k] === back[k]; }),"resume parameters");
assert(request("auth","GET",{client_id:"app",redirect_uri:"https://unknown.example"},v4).status === "400 Bad Request"
    && loginRequests.length === 1,"unregistered callback reached server sign-in");
serverSession.auth_time = now-120;
req.prompt = "login";
request("auth","GET",req,v4);
assert(loginRequests.length === 2 && loginRequests[1].force === true,"forced server reauthentication");
back = resumed(loginRequests[1].path);
assert(back.prompt === undefined && back.nonce === req.nonce,"fresh sign-in would loop or lose nonce");
serverSession.auth_time = now-5;
var authorized = request("auth","GET",back,v4);
assert(!authorized.header["Set-Cookie"],"server sign-in issued a provider cookie");
var serverTokens = JSON.parse(exchange(gotCode(authorized),verifier).body);
var payload = serverTokens.id_token.split(".")[1].replace(/-/g,"+").replace(/_/g,"/");
var claims = JSON.parse(base64_decode(payload));
assert(claims.auth_time === serverSession.auth_time && claims.sub === sub && claims.nonce === req.nonce,
    "server-authenticated ID token");
allowed = false;
assert(request("auth","GET",back,v4).header.Location.indexOf("error=access_denied")>0,"server session bypassed ARS");
allowed = true;
back.username = account.alias;
assert(request("auth","POST",back,v4).status === "400 Bad Request","server mode accepted provider credentials");
delete back.username;
assert(request("logout","GET",{},v4).body.indexOf("Sign out")>=0 && logouts===0,"logout confirmation");
assert(request("logout","POST",{csrf:csrf},v4).status==="403 Forbidden" && logouts===0,"server logout CSRF");
v4["__Host-sbbs_oidc_form"]=[csrf];
assert(request("logout","POST",{csrf:csrf},v4).status==="200 OK" && logouts===1,"server logout hook");
assert(file_exists(v4file.name) &&
    JSON.stringify(db.query("SELECT hash,seen FROM sessions ORDER BY hash")) === JSON.stringify(sessionsBefore),
    "server hooks modified standalone sessions");
back.prompt = "none";
assert(request("auth","GET",back,v4).header.Location.indexOf("error=login_required")>0,"revoked session fell back");
serverSession = undefined;
delete req.prompt;
file_remove(v4file.name);
assert(request("auth","GET",req,v4).body.indexOf("BBS username")>=0,"webv4 source must still match");
file_remove(tmp+"user/0002.oidc");
assert(P.subject(2,true)!==sub,"recycled user slot");
assert(request("auth","GET",{client_id:"app",redirect_uri:"https://unknown.example"}).status === "400 Bad Request","unregistered redirect");
// A previously unknown site can provision a client without browser credentials.
function dynamic(data) {
	return request("client-registration","POST",{},null,{"content-type":"application/json"},
	    typeof data === "string" ? data : JSON.stringify(data));
}
var registration = {redirect_uris:[callback,callback+"2"], scope:"openid profile", client_name:"Example <site>"};
assert(dynamic("{").status === "400 Bad Request", "malformed registration JSON");
assert(JSON.parse(dynamic({redirect_uris:[callback,"http://example.org/"]}).body).error === "invalid_redirect_uri",
    "reject all callbacks if any is invalid");
assert(JSON.parse(dynamic({redirect_uris:[callback],grant_types:["implicit"]}).body).error === "invalid_client_metadata", "unsupported grant type");
assert(request("client-registration","GET",{}).status === "405 Method Not Allowed", "registration GET");
assert(request("client-registration","POST",{}).status === "415 Unsupported Media Type", "registration requires JSON");
registrationOptions.dynamic_registrations_per_hour = 1;
assert(JSON.parse(dynamic({redirect_uris:[callback], id_token_signed_response_alg:"none"}).body).error === "invalid_client_metadata", "invalid signing algorithm");
registration.id_token_signed_response_alg = "ES256";
var created = dynamic(registration);
assert(created.status === "201 Created" && created.header["Cache-Control"] === "no-store", "registration response");
var rp = JSON.parse(created.body);
assert(P.dynamicClientId(rp.client_id) && rp.client_secret.length === 43 && rp.client_secret_expires_at === 0 &&
    rp.token_endpoint_auth_method === "client_secret_post" && rp.redirect_uris.join() === registration.redirect_uris.join(), "provisioned metadata");
assert(dynamic(registration).status === "429 Too Many Requests", "registration limit");
registrationOptions.dynamic_registrations_per_hour = 0;
assert(dynamic(registration).status === "403 Forbidden", "disabled registration");
delete registrationOptions.dynamic_registrations_per_hour;
assert(db.query("SELECT secret FROM clients WHERE id=?",[rp.client_id])[0].secret === P.digest(rp.client_secret), "secret digest");
var dynReq = {response_type:"code",scope:"openid profile",client_id:rp.client_id,redirect_uri:callback,
    code_challenge_method:"S256",code_challenge:P.url64(sha256_calc(verifier)),nonce:"first"};
function dynExchange(code) {
	var reply = request("token","POST",{grant_type:"authorization_code",client_id:rp.client_id,client_secret:rp.client_secret,
	    code:code,redirect_uri:callback,code_verifier:verifier});
	return JSON.parse(reply.body);
}
var anonymous = request("auth","GET",dynReq);
assert(anonymous.body.indexOf("Example &lt;site&gt;")>=0 && anonymous.body.indexOf("BBS username")>=0, "first visit prompts for sign-in and consent");
var dynCsrf=anonymous.header["Set-Cookie"].split(/[=;]/)[1], dynCookies={"__Host-sbbs_oidc_form":[dynCsrf]};
dynReq.username=account.alias; dynReq.password="fixture"; dynReq.csrf=dynCsrf; dynReq.decision="allow";
var approved = request("auth","POST",dynReq,dynCookies);
var initialTokens = dynExchange(gotCode(approved));
dynCookies["__Host-sbbs_oidc"]=[approved.header["Set-Cookie"].split(/[=;]/)[1]];
function tokenSubject(access) {
	return JSON.parse(request("userinfo","GET",{},{},{authorization:"Bearer "+access}).body).sub;
}
assert(JSON.parse(base64_decode(initialTokens.id_token.split(".")[0])).alg === "ES256", "negotiated EC signature");
var initialSub=tokenSubject(initialTokens.access_token);
assert(initialSub && initialTokens.id_token, "first sign-in to new site");
delete dynReq.username; delete dynReq.password;
dynReq.prompt="none";
assert(request("auth","GET",dynReq,dynCookies).header.Location.indexOf("consent_required")>0, "dynamic client requires consent");
delete dynReq.prompt;
dynReq.decision="deny";
assert(request("auth","POST",dynReq,dynCookies).header.Location.indexOf("access_denied")>0, "denied dynamic client");
dynReq.decision="allow";
var returnTokens = dynExchange(gotCode(request("auth","POST",dynReq,dynCookies)));
assert(tokenSubject(returnTokens.access_token) === initialSub, "returning user has same identity");
dynReq.scope="openid email";
assert(request("auth","GET",dynReq,dynCookies).header.Location.indexOf("invalid_scope")>0, "registered scope enforced");
dynReq.redirect_uri="https://other.example/callback";
assert(request("auth","GET",dynReq,dynCookies).status === "400 Bad Request", "registered redirect enforced");
db.run("DELETE FROM clients WHERE id=?", [rp.client_id]);
assert(!tokenSubject(returnTokens.access_token) && !db.query("SELECT id FROM dynamic_clients WHERE id=?", [rp.client_id]).length,
    "removal cascades to dynamic metadata and grants");

// Self-service clients and their grants live entirely in the owner's file.
var regFields = {action:"create",signing_alg:"ES256",name:"Example App",redirect_uri:callback,csrf:csrf};
var formCookies = {"__Host-sbbs_oidc_form":[csrf]};
var ownerPath = tmp+"user/0002.apps.sqlite";
assert(request("register","GET",{}).body.indexOf("BBS username") >= 0, "anonymous registration");
assert(request("register","POST",regFields,{}).status === "403 Forbidden", "registration CSRF");
var sign = request("register","POST",{action:"login",username:account.alias,password:"fixture",csrf:csrf},formCookies);
assert(sign.status === "303 See Other", "owner sign-in");
formCookies["__Host-sbbs_oidc"] = [sign.header["Set-Cookie"].split(/[=;]/)[1]];
assert(request("register","GET",{},formCookies).body.indexOf("Register an application") >= 0 && !file_exists(ownerPath), "empty dashboard");
regFields.redirect_uri = "http://app.example/";
assert(request("register","POST",regFields,formCookies).body.indexOf("exact HTTPS") >= 0 && !file_exists(ownerPath), "invalid registration callback");
regFields.redirect_uri = callback;
function credentials(reply) {
	var m = /Client ID<\/dt><dd><code>(user:[^<]+)<\/code><\/dd><dt>Client secret<\/dt><dd><code>([a-zA-Z0-9_-]{43})/.exec(reply.body);
	assert(reply.status === "200 OK" && m, "expected client credentials: "+reply.status);
	return {id:m[1],secret:m[2]};
}
var app = credentials(request("register","POST",regFields,formCookies));
assert(file_exists(ownerPath) && !db.query("SELECT 1 FROM clients WHERE id=?", [app.id]).length, "per-user registration");
assert(request("register","POST",regFields,formCookies).body.indexOf("already have") >= 0, "duplicate registration");
assert(request("register","GET",{},formCookies).body.indexOf(app.secret) < 0, "secret only shown once");
req.client_id = app.id;
req.prompt = "none";
assert(request("auth","GET",req,formCookies).header.Location.indexOf("error=consent_required") > 0, "silent disclosure");
delete req.prompt;
assert(request("auth","GET",req,formCookies).body.indexOf("your email address") >= 0, "consent scopes");
req.csrf = csrf;
req.decision = "deny";
assert(request("auth","POST",req,formCookies).header.Location.indexOf("error=access_denied") > 0, "declined consent");
req.decision = "allow";
var appCode = gotCode(request("auth","POST",req,formCookies));
function redeemApp(pw) {
	return JSON.parse(request("token","POST",{grant_type:"authorization_code",client_id:app.id,client_secret:pw,
	    code:appCode,redirect_uri:callback,code_verifier:verifier}).body);
}
var change = {csrf:csrf,action:"rotate",client_id:app.id};
var rotated = credentials(request("register","POST",change,formCookies));
assert(rotated.secret !== app.secret && redeemApp(app.secret).error === "invalid_client", "old secret rejected");
var memberTokens = redeemApp(rotated.secret);
assert(JSON.parse(base64_decode(memberTokens.id_token.split(".")[0])).alg === "ES256", "member signing preference");
var access = memberTokens.access_token;
function appInfo() { return request("userinfo","GET",{},{},{authorization:"Bearer "+access}); }
assert(access && JSON.parse(appInfo().body).preferred_username === account.alias, "member-owned app token and userinfo");
account.settings = 1;
assert(appInfo().status === "401 Unauthorized", "deleted owner");
account.settings = 0;
serverSession = {auth_time:now,request_login:function () {},logout:function () {}};
account.number = 3;
assert(request("register","GET",{},formCookies).body.indexOf(app.id) < 0, "other owner's list");
change.action = "remove";
assert(request("register","POST",change,formCookies).status === "404 Not Found", "cross-owner removal");
account.number = 2;
assert(request("register","POST",change,formCookies).status === "303 See Other" && appInfo().status === "401 Unauthorized", "remove revokes access");
app = credentials(request("register","POST",regFields,formCookies));
file_remove(tmp+"user/0002.oidc");
assert(request("register","GET",{},formCookies).body.indexOf(app.id) < 0, "replacement owner cannot inherit registry");
file_remove(ownerPath);
req.client_id = app.id;
assert(request("auth","GET",req,formCookies).status === "400 Bad Request" && !file_exists(ownerPath), "deleted registration stays gone");
account.is_guest = true;
assert(request("register","POST",regFields,formCookies).status === "403 Forbidden", "guest registration");
// Cryptlib exports minimal EC coordinates. JWK and JOSE require their full widths.
function unurl(value) { return base64_decode(value.replace(/-/g, "+").replace(/_/g, "/")); }
[["ES256",32],["ES384",48],["ES512",66]].forEach(function (spec) {
	var signer = P.addKey(spec[0]), width = spec[1];
	var jwk = P.jwk(signer);
	assert(unurl(jwk.x).length === width && unurl(jwk.y).length === width, "coordinate width "+spec[0]);
	var jwt = P.sign({sub:"fixture"},signer).split(".");
	var header = JSON.parse(unurl(jwt[0]));
	assert(header.kid === jwk.kid && header.alg === jwk.alg && unurl(jwt[2]).length === 2*width, "signature format "+spec[0]);
	assert(P.jwk(P.addKey(spec[0])).x === jwk.x, "existing key replaced");
});
assert(directory(tmp+"*.key").length === 1 && file_exists(tmp+"oid.key"), "one signing keyset");
assert(P.signingKeys().length === 4 && P.jwk(P.signingKey()).x === keys[0].x, "all four keys survive reopen");

// The original provider wrote one host-labelled key. Add EC without rotating RSA.
var oldDir = tmp+"legacy/";
assert(mkdir(oldDir) && file_copy(tmp+"main.ini",oldDir+"main.ini"), "legacy setup");
var original = new CryptContext(CryptContext.ALGO.RSA);
original.keysize = 256;
original.label = site.host_name;
original.generate_key();
var oldStore = new CryptKeyset(oldDir+"oid.key",CryptKeyset.KEYOPT.CREATE);
try { oldStore.add_private_key(original,"fixture"); } finally { oldStore.close(); }
var legacy = load({system:{ctrl_dir:oldDir,host_name:site.host_name}},dir+"provider.js");
var oldKeys = legacy.signingKeys().map(legacy.jwk);
assert(oldKeys.length === 2 && oldKeys[0].kid === "1" && oldKeys[0].n === original.public_key.n &&
    oldKeys[1].alg === "ES256" && oldKeys[1].kid !== "1" && directory(oldDir+"*.key").length === 1,
    "add EC to the original keyset");
// Delete either default key and let an ordinary request repair the store.
["RS256","ES256"].forEach(function (alg) {
	var before = P.signingKeys().map(P.jwk);
	var store = new CryptKeyset(tmp+"oid.key",CryptKeyset.KEYOPT.NONE);
	try { store.delete_key("OIDC " + alg); } finally { store.close(); }
	var after = JSON.parse(request("jwks","GET").body).keys;
	assert(after.length === before.length && before.every(function (old) {
		var next = after.filter(function (k) { return k.alg === old.alg; })[0];
		return next && (old.alg === alg ? next.kid !== old.kid : JSON.stringify(next) === JSON.stringify(old));
	}), "regenerate only missing "+alg);
});
var padded = P.jwk({alg:"ES512",kid:"fixture",key:{public_key:{x:"AQ",y:"Ag"}}});
assert(unurl(padded.x).length === 66 && unurl(padded.x).charCodeAt(65) === 1, "short P-521 coordinate");
db.close();
print("Passed provider tests");
