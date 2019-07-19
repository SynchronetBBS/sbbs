load('sbbsdefs.js');
load('modopts.js');

if (!settings) var settings = get_mod_options('web');

// Paths
settings.web_directory = fullpath(
	backslash(
		typeof settings.web_directory === 'undefined'
		? '../web'
		: settings.web_directory
	)
);
settings.web_root = fullpath(
	backslash(
		typeof settings.web_root === 'undefined'
		? settings.web_directory + 'root'
		: settings.web_root
	)
);
settings.web_lib = backslash(settings.web_directory + 'lib/');

settings.web_pages = backslash(fullpath(settings.web_root + '../pages'));
settings.web_sidebar = backslash(fullpath(settings.web_root + '../sidebar'));

var defaults = {
  guest : {
    default : 'Guest',
    test : function () {
      return system.matchuser(settings.guest) ? null : 'Guest account unavailable';
    }
  },
  timeout : { default : 43200 },
  user_registration : { default : false },
  minimum_password_length : { default : 4 },
  email_validation : { default : true },
  email_validation_level : { default : 50 },
  max_messages : {
    default : 0,
    test : function () {
      return settings.max_messages >= 0 ? null : 'max_messages must be >= 0';
    }
  },
  page_size : {
    default : 25,
    test : function () {
      return settings.page_size >= 1 ? null : 'page_size must be >= 1';
    }
  },
  forum_extended_ascii : { default : true },
  active_node_list : { default : true },
  hide_empty_stats : { default : true }
};

Object.keys(defaults).forEach(function (e) {
  if (typeof settings[e] == 'undefined') {
    settings[e] = defaults[e].default;
  } else if (typeof settings[e] != typeof defaults[e].default) {
    log(LOG_ERROR, 'Invalid ' + e + ' setting: ' + settings[e]);
    exit();
  } else if (typeof defaults[e].test == 'function') {
    const t = defaults[e].test();
    if (t !== null) {
      log(LOG_ERROR, t);
      exit();
    }
  }
});

defaults = undefined;

load(settings.web_lib + 'locale.js');

if (typeof alert === 'undefined') {
    function alert(msg) {
        log(LOG_ERR, msg);
    }
}

// Helpers for http_request
const Req = {
    // Query parameter p exists and first instance is of optional type t
    has_param: function (p, t) {
        if (!t) return typeof http_request.query[p] != 'undefined';
        return typeof http_request.query[p] != 'undefined' && typeof http_request.query[p] == t;
    },
    // First instance of query parameter p, or undefined
    get_param: function (p) {
        if (Array.isArray(http_request.query[p])) {
            return http_request.query[p][0];
        }
    }
};
