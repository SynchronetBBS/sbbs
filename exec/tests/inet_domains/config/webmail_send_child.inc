// Run by webmail_send.js in a child jsexec whose ctrl directory is a
// scratch copy with a [domain:] section and whose data directory is a new,
// empty scratch tree: sends through the web mail client's Send with a real
// User object, the real Address.domains_for() and system resolvers, and
// the real system values (no fakes).  Writes what it observes as JSON to
// argv[0].  Not a test on its own (the runner only runs *.js files).

load("sbbsdefs.js");

var result = { dirs: { ctrl: system.ctrl_dir, data: system.data_dir, temp: system.temp_dir } };

(function () {
	var root = fullpath(system.ctrl_dir + "..");
	if (fullpath(system.data_dir).indexOf(root) != 0 || root.length < 8)
		throw new Error("data_dir " + system.data_dir + " is not under " + root);
})();

// The worktree's root: this file is in <root>/exec/tests/inet_domains/config/
var ROOT = js.exec_dir + "../../../../";
function lib(path)
{
	return load({}, ROOT + path);
}
var Send = lib("webmail/lib/send.js");
var Drafts = lib("webmail/lib/drafts.js");
var Config = lib("webmail/lib/config.js");
var UserData = lib("webmail/lib/userdata.js");
var Store = lib("exec/load/mailbox.js");
var logged = [];
Send.log = Drafts.log = function (level, text) {
	logged.push(text);
};

function make_user(name, level, restrictions)
{
	var u = system.new_user(name);
	if (typeof u != "object")
		throw new Error("new_user(" + name + ") returned " + u);
	u.security.level = level;
	if (restrictions)
		u.security.restrictions |= restrictions;
	return u.number;
}

var shared_path = system.temp_dir + "webmail_shared_mail";
var stores = [];

function context(usernum)
{
	var u = new User(usernum);
	var store = new Store({ usernum: usernum, alias: u.alias, name: u.name, shared_path: shared_path,
		data_dir: system.data_dir });
	store.open();
	stores.push(store);
	Config.warn = function () {};
	return { usernum: usernum, alias: u.alias, name: u.name, user: u, store: store,
		userdata: new UserData(system.data_dir + format("user/%04u/webmail/", usernum)),
		config: Config.load({}) };
}

function draft(ctx, fields)
{
	var d = Drafts.create(ctx, { mode: "new" });
	fields.rev = d.rev;
	return Drafts.save(ctx, d.id, fields).id;
}

function attempt(fn)
{
	try {
		return fn();
	} catch (e) {
		return { error: e && e.code ? e.code : String(e), reason: e && e.reason };
	}
}

try {
	var sender = make_user("Webmail Sender", 50);
	var rcpt = make_user("Webmailrcpt", 50);
	var nonet = make_user("Webmail Nonet", 50, UREST_SEND_NETMAIL);
	result.users = { sender: sender, rcpt: rcpt, nonet: nonet };
	var A = context(sender);
	result.email = A.user.email;
	result.domains = lib("webmail/lib/address.js").domains_for(sender);
	// A From domain the user may not use is refused when the draft is saved
	result.bad_domain = attempt(function () {
		return draft(A, { to: "ext@example.org", body: "x", from_domain: "plain-off.example" });
	});
	var id = draft(A, { to: "Ext Person <ext@example.org>", cc: "Webmailrcpt", subject: "real",
		body: "hello", from_domain: "plain-on.example" });
	result.sent = attempt(function () {
		return Send.send(A, id, "realuser-key-0001");
	});
	result.copies = [];
	var mb = new MsgBase(shared_path, true, true);
	if (mb.open()) {
		var idx = mb.get_index() || [];
		for (var i = 0; i < idx.length; i++) {
			var h = mb.get_msg_header(false, idx[i].number);
			result.copies.push({ to: h.to, to_ext: h.to_ext, to_net_addr: h.to_net_addr, from: h.from,
				from_ext: h.from_ext, from_net_addr: h.from_net_addr, reverse_path: h.reverse_path,
				cc_list: h.cc_list, to_list: h.to_list, id: h.id });
		}
		mb.close();
	}
	result.emails_today = new User(sender).stats.email_today;
	var N = context(nonet);
	result.nonet = attempt(function () {
		return Send.send(N, draft(N, { to: "ext@example.org", body: "x" }), "realuser-key-0002");
	});
	result.nonet_local = attempt(function () {
		return Send.send(N, draft(N, { to: "Webmailrcpt", body: "x" }), "realuser-key-0003");
	});
} catch (e) {
	result.exception = String(e) + (e && e.fileName ? " " + e.fileName + ":" + e.lineNumber : "");
} finally {
	for (var si = 0; si < stores.length; si++)
		stores[si].close();
}
result.logged = logged;

var f = new File(argv[0]);
if (!f.open("w"))
	throw new Error("Error " + f.error + " creating " + f.name);
f.write(JSON.stringify(result));
f.close();
