load('sbbsdefs.js');

function randomString(length) {
	var chars = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXTZabcdefghiklmnopqrstuvwxyz'.split("");
	var str = '';
	for (var i = 0; i < length; i++) {
		str += chars[Math.floor(Math.random() * chars.length)];
	}
	return str;
}

function getSession(un) {
	var fn = format('%suser/%04d.web', system.data_dir, un);
	if (!file_exists(fn)) return false;
	var f = new File(fn);
	if (!f.open('r')) return false;
	var session = f.iniGetObject();
	f.close();
	return session;
}

function getSessionValue(un, key) {
	var session = getSession(un);
	if (!session || typeof session[key] === 'undefined') return null;
	return session[key];
}

function setSessionValue(un, key, value) {
	var fn = format('%suser/%04d.web', system.data_dir, un);
	var f = new File(fn);
	f.open(f.exists ? 'r+' : 'w+');
	f.iniSetValue(null, key, value);
	f.close();
}

function setCookie(usr, sessionKey) {
	if (usr instanceof User && usr.number > 0) {
		set_cookie(
			'synchronet',
			usr.number + ',' + sessionKey,
			(time() + settings.timeout),
			http_request.host.replace(/\:\d*/g, ''),
			'/'
		);
		setSessionValue(usr.number, 'key', sessionKey);
	}
}

function validateSession(cookies) {

	for (var c in cookies) {

		if (cookies[c].search(/^\d+,\w+$/) < 0) continue;

		var cookie = cookies[c].split(',');

		try {
			var usr = new User(cookie[0]);
			if (usr.number < 1) {
				throw 'Invalid user number ' + cookie[0] + ' in cookie.';
			}
		} catch (err) {
			log(LOG_DEBUG, err);
			continue;
		}

		var session = getSession(usr.number);
		if (typeof session !== 'object') continue;
		if (typeof session.key != 'string' || session.key != cookie[1]) {
			continue;
		}

		authenticate(usr.alias, usr.security.password);
		setCookie(usr, session.key);
		setSessionValue(usr.number, 'ip_address', client.ip_address);
		break;

	}

}

function destroySession(cookies) {

	for (var c in cookies) {

		if (cookies[c].search(/^\d+,\w+$/) < 0)	continue;

		var cookie = cookies[c].split(',');

		try {

			var usr = new User(cookie[0]);
			if(usr.number < 1) {
				throw 'Invalid user number ' + cookie[0] + ' in cookie.';
			}

			var session = getSession(usr.number);
			if (typeof session !== 'object') {
				throw 'Invalid session for user #' + usr.number;
			}

			if (session.key !== cookie[1]) {
				throw 'Invalid session key for user #' + user.number;
			}

			set_cookie(
				'synchronet',
				usr.number + ',' + session.key,
				(time() - settings.timeout),
				http_request.host.replace(/\:\d*/g, ''),
				'/'
			);

			var fn = format('%suser/%04d.web', system.data_dir, usr.number);
			file_remove(fn);

			break;

		} catch (err) {
			log(LOG_DEBUG,
				'Error destroying session: ' + err + ', cookie: ' + cookies[c]
			);
		}

	}

}

function authenticate(alias, password) {
	var un = system.matchuser(alias);
	if (un < 1) return false;
	var usr = new User(un);
	if (usr.settings&USER_DELETED) return false;
	if (usr.security.password.toUpperCase() !== password.toUpperCase()) {
		return false;
	}
	login(usr.alias, usr.security.password);
	return usr;
}

// If someone is trying to log in
if (typeof http_request.query.username !== 'undefined' &&
	http_request.query.username[0].length <= LEN_ALIAS &&
	typeof http_request.query.password != 'undefined' &&
	http_request.query.password[0].length <= LEN_PASS
) {
	var usr = authenticate(
		http_request.query.username[0],
		http_request.query.password[0]
	);
	if (usr instanceof User) setCookie(usr, randomString(512));
// If they have a cookie
} else if(
	typeof http_request.cookie.synchronet !== 'undefined' &&
	http_request.cookie.synchronet.some(
		function (e) {
		 	return(e.search(/^\d+,\w+$/) != -1);
		}
	)
) {
	// Verify & update their session, or log them out if requested
	if (typeof http_request.query.logout === 'undefined') {
		validateSession(http_request.cookie.synchronet);
	} else {
		destroySession(http_request.cookie.synchronet);
	}
}

// If they haven't authenticated as an actual user yet
if (user.number === 0) {

	// Try to log them in as the guest user
	var gn = system.matchuser(settings.guest);
	if (gn > 0) {
		var gu = new User(gn);
		login(gu.alias, gu.security.password);
	} else {
		// Otherwise just kill the script, for security's sake
		exit();
	}
}
