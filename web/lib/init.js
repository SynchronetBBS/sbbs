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

if (typeof settings.xtrn_sections === 'string') {
	settings.xtrn_sections = settings.xtrn_sections.split(',').filter(
		function (section) {
			if (typeof xtrn_area.sec[section] === 'undefined') return false;
			if (!xtrn_area.sec[section].can_access) return false;
			if (xtrn_area.sec_list[
					xtrn_area.sec[section].index
				].prog_list.length < 1
			) {
				return false;
			}
			return true;	
		}
	);
}

if (typeof settings.max_messages !== 'number' || settings.max_messages < 0) {
	settings.max_messages = 0;
} 