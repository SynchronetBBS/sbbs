// webInit.ssjs, by echicken -at- bbs.electronicchicken.com

// Some bootstrapping stuff for the web interface, kept in exec/load/ so that
// layout.ssjs can find it.  Loads the web interface configuration into the
// webIni object, logs in the current user.

load('sbbsdefs.js');

var f = new File(system.ctrl_dir + 'web.ini');
f.open("r");
var webIni = f.iniGetObject();
f.close();

function randomString(length) {
        var chars = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXTZabcdefghiklmnopqrstuvwxyz'.split('');
        var str = '';
        for (var i = 0; i < length; i++) str += chars[Math.floor(Math.random() * chars.length)];
        return str;
}

if(http_request.query.hasOwnProperty('username') && http_request.query.hasOwnProperty('password')) {
	var sessionKey = randomString(30); // user.note seems to truncate at 30
	var UID = system.matchuser(http_request.query.username);
	var u = new User(UID);
	if(u && http_request.query.password.toString().toUpperCase() == u.security.password.toUpperCase()) {
		set_cookie('synchronet', UID + ',' + sessionKey, time() + webIni.sessionTimeout, system.inet_addr, "/");
		login(u.alias, u.security.password);
		u.note = sessionKey; 
	}
} else if(http_request.header.hasOwnProperty('cookie') && http_request.header.cookie.match(/synchronet\=\d+,\w+/) != null && !http_request.query.hasOwnProperty('logout')) {
	var cookie = http_request.header.cookie.toString().match(/\d+,\w+/)[0].split(',');
	var u = new User(cookie[0]);
	if(u && u.note == cookie[1].toString()) {
		set_cookie('synchronet', u.number + ',' + cookie[1], time() + webIni.sessionTimeout, system.inet_addr, "/");
		login(u.alias, u.security.password);
		u.note = cookie[1];
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
