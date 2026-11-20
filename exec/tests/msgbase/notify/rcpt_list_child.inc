// Run by rcpt_list.js in a child jsexec whose ctrl directory is a scratch
// copy and whose data directory is a new, empty scratch tree: creates two
// users and saves mail to the scratch "mail" base (and to a mail base
// opened by path) with and without a recipient list, recording after each
// save the short messages each user got and whether the netmail semaphore
// was touched. Writes the result as JSON to argv[0]. Not a test on its
// own (the runner only runs *.js files).

load("sbbsdefs.js");

var result = { data_dir: system.data_dir, cases: {} };

// Everything below writes to the data directory: refuse to run unless it
// is the scratch tree next to this ctrl directory
(function () {
	var root = fullpath(system.ctrl_dir + "..");
	if (fullpath(system.data_dir).indexOf(root) != 0 || root.length < 8)
		throw new Error("data_dir " + system.data_dir + " is not under " + root);
})();

function make_user(name)
{
	var u = system.new_user(name);
	if (typeof u != "object")
		throw new Error("new_user(" + name + ") returned " + u);
	return u;
}

function sem_path()
{
	var f = new File(system.ctrl_dir + "msgs.ini");
	if (!f.open("r"))
		throw new Error("Error " + f.error + " opening " + f.name);
	var v = f.iniGetValue("FidoNet", "netmail_sem", "");
	f.close();
	return v.replace(/^%j/, system.data_dir);
}

var mb = null;
var pmb = null;
var path_base = system.temp_dir + "notify_path_mail";

try {
	var sender = make_user("Notifysender");
	var rcpt = make_user("Notifyrcpt");
	var sem = sem_path();
	result.sem = sem;
	var S = { to: sender.alias, to_ext: String(sender.number) };
	var R = { to: rcpt.alias, to_ext: String(rcpt.number) };
	var F = { to: "Fido Person", to_net_type: NET_FIDO, to_net_addr: "1:2/3" };

	mb = new MsgBase("mail");
	if (!mb.open())
		throw new Error("open mail: " + mb.last_error);
	result.mail_file = mb.file;

	// Clear the notices and the semaphore, save, and record what the save did
	var run = function (name, base, hdr, list) {
		system.get_telegram(sender.number);
		system.get_telegram(rcpt.number);
		file_remove(sem);
		var before = base.total_msgs;
		hdr.from = sender.alias;
		hdr.from_ext = String(sender.number);
		hdr.subject = name;
		var saved = list ? base.save_msg(hdr, "body of " + name, list) : base.save_msg(hdr, "body of " + name);
		result.cases[name] = {
			saved: saved,
			error: saved ? undefined : base.last_error,
			copies: base.total_msgs - before,
			rcpt: system.get_telegram(rcpt.number),
			sender: system.get_telegram(sender.number),
			sem: file_exists(sem)
		};
	};

	run("list", mb, {}, [R, S, F]);
	run("anon_list", mb, { attr: MSG_ANONYMOUS }, [R, S, F]);
	run("local_list", mb, {}, [R]);
	run("single", mb, { to: R.to, to_ext: R.to_ext }, null);
	run("single_self", mb, { to: S.to, to_ext: S.to_ext }, null);
	run("single_fido", mb, { to: F.to, to_net_type: F.to_net_type, to_net_addr: F.to_net_addr }, null);
	run("single_anon", mb, { to: R.to, to_ext: R.to_ext, attr: MSG_ANONYMOUS }, null);

	pmb = new MsgBase(path_base, /* is_path: */ true, /* is_email: */ true);
	if (!pmb.open())
		throw new Error("open " + path_base + ": " + pmb.last_error);
	run("path_list", pmb, {}, [R]);
	run("path_single", pmb, { to: R.to, to_ext: R.to_ext }, null);

	// A sample of the notice text, for the parent to compare against
	system.get_telegram(rcpt.number);
	if (!mb.save_msg({ to: R.to, to_ext: R.to_ext, from: sender.alias, from_ext: String(sender.number),
		subject: "sample" }, "x"))
		throw new Error("save_msg sample: " + mb.last_error);
	result.notice = system.get_telegram(rcpt.number);
} catch (e) {
	result.exception = String(e) + (e && e.fileName ? " " + e.fileName + ":" + e.lineNumber : "");
} finally {
	if (mb !== null)
		mb.close();
	if (pmb !== null)
		pmb.close();
	[".shd", ".sdt", ".sid", ".sha", ".sda", ".ini", ".hash"].forEach(function (ext) {
		file_remove(path_base + ext);
	});
}

var f = new File(argv[0]);
if (!f.open("w"))
	throw new Error("Error " + f.error + " creating " + f.name);
f.write(JSON.stringify(result));
f.close();
