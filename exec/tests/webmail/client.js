// The browser client's safety-critical constants, read from the shipped
// sources as text (no browser): the sandbox of the message frame, the
// frame document's policy prefix, the request header and endpoint, no
// markup-writing or eval calls, only relative URLs for the app's own
// files, and the shell page's fallback wiring.

load(js.exec_dir + "kit.js");

// argv[0]: another copy of webmail/root/ to check (to try the test
// against a changed copy); the runner passes none
var ROOT = typeof argv !== "undefined" && argv.length > 0 ? argv[0] : Kit.exec_root() + "../webmail/root/";

function read_text(path)
{
	var f = new File(path);
	Kit.assert(f.open("r"), "open " + path);
	var text = f.read();
	f.close();
	return text;
}

var modules = directory(ROOT + "js/*.js");
Kit.assert(modules.length > 0, "no modules in " + ROOT + "js/");
var src = {};
var all = [];
for (var i = 0; i < modules.length; i++) {
	var name = file_getname(modules[i]);
	src[name] = read_text(modules[i]);
	all.push(name);
}
var INDEX = read_text(ROOT + "index.xjs");
Kit.assert(src["reader.js"] && src["body.js"] && src["rpc.js"] && src["app.js"], "the modules read");

// Every match of re (global) in text: [match arrays]
function matches(re, text)
{
	var out = [];
	var m;
	re.lastIndex = 0;
	while ((m = re.exec(text)) !== null)
		out.push(m);
	return out;
}

// The string literals of a piece of source, in order, unescaped for \" \' \\
function literals(text)
{
	var out = [];
	var all_m = matches(/"((?:[^"\\\n]|\\.)*)"|'((?:[^'\\\n]|\\.)*)'/g, text);
	for (var j = 0; j < all_m.length; j++) {
		var v = all_m[j][1] !== undefined ? all_m[j][1] : all_m[j][2];
		out.push(v.replace(/\\(.)/g, "$1"));
	}
	return out;
}

// The text of function 'name' in source s (from its header to the brace
// that closes it)
function function_text(s, name)
{
	var at = s.search(new RegExp("function\\s+" + name + "\\s*\\("));
	Kit.assert(at >= 0, "function " + name + " found");
	var open = s.indexOf("{", at);
	var depth = 0;
	for (var j = open; j < s.length; j++) {
		var c = s.charAt(j);
		if (c === "{")
			depth++;
		else if (c === "}" && --depth === 0)
			return s.substring(at, j + 1);
	}
	throw new Error("function " + name + ": no closing brace");
}

