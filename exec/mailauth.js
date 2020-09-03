/* $Id: mailauth.js,v 1.6 2019/07/07 21:07:44 rswindell Exp $ */

// This script is an external mail processor that verifies that mail received
// from sender's claiming to be <anyone>@<yourdomains> are SMTP-authenticated
// using a non-Guest user account

// Configured in ctrl/mailproc.ini as follows:

// [mailauth.js]
// AccessRequirements=user equal 0 or guest

load("sbbsdefs.js");

// Do nothing if executed with an authenticated non-Guest user account
if(user.number && !(user.security.restrictions&UFLAG_G))
	exit();	

var sender_host = sender_address.slice(sender_address.indexOf('@')+1);

if((sender_host == system.host_name
		|| system.findstr(system.ctrl_dir + "domains.cfg", sender_host))
		&& client.ip_address!="127.0.0.1"
		&& client.ip_address.substring(0, 7) != "192.168."
		&& client.ip_address!=resolve_ip(sender_host)) {
	var i;
	var matched = false;
	for (i in server.interface_ip_addr_list) {
		if (client.ip_address == server.interface_ip_addr_list[i])
			matched = true;
	}
	if (!matched) {
		var error_file = new File(processing_error_filename);
		if(!error_file.open("w")) {
			log(LOG_ERR,format("!ERROR %d opening processing error file: %s"
				,error_file.error, processing_error_filename));
		} else {
			error_file.writeln("Mail deliveries from " + sender_host + " must use SMTP-AUTH");
			log("sender_address = " + sender_address);
			error_file.close();
		}
	}
}
