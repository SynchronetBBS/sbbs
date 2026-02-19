// Inactive user email warning sender

const module = "inactive_user_email";

if (system.autodel < 1)
	exit();

var options = load({}, "modopts.js", module);
if (!options)
	options = {};

require("userdefs.js", "UFLAG_P");
require("sbbsdefs.js", "NET_INTERNET");

var userprops = load({}, "userprops.js");

function seconds(days) { return days * 24 * 60 * 60; }

var threshold = (argv[0] || options.threshold || 30);
var frequency = (argv[1] || options.frequency || 10);
if (threshold < system.autodel)
	threshold = system.autodel - threshold;
var lastuser = system.lastuser;
var total = 0;
var sent = 0;
var now = time();
for (var u = new User(1); u.number <= lastuser; ++u.number) {
	if (u.settings & (USER_DELETED | USER_INACTIVE))
		continue;
	if (u.security.exemptions & UFLAG_P)
		continue;
	if (u.security.restrictions & (UFLAG_G | UFLAG_Q))
		continue;
	if (u.stats.laston_date > now - seconds(threshold))
		continue;
	log(format("User #%u (%s) has not logged-in since %s"
		,u.number, u.alias, system.datestr(u.stats.laston_date)));
	if (!system.check_netmail_addr(u.netmail)) {
		log("Unsupported netmail address: " + u.netmail);
		continue;
	}
	var d = userprops.get(module, "sent", new Date(0), u.number);
	var t = d.getTime();
	if (t != NaN && (t / 1000) > now - seconds(frequency)) {
		log("email sent within the past " + frequency + " days");
		continue;
	}
	log(format("Sending inactivity warning email to user #%u (%s) at %s"
		, u.number, u.alias, u.netmail));
	++total;
	if (send_warning_msg(system.text_dir + "inactive.msg", u)) {
		userprops.set(module, "sent", new Date(), u.number);
		++sent;
	}
}

log(format("%u of %u emails sent successfully", sent, total));

function send_warning_msg(fname, user)
{
	file = new File(fname);
	if(!file.open("rt")) {
		log(LOG_ERR,"!ERROR " + errno_str + " opening " + fname);
		return(false);
	}
	msgtxt = lfexpand(file.read(file.length));
	file.close();
	delete file;

	msgbase = new MsgBase("mail");
	if(msgbase.open()==false) {
		log(LOG_ERR,"!ERROR " + msgbase.error);
		return(false);
	}

	hdr = { 
		to: user.name, 
		to_net_addr: u.netmail,
		from: system.operator,
		replyto_net_addr: User(1).email,
		netattr: NETMSG_KILLSENT,
		subject: system.name + " inactivity warning for user: " + u.alias
	};

	msgtxt = msgtxt.replace(/@(\S+)@/g, function (code) { return eval(code.slice(1, -1)); });
	var result = msgbase.save_msg(hdr, msgtxt);
	if(!result)
		log(LOG_ERR, "!ERROR " + msgbase.error + " saving mail message");

	msgbase.close();
	return result;
}
