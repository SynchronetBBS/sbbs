// Webv4: sign-in from a webv4 session cookie (webmail/lib/webv4.js)

load(js.exec_dir + "kit.js");
var Webv4 = Kit.wlib("webv4");

var KEY = "AbC123xyz_" + "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
var scr = Kit.scratch();
var real_read = Webv4.read_key;
try {
	var data = scr.data_dir;
	var web_file = data + "user/60001.web";

	function put(path, text)
	{
		var f = new File(path);
		Kit.assert(f.open("w"), "create " + path);
		f.write(text);
		f.close();
	}

	// The stored file, as webv4's auth.js writes it (iniSetValue, root section)
	put(web_file, "key=" + KEY + "\nip_address=192.0.2.1\nsession_start=1700000000\n"
		+ "csrf_token=zzz\n");
	var stamp = file_date(web_file);
	var size = file_size(web_file);

	// Count the files read, and the paths, to show what is touched
	var reads = [];
	Webv4.read_key = function (path) {
		reads.push(path);
		return real_read(path);
	};

	// --- a valid cookie gives the user
	Kit.eq(Webv4.session_user({ synchronet: ["60001," + KEY] }, data), 60001, "valid cookie");
	Kit.eq(reads, [web_file], "one file read, the user's .web file");

	// A cookie array with a bad value first: the next one is still tried, as webv4 does
	reads = [];
	Kit.eq(Webv4.session_user({ synchronet: ["junk", "60001," + KEY] }, data), 60001,
		"second value");
	Kit.eq(reads.length, 1, "the malformed value was not looked up");

	// A plain string value (not an array) is accepted as one value
	Kit.eq(Webv4.session_user({ synchronet: "60001," + KEY }, data), 60001, "string value");

	// --- wrong key
	reads = [];
	Kit.eq(Webv4.session_user({ synchronet: ["60001," + KEY + "x"] }, data), 0, "longer key");
	Kit.eq(Webv4.session_user({ synchronet: ["60001," + KEY.substr(0, KEY.length - 1)] }, data), 0,
		"shorter key");
	Kit.eq(Webv4.session_user({ synchronet: ["60001,b" + KEY.substr(1)] }, data), 0,
		"one character different");
	Kit.eq(Webv4.session_user({ synchronet: ["60001,abc"] }, data), 0, "other key");
	Kit.eq(reads.length, 4, "each well-formed value was looked up");

	// --- malformed values: 0, and no path is built
	reads = [];
	var bad = ["0,abc", "1e3,abc", "../1,abc", "1,", ",abc", "60001", "060001," + KEY,
		"-1,abc", "1 ,abc", "1,ab c", "1,abc\n", "1,ab-c", "1,ab.c", "12345678901,abc",
		"4294967296,abc", "1,ab/../c", "1," + new Array(1100).join("a"), "", null, 5, {}];
	for (var i = 0; i < bad.length; i++)
		Kit.eq(Webv4.session_user({ synchronet: [bad[i]] }, data), 0, "rejects " + JSON.stringify(bad[i]));
	Kit.eq(reads, [], "no file looked up for a malformed value");

	// --- wrong cookie name, no cookies, bad cookie objects
	Kit.eq(Webv4.session_user({ webmail: ["60001," + KEY] }, data), 0, "other cookie name");
	Kit.eq(Webv4.session_user({}, data), 0, "no cookies");
	Kit.eq(Webv4.session_user(null, data), 0, "null");
	Kit.eq(Webv4.session_user(undefined, data), 0, "undefined");
	Kit.eq(Webv4.session_user({ synchronet: 5 }, data), 0, "number");
	Kit.eq(reads, [], "nothing looked up");

	// --- a missing file, a file without a key, an empty key, a key webv4
	// would read as a number, and an unparsable file all give 0
	Kit.eq(Webv4.session_user({ synchronet: ["60002," + KEY] }, data), 0, "no file");
	put(data + "user/60003.web", "ip_address=192.0.2.1\n");
	Kit.eq(Webv4.session_user({ synchronet: ["60003," + KEY] }, data), 0, "no key in file");
	put(data + "user/60004.web", "key=\n");
	Kit.eq(Webv4.session_user({ synchronet: ["60004,a"] }, data), 0, "empty key");
	put(data + "user/60005.web", "key=12345\n");
	Kit.eq(Webv4.session_user({ synchronet: ["60005,12345"] }, data), 0,
		"a key iniGetObject turns into a number (webv4 refuses it too)");
	put(data + "user/60006.web", "\x00\xff[[[\n=\n");
	Kit.eq(Webv4.session_user({ synchronet: ["60006,abc"] }, data), 0, "garbage file");
	mkpath(data + "user/60007.web/");	// a directory where the file should be
	Kit.eq(Webv4.session_user({ synchronet: ["60007,abc"] }, data), 0, "a directory");

	// A user number of five digits uses the same %04u name webv4 uses
	put(data + "user/123456.web", "key=" + KEY + "\n");
	Kit.eq(Webv4.session_user({ synchronet: ["123456," + KEY] }, data), 123456, "six digits");
	put(data + "user/0042.web", "key=" + KEY + "\n");
	Kit.eq(Webv4.session_user({ synchronet: ["42," + KEY] }, data), 42, "zero padded file name");

	// --- the session file is never written
	Kit.eq(file_date(web_file), stamp, "webv4 file not modified");
	Kit.eq(file_size(web_file), size, "webv4 file size unchanged");

	// The cookie name is webv4's own (fixed in its auth.js)
	Kit.eq(Webv4.COOKIE, "synchronet");
} finally {
	Webv4.read_key = real_read;
	scr.cleanup();
}
