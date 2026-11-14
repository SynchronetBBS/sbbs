// Every stock JavaScript file must be ASCII or valid UTF-8 (#1341).
//
// SpiderMonkey 1.8.5 reads source byte-for-byte, so a raw CP437 or
// Windows-1252 byte works there; later engines compile source as UTF-8, where
// such a byte is malformed. Write those characters as escapes ("\xCD"),
// ascii(205), or plain ASCII instead.
//
// Only run from a source checkout (see skipif): a live install's xtrn/ also
// holds third-party doors.

var root = fullpath(js.exec_dir + "../../../");
var exts = /\.(js|ssjs|xjs)$/i;
var bad = [];
var checked = 0;

// Returns the 1-based line number of the first invalid UTF-8 sequence in
// the byte string 's', or 0 if it is valid
function invalid_utf8_line(s)
{
	var line = 1;
	var i = 0;
	while (i < s.length) {
		var c = s.charCodeAt(i);
		if (c == 0x0a)
			line++;
		if (c < 0x80) {
			i++;
			continue;
		}
		var len, min;
		if (c >= 0xc2 && c <= 0xdf) {
			len = 2;
			min = 0x80;
		} else if (c >= 0xe0 && c <= 0xef) {
			len = 3;
			min = 0x800;
		} else if (c >= 0xf0 && c <= 0xf4) {
			len = 4;
			min = 0x10000;
		} else
			return line;
		var cp = c & (0xff >> (len + 1));
		for (var j = 1; j < len; j++) {
			var cc = s.charCodeAt(i + j);
			if (isNaN(cc) || (cc & 0xc0) != 0x80)
				return line;
			cp = (cp << 6) | (cc & 0x3f);
		}
		if (cp < min || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
			return line;
		i += len;
	}
	return 0;
}

function scan(dir, depth)
{
	if (depth > 16)
		return;
	directory(dir + "*").forEach(function (path) {
		var last = path.charAt(path.length - 1);
		if (last == "/" || last == "\\") {
			scan(path, depth + 1);
			return;
		}
		if (!exts.test(path))
			return;
		var f = new File(path);
		if (!f.open("rb")) // e.g. a dangling symlink in a working tree
			return;
		var s = f.read();
		f.close();
		checked++;
		var line = invalid_utf8_line(s);
		if (line)
			bad.push(path.substr(root.length) + ":" + line);
	});
}

["exec/", "xtrn/", "web/", "webv4/"].forEach(function (dir) {
	if (file_isdir(root + dir))
		scan(root + dir, 0);
});

if (checked == 0)
	throw new Error("No source files found under " + root);
if (bad.length)
	throw new Error(bad.length + " file(s) not valid UTF-8 (first bad line): "
		+ bad.join(", "));
