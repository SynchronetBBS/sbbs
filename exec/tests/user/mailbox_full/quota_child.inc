// Run by quota.js in a child jsexec whose ctrl directory is a scratch copy
// with its own (new, empty) user base and mail base; writes
// system.mailbox_quota, system.max_msgs_waiting and what was built and
// answered for every user as JSON to argv[0].  The quota must be QUOTA
// (below) or 0: the mailboxes are built to exact sizes around QUOTA.
// Not a test on its own (the runner only runs *.js files).

load("sbbsdefs.js");
load("userdefs.js");
load("smbdefs.js");

var QUOTA = 16384;
var BLOCK = 256;

// Users and mail are created below: refuse to run unless the data
// directory is inside the scratch tree that holds the ctrl directory
(function () {
	var root = fullpath(system.ctrl_dir + "..");
	if (fullpath(system.data_dir).indexOf(root) != 0 || root.length < 8)
		throw new Error("data_dir " + system.data_dir + " is not under " + root);
})();

var result = { quota: system.mailbox_quota, limit: system.max_msgs_waiting, users: {} };
var mb = new MsgBase("mail");
if (!mb.open())
	throw new Error("open mail: " + mb.last_error);

function blocks(len)
{
	return Math.ceil(len / BLOCK) * BLOCK;
}

// A message's size as the mailbox library counts it: its header and its
// data in whole blocks
function stored(num)
{
	var h = mb.get_msg_header(false, num, false);
	var lines = mb.dump_msg_header(h);
	for (var i = 0; i < lines.length; i++) {
		var m = lines[i].match(/^header_length\s+(\d+)/);
		if (m)
			return blocks(parseInt(m[1], 10)) + blocks(h.data_length);
	}
	throw new Error("no header_length for #" + num);
}

function save(u, body_len, attr)
{
	var body = "";
	while (body.length < body_len)
		body += "x";
	var hdr = { to: u.alias, to_ext: String(u.number), from: "Test", subject: "quota", attr: attr || 0 };
	if (!mb.save_msg(hdr, body))
		throw new Error("save_msg: " + mb.last_error);
	return mb.last_msg;
}

// Mail for u holding exactly 'bytes' (a multiple of 256, at least 1536
// per message), in 'count' messages (default 2); attrs[i] is the i-th
// message's attributes.  Returns the message numbers.
function fill(u, bytes, count, attrs)
{
	count = count || 2;
	attrs = attrs || [];
	var nums = [];
	var total = 0;
	for (var i = 0; i < count - 1; i++) {
		nums.push(save(u, BLOCK / 2, attrs[i]));
		total += stored(nums[i]);
	}
	// The last one's body ends in the middle of a block, so the few bytes
	// the base adds to a body cannot change the number of blocks
	var first = stored(nums[0]);
	var data = bytes - total - (first - BLOCK);
	if (count < 2 || data < BLOCK || data % BLOCK)
		throw new Error("fill: cannot make " + bytes + " bytes of " + count + " messages");
	nums.push(save(u, data - BLOCK / 2, attrs[count - 1]));
	total += stored(nums[count - 1]);
	if (total != bytes)
		throw new Error("fill: made " + total + " bytes, not " + bytes);
	return nums;
}

function user_dir(u)
{
	var dir = system.data_dir + format("user/%04u/", u.number);
	mkpath(dir);
	return dir;
}

function write(path, text)
{
	var f = new File(path);
	if (!f.open("wb") || (text.length && !f.write(text)))
		throw new Error("Error " + f.error + " writing " + path);
	f.close();
}

function filler(len)
{
	var s = "";
	while (s.length < len)
		s += ".";
	return s;
}

// build(u) makes the user's mail and returns the bytes the quota is to
// count for the user (not the bytes of files only standing in for a base)
function make_user(key, build, exemptions)
{
	var u = system.new_user("Quotafull" + String.fromCharCode(97 + Object.keys(result.users).length));
	if (typeof u != "object")
		throw new Error("new_user(" + key + ") returned " + u);
	if (exemptions)
		u.security.exemptions |= exemptions;
	var bytes = build(u);
	u = new User(u.number);
	result.users[key] = {
		bytes: bytes,
		waiting: u.stats.mail_waiting,
		full: u.mailbox_full(),
		full_false: u.mailbox_full(false),
		full_system: u.mailbox_full(true)
	};
	return u;
}

function inbox(bytes, count, attrs)
{
	return function (u) {
		fill(u, bytes, count, attrs);
		return bytes;
	};
}

make_user("none", function () { return 0; });
make_user("under", inbox(QUOTA - BLOCK));
make_user("at", inbox(QUOTA));
make_user("over", inbox(QUOTA + BLOCK));
make_user("over_w", inbox(QUOTA + BLOCK), UFLAG_W);
make_user("spam", inbox(QUOTA, 2, [0, MSG_SPAM]));
make_user("deleted", function (u) {
	var nums = fill(u, QUOTA, 3);
	var gone = stored(nums[0]);
	if (!mb.remove_msg(nums[0]))
		throw new Error("remove_msg: " + mb.last_error);
	return QUOTA - gone;
});
// Three messages: over a Max Messages Waiting of 2
make_user("three_under", inbox(QUOTA - BLOCK, 3));
make_user("three_at", inbox(QUOTA, 3));

// A private base (the files stand in for one) whose size the mailbox
// library recorded
make_user("ini_at", function (u) {
	fill(u, QUOTA - 1024);
	write(user_dir(u) + "mailbox.shd", filler(100000));	// not used: the recorded figure is
	write(user_dir(u) + "mailbox.usage.ini", "bytes=1024\n");
	return QUOTA;
});
make_user("ini_under", function (u) {
	fill(u, QUOTA - 1024);
	write(user_dir(u) + "mailbox.shd", filler(100000));
	write(user_dir(u) + "mailbox.usage.ini", "bytes=1023\n");
	return QUOTA - 1;
});
make_user("ini_alone", function (u) {
	write(user_dir(u) + "mailbox.shd", filler(32));
	write(user_dir(u) + "mailbox.usage.ini", "bytes=" + QUOTA + "\n");
	return QUOTA;
});
// A recorded figure without a base counts nothing
make_user("ini_no_base", function (u) {
	fill(u, QUOTA - BLOCK);
	write(user_dir(u) + "mailbox.usage.ini", "bytes=" + QUOTA + "\n");
	return QUOTA - BLOCK;
});
// No recorded figure: the sizes of the base's header and data files
make_user("files_at", function (u) {
	fill(u, QUOTA - 1024);
	write(user_dir(u) + "mailbox.shd", filler(512));
	write(user_dir(u) + "mailbox.sdt", filler(512));
	return QUOTA;
});
make_user("files_under", function (u) {
	fill(u, QUOTA - 1024);
	write(user_dir(u) + "mailbox.shd", filler(512));
	write(user_dir(u) + "mailbox.sdt", filler(511));
	return QUOTA - 1;
});
make_user("files_no_key", function (u) {
	fill(u, QUOTA - 1024);
	write(user_dir(u) + "mailbox.shd", filler(1024));
	write(user_dir(u) + "mailbox.usage.ini", "count=1\n");
	return QUOTA;
});

mb.close();

var f = new File(argv[0]);
if (!f.open("w"))
	throw new Error("Error " + f.error + " creating " + f.name);
f.write(JSON.stringify(result));
f.close();
