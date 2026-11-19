// The browser client's safety-critical constants, read from the shipped
// sources as text (no browser): the sandbox of the message frame, the
// frame document's policy prefix, the request header and endpoint, no
// markup-writing or eval calls, only relative URLs for the app's own
// files, and the shell page's fallback wiring.

load(js.exec_dir + "kit.js");

// argv[0]: another copy of webmail/root/ to check (to try the test
// against a changed copy); the runner passes none
var ROOT = typeof argv !== "undefined" && argv.length > 0 ? argv[0] : Kit.exec_root() + "../webmail/root/";

function read(path)
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
	src[name] = read(modules[i]);
	all.push(name);
}
var INDEX = read(ROOT + "index.xjs");
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
Kit.assert(/id\s*=\s*["']wm-unsupported["']/.test(INDEX), "index.xjs: the fallback block");
