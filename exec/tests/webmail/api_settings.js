// Api: settings.get and settings.set (display name, From domain, theme,
// images_always, signature, mail forwarding), folder.list's drafts count,
// the limits in session.get, and the reason texts, driven through
// Api.dispatch with a fake env over scratch files

load(js.exec_dir + "kit.js");
var W = Kit.api_world();
var Api = W.Api;
var U = W.U;
var ALICE = W.ALICE;
var BOB = W.BOB;
var call = W.call;
var err = W.err;
var MailSig = Kit.lib("mailsig");
var Drafts = Kit.wlib("drafts");
var DEFS = {};
load(DEFS, Kit.exec_root() + "load/sbbsdefs.js");

Api.log = function () {};

var DOMAINS = ["vert.example", "family.example"];
var USERS = W.USERS;
USERS[ALICE].email = "alice@vert.example";
USERS[ALICE].netmail = "";
USERS[BOB].email = "bob@vert.example";
USERS[BOB].netmail = "bob@outside.example";

// The system values the settings read (env.sys); each test changes what
// it needs
var SYS;
function reset_sys()
{
	SYS = { settings: DEFS.SYS_FWDTONET, newuser_questions: 0, inet_domain_list: DOMAINS,
		msg_settings: 0, MM_FWDKEEP: undefined, trashed: [], matches: {}, unsupported: [],
		check_netmail_addr: function (a) {
			return SYS.unsupported.indexOf(a) < 0;
		},
		trashcan: function (base, s) {
			return base === "email" && SYS.trashed.indexOf(s) >= 0;
		},
		matchuserdata: function (field, data) {
			Kit.eq(field, DEFS.U_NETMAIL, "matchuserdata looks up the netmail field");
			return SYS.matches.hasOwnProperty(data) ? SYS.matches[data] : 0;
		} };
}
reset_sys();

function env(cookie, o)
{
	o = o || {};
	var ev = W.make_env({ cookie: cookie });
	ev.data_dir = W.S.data_dir;
	ev.sys = SYS;
	ev.lib_ctx = { domains: DOMAINS };
	return ev;
}

function set(cookie, args, what)
{
	var r = call(env(cookie), "settings.set", args);
	Kit.eq(r.ok, true, (what || "settings.set") + ": " + JSON.stringify(r));
	return r;
}

function get(cookie)
{
	var r = call(env(cookie), "settings.get", {});
	Kit.eq(r.ok, true, "settings.get: " + JSON.stringify(r));
	return r;
}

// A refused settings.set: the code, the reason and its own text
function refused(cookie, args, code, reason, what)
{
	var r = call(env(cookie), "settings.set", args);
	Kit.eq(r.ok, false, what + ": refused: " + JSON.stringify(r));
	Kit.eq(r.error, code, what + ": code");
	Kit.eq(r.reason, reason, what + ": reason");
	if (reason !== undefined) {
		var key = "errors." + code + "_" + reason;
		Kit.eq(r.message, W.STRINGS.hasOwnProperty(key) ? W.STRINGS[key] : W.STRINGS["errors." + code],
			what + ": the reason's text");
	}
	return r;
}

