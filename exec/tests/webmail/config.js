// Config: web client settings (webmail/lib/config.js)

load(js.exec_dir + "kit.js");
var Config = Kit.wlib("config");

var warnings = [];
Config.warn = function (text) {
	warnings.push(text);
};

var DEFAULTS = {
	session_timeout: 7200,
	remember_days: 30,
	poll_interval: 60,
	max_attachment_bytes: 26214400,
	max_text_part_bytes: 1048576,
	max_drafts: 50,
	max_draft_bytes: 104857600,
	access_requirements: "",
	webv4_login: false,
	require_tls: true,
	tls_proxy_header: "",
	hsts_max_age: 0,
	locale: "en_us",
	product_name: "{system} Webmail",
	brand: "",
	login_text: "",
	footer_links: "",
	fallback_url: ""
};

// Nothing configured: modopts returns null or undefined
[undefined, null, {}].forEach(function (raw) {
	var c = Config.load(raw);
	for (var k in DEFAULTS) {
		Kit.assert(typeof c[k] === typeof DEFAULTS[k], k + " type " + typeof c[k]);
		Kit.assert(c[k] === DEFAULTS[k], k + " default " + JSON.stringify(c[k]));
	}
	Kit.eq(c.directory, system.exec_dir + "../webmail/", "directory default");
	var n = 0;
	for (var kk in c)
		n++;
	Kit.eq(n, 19, "key count");
});

// No argument: reads this install's real options; the stock file changes
// nothing, so a stock install yields the defaults
(function () {
	var c = Config.load();
	Kit.eq(typeof c.require_tls, "boolean");
	Kit.eq(typeof c.session_timeout, "number");
})();

// Booleans, from text or from a value modopts already converted
(function () {
	var yes = ["true", "yes", "1", "TRUE", "Yes", " on ", true, 1];
	var no = ["false", "no", "0", "FALSE", "No", "off", false, 0];
	yes.forEach(function (v) {
		Kit.assert(Config.load({ require_tls: v }).require_tls === true, "yes: " + JSON.stringify(v));
	});
	no.forEach(function (v) {
		Kit.assert(Config.load({ require_tls: v }).require_tls === false, "no: " + JSON.stringify(v));
	});
	// Not a boolean: the default (true)
	["maybe", "", "2", {}].forEach(function (v) {
		Kit.assert(Config.load({ require_tls: v }).require_tls === true, "bad bool: " + JSON.stringify(v));
	});
})();

// Numbers, from text or numeric
(function () {
	var c = Config.load({ session_timeout: "900", remember_days: 7, poll_interval: "15",
		hsts_max_age: "31536000", max_drafts: " 12 " });
	Kit.assert(c.session_timeout === 900, "text number");
	Kit.assert(c.remember_days === 7, "number");
	Kit.assert(c.poll_interval === 15, "minimum is valid");
	Kit.assert(c.hsts_max_age === 31536000, "large");
	Kit.assert(c.max_drafts === 12, "padded");
	// Negative or non-numeric: the default
	["-5", -5, "abc", "", "12x", "1.5", 1.5, "NaN", NaN, Infinity, true, {}].forEach(function (v) {
		var d = Config.load({ session_timeout: v });
		Kit.assert(d.session_timeout === 7200, "bad number " + JSON.stringify(v) + " gave " + d.session_timeout);
	});
})();

// Strings pass through; non-strings become the default
(function () {
	var c = Config.load({ product_name: "Our Mail", locale: "de", brand: "<b>x</b>",
		login_text: "Hi", footer_links: "<a>f</a>", fallback_url: "https://e.example/",
		tls_proxy_header: "X-Forwarded-Proto", access_requirements: "LEVEL 20" });
	Kit.eq(c.product_name, "Our Mail");
	Kit.eq(c.locale, "de");
	Kit.eq(c.brand, "<b>x</b>");
	Kit.eq(c.login_text, "Hi");
	Kit.eq(c.footer_links, "<a>f</a>");
	Kit.eq(c.fallback_url, "https://e.example/");
	Kit.eq(c.tls_proxy_header, "X-Forwarded-Proto");
	Kit.eq(c.access_requirements, "LEVEL 20");
	Kit.eq(Config.load({ product_name: {} }).product_name, "{system} Webmail");
	// The raw setting is kept: the placeholders are filled per request
	Kit.eq(Config.load({ product_name: "{host} Webmail" }).product_name, "{host} Webmail");
})();

// Unknown keys are ignored
(function () {
	var c = Config.load({ nonsense: "1", session_timeout: "500" });
	Kit.assert(c.nonsense === undefined, "unknown key kept");
	Kit.eq(c.session_timeout, 500);
})();

// directory: normalized to end with a separator
(function () {
	Kit.eq(Config.load({ directory: "/opt/wm" }).directory, "/opt/wm/");
	Kit.eq(Config.load({ directory: "/opt/wm/" }).directory, "/opt/wm/");
	Kit.eq(Config.load({ directory: "C:\\wm\\" }).directory, "C:\\wm\\");
	Kit.eq(Config.load({ directory: "C:\\wm" }).directory, "C:\\wm/");
	Kit.eq(Config.load({ directory: "" }).directory, system.exec_dir + "../webmail/");
})();

