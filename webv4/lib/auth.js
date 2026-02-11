require('sbbsdefs.js', 'SYS_CLOSED');
var request = require({}, settings.web_lib + 'request.js', 'request');

function randomString(length) {
	var chars = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXTZabcdefghiklmnopqrstuvwxyz'.split('');
	var str = '';
	for (var i = 0; i < length; i++) {
		var rn = Math.floor(Math.random() * chars.length);
		if (rn >= chars.length) log(LOG_DEBUG, "Impossible number: " + rn);
		str += chars[rn];
	}
	return str;
}

function getCsrfToken() {
	if (is_user()) {
		return getSessionValue(user.number, 'csrf_token');
	} else {
		return undefined;
	}
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

function validateCsrfToken() {
	try {
		// Check for CSRF token (in header or query data)
		var input_token = null;
		if (http_request.header['x-csrf-token']) {
			input_token = http_request.header['x-csrf-token'];
		} else if (request.has_param('csrf_token')) {
			input_token = request.get_param('csrf_token');
		}

		// If we didn't find an input token, then validation fails
		if (!input_token) {
			return false;
		}

		// If we did find an input token, confirm it matches the token stored in the user's session
		return input_token === getCsrfToken();
	} catch (error) {
		// In case of error, return false to avoid allowing CSRF when one shouldn't be allowed
		log(LOG_ERR, 'auth.js validateCsrfToken error: ' + error);
		return false;
	}
}


function validateSession(cookies) {

	var usr = new User(0);
	for (var c in cookies) {

		if (cookies[c].search(/^\d+,\w+$/) < 0) continue;

		var cookie = cookies[c].split(',');

		try {
			usr.number = cookie[0];
			if (usr.number < 1) throw new Error('Invalid user number ' + cookie[0] + ' in cookie.');
		} catch (err) {
			log(LOG_DEBUG, err);
			continue;
		}

		var session = getSession(usr.number);
		if (typeof session !== 'object') continue;
		if (typeof session.key != 'string' || session.key != cookie[1]) continue;

		var _usr = authenticate(usr.alias, usr.security.password, false);
		_usr = undefined;
		setCookie(usr, session.key);
		setSessionValue(usr.number, 'ip_address', client.ip_address);
		if (session.session_start === undefined || time() - parseInt(session.session_start, 10) > settings.timeout) {
			setSessionValue(usr.number, 'session_start', time());
			
			// Generate a csrf token.  Minimum recommended is 128 bits of entropy, and 43 characters of 0-9A-Za-z should equal 256 bits of entropy
			// according to this formula: log2(62^43) -- https://www.wolframalpha.com/input?i=log2%2862%5E43%29
			// (62 refers to the fact that there are 62 characters to choose from in the 0-9A-Za-z set)
			setSessionValue(usr.number, 'csrf_token', randomString(43))
			
			if(!usr.is_sysop || (system.settings&SYS_SYSSTAT)) {
				load({}, 'logonlist_lib.js').add({ node: 'Web' });
			}
		}
		break;

	}
	usr = undefined;

}

function destroySession(cookies) {

  var usr = new User(0);
	for (var c in cookies) {

		if (cookies[c].search(/^\d+,\w+$/) < 0)	continue;

		var cookie = cookies[c].split(',');

		try {

			usr.number = cookie[0];
			if(usr.number < 1) {
				throw new Error('Invalid user number ' + cookie[0] + ' in cookie.');
			}

			var session = getSession(usr.number);
			if (typeof session !== 'object') {
				throw new Error('Invalid session for user #' + usr.number);
			}

			if (session.key !== cookie[1]) {
				throw new Error('Invalid session key for user #' + user.number);
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
  usr = undefined;

}

function authenticate(alias, password, inc_logons) {
	var un = system.matchuser(alias);
	if (un < 1) return false;
	var usr = new User(un);
	if (usr.settings&USER_DELETED) return false;
	if (!login(usr.alias, password, inc_logons)) return false;
	return usr;
}

function is_user() {
    return user.number > 0 && user.alias != settings.guest;
}

(function () {
    // If someone is trying to log in
    if (http_request.query.username !== undefined &&
    	http_request.query.username[0].length <= LEN_ALIAS &&
    	http_request.query.password !== undefined &&
    	http_request.query.password[0].length <= LEN_PASS
    ) {
    	var usr = authenticate(
    		http_request.query.username[0],
			http_request.query.password[0],
			true
    	);
		if (usr instanceof User) {
			destroySession(http_request.cookie.synchronet || {});
			setCookie(usr, randomString(512));
		}
		usr = undefined;
    // If they have a cookie
    } else if (
		http_request.cookie.synchronet !== undefined &&
		http_request.cookie.synchronet.some(function (e) {
			return(e.search(/^\d+,\w+$/) != -1);
		})
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
    		if (!login(gu.alias, gu.security.password))
			exit();
        gu = undefined;
    	} else {
    		// Otherwise just kill the script, for security's sake
    		exit();
    	}
    }
})();