// ---- The message frame's sandbox: exactly two tokens, set in one place
var SANDBOX = ["allow-popups", "allow-popups-to-escape-sandbox"];
var sets = [];
for (i = 0; i < all.length; i++) {
	var s = src[all[i]];
	var found = matches(/setAttribute\(\s*["']sandbox["']\s*,\s*(["'])([^"']*)\1/g, s)
		.concat(matches(/\.sandbox(?:\.value)?\s*=\s*(["'])([^"']*)\1/g, s));
	for (var j = 0; j < found.length; j++)
		sets.push([all[i], found[j][2]]);
	Kit.eq(matches(/\.sandbox\.add\s*\(|\.sandbox\.(?:replace|toggle)\s*\(/g, s).length, 0,
		all[i] + ": the sandbox is not changed token by token");
	Kit.eq(matches(/["']sandbox["']\s*:/g, s).length, 0, all[i] + ": no sandbox in an attribute object");
}
Kit.eq(sets.length, 1, "the sandbox is set once: " + JSON.stringify(sets));
Kit.eq(sets[0][0], "reader.js", "the sandbox is set by reader.js");
Kit.eq(sets[0][1].split(/\s+/).filter(function (t) { return t !== ""; }), SANDBOX,
	"the sandbox tokens, exactly");
var NEVER = ["allow-scripts", "allow-same-origin", "allow-forms", "allow-top-navigation"];
for (i = 0; i < NEVER.length; i++) {
	for (j = 0; j < all.length; j++)
		Kit.eq(src[all[j]].indexOf(NEVER[i]), -1, all[j] + " never mentions " + NEVER[i]);
	Kit.eq(INDEX.indexOf(NEVER[i]), -1, "index.xjs never mentions " + NEVER[i]);
}

// ---- The frame document's prefix: the policy <meta> first, then the
// DNS prefetch, base target and referrer metas
var body = src["body.js"];
function constant(s, name)
{
	var m = new RegExp("const\\s+" + name + "\\s*=\\s*(\"(?:[^\"\\\\]|\\\\.)*\")\\s*;").exec(s);
	Kit.assert(m !== null, name + " is one string constant");
	return literals(m[1])[0];
}
Kit.eq(constant(body, "POLICY_BLOCKED"), "default-src 'none'; img-src data:; style-src 'unsafe-inline'",
	"the frame's policy");
Kit.eq(constant(body, "POLICY_IMAGES"), "default-src 'none'; img-src data: https:; style-src 'unsafe-inline'",
	"the frame's policy after Show images");
var fd = function_text(body, "frameDoc");
var ret = fd.substr(fd.lastIndexOf("return "));
var parts = literals(ret);
var CSP_META = "<meta http-equiv=\"Content-Security-Policy\" content=\"";
Kit.eq(/^return\s*["']/.test(ret), true, "frameDoc's result starts with a constant");
Kit.eq(parts[0], CSP_META, "the prefix begins with the policy meta");
// between the first two constants: only the choice of the two policies
var lit_at = matches(/"(?:[^"\\\n]|\\.)*"|'(?:[^'\\\n]|\\.)*'/g, ret);
Kit.assert(lit_at.length >= 2, "frameDoc: constants in the result");
var between = ret.substring(lit_at[0].index + lit_at[0][0].length, lit_at[1].index);
var idents = between.match(/[A-Za-z_$][A-Za-z0-9_$]*/g) || [];
for (i = 0; i < idents.length; i++)
	Kit.assert(["opts", "images", "true", "POLICY_IMAGES", "POLICY_BLOCKED"].indexOf(idents[i]) >= 0,
		"the policy content is one of the two constants: " + between);
Kit.assert(idents.indexOf("POLICY_BLOCKED") >= 0 && idents.indexOf("POLICY_IMAGES") >= 0,
	"both policies named: " + between);
var NEXT = ["\">", "<meta http-equiv=\"x-dns-prefetch-control\" content=\"off\">", "<base target=\"_blank\">",
	"<meta name=\"referrer\" content=\"no-referrer\">"];
Kit.eq(parts.slice(1, 1 + NEXT.length), NEXT, "the metas after the policy, in order");

// ---- Requests: the X-Webmail header, POST to the relative "api.ssjs"
var rpc = src["rpc.js"];
var ep = /const\s+ENDPOINT\s*=\s*["']([^"']*)["']/.exec(rpc);
Kit.assert(ep !== null, "rpc.js: ENDPOINT constant");
Kit.eq(ep[1], "api.ssjs", "rpc.js: the endpoint, relative");
var fetches = matches(/\bfetch\s*\(\s*([^,)]*)/g, rpc);
Kit.eq(fetches.length, 1, "rpc.js: one fetch");
Kit.eq(fetches[0][1].replace(/\s+/g, ""), "ENDPOINT", "rpc.js: fetch(ENDPOINT, ...)");
Kit.assert(/["']X-Webmail["']\s*:\s*["']1["']/.test(rpc), "rpc.js: X-Webmail: 1 header");
Kit.assert(/method\s*:\s*["']POST["']/.test(rpc), "rpc.js: POST");

// ---- No markup written from strings, no eval
var FORBIDDEN = [/\binnerHTML\b/, /\binsertAdjacentHTML\b/, /\bdocument\s*\.\s*write(?:ln)?\b/,
	/\beval\s*\(/, /\bnew\s+Function\b/, /\bsetTimeout\s*\(\s*["'`]/, /\bsetInterval\s*\(\s*["'`]/,
	/\bcreateContextualFragment\b/];
for (i = 0; i < all.length; i++)
	for (j = 0; j < FORBIDDEN.length; j++)
		Kit.assert(!FORBIDDEN[j].test(src[all[i]]), all[i] + " contains " + FORBIDDEN[j]);
// outerHTML: only body.js's one read of the inert (parsed, never shown)
// document; never written
var outer = [];
for (i = 0; i < all.length; i++) {
	var om = matches(/[^\n]*\bouterHTML\b[^\n]*/g, src[all[i]]);
	for (j = 0; j < om.length; j++)
		outer.push(all[i] + ": " + om[j][0].replace(/^\s+/, ""));
}
Kit.eq(outer, ["body.js: out.html = doc.documentElement.outerHTML;"], "outerHTML: the one read");

// ---- Only relative URLs for the app's own files and endpoints (the
// shell page's HTML: its server-side code blocks name file system paths)
var INDEX_HTML = INDEX.replace(/<\?(?:xjs)?[\s\S]*?\?>/g, "");
Kit.assert(INDEX_HTML.indexOf("<script") >= 0 && INDEX_HTML.indexOf("system.") < 0,
	"index.xjs: server code removed, HTML kept");
var pages = all.slice(0);
src["index.xjs"] = INDEX_HTML;
pages.push("index.xjs");
for (i = 0; i < pages.length; i++) {
	var t = src[pages[i]];
	Kit.eq(t.indexOf("/webmail/"), -1, pages[i] + ": no /webmail/ path");
	var lits = matches(/(["'`])\/[A-Za-z0-9_.~-]/g, t);
	Kit.eq(lits.length, 0, pages[i] + ": a quoted absolute path: "
		+ (lits.length ? t.substr(lits[0].index, 30) : ""));
	var imports = matches(/\bimport\s*(?:[^"';]*?\bfrom\s*)?\(?\s*["']([^"']*)["']/g, t);
	for (j = 0; j < imports.length; j++)
		Kit.eq(imports[j][1].substr(0, 2), "./", pages[i] + ": import is relative: " + imports[j][1]);
}
var attrs = matches(/\b(?:src|href|action)\s*=\s*["']([^"']*)["']/g, INDEX_HTML);
Kit.assert(attrs.length >= 4, "index.xjs: src and href attributes found");
for (i = 0; i < attrs.length; i++)
	Kit.assert(!/^\/|^[a-z][a-z0-9+.-]*:/i.test(attrs[i][1]), "index.xjs: relative URL: " + attrs[i][0]);

// ---- The shell page: the fallback stylesheet first, the module, then
// check.js after it, and no inline script
// (each of the page's documents: the main one and the "needs HTTPS" one)
var docs = INDEX.split(/<!DOCTYPE html>/i).slice(1);
Kit.eq(docs.length, 2, "index.xjs: two documents");
for (i = 0; i < docs.length; i++) {
	var links = matches(/<link\b[^>]*\brel\s*=\s*["']stylesheet["'][^>]*>/g, docs[i]);
	Kit.assert(links.length >= 1, "index.xjs: stylesheets");
	Kit.assert(/\bhref\s*=\s*["']css\/base\.css["']/.test(links[0][0]),
		"index.xjs: css/base.css is the first stylesheet: " + links[0][0]);
}
// The page writes each script's versioned path from a page.<name> value
function asset_var(file)
{
	var m = new RegExp("page\\.([a-z_]+)\\s*=\\s*Shell\\.asset\\(\\s*root\\s*,\\s*[\"']"
		+ file.replace(/\./g, "\\.") + "[\"']").exec(INDEX);
	Kit.assert(m !== null, "index.xjs: the path of " + file);
	return new RegExp("wm\\." + m[1] + "\\b");
}
var APP_SRC = asset_var("js/app.js");
var CHECK_SRC = asset_var("js/check.js");
var scripts = matches(/<script\b((?:[^>?]|\?(?!>)|<\?[^?]*\?>)*)>/g, INDEX);
var app_at = -1;
var check_at = -1;
for (i = 0; i < scripts.length; i++) {
	var a = scripts[i][1];
	if (/\bsrc\s*=/.test(a)) {
		if (/\btype\s*=\s*["']module["']/.test(a) && APP_SRC.test(a))
			app_at = scripts[i].index;
		else if (CHECK_SRC.test(a)) {
			check_at = scripts[i].index;
			Kit.assert(/\bdefer\b/.test(a) && !/\btype\s*=\s*["']module["']/.test(a),
				"index.xjs: check.js is a deferred classic script");
		}
	} else
		Kit.assert(/\btype\s*=\s*["']application\/json["']/.test(a),
			"index.xjs: no inline script: <script" + a + ">");
}
Kit.assert(app_at >= 0, "index.xjs: app.js loaded as a module");
Kit.assert(check_at > app_at, "index.xjs: check.js after app.js");
// js/theme.js puts the stored theme on the page before it is drawn: a
// classic script that blocks (no defer, async or module), in the head
var THEME_SRC = asset_var("js/theme.js");
var theme_tags = scripts.filter(function (sc) { return THEME_SRC.test(sc[1]); });
Kit.eq(theme_tags.length, 1, "index.xjs: theme.js loaded once");
Kit.assert(!/\b(?:defer|async)\b|\btype\s*=\s*["']module["']/.test(theme_tags[0][1]),
	"index.xjs: theme.js blocks: " + theme_tags[0][0]);
Kit.assert(theme_tags[0].index < INDEX.indexOf("</head>", theme_tags[0].index)
	&& INDEX.lastIndexOf("<body", theme_tags[0].index) < INDEX.lastIndexOf("<head>", theme_tags[0].index),
	"index.xjs: theme.js is in the head");
Kit.assert(/id\s*=\s*["']wm-unsupported["']/.test(INDEX), "index.xjs: the fallback block");

// ---- The message list is a grid (rows of cells with real controls), not
// a listbox: no option role anywhere, the grid's roles in list.js
var list = src["list.js"];
Kit.assert(list !== undefined, "list.js read");
Kit.assert(/role\s*:\s*["']grid["']/.test(list), "list.js: role grid");
Kit.assert(/role\s*:\s*["']row["']/.test(list), "list.js: role row");
Kit.assert(/role\s*:\s*["']gridcell["']/.test(list), "list.js: role gridcell");
Kit.assert(/["']aria-rowcount["']/.test(list) && /["']aria-rowindex["']/.test(list),
	"list.js: aria-rowcount and aria-rowindex");
// (compose.js alone has a listbox: the recipient suggestions, the popup
// of a combobox)
for (i = 0; i < all.length; i++) {
	if (all[i] === "compose.js")
		continue;
	Kit.eq(matches(/role\s*:\s*["'](?:option|listbox)["']/g, src[all[i]]).length, 0,
		all[i] + ": no option or listbox role");
	Kit.eq(matches(/aria-activedescendant/g, src[all[i]]).length, 0, all[i] + ": no aria-activedescendant");
}

// ---- Compose: the recipient fields are comboboxes with a listbox popup
// (WAI-ARIA combobox pattern); the body is a plain-text <textarea>; no
// element anywhere is made editable as rich text
var compose = src["compose.js"];
Kit.assert(compose !== undefined, "compose.js read");
Kit.assert(/role\s*:\s*["']combobox["']/.test(compose) && /role\s*:\s*["']listbox["']/.test(compose)
	&& /role\s*:\s*["']option["']/.test(compose), "compose.js: combobox, listbox and option roles");
Kit.assert(/["']aria-autocomplete["']\s*:\s*["']list["']/.test(compose) && /["']aria-controls["']/.test(compose)
	&& /aria-activedescendant/.test(compose), "compose.js: the combobox's aria-autocomplete, aria-controls and active option");
Kit.assert(/h\(\s*["']textarea["']\s*,\s*\{\s*id\s*:\s*["']wm-c-body["']/.test(compose), "compose.js: the body is a textarea");
for (i = 0; i < all.length; i++)
	Kit.eq(matches(/contenteditable|contentEditable|designMode/g, src[all[i]]).length, 0,
		all[i] + ": nothing is made editable as rich text");
Kit.eq(matches(/contenteditable/gi, INDEX).length, 0, "index.xjs: no contenteditable");

// ---- Images from other sites load without asking only from a sender on
// the user's list, never for a message in Spam or Trash, only for
// Internet mail whose structured sender address is plain and has no
// control character (reader.js autoImages, run here); the reader feeds it
// the sender's ADDRESS (headers.from_addr, "" when the server marked it
// malformed; never the display name) and both sources of the folder (the
// list item's and the message's); frame() decides with it and offers
// "Always show images" only where it would apply
(function () {
	var reader = src["reader.js"];
	var auto = eval("(" + function_text(reader, "autoImages") + ")");
	var L = ["photos@pics.example", "b@x.example"];
	var I = "internet";
	var cases = [
		[["inbox"], "photos@pics.example", I, L, true, "listed sender, Inbox"],
		[["u:Family", undefined], "Photos@Pics.Example", I, L, true, "ASCII case ignored"],
		[["spam"], "photos@pics.example", I, L, false, "never in Spam"],
		[["trash"], "photos@pics.example", I, L, false, "never in Trash"],
		[["inbox", "spam"], "photos@pics.example", I, L, false, "never when the message says Spam"],
		[["spam", "inbox"], "photos@pics.example", I, L, false, "never when the list item says Spam"],
		[["inbox", "trash"], "photos@pics.example", I, L, false, "never when the message says Trash"],
		[[undefined, undefined], "photos@pics.example", I, L, false, "folder unknown"],
		[[], "photos@pics.example", I, L, false, "no folder"],
		["inbox", "photos@pics.example", I, L, false, "folders not a list"],
		[["inbox"], "photos@pics.example", "local", L, false, "not Internet mail (local)"],
		[["inbox"], "photos@pics.example", "fido", L, false, "not Internet mail (FidoNet)"],
		[["inbox"], "photos@pics.example", undefined, L, false, "network unknown"],
		[["inbox"], "other@pics.example", I, L, false, "sender not listed"],
		[["inbox"], "", I, L.concat([""]), false, "empty address, even listed"],
		[["inbox"], "photos@pics\u202e.example", I, L, false, "a bidirectional control"],
		[["inbox"], "\u200ephotos@pics.example", I, L.concat(["\u200ephotos@pics.example"]), false, "a control, even listed"],
		[["inbox"], "photos@pics.example\n", I, L, false, "a line break"],
		[["inbox"], "photos@pics.example\u0085", I, L, false, "a C1 control"],
		[["inbox"], "Photo <photos@pics.example>", I, L.concat(["photo <photos@pics.example>"]), false, "a name, not an address"],
		[["inbox"], "photos@localhost", I, L.concat(["photos@localhost"]), false, "no dot in the domain"],
		[["inbox"], "photos@pics.example", I, [], false, "empty list"],
		[["inbox"], "photos@pics.example", I, null, false, "no list"]
	];
	for (var i = 0; i < cases.length; i++)
		Kit.eq(auto(cases[i][0], cases[i][1], cases[i][2], cases[i][3]), cases[i][4], "autoImages: " + cases[i][5]);
	var fr = function_text(reader, "frame");
	Kit.assert(/const auto = listed\(\);/.test(fr) && /const allowed = imagesOn\.has\(id\) \|\| auto;/.test(fr),
		"reader.js frame(): images allowed only by the user's choice or autoImages");
	Kit.assert(/autoImages\(foldersOf\(\), senderOf\(\), netOf\(\),/.test(function_text(reader, "listed")),
		"reader.js listed(): autoImages(foldersOf(), senderOf(), netOf(), ...)");
	var so = function_text(reader, "senderOf");
	Kit.assert(/hd\.from_addr_bad !== true/.test(so) && /hd\.from_addr\b/.test(so) && !/\.from\b(?!_)/.test(so),
		"reader.js senderOf(): the structured address, not the display name, never a malformed one: " + so);
	var fo = function_text(reader, "foldersOf");
	Kit.assert(/cur\.item\.folder/.test(fo) && /cur\.msg\.folder/.test(fo), "reader.js foldersOf(): the list item's and the message's");
	Kit.assert(/net_type/.test(function_text(reader, "netOf")), "reader.js netOf(): headers.net_type");
	Kit.assert(/const always = autoImages\(foldersOf\(\), from, netOf\(\),/.test(fr),
		"reader.js frame(): Always is offered only where the rule would apply");
})();

// ---- A name that could pass for an address shows the real address, in
// list rows and in compose's recipient chips alike (format.js senderText)
Kit.assert(/senderText\(/.test(function_text(src["compose.js"], "chipText")), "compose.js chipText uses senderText");
Kit.assert(/senderText\(/.test(src["list.js"]), "list.js uses senderText");
Kit.assert(/import \{ entryOf \} from "\.\/compose\.js"/.test(src["contacts.js"]), "contacts.js builds entries with compose's entryOf");

// ---- compose.js's ports of server code, run here beside the originals:
// splitList (Address.split_entries) and estimateBytes (MimeWrite.estimate)
(function () {
	var WEBMAIL = Kit.exec_root() + "../webmail/lib/";
	var addr_src = read_text(WEBMAIL + "address.js");
	var server_split = eval("(" + function_text(addr_src, "split_entries") + ")");
	var client_split = eval("(" + function_text(compose, "splitList") + ")");
	var lists = ["", "a@b.c", "a@b.c, d@e.f", "a@b.c; d@e.f,,", '"Doe, John" <j@d.org>, x@y.z',
		'"Q \\" , q" <q@q.q>; r@r.r', "Name (comment, with comma) <n@n.n>, m@m.m",
		"Name (nested (deep, x) \\) still) <n@n.n>, o@o.o", "Name (a \\( b, c) <n@n.n>, m@m.m", "<a,b@c.d>, e@f.g", "a@b.c;d@e.f;",
		"\xc3\xa9t\xc3\xa9 <e@e.e>, f@f.f", '"unclosed, quote <u@u.u>, v@v.v'];
	for (var i = 0; i < lists.length; i++)
		Kit.eq(client_split(lists[i]), server_split(lists[i]), "splitList matches split_entries: " + lists[i]);
	var MimeWrite = Kit.wlib("mimewrite");
	var client_est = eval("(" + function_text(compose, "estimateBytes") + ")");
	var bodies = ["", "Hello", "Hello\nworld\n", "\xc3\xa9t\xc3\xa9\r\n", "trailing space \nx", "From here\n",
		new Array(1200).join("x"), "tab\there", "ctl\x01x", new Array(5000).join("line\n")];
	var sizes = [[], [0], [1], [2], [3], [57], [1000000], [15323844], [100, 200, 300]];
	for (i = 0; i < bodies.length; i++) {
		for (var j = 0; j < sizes.length; j++) {
			var files = sizes[j].map(function (n) { return { size: n }; });
			Kit.eq(client_est(bodies[i], sizes[j]), MimeWrite.estimate(bodies[i], files),
				"estimateBytes matches MimeWrite.estimate: body " + i + ", files " + sizes[j].join("+"));
		}
	}
})();

// ---- rpc.js's method lists agree with the server's method table: WRITE
// is every method with write: true, READ_ONLY every other one but
// session.login and session.logout, and CHUNKED gives each method that
// takes a list of message ids the server's cap on that list
(function () {
	var Api = Kit.wlib("api");
	var rpc = src["rpc.js"];
	function list_of(name) {
		var m = new RegExp("const " + name + " = \\[([^\\]]*)\\]").exec(rpc);
		Kit.assert(m !== null, "rpc.js defines " + name);
		return literals(m[1]).sort();
	}
	var write = [];
	var read = [];
	var capped = {};
	for (var name in Api.methods) {
		var m = Api.methods[name];
		if (m.write === true)
			write.push(name);
		else if (name !== "session.login" && name !== "session.logout")
			read.push(name);
		if (m.args && m.args.ids && m.args.ids.type === "list")
			capped[name] = m.args.ids.max;
	}
	Kit.eq(list_of("WRITE"), write.sort(), "rpc.js WRITE: the methods with write: true");
	Kit.eq(list_of("READ_ONLY"), read.sort(), "rpc.js READ_ONLY: every other method but sign-in and sign-out");
	var cm = /const CHUNKED = \{([^}]*)\}/.exec(rpc);
	Kit.assert(cm !== null, "rpc.js defines CHUNKED");
	var chunked = {};
	var pairs = matches(/"([a-z.]+)"\s*:\s*([0-9]+)/g, cm[1]);
	for (var i = 0; i < pairs.length; i++)
		chunked[pairs[i][1]] = parseInt(pairs[i][2], 10);
	function pairs_of(o) {
		var out = [];
		for (var k in o)
			out.push(k + "=" + o[k]);
		return out.sort();
	}
	Kit.eq(pairs_of(chunked), pairs_of(capped), "rpc.js CHUNKED: the server's id cap of each method with ids");
})();

// ---- rpc.js writes a request with "calls" first and "as" last, which the
// server's checks of a large body before parsing rely on (Api.body_check
// reads the method from the start, large_gate "as" from the end; their
// side is tested in api_body.js with bodies in this order)
(function () {
	var rq = function_text(src["rpc.js"], "request");
	Kit.assert(/^function request\(batch\) \{\s*const body = \{ calls: [^\n]*\};\s*$/m.test(rq.split("\n").slice(0, 2).join("\n")),
		"rpc.js request(): the body object is made with calls as its first property: " + rq);
	var as_at = rq.indexOf("body.as = ");
	Kit.assert(as_at > rq.indexOf("body.background = ") && as_at > rq.indexOf("const body = "),
		"rpc.js request(): as is added last");
	Kit.assert(!/body\.[a-z_]+ = /.test(rq.substr(as_at + 10)), "rpc.js request(): nothing added after as");
	Kit.assert(/body: JSON\.stringify\(request\(batch\)\)/.test(function_text(src["rpc.js"], "post")),
		"rpc.js post(): the body is JSON.stringify(request(batch))");
})();

// ---- compose.js's send rule: draft.send is never retried by rpc.js; a
// retry of the same attempt keeps its key; "unknown" and "maybe_sent"
// ask first, then send with a NEW key and confirm_resend; the recipient
// limit is checked before sending
(function () {
	var c = src["compose.js"];
	var at = function_text(c, "attempt");
	Kit.assert(/const args = \{ id: s\.id, key: job\.key \};/.test(at), "compose.js attempt(): the job's key");
	Kit.assert(/if \(job\.confirm\)\s*args\.confirm_resend = true;/.test(at), "compose.js attempt(): confirm_resend from the job");
	Kit.assert(/rpc\.call\("draft\.send", args, \{ retry: false/.test(at), "compose.js attempt(): draft.send with retry: false");
	var rl = function_text(c, "retryLater");
	Kit.assert(/attempt\(job\)/.test(rl) && !/newKey\(\)/.test(rl), "compose.js retryLater(): the same job, the same key");
	var fl = function_text(c, "failed");
	var unk = fl.substr(fl.indexOf("case \"unknown\":"));
	Kit.assert(/case "maybe_sent":\s*s\.maybeSent = true;\s*reopen\(s, null\);\s*askResend\(s\);\s*return;/.test(unk),
		"compose.js failed(): unknown and maybe_sent ask first");
	Kit.assert(!/newKey\(\)/.test(fl.substr(0, fl.indexOf("case \"key\":"))), "compose.js failed(): no new key before the key case");
	Kit.assert(/go\(s, \{ s, key: newKey\(\), confirm: true, tries: 0, rekeyed: false \}\)/.test(function_text(c, "askResend")),
		"compose.js askResend(): a new key and confirm");
	Kit.assert(/limits\.max_recipients/.test(function_text(c, "sendNow")) && /recipientCount\(s\) > maxRecipients/.test(function_text(c, "sendNow")),
		"compose.js sendNow(): the recipient limit before sending");
})();

// ---- reader.js keeps at most MAX_AVATARS senders' avatars, dropping the
// oldest
(function () {
	var r = src["reader.js"];
	var m = /const MAX_AVATARS = ([0-9]+);/.exec(r);
	Kit.assert(m !== null && parseInt(m[1], 10) > 0 && parseInt(m[1], 10) <= 1000, "reader.js: a bound on the avatar cache");
	Kit.assert(/while \(avatars\.size > MAX_AVATARS\)\s*avatars\.delete\(avatars\.keys\(\)\.next\(\)\.value\);/
		.test(function_text(r, "learnAvatar")), "reader.js learnAvatar(): the oldest dropped past the bound");
})();