// Directory: relative values resolve against system.exec_dir
(function () {
	var dflt = system.exec_dir + "../webmail/";
	Kit.eq(Config.load({ directory: "../webmail/" }).directory, dflt);
	Kit.eq(Config.load({ directory: " ../webmail \t" }).directory, dflt);
	Kit.eq(Config.load({ directory: "sub" }).directory, system.exec_dir + "sub/");
	Kit.eq(Config.load({ directory: "/opt/wm" }).directory, "/opt/wm/");
	Kit.eq(Config.load({ directory: "\\\\host\\wm" }).directory, "\\\\host\\wm/");
	Kit.eq(Config.load({ directory: "C:\\wm" }).directory, "C:\\wm/");
	Kit.eq(Config.load({ directory: "c:wm" }).directory, "c:wm/");
})();

// fallback_url: an http or https URL, or a relative path; anything else
// (another scheme, a scheme-relative URL, white space or control
// characters) warns and is empty
(function () {
	var ok = { "https://e.example/": "https://e.example/", "HTTP://e.example/x?y=1": "HTTP://e.example/x?y=1",
		"/old/": "/old/", "old.html": "old.html", "../basic/index.html": "../basic/index.html",
		" https://e.example/ ": "https://e.example/" };
	for (var k in ok) {
		warnings = [];
		Kit.eq(Config.load({ fallback_url: k }).fallback_url, ok[k], "accepted " + JSON.stringify(k));
		Kit.eq(warnings.length, 0, "no warning for " + JSON.stringify(k));
	}
	var bad = ["javascript:alert(1)", "JavaScript:x", " javascript:x", "data:text/html,x", "vbscript:x",
		"ftp://e.example/", "//evil.example/", "\\\\evil\\x", "/\\evil.example/", "\\/evil.example/",
		"https://", "https://e.example/a b",
		"/a\tb", "/a\x01b", "mailto:a@b.example", "https:/e.example"];
	for (var i = 0; i < bad.length; i++) {
		warnings = [];
		Kit.eq(Config.load({ fallback_url: bad[i] }).fallback_url, "", "refused " + JSON.stringify(bad[i]));
		Kit.eq(warnings.length, 1, "one warning for " + JSON.stringify(bad[i]));
		Kit.assert(warnings[0].indexOf("fallback_url") >= 0, "warning names the key");
	}
	warnings = [];
	Kit.eq(Config.load({ fallback_url: "" }).fallback_url, "", "empty");
	Kit.eq(warnings.length, 0, "empty: no warning");
})();

// Strings given as numbers or booleans by modopts keep their text
(function () {
	var c = Config.load({ locale: 1, product_name: 2024, access_requirements: 60, brand: true });
	Kit.assert(c.locale === "1", "locale");
	Kit.assert(c.product_name === "2024", "product_name");
	Kit.assert(c.access_requirements === "60", "access_requirements");
	Kit.assert(c.brand === "true", "brand");
})();

// Lower bounds
(function () {
	Kit.eq(Config.load({ poll_interval: "0" }).poll_interval, 15);
	Kit.eq(Config.load({ poll_interval: 14 }).poll_interval, 15);
	Kit.eq(Config.load({ poll_interval: 15 }).poll_interval, 15);
	Kit.eq(Config.load({ session_timeout: "59" }).session_timeout, 60);
	Kit.eq(Config.load({ session_timeout: 60 }).session_timeout, 60);
	Kit.eq(Config.load({ remember_days: 0 }).remember_days, 1);
	Kit.eq(Config.load({ remember_days: "1" }).remember_days, 1);
	warnings.length = 0;
	Config.load({ remember_days: 0 });
	Kit.eq(warnings.length, 1, "remember_days below minimum warns");
	Kit.assert(warnings[0].indexOf("remember_days") >= 0, "names remember_days");
})();

// webv4_login
(function () {
	Kit.assert(Config.load({}).webv4_login === false);
	Kit.assert(Config.load({ webv4_login: "yes" }).webv4_login === true);
	Kit.assert(Config.load({ webv4_login: true }).webv4_login === true);
	Kit.assert(Config.load({ webv4_login: "banana" }).webv4_login === false);
})();

// Warnings: one per unusable present value, naming the key; none for an
// absent or empty one
(function () {
	warnings.length = 0;
	Config.load({});
	Config.load({ hsts_max_age: "", require_tls: "", locale: "" });
	Kit.eq(warnings.length, 0, "no warning when absent or empty");
	Config.load({ hsts_max_age: "1y" });
	Kit.eq(warnings.length, 1);
	Kit.assert(warnings[0].indexOf("hsts_max_age") >= 0, "names key");
	warnings.length = 0;
	Config.load({ require_tls: "maybe", session_timeout: 5, max_drafts: "-1", locale: {} });
	Kit.eq(warnings.length, 4);
	Kit.assert(warnings[0].indexOf("max_drafts") >= 0 || warnings.join(" ").indexOf("max_drafts") >= 0);
	warnings.length = 0;
	Config.load({ poll_interval: 60, require_tls: "no", locale: 5 });
	Kit.eq(warnings.length, 0, "valid values are silent");
})();
