// Basic FormMail module

// $Id$

// Requires query values: to, from, and subject
// Message body text is contained in the post data

load("sbbsdefs.js");

if(http_request.query.to==undefined
    || http_request.query.from==undefined
    || http_request.query.subject==undefined) {
	writeln(log(LOG_ERR,"!ERROR: mising query field"));
	exit();
}

if(http_request.post_data==undefined) {
	writeln(log(LOG_ERR,"!ERROR: mising post data"));
	exit();
}

var msgbase=new MsgBase("mail");
if(!msgbase.open()) {
	writeln(log(LOG_ERR,format("!ERROR %s opening mail base", msgbase.error)));
	exit();
}

var hdr = { from: http_request.query.from,
			to: http_request.query.to,
			subject: http_request.query.subject };

hdr.to_net_type=netaddr_type(hdr.to);
if(hdr.to_net_type!=NET_NONE)
	hdr.to_net_addr=hdr.to;
else {
	var usrnum=system.matchuser(hdr.to);
	if(usrnum==0) {
		writeln(log(LOG_ERR,"!Bad local username specified: " + hdr.to));
		exit();
	}
	hdr.to_ext=usrnum;
}


if(!msgbase.save_msg(hdr,client,http_request.post_data))
	writeln(log(LOG_ERR,format("!ERROR %s saving message", msgbase.error)));
else
	writeln(log(LOG_INFO,"E-mail sent to " + hdr.to + " successfully"));

msgbase.close();
