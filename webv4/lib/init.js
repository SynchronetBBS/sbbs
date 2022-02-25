require('sbbsdefs.js', 'SYS_CLOSED');
load('array.js');

(function () {
	// Paths
	settings.web_directory = fullpath(backslash(settings.web_directory === undefined ? '../webv4' : settings.web_directory));
	settings.web_root = fullpath(backslash(settings.web_root === undefined ? settings.web_directory + 'root' : settings.web_root));
	settings.web_lib = backslash(settings.web_directory + 'lib/');
	settings.web_components = backslash(settings.web_directory + 'components/');
	settings.web_pages = backslash(fullpath(settings.web_root + '../pages'));
	settings.web_sidebar = backslash(fullpath(settings.web_root + '../sidebar'));
	settings.web_mods = backslash(fullpath(settings.web_directory + 'mods/'));
	settings.web_mods_pages = backslash(fullpath(settings.web_mods + 'pages/'));
	settings.web_mods_sidebar = backslash(fullpath(settings.web_mods + 'sidebar/'));

	var defaults = {
		guest: {
			default: 'Guest',
			test: function (v) {
				return system.matchuser(v) ? null : (new Error('Guest account unavailable'));
			}
		},
		timeout: { default: 43200 },
		user_registration: { default: false },
		minimum_password_length: { default: 4 },
		email_validation: { default: true },
		email_validation_level: { default: 50 },
		max_messages: {
			default: 0,
			test: function (v) {
				return v >= 0 ? null : (new Error('max_messages must be >= 0'));
			}
		},
		page_size: {
			default: 25,
			test: function (v) {
				return v >= 1 ? null : (new Error('page_size must be >= 1'));
			}
		},
		active_node_list: { default: true },
		hide_empty_stats: { default: true },
		files_inline: { default: false },
		files_inline_blacklist: {
			default: [ "htm", "html" ],
			parse: function (v) {
				if (typeof v !== 'string') return new Error('Invalid files_inline_blacklist setting ' + v);
				return v.split(',');
			}
		},
		darkmode_allow: { default: true },
		darkmode_on: { default: false },
		ftelnet: { default: true },
		vote_functions: { default: true },
		xtrn_blacklist: { default: 'scfg' },
		forum_default_message_view: { default: 'normal' },
		forum_auto_ansi_subject: { default: [ '^\\[ansi\\]' ] },
		forum_auto_detect_ansi: { default: true },
		forum_auto_ansi_mode: { default: 'html' },
		favicon: { default: './images/favicon.ico' },
	};

	if (settings.darkmode_off) settings.darkmode_allow = false;

	function doError(e) {
		log(LOG_ERROR, e);
		http_reply.status = '500 Internal Server Error';
		writeln('500 Internal Server Error');
		exit();
	}

	Object.keys(defaults).forEach(function (e) {

		if (settings[e] === undefined) {
			settings[e] = defaults[e].default;
		} else if (typeof defaults[e].parse === 'function') {
			const v = defaults[e].parse(settings[e]);
			if (v instanceof Error) doError(v);
			settings[e] = v;
		}
		
		if (typeof settings[e] !== typeof defaults[e].default) {
			doError(new Error('Invalid ' + e + ' setting: ' + settings[e]));
		}
		
		if (typeof defaults[e].test === 'function') {
			const t = defaults[e].test(settings[e]);
			if (t instanceof Error) doError(t);
		}

	});

	defaults = undefined;
})();

function webRequire(filename, propname, scope, args) {
	if (file_isdir(settings.web_mods + 'lib/')) {
		if (file_exists(settings.web_mods + 'lib/' + filename)) {
			return require(scope || {}, settings.web_mods + 'lib/' + filename, propname, args);
		}
	}
	return require(scope || {}, settings.web_lib + filename, propname, args);
}

var locale = webRequire('locale.js', 'locale');
