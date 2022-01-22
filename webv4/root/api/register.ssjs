require('sbbsdefs.js', 'SYS_CLOSED');
var settings = load('modopts.js', 'web') || { web_directory: '../webv4' };
load(settings.web_directory + '/lib/init.js');
load(settings.web_lib + 'auth.js');
var request = require({}, settings.web_lib + 'request.js', 'request');

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
	gender : ' ',
	password : ''
};

function required(mask) {
	return (system.new_user_questions&mask);
}

function clean_param(param) {
	if (request.hasParam(param)) return request.getParam(param).replace(/[\x00-\x19\x7F]/g, '');
	return "";
}

function in_range(n, min, max) {
	return n >= min && n <= max;
}

function valid_param(p, min, max) {
	if (!request.hasParam(p)) return false;
	if (!in_range(clean_param(p).length, min, max)) return false;
	return true;
}

function is_dupe(field, str) {
	return system.matchuserdata(field, str) !== 0;
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
	if (typeof settings.newuser_level == 'number' && settings.newuser_level >= 0 && settings.newuser_level <= 99) {
		usr.security.level = settings.newuser_level;
	}
	['flags1', 'flags2', 'flags3', 'flags4', 'exemptions', 'restrictions'].forEach(function (e) {
		const k = 'newuser_' + e;
		if (settings[k] && settings[k].search(/[^a-zA-Z]/) < 0) {
			usr.security[e] = '+' + settings[k];
		}
	});
	reply.userNumber = usr.number;
}

// See if the hidden form fields were filled
if (request.getParam('send-me-free-stuff') != '' || request.getParam('subscribe-to-newsletter') !== undefined) {
	log(LOG_WARNING, locale.strings.api_register.log_bot_attempt);
	exit();
}

if (system.newuser_password !== '' && (!request.hasParam('newuser-password') || request.getParam('newuser-password') != system.newuser_password)) {
	reply.errors.push(locale.strings.api_register.error_bad_syspass);
}

 if (system.matchuser(clean_param('alias')) > 0) {
	reply.errors.push(locale.strings.api_register.error_alias_taken);
} else  if (!valid_param('alias', MIN_ALIAS, LEN_ALIAS) || !system.check_name(clean_param('alias'))) {
	reply.errors.push(locale.strings.api_register.error_invalid_alias);
} else {
	prepUser.alias = clean_param('alias');
	prepUser.handle = clean_param('alias');
}

if (!request.hasParam('password1') || !request.hasParam('password2') || clean_param('password1') != clean_param('password2')) {
	reply.errors.push(locale.strings.api_register.error_password_mismatch);
} else if (!in_range(clean_param('password1').length, system.min_password_length, system.max_password_length)) {
	reply.errors.push(format(locale.strings.api_register.error_password_length, system.min_password_length, system.max_password_length));
} else {
	prepUser.password = clean_param('password1');
}

if (valid_param('netmail', MIN_NETMAIL, LEN_NETMAIL)) {
	prepUser.netmail = clean_param('netmail');
} else if (!required(UQ_NONETMAIL)) {
	reply.errors.push(locale.strings.api_register.error_email_required);
}

if (valid_param('realname', MIN_REALNAME, LEN_NAME) && (!required(UQ_DUPREAL) || !is_dupe(U_NAME, clean_param('realname')))) {
	prepUser.name = clean_param('realname');
} else if (required(UQ_REALNAME)) {
	reply.errors.push(locale.strings.api_register.error_invalid_name);
}

// UQ_NOCOMMAS should be checked and acted on
if (valid_param('location', MIN_LOCATION, LEN_LOCATION)) {
	prepUser.location = clean_param('location');
} else if (required(UQ_LOCATION)) {
	reply.errors.push(locale.strings.api_register.error_invalid_location);
}

if (valid_param('address', MIN_ADDRESS, LEN_ADDRESS) && valid_param('zipcode', 3, LEN_ADDRESS)) {
	prepUser.address = clean_param('address');
	prepUser.zipcode = clean_param('zipcode');
} else if (required(UQ_ADDRESS)) {
	reply.errors.push(locale.strings.api_register.error_invalid_street_address);
}

// Validate?  Who cares?
if (valid_param('phone', MIN_PHONE, LEN_PHONE)) {
	prepUser.phone = clean_param('phone');
} else if (required(UQ_PHONE)) {
	reply.errors.push(locale.strings.api_register.error_invalid_phone);
}

if (valid_param('gender', 1, 1) && ['X', 'M', 'F', 'O'].indexOf(request.getParam('gender')) > -1) {
	prepUser.gender = clean_param('gender');
} else if (required(UQ_SEX)) {
	reply.errors.push(locale.strings.api_register.error_invalid_gender);
}

if (request.hasParam('birth') && clean_param('birth').match(/^\d\d\/\d\d\/\d\d$/) !== null) {
	// Should really test for valid date (and date format per system config)
	prepUser.birthdate = clean_param('birth');
} else if (required(UQ_BIRTH)) {
	reply.errors.push(locale.strings.api_register.error_invalid_birthdate);
}

if (reply.errors.length < 1) newUser();

reply = JSON.stringify(reply);
http_reply.header['Content-Type'] = 'application/json';
http_reply.header['Content-Length'] = reply.length;
write(reply);

prepUser = undefined;
reply = undefined;
