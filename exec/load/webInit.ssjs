// webInit.ssjs, by echicken -at- bbs.electronicchicken.com

// Some bootstrapping stuff for the web interface, kept in exec/load/ so that
// layout.ssjs can find it.  Loads the web interface configuration into the
// webIni object, logs in the current user.

load('sbbsdefs.js');

var f = new File(system.ctrl_dir + 'web.ini');
f.open("r");
var webIni = f.iniGetObject();
f.close();

// Returns a string of random characters 'length' characters long
function randomString(length) {
        var chars = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXTZabcdefghiklmnopqrstuvwxyz'.split('');
        var str = '';
        for (var i = 0; i < length; i++) str += chars[Math.floor(Math.random() * chars.length)];
        return str;
}

// Returns an unopened file object representing the user's .session file
function getSessionKeyFile(userNumber) {
	var sessionKeyFile = userNumber;
	while(sessionKeyFile.length < 4) sessionKeyFile = "0" + sessionKeyFile;
	sessionKeyFile += ".session";
	var f =	new File(system.data_dir + "user/" + sessionKeyFile);
	return f;
}

if(http_request.query.hasOwnProperty('username') && http_request.query.hasOwnProperty('password')) {
	// Script was (we'll assume) called from the login form.  Attempt to authenticate the user.
	var sessionKey = randomString(512); // Arbitrary length, can be shorter, have seen problems with longer.
	var UID = system.matchuser(http_request.query.username);
	var u = new User(UID);
	if(u && http_request.query.password.toString().toUpperCase() == u.security.password.toUpperCase()) {
		// The supplied username was valid, and the supplied password is correct. Create a cookie, log the user in and populate their .session file.
		set_cookie('synchronet', UID + ',' + sessionKey, time() + webIni.sessionTimeout, http_request.host, "/");
		login(u.alias, u.security.password);
		var f = getSessionKeyFile(user.number.toString());
		if(f.open("w"))	{
			// If this fails, the user will only be logged in for the duration of this page load.
			f.write(sessionKey);
			f.close();
		}
	}
} else if(http_request.cookie.hasOwnProperty('synchronet') && http_request.cookie.synchronet[0].match(/\d+,\w+/) != null && !http_request.query.hasOwnProperty('logout')) {
	// A 'synchronet' cookie exists and matches our '<user.number>,<sessionKey>' format.
	var cookie = http_request.cookie.synchronet[0].match(/\d+,\w+/)[0].split(',');
	var u = new User(cookie[0]);
	var sessionKey = false;
	var f = getSessionKeyFile(u.number.toString());
	if(f.exists) {
			f.open("r");
			sessionKey = f.read();
			f.close();
	}
	// If the user was not valid, 'f' should not have existed, and sessionKey will evaluate false.
	if(u && sessionKey && sessionKey == cookie[1].toString()) {
			// The user specified in the cookie exists, and the sessionKey from the cookie matches that on file. Update the cookie's expiration and log the user in.
			set_cookie('synchronet', u.number + ',' + cookie[1], time() + webIni.sessionTimeout, system.inet_addr, "/");
			login(u.alias, u.security.password);
	}
}

// If none of the above conditions were met, user 0 is still signed in, so we should log in the guest user
if(user.number == 0) {
	var guestUID = system.matchuser(webIni.guestUser);
	var u = new User(guestUID);
	set_cookie('synchronet', guestUID, time() + webIni.sessionTimeout, system.inet_addr, "/");
	login(u.alias, u.security.password);
}

// Yeah, this kinda sucks, but it works.
if(http_request.query.hasOwnProperty('callback')) {
	if(http_request.query.hasOwnProperty('username') && user.alias == webIni.guestUser) {
		if(http_request.query.callback.toString().match(/\?/) != null) {
			var loc = http_request.query.callback + "&loginfail=true";
		} else {
			var loc = http_request.query.callback + "?loginfail=true";
		}
	} else {
		var loc = http_request.query.callback;
	}
	print("<html><head><script type=text/javascript>window.location='" + loc + "'</script></head></html>");
}
