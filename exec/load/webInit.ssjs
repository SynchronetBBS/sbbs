// webInit.ssjs, by echicken -at- bbs.electronicchicken.com

// Some bootstrapping stuff for the web interface, kept in exec/load/ so that
// other scripts can find it.  Could/should be moved to /sbbs/web/lib.

load('sbbsdefs.js');

var webIni=(function() {
	// Returns a string of random characters 'length' characters long
	function randomString(length) {
		var chars = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXTZabcdefghiklmnopqrstuvwxyz';
		var str = '';
		for (var i = 0; i < length; i++)
			str += chars[Math.floor(Math.random() * chars.length)];
		return str;
	}

	// Returns an unopened file object representing the user's .session file
	function getSessionKeyFile(userNumber) {
		return new File(system.data_dir + format("user/%04u.session", userNumber));
	}

	function setLoginCookie(u, sessionKey)
	{
		set_cookie('synchronet', u.number.toString() + ',' + sessionKey, time() + webIni.sessionTimeout, http_request.host, "/");
		login(u.alias, u.security.password);
	}
	
	var f = new File(system.ctrl_dir + 'modopts.ini');
	f.open("r");
	var webIni = f.iniGetObject("Web");
	f.close();
	
	var f = new File(system.ctrl_dir + 'sbbs.ini');
	f.open("r");
	var sbbsIni = f.iniGetObject("Web");
	f.close();

	webIni.HTTPPort = Number(sbbsIni.Port).toFixed();

	if(http_request.query.username != undefined && http_request.query.password != undefined) {
		// Script was (we'll assume) called from the login form.  Attempt to authenticate the user.
		var sessionKey = randomString(512); // Arbitrary length, can be shorter, have seen problems with longer.
		var UID = system.matchuser(http_request.query.username[0]);
		var u = new User(UID);
		if(u && http_request.query.password[0].toUpperCase() == u.security.password.toUpperCase()) {
			// The supplied username was valid, and the supplied password is correct. Create a cookie, log the user in and populate their .session file.
			setLoginCookie(u, sessionKey);
			var f = getSessionKeyFile(user.number);
			if(f.open("w"))	{
				// If this fails, the user will only be logged in for the duration of this page load.
				f.write(sessionKey);
				f.close();
			}
		}
	} else if(http_request.cookie.synchronet != undefined 
			&& http_request.cookie.synchronet.some(Function('e', 'return(e.search(/^\\d+,\\w+$/) != -1)'))
			&& http_request.query.logout == undefined) {
		// A 'synchronet' cookie exists that matches our '<user.number>,<sessionKey>' format.
		for(var c in http_request.cookie.synchronet) {
			if(http_request.cookie.synchronet[c].search(/^\d+,\w+$/)==-1)
				continue;
			var cookie = http_request.cookie.synchronet[c].split(',');
			var u = new User(cookie[0]);
			var sessionKey = false;
			var f = getSessionKeyFile(u.number);
			if(f.open("r")) {
				sessionKey = f.read();
				f.close();
			}
			// If the user was not valid, 'f' should not have existed, and sessionKey will evaluate false.
			if(u && sessionKey && sessionKey == cookie[1]) {
				// The user specified in the cookie exists, and the sessionKey from the cookie matches that on file. Update the cookie's expiration and log the user in.
				setLoginCookie(u, sessionKey);
				break;
			}
		}
	}

	// If none of the above conditions were met, user 0 is still signed in, so we should log in the guest user
	if(user.number == 0) {
		var guestUID = system.matchuser(webIni.WebGuest);
		var u = new User(guestUID);
		setLoginCookie(u, sessionKey);
	}

	// Yeah, this kinda sucks, but it works.
	if(http_request.query.callback != undefined) {
		if(http_request.query.username != undefined && user.alias == webIni.WebGuest) {
			if(http_request.query.callback[0].match(/\?/) != null) {
				var loc = http_request.query.callback[0] + "&loginfail=true";
			} else {
				var loc = http_request.query.callback[0] + "?loginfail=true";
			}
		} else {
			var loc = http_request.query.callback[0];
		}
		print("<html><head><script type=text/javascript>window.location='" + loc + "'</script></head></html>");
	}
	return webIni;
})();
