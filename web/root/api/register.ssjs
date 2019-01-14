load('sbbsdefs.js');
load(system.exec_dir + '../web/lib/init.js');
load(settings.web_lib + '/auth.js');

if (user.alias !== settings.guest) exit();
if (!settings.user_registration) exit();

var MIN_ALIAS = 1,
	MIN_REALNAME = 3,
	MIN_NETMAIL = 6,
	MIN_LOCATION = 4,
	MIN_ADDRESS = 6,
	MIN_PHONE = 3;

var reply = {
	errors : [],
	userNumber : 0
};

var prepUser = {
	alias : '',
	handle : '',
	name : '',
	netmail : '',
	address : '',
	zipcode : '',
	location : '',
	phone : '',
	birthdate : '',
	gender : '',
	password : ''
};

function required(mask) {
	return (system.new_user_questions&mask);
}

function cleanParam(param) {
	if (paramExists(param)) {
		return http_request.query[param][0].replace(/[\x00-\x19\x7F]/g, '');
	}
	return "";
}

function paramExists(param) {
	if (typeof http_request.query[param] !== 'undefined' &&
		http_request.query[param][0] !== ''
	) {
		return true;
	}
	return false;
}

function paramLength(param) {
	if (typeof http_request.query[param] === 'undefined') {
		return 0;
	} else if (http_request.query[param][0].replace(' ', '').length < 1) {
		return 0;
	} else if (cleanParam(param).length < 1) {
		return 0;
	} else {
		return http_request.query[param][0].length;
	}
}

function newUser() {
	var usr = system.new_user(prepUser.alias);
	if (typeof usr === 'number') {
		reply.errors.push(locale.strings.api_register.error_failed);
		return;
	}
	log(LOG_INFO, format(locale.strings.api_register.log_success, usr.number));
	usr.security.password = prepUser.password;
	for (var property in prepUser) {
		if (property === 'alias' || property === 'password') continue;
		usr[property] = prepUser[property];
	}
	reply.userNumber = usr.number;
}

// See if the hidden form fields were filled
if ((	paramExists('send-me-free-stuff') &&
		http_request.query['send-me-free-stuff'][0] !== ''
	) ||
	(	paramExists('subscribe-to-newsletter') &&
		http_request.query['subscribe-to-newsletter'][0] !== ''
	)
) {
	log(LOG_WARNING, locale.strings.api_register.log_bot_attempt);
	exit();
}

if (system.newuser_password !== '' &&
	(	typeof http_request.query['newuser-password'] === 'undefined' ||
		http_request.query['newuser-password'][0] != system.newuser_password
	)
) {
	reply.errors.push(locale.strings.api_register.error_bad_syspass);
}

// More could be done to respect certain newuser question toggles
// (UQ_DUPREAL, UQ_NOUPPRLWR, UQ_NOCOMMAS), but I don't care right now.

if (!paramExists('alias') ||
	paramLength('alias') < MIN_ALIAS ||
	paramLength('alias') > LEN_ALIAS
) {
	reply.errors.push(locale.strings.api_register.error_invalid_alias);
} else if (system.matchuser(http_request.query.alias[0]) > 0) {
	reply.errors.push(locale.strings.api_register.error_alias_taken);
} else {
	prepUser.alias = cleanParam('alias');
	prepUser.handle = cleanParam('alias');
}

if ((!paramExists('password1') || !paramExists('password2')) ||
	http_request.query.password1[0] !== http_request.query.password2[0]
) {
	reply.errors.push(locale.strings.api_register.error_password_mismatch);
} else if (
	paramLength('password1') < settings.minimum_password_length ||
	paramLength('password1') > LEN_PASS
) {
	reply.errors.push(
		format(
			locale.strings.api_register.error_password_length,
			settings.minimum_password_length, LEN_PASS
		)
	);
} else {
	prepUser.password = cleanParam('password1');
}

if (!paramExists('netmail') && !required(UQ_NONETMAIL)) {
	reply.errors.push(locale.strings.api_register.error_email_required);
} else if (
	(	paramLength('netmail') < MIN_NETMAIL ||
		paramLength('netmail') > LEN_NETMAIL
	) && !required(UQ_NONETMAIL)
) {
	reply.errors.push(locale.strings.api_register.error_invalid_email);
} else {
	prepUser.netmail = cleanParam('netmail');
}

if (required(UQ_REALNAME) &&
	(	!paramExists('realname') ||
		paramLength('realname') < MIN_REALNAME ||
		paramLength('realname') > LEN_NAME
	)
) {
	reply.errors.push(locale.strings.api_register.error_invalid_name);
} else {
	prepUser.name = cleanParam('realname');
}

if (required(UQ_LOCATION) &&
	(	!paramExists('location') ||
		paramLength('location') < MIN_LOCATION ||
		paramLength('location') > LEN_LOCATION
	)
) {
	reply.errors.push(locale.strings.api_register.error_invalid_location);
} else {
	prepUser.location = cleanParam('location');
}

if (required(UQ_ADDRESS) &&
	(	!paramExists('address') ||
		paramLength('address') < MIN_ADDRESS ||
		paramLength('address') > LEN_ADDRESS ||
		!paramExists('zipcode') ||
		paramLength('zipcode') < 3 ||
		paramLength('zipcode') > LEN_ADDRESS
	)
) {
	reply.errors.push(locale.strings.api_register.error_invalid_street_address);
} else {
	prepUser.address = cleanParam('address');
	prepUser.zipcode = cleanParam('zipcode');
}

if (required(UQ_PHONE) &&
	(	!paramExists('phone') ||
		paramLength('phone') < MIN_PHONE ||
		paramLength('phone') > LEN_PHONE
	)
) {
	reply.errors.push(locale.strings.api_register.error_invalid_phone);
} else {
	prepUser.phone = cleanParam('phone');
}

if (required(UQ_SEX) &&
	(	!paramExists('gender') ||
		paramLength('gender') != 1 ||
		['X','M','F','O'].indexOf(http_request.query.gender[0]) < 0
	)
) {
	reply.errors.push(locale.strings.api_register.error_invalid_gender);
} else {
	prepUser.gender = http_request.query.gender[0];
}

if (paramExists('birth') &&
	http_request.query.birth[0].match(/^\d\d\/\d\d\/\d\d$/) !== null
) {
	// Should really test for valid date (and date format per system config)
	prepUser.birthdate = cleanParam('birth');
} else if (required(UQ_BIRTH)) {
	reply.errors.push(locale.strings.api_register.error_invalid_birthdate);
}

if (reply.errors.length < 1) newUser();

reply = JSON.stringify(reply);
http_reply.header['Content-Type'] = 'application/json';
http_reply.header['Content-Length'] = reply.length;
write(reply);
