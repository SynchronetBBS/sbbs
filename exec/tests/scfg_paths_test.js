// scfg_paths_test.js -- exercises fix_scfg_paths() in chat_llm.js.
// Run: jsexec exec/tests/scfg_paths_test.js
//
// The guru invents SCFG menu paths ("Navigate to Util -> Modules ->
// Certtool"; SCFG has no Util menu).  fix_scfg_paths() checks a reply's
// SCFG path claims against the option index built from scfgindex.h by
// llm_index/scfg.js, corrects the ones it can place, and drops the rest.
// The inputs below are replies the bot actually produced in #synchronet,
// plus the text that MUST come through untouched -- the conservative half
// of the feature, since other Synchronet UIs (sbbsctrl, the terminal
// interface) have menu paths of their own that must not be "corrected".
//
// Needs an index whose index_sources includes the scfg crawler; skips
// cleanly when it doesn't.
//
// SpiderMonkey 1.8.5: no let/const, no arrow functions, no Object.keys().

var CHAT_LLM_NO_STANDALONE = true;
load("chat_llm.js");

var failures = 0;

function check(cond, what)
{
	if (!cond) {
		print("FAIL: " + what);
		failures++;
	} else
		print("ok: " + what);
}

var cfg = load_config("guru:irc");
var idx = load_index("guru:irc", cfg);
var pi  = idx ? scfg_path_index(idx) : null;
if (!pi) {
	print("SKIP: no SCFG option index in this install's retrieval index "
	    + "(add scfg:<path-to-scfgindex.h> to index_sources to run this)");
	exit(0);
}
print("SCFG option index: " + pi.count + " known locations");

var ctx = { persona: { code: "guru:irc" }, _scfg_paths: pi };

// [description, input, [must appear], [must NOT appear]]
var CASES = [
	[ "fabricated Util/Modules chain is dropped",
	  "Use SCFG -> Util -> Modules -> Certtool to generate a new cert.",
	  [ "SCFG" ], [ "Util", "Modules" ] ],

	[ "rewrite keeps the sentence's preposition",
	  "Use SCFG -> Util -> Modules -> Certtool to generate a new cert.",
	  [ "to generate a new cert" ], [] ],

	[ "wrong parent menu with a unique label is corrected",
	  "that's handled in SCFG -> File Options -> Downloads Per Day.",
	  [ "Security Level Values > Downloads Per Day" ],
	  [ "File Options > Downloads Per Day" ] ],

	[ "unplaceable label falls back to the real prefix",
	  "set it in SCFG->Message Areas->Sub-board Main Options.",
	  [ "Message Areas" ], [ "Sub-board Main Options" ] ],

	[ "wiki page ID written as a menu path is dropped",
	  "check util:SCFG:Servers->SMTP Server settings for that.",
	  [ "SCFG" ], [ "util:SCFG:Servers" ] ],

	[ "real path survives (> form)",
	  "you set that in SCFG under Servers > Web Server > Access Log Format.",
	  [ "Servers > Web Server > Access Log Format" ], [] ],

	[ "real path survives (-> form, label ending in ...)",
	  "SCFG->System->Security Options... is where the system password lives.",
	  [ "System->Security Options" ], [] ],

	[ "ambiguous label is never rewritten",
	  "turn it on in SCFG under Servers > FTP Server > Enabled.",
	  [ "Servers > FTP Server > Enabled" ], [] ],

	[ "another UI's path, unattributed, is untouched",
	  "disable it in the control panel under servers -> ftp -> configure.",
	  [ "servers -> ftp -> configure" ], [] ],

	[ "wiki URL is not mangled (colons look like separators)",
	  "see https://wiki.synchro.net/config:ssl.cert and util:scfg for docs.",
	  [ "https://wiki.synchro.net/config:ssl.cert" ], [] ],

	[ "numeric comparison near an SCFG mention is untouched",
	  "SCFG is fine here. 9 > 5 on that scale, trust me.",
	  [ "9 > 5" ], [] ],

	[ "omitted submenu ellipsis still counts as the real path",
	  "set it in SCFG > System > Security Options > "
	  + "Create Self-signed Certificate.",
	  [ "Security Options > Create Self-signed Certificate" ], [] ],

	[ "real path with no SCFG attribution is left alone",
	  "Posting Requirements lives under Message Areas > Sub-boards.",
	  [ "Message Areas > Sub-boards" ], [] ]
];

for (var i = 0; i < CASES.length; i++) {
	var desc = CASES[i][0], input = CASES[i][1];
	var want = CASES[i][2], unwant = CASES[i][3];
	var out;
	try { out = fix_scfg_paths(input, ctx); }
	catch (e) { out = "[EXCEPTION: " + e + "]"; }
	var ok = true, j;
	for (j = 0; j < want.length; j++)
		if (String(out).indexOf(want[j]) < 0) ok = false;
	for (j = 0; j < unwant.length; j++)
		if (String(out).indexOf(unwant[j]) >= 0) ok = false;
	check(ok, desc);
	if (!ok) {
		print("      in : " + input);
		print("      out: " + out);
	}
}

print(failures ? (failures + " failure(s)") : "all passed");
exit(failures ? 1 : 0);
