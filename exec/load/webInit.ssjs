// webInit.ssjs, by echicken -at- bbs.electronicchicken.com

// Some bootstrapping stuff for the web interface, kept in exec/load/ so that
// layout.ssjs can find it.  Loads the web interface configuration into the
// webIni object, logs in the current user.

load('sbbsdefs.js');

var f = new File(system.ctrl_dir + 'web.ini');
f.open("r");
var webIni = f.iniGetObject();
f.close();

if(http_request.query.hasOwnProperty('username') && http_request.query.hasOwnProperty('password')) {
	var UID = system.matchuser(http_request.query.username);
	var u = new User(UID);
	if(u && http_request.query.password.toString().toUpperCase() == u.security.password) {
		set_cookie('synchronet', UID, time() + webIni.sessionTimeout, system.inet_addr, "/");
		login(u.alias, u.security.password);
	}
} else if(http_request.header.hasOwnProperty('cookie') && http_request.header.cookie.match(/synchronet\=\d+/) != null && !http_request.query.hasOwnProperty('logout')) {
	var UID = http_request.header.cookie.match(/\d+/);
	var u = new User(UID);
	if(u.ip_address == client.ip_address) {
		set_cookie('synchronet', UID, time() + webIni.sessionTimeout, system.inet_addr, "/");
		login(u.alias, u.security.password);
	}
}

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