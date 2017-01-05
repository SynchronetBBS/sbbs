load('modopts.js');

var settings = get_mod_options('web');

// Paths
settings.web_directory = fullpath(
	backslash(
		typeof settings.web_directory === 'undefined'
		? '../web'
		: settings.web_directory
	)
);
settings.web_root = backslash(settings.web_directory + 'root/');
settings.web_lib = backslash(settings.web_directory + 'lib/');

// Guest
if (typeof settings.guest === 'undefined') settings.guest = 'Guest';
if (system.matchuser(settings.guest) == 0) exit();

// Timeout
if (typeof settings.timeout !== 'number') settings.timeout = 43200;

// Registration
if (typeof settings.user_registration !== 'boolean') {
	settings.user_registration = false;
} else {

	if (typeof settings.minimum_password_length !== 'number') {
		settings.minimum_password_length = 4;
	}

	if (typeof settings.email_validation !== 'boolean') {
		settings.email_validation = true;
	}

	if (typeof settings.email_validation_level !== 'number') {
		settings.email_validation_level = 50;
	}

}

if (typeof settings.max_messages !== 'number' || settings.max_messages < 0) {
	settings.max_messages = 0;
}

if (typeof settings.page_size !== 'number' || settings.page_size < 1) {
	settings.page_size = 25;
}

if (typeof settings.forum_extended_ascii !== 'boolean') {
	settings.forum_extended_ascii = true;
}