try {
	var alice = W.sign_in(ALICE);
	var bob = W.sign_in(BOB);
	var r, i, g;

	// --- The reason texts: the built-in English equals en_us.ini
	var src = new File(Kit.exec_root() + "../webmail/lib/api.js");
	Kit.assert(src.open("r"), "read api.js");
	var text = src.read();
	src.close();
	var block = /var REASON_FALLBACK = \{([\s\S]*?)\n\t\};/.exec(text);
	Kit.assert(block !== null, "REASON_FALLBACK found");
	var re = /\n\t\t([a-z0-9_]+): "([^"]*)"/g;
	var m, n = 0;
	while ((m = re.exec(block[1])) !== null) {
		n++;
		Kit.eq(W.STRINGS["errors." + m[1]], m[2], "en_us errors." + m[1] + " matches the built-in text");
	}
	Kit.assert(n >= 30, "reason texts read: " + n);
	Kit.assert(W.STRINGS.hasOwnProperty("errors.invalid_toomany_one"), "a _one form for the count");
	Kit.assert(W.STRINGS.hasOwnProperty("compose.label_dropped_recipients_one"), "dropped recipients, _one");

	// --- session.get: the limits, the drafts library's fixed ones included
	r = call(env(alice), "session.get", {});
	var dl = Drafts.limits({ config: W.cfg });
	for (var k in dl)
		Kit.eq(r.limits[k], dl[k], "session.get limits." + k + " is Drafts.limits'");
	Kit.eq(r.limits.max_recipients, W.cfg.max_recipients, "limits.max_recipients");
	Kit.eq(r.limits.max_message_bytes, W.cfg.max_message_bytes, "limits.max_message_bytes");
	Kit.eq(Api.ATTACH_CHUNK, Drafts.MAX_CHUNK, "the API's chunk size is the library's");
	Kit.eq(Api.MAX_ATTACHMENTS, Drafts.MAX_ATTACHMENTS, "attachments per draft");
	Kit.eq(Api.MAX_ADDR_FIELD, Drafts.MAX_ADDR_FIELD, "address field limit");

	// --- folder.list: the number of drafts (beside the folders)
	r = call(env(alice), "folder.list", {});
	Kit.eq(r.drafts, 0, "no drafts yet");
	for (i = 0; i < 2; i++)
		Kit.eq(call(env(alice), "draft.create", { mode: "new" }).ok, true, "draft.create");
	call(env(bob), "draft.create", { mode: "new" });
	r = call(env(alice), "folder.list", {});
	Kit.eq(r.drafts, 2, "two drafts counted");
	for (i = 0; i < r.folders.length; i++)
		Kit.assert(r.folders[i].id !== "drafts", "the folder list itself is unchanged");
	Kit.eq(call(env(bob), "folder.list", {}).drafts, 1, "another user's drafts not counted");

	// --- settings.get: defaults
	g = get(alice);
	Kit.eq([g.display_name, g.from_domain, g.theme, g.images_always],
		["", "", "system", []], "defaults");
	Kit.eq(g.from_domains, DOMAINS, "from_domains: Address.domains_for (ctx.domains here)");
	Kit.eq(call(env(alice), "session.get", {}).domains, g.from_domains, "session.get: the same list");
	Kit.eq(g.from_address_local, "alice", "from_address_local: user.email's local part");
	Kit.eq(g.signature_source, "none", "no signature");
	Kit.eq(g.signature, "", "no signature text");
	Kit.eq(g.signature_enabled, MailSig.enabled(), "signature_enabled: MailSig.enabled()");
	Kit.eq(g.forward, { allowed: true, enabled: false, address: "", keeps_copy: false }, "forward");

	// --- display name: cleaned (no line break), cut to 64 bytes
	g = set(alice, { display_name: "Alice\r\nBcc: evil@x.example" }, "display name");
	Kit.eq(g.display_name, "AliceBcc: evil@x.example", "stored without the line break");
	g = set(alice, { display_name: new Array(101).join("n") });
	Kit.eq(g.display_name.length, 64, "cut to 64");
	set(alice, { display_name: "Alice Smith" });

	// --- From domain: one of the allowed, lowercased; "" is the system's
	refused(alice, { from_domain: "evil.example" }, "invalid", "from_domain", "a domain outside the list");
	g = set(alice, { from_domain: "Family.Example" });
	Kit.eq(g.from_domain, "family.example", "stored lowercased");
	g = set(alice, { from_domain: "" });
	Kit.eq(g.from_domain, "", "back to the system address");
	set(alice, { from_domain: "family.example" });

	// --- theme: system, light or dark
	err(call(env(alice), "settings.set", { theme: "auto" }), "invalid", "theme auto");
	g = set(alice, { theme: "dark" });
	Kit.eq(g.theme, "dark", "theme stored");
	Kit.eq(g.display_name, "Alice Smith", "other settings kept");
	Kit.eq(g.from_domain, "family.example", "other settings kept");

	// --- images_always: replace, add, remove; lowercased; at most 200
	g = set(alice, { images_always: ["News@Example.org", "news@example.org", "a@b.example"] });
	Kit.eq(g.images_always, ["news@example.org", "a@b.example"], "replaced, lowercased, once each");
	g = set(alice, { images_add: "Shop@Store.example" });
	Kit.eq(g.images_always, ["news@example.org", "a@b.example", "shop@store.example"], "added");
	// another tab adds one too: both kept
	g = set(alice, { images_add: "other@tab.example" });
	Kit.eq(g.images_always.length, 4, "a second tab's addition kept the first");
	g = set(alice, { images_remove: "A@B.example" });
	Kit.eq(g.images_always, ["news@example.org", "shop@store.example", "other@tab.example"], "removed");
	g = set(alice, { images_remove: "nobody@nowhere.example" });
	Kit.eq(g.images_always.length, 3, "removing an absent sender changes nothing");
	g = set(alice, { images_add: "news@example.org" });
	Kit.eq(g.images_always.length, 3, "adding a present sender changes nothing");
	var bad_senders = ["not an address", "<a@b.example>", "Name <a@b.example>", "a@localhost", "a@127.0.0.1",
		"a@b.example, c@d.example", "a\r\n@b.example", "1:103/705", "a@b@c.example", ""];
	for (i = 0; i < bad_senders.length; i++) {
		refused(alice, { images_add: bad_senders[i] }, "invalid", "images", "images_add " + JSON.stringify(bad_senders[i]));
		refused(alice, { images_always: ["ok@example.org", bad_senders[i]] }, "invalid", "images",
			"images_always with " + JSON.stringify(bad_senders[i]));
	}
	Kit.eq(get(alice).images_always.length, 3, "nothing changed by the refusals");
	err(call(env(alice), "settings.set", { images_always: [], images_add: "x@y.example" }), "invalid",
		"the whole list and an addition together");
	var many = [];
	for (i = 0; i < 201; i++)
		many.push("s" + i + "@example.org");
	err(call(env(alice), "settings.set", { images_always: many }), "invalid", "201 senders");
	g = set(alice, { images_always: many.slice(0, 200) });
	Kit.eq(g.images_always.length, 200, "200 senders");
	refused(alice, { images_add: "one.more@example.org" }, "toolarge", "images", "a 201st sender");
	// checked before anything is written: the refused addition leaves the
	// signature, the name and the forwarding as they were
	USERS[ALICE].netmail = "";
	refused(alice, { signature: "NEW", images_add: "x@y.example", display_name: "Other",
		forward_address: "kept@outside.example" }, "toolarge", "images", "a 201st sender with other changes");
	g = get(alice);
	Kit.eq([g.signature_source, g.display_name, USERS[ALICE].netmail], ["none", "Alice Smith", ""],
		"nothing else written");
	Kit.assert(!file_exists(W.S.data_dir + "user/60001.mail.sig"), "no signature file");
	g = set(alice, { images_always: [] });
	Kit.eq(g.images_always, [], "emptied");
	err(call(env(alice), "settings.set", { images_always: "a@b.example" }), "invalid", "not a list");

	// --- argument checks
	err(call(env(alice), "settings.set", {}), "invalid", "no setting");
	err(call(env(alice), "settings.set", { colour: "red" }), "invalid", "an unknown setting");
	err(call(env(alice), "settings.set", { forward_enabled: "yes" }), "invalid", "forward_enabled not a boolean");
	err(call(env(alice), "settings.set", { display_name: 5 }), "invalid", "display_name not a string");

	// --- a background request and a request without "as" cannot write;
	// settings.get is a read
	var ev = env(alice);
	r = Api.dispatch(ev, { calls: [["settings.set", { theme: "light" }, 1], ["settings.get", {}, 2]],
		as: ALICE, background: true });
	err(r.results[0][1], "forbidden", "settings.set in a background request");
	Kit.eq(r.results[1][1].ok, true, "settings.get in a background request");
	r = Api.dispatch(env(alice), { calls: [["settings.set", { theme: "light" }, 1]] });
	err(r.results[0][1], "invalid", "settings.set without as");
	r = Api.dispatch(env(alice), { calls: [["settings.set", { theme: "light" }, 1]], as: BOB });
	err(r.results[0][1], "stale", "settings.set as another user");
	Kit.eq(get(alice).theme, "dark", "nothing changed");
	err(call(W.make_env(), "settings.get", {}), "auth", "signed out: settings.get");

	// --- another user's settings are their own
	g = get(bob);
	Kit.eq([g.display_name, g.theme, g.from_domain], ["", "system", ""], "Bob's are untouched");

	// --- signature: text, "" and null reach MailSig; settings.get reports
	// the source
	g = set(alice, { signature: "Alice\n-- \xc3\xa9" });
	Kit.eq(MailSig.get(ALICE, W.S.data_dir), { text: "Alice\n-- \xc3\xa9", source: "mail" }, "text: MailSig.set");
	Kit.eq([g.signature, g.signature_source], ["Alice\n-- \xc3\xa9", "mail"], "reported");
	g = set(alice, { signature: "" });
	Kit.eq([g.signature, g.signature_source], ["", "mail"], "\"\": no signature on mail");
	g = set(alice, { signature: null });
	Kit.eq(g.signature_source, "none", "null: the mail signature removed");
	Kit.assert(!file_exists(W.S.data_dir + "user/60001.mail.sig"), "file removed");
	refused(alice, { signature: new Array(2002).join("x") }, "invalid", "signature", "2001 characters");
	refused(alice, { signature: new Array(2001).join("\xc3\xa9") + "x" }, "invalid", "signature",
		"2001 two-byte characters");
	refused(alice, { signature: "bad \xc3" }, "invalid", "signature", "invalid UTF-8");
	g = set(alice, { signature: new Array(2001).join("\xc3\xa9") }, "2000 two-byte characters");
	Kit.eq(g.signature.length, 4000, "stored");
	var mailsig_src = new File(Kit.exec_root() + "load/mailsig.js");
	Kit.assert(mailsig_src.open("r"), "read mailsig.js");
	Kit.eq(/var MAX_LENGTH = (\d+);/.exec(mailsig_src.read())[1], "2000", "the API's limit is MailSig's");
	mailsig_src.close();
	set(alice, { signature: null });
	r = call(env(alice), "settings.set", { signature: new Array(2002).join("x"), theme: "light" });
	Kit.eq(get(alice).theme, "dark", "a refused signature writes nothing else");
	Kit.eq(get(alice).display_name, "Alice Smith", "nor the name");

	// --- forwarding: off at the system
	reset_sys();
	SYS.settings = 0;
	g = get(alice);
	Kit.eq(g.forward.allowed, false, "system switch off: not allowed");
	Kit.eq(g.forward.reason, "off", "reason off");
	refused(alice, { forward_enabled: true, forward_address: "alice@outside.example" }, "forbidden",
		"forward_off", "enabling with the switch off");
	refused(alice, { forward_address: "alice@outside.example" }, "forbidden", "forward_off", "an address");
	Kit.eq([USERS[ALICE].netmail, USERS[ALICE].settings & U.USER_NETMAIL], ["", 0], "nothing written");

	// --- forwarding: the user's forward-mail restriction
	reset_sys();
	USERS[ALICE].security.restrictions = U.UREST_FORWARD_MAIL;
	g = get(alice);
	Kit.eq([g.forward.allowed, g.forward.reason], [false, "restricted"], "restricted user");
	refused(alice, { forward_enabled: true }, "forbidden", "forward_restricted", "restricted user enabling");
	USERS[ALICE].security.restrictions = 0;

	// --- keeps_copy: MM_FWDKEEP undefined -> false; defined: its bit
	Kit.eq(DEFS.MM_FWDKEEP, undefined, "MM_FWDKEEP is not defined in this tree's sbbsdefs.js");
	SYS.msg_settings = -1;
	Kit.eq(get(alice).forward.keeps_copy, false, "undefined: false, whatever the settings");
	SYS.MM_FWDKEEP = 1 << 30;
	SYS.msg_settings = 1 << 30;
	Kit.eq(get(alice).forward.keeps_copy, true, "defined and set");
	SYS.msg_settings = 1 << 29;
	Kit.eq(get(alice).forward.keeps_copy, false, "defined, not set");
	reset_sys();

	// --- forwarding allowed: the address rules
	refused(alice, { forward_enabled: true }, "invalid", "forward_empty", "enabling with no address");
	refused(alice, { forward_address: "" }, "invalid", "forward_empty", "an empty address");
	refused(alice, { forward_enabled: true, forward_address: "me@vert.example" }, "invalid", "forward_own",
		"the system's own address");
	refused(alice, { forward_enabled: true, forward_address: "me@Family.Example" }, "invalid", "forward_own",
		"an additional domain, any case");
	var long_addr = new Array(51).join("a") + "@x.example";	// 50 + 10 = 60
	Kit.eq(long_addr.length, 60, "a 60-character address");
	refused(alice, { forward_address: "b" + long_addr }, "invalid", "forward_toolong", "61 characters");
	var bad_fwd = ["not an address", "Name <a@b.example>", "a@localhost", "a@b", "1:103/705", "a@b.example\r\nX",
		"a@[127.0.0.1]"];
	for (i = 0; i < bad_fwd.length; i++)
		refused(alice, { forward_enabled: true, forward_address: bad_fwd[i] }, "invalid", "forward_address",
			"forward to " + JSON.stringify(bad_fwd[i]));
	// turning forwarding on checks the STORED address by the same rules
	var stored_cases = [["me@vert.example", "forward_own"], ["not an address", "forward_address"],
		["spam@bad.example", "forward_blocked"], ["nope@unsupported.example", "forward_unsupported"]];
	SYS.trashed = ["spam@bad.example"];
	SYS.unsupported = ["nope@unsupported.example"];
	for (i = 0; i < stored_cases.length; i++) {
		USERS[ALICE].netmail = stored_cases[i][0];
		refused(alice, { forward_enabled: true }, "invalid", stored_cases[i][1],
			"turning on with a stored " + stored_cases[i][1]);
		Kit.eq(USERS[ALICE].settings & U.USER_NETMAIL, 0, "still off");
		set(alice, { forward_enabled: false }, "turning off needs no valid address");
	}
	refused(alice, { forward_address: "nope@unsupported.example" }, "invalid", "forward_unsupported",
		"a new address the system cannot send to");
	USERS[ALICE].netmail = "";
	refused(alice, { forward_address: "spam@bad.example" }, "invalid", "forward_blocked", "in email.can");
	SYS.newuser_questions = DEFS.UQ_DUPNETMAIL;
	SYS.matches = { "bob@outside.example": BOB, "mine@outside.example": ALICE };
	refused(alice, { forward_address: "bob@outside.example" }, "invalid", "forward_duplicate",
		"another user's address, duplicates not allowed");
	Kit.eq([USERS[ALICE].netmail, USERS[ALICE].settings & U.USER_NETMAIL], ["", 0], "nothing written yet");
	g = set(alice, { forward_enabled: true, forward_address: "mine@outside.example" }, "own address again");
	Kit.eq(USERS[ALICE].netmail, "mine@outside.example", "netmail written");
	Kit.assert(USERS[ALICE].settings & U.USER_NETMAIL, "USER_NETMAIL set");
	Kit.eq(g.forward, { allowed: true, enabled: true, address: "mine@outside.example", keeps_copy: false },
		"reported");
	SYS.newuser_questions = 0;
	g = set(alice, { forward_enabled: true, forward_address: long_addr }, "60 characters");
	Kit.eq(USERS[ALICE].netmail, long_addr, "a 60-character address stored");
	g = set(alice, { forward_address: "alice@outside.example" }, "a new address alone");
	Kit.eq(USERS[ALICE].netmail, "alice@outside.example", "address changed");
	Kit.assert(USERS[ALICE].settings & U.USER_NETMAIL, "still forwarding");
	g = set(alice, { forward_enabled: false }, "turn off");
	Kit.eq(USERS[ALICE].settings & U.USER_NETMAIL, 0, "USER_NETMAIL cleared");
	Kit.eq(USERS[ALICE].netmail, "alice@outside.example", "the address stays");
	Kit.eq([g.forward.enabled, g.forward.address], [false, "alice@outside.example"], "reported");
	g = set(alice, { forward_enabled: true }, "turn on again with the stored address");
	Kit.assert(USERS[ALICE].settings & U.USER_NETMAIL, "on again");
	Kit.eq(get(bob).forward.address, "bob@outside.example", "Bob's untouched");

	// --- a background request cannot change forwarding
	r = Api.dispatch(env(alice), { calls: [["settings.set", { forward_enabled: false }, 1]], as: ALICE,
		background: true });
	err(r.results[0][1], "forbidden", "forwarding in a background request");
	Kit.assert(USERS[ALICE].settings & U.USER_NETMAIL, "unchanged");
} finally {
	W.cleanup();
}
