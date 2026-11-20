// Every string key the client uses exists in the English locale
// (en_us.ini): each "section.key" string literal, in any quotes, in
// webmail/root/js/*.js and webmail/root/index.xjs whose section is one of
// the locale's sections (so method names such as "session.get" and file
// names are not taken for keys), and that is not one of the API's method
// names (settings.get); and every error code the API can return
// has its errors.<code> text.

load(js.exec_dir + "kit.js");
var Locale = Kit.wlib("locale");

var WEBMAIL_DIR = Kit.exec_root() + "../webmail/";
var ROOT = WEBMAIL_DIR + "root/";
var strings = Locale.load(WEBMAIL_DIR + "lib/", WEBMAIL_DIR + "mods/", "en_us");

function has(key)
{
	return Object.prototype.hasOwnProperty.call(strings, key) && typeof strings[key] === "string";
}

function read_text(path)
{
	var f = new File(path);
	Kit.assert(f.open("r"), "open " + path);
	var text = f.read();
	f.close();
	return text;
}

var sections = {};
for (var k in strings) {
	if (Object.prototype.hasOwnProperty.call(strings, k))
		sections[k.substr(0, k.indexOf("."))] = true;
}
Kit.assert(sections.main && sections.errors, "locale sections read");

// The API's method names ("settings.get" has the form of a key of the
// settings section): read from api.js's method table, never taken for keys
var api_text = read_text(WEBMAIL_DIR + "lib/api.js");
var method_names = {};
var mre = /^\t\t"([a-z][a-z0-9_.]*)": \{ auth:/gm;
var mm;
while ((mm = mre.exec(api_text)) !== null)
	method_names[mm[1]] = true;
Kit.assert(method_names["settings.get"] && method_names["message.list"], "API method names read");

var files = directory(ROOT + "js/*.js");
Kit.assert(files.length > 0, "no .js files in " + ROOT + "js/");
files.push(ROOT + "index.xjs");

var used = 0;
var missing = [];
for (var i = 0; i < files.length; i++) {
	var text = read_text(files[i]);
	var re = /(["'])([a-z0-9_]+)\.([a-z0-9_]+)\1/g;
	var m;
	while ((m = re.exec(text)) !== null) {
		if (!Object.prototype.hasOwnProperty.call(sections, m[2])
			|| Object.prototype.hasOwnProperty.call(method_names, m[2] + "." + m[3]))
			continue;
		used++;
		if (!has(m[2] + "." + m[3]))
			missing.push(file_getname(files[i]) + ": " + m[2] + "." + m[3]);
	}
}
Kit.assert(used > 0, "no string keys found");
Kit.eq(missing, [], "string keys missing from en_us.ini");

// The API's error codes: its CODES list plus "internal"
var api = read_text(WEBMAIL_DIR + "lib/api.js");
var cm = /var CODES = \[([^\]]*)\]/.exec(api);
Kit.assert(cm !== null, "CODES list found in api.js");
var codes = cm[1].match(/"[a-z]+"/g).map(function (q) {
	return q.substr(1, q.length - 2);
});
codes.push("internal");
Kit.assert(codes.length >= 10, "codes read: " + codes.join(","));
var no_text = [];
for (var j = 0; j < codes.length; j++) {
	if (!has("errors." + codes[j]))
		no_text.push(codes[j]);
}
Kit.eq(no_text, [], "error codes without an errors.<code> string");
