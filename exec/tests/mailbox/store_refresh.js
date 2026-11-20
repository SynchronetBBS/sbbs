// Store.refresh(): only the shared handle Store.open() made is used
// without reopening, and only by the first call.  A handle reopened by an
// earlier call and not used since holds the header file's bytes as of
// that reopen: after another handle rewrites a header in place, the next
// call must see the new header, and a flag change must not write the old
// fields back.

load(js.exec_dir + "kit.js");
var Store = Kit.lib("mailbox");
var D = {};
load(D, Kit.exec_root() + "load/smbdefs.js");

var ALICE = 60001;
var S = Kit.scratch();
var shared = null;
var st = null;

function to_alice(subject)
{
	var hdr = { to_ext: String(ALICE), to: "Alice", from: "Sender", subject: subject };
	if (!shared.save_msg(hdr, "body\r\n"))
		throw new Error("save_msg: " + shared.last_error);
	return shared.last_msg;
}

// Another handle (as the mail server or another session) sets bits in a
// header's netattr, in place
function other_sets_netattr(num, bits)
{
	var mb = Kit.email_base(S.shared_path);
	try {
		var h = mb.get_msg_header(false, num, false);
		h.netattr |= bits;
		if (!mb.put_msg_header(false, num, h))
			throw new Error("put_msg_header: " + mb.last_error);
	} finally {
		mb.close();
	}
}

function fresh_header(num)
{
	var mb = Kit.email_base(S.shared_path);
	try {
		return mb.get_msg_header(false, num, false);
	} finally {
		mb.close();
	}
}

try {
	shared = Kit.email_base(S.shared_path);
	var M1 = to_alice("first");
	var M2 = to_alice("second");
	shared.close();
	shared = null;

	st = new Store({ usernum: ALICE, alias: "alice", name: "Alice",
		shared_path: S.shared_path, data_dir: S.data_dir });
	st.open();
	st.list("inbox");			// call 1: the handle open() made, used
	st.refresh();				// call 2 starts: reopened, not used yet

	// call 3: must reopen, so it reads the header as it is now
	other_sets_netattr(M1, D.NETMSG_INTRANSIT);
	st.refresh();
	var h = st.shared_base().get_msg_header(false, M1, false);
	Kit.eq(h.netattr & D.NETMSG_INTRANSIT, D.NETMSG_INTRANSIT, "a later call sees the rewritten header");

	// a flag change after a call that left the handle unused does not
	// write the old netattr back
	st.refresh();
	other_sets_netattr(M2, D.NETMSG_SENT);
	var r = st.set(["s" + M2], { starred: true });
	Kit.assert(r instanceof Array && r[0].ok === true, "starred: " + JSON.stringify(r));
	var f = fresh_header(M2);
	Kit.eq(f.netattr & D.NETMSG_SENT, D.NETMSG_SENT, "the other handle's change kept");
	Kit.eq(f.attr & D.MSG_VALIDATED, D.MSG_VALIDATED, "the star written");
} finally {
	if (st)
		st.close();
	if (shared)
		shared.close();
	S.cleanup();
}
