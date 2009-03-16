/* $Id$ */

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

if(sender_host == system.host_name
	|| system.findstr(system.ctrl_dir + "domains.cfg", sender_host)) {
	var error_file = new File(processing_error_filename);
	if(!error_file.open("w")) {
		log(LOG_ERR,format("!ERROR %d opening processing error file: %s"
			,error_file.error, processing_error_filename));
	} else {
		error_file.writeln("Mail deliveries from " + sender_host + " must use SMTP-AUTH");
		error_file.close();
	}
}
