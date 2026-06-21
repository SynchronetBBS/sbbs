// Faithful SpiderMonkey-1.8.5 (the door's runtime) parse-only check: compile, don't run.
// Usage: jsexec test/syntaxcheck.js <path-to-file>
var path = argv[0];
if (!path) { print("usage: jsexec test/syntaxcheck.js <file>"); exit(2); }
var f = new File(path);
if (!f.open("r")) { print("cannot open " + path); exit(2); }
var src = f.read(); f.close();
try { new Function(src); print("PARSE OK (SpiderMonkey): " + path); }
catch (e) { print("PARSE ERROR (SpiderMonkey): " + e); exit(1); }
