// login.js

// Login module for Synchronet BBS v3.1

"use strict";

load("sbbsdefs.js");

var options;
if((options=load("modopts.js","login")) == null)
	options={email_passwords: true};
if(!options.login_prompts)
	options.login_prompts = 10;
if(!options.inactive_hangup)
	options.inactive_hangup = 30;	 // seconds
if(options.guest === undefined)
	options.guest = true;

if(bbs.sys_status & SS_USERON) {
	// Only required for "Re-login" capability
	bbs.logout();
}
var guest = options.guest && system.matchuser("guest");

if(!bbs.online)
	exit();
var inactive_hangup = parseInt(options.inactive_hangup, 10);
if(inactive_hangup && inactive_hangup < console.max_socket_inactivity
	&& !(console.autoterm&(USER_ANSI | USER_PETSCII | USER_UTF8))) {
	console.max_socket_inactivity = inactive_hangup;
	log(LOG_NOTICE, "terminal not detected, reducing inactivity hang-up timeout to " + console.max_socket_inactivity + " seconds");
}
if(console.max_socket_inactivity > 0 && bbs.node_num == bbs.last_node) {
	console.max_socket_inactivity /= 2;
	log(LOG_NOTICE, "last node login inactivity timeout reduced to " + console.max_socket_inactivity);
}

for(var c=0; c < options.login_prompts; c++) {

	system.node_list[bbs.node_num-1].status = NODE_LOGON;

	// The "node sync" is required for sysop interruption/chat/etc.
	bbs.nodesync();

	// Display login prompt
	const legacy_login_prompt = options.legacy_prompts ? "NN: \x01[" : "";
	const legacy_password_prompt = options.legacy_prompts ? "PW: \x01[" : "";
	var str = "\x01n\x01h\x01cEnter \x01wUser Name";
	if(system.login_settings & LOGIN_USERNUM)
		str += "\x01c or \x01wNumber";
	if(!(system.settings&SYS_CLOSED))
		str += "\x01c or '\x01yNew\x01c'";
	if(guest)
		str += "\x01c or '\x01yGuest\x01c'";
	str += "\r\nLogin: \x01w";
	console.print("\r\n"
		+ legacy_login_prompt
		+ word_wrap(str, console.screen_columns-1).trimRight());

	// Get login string
	var str;
	if(bbs.rlogin_name.length)
		print(str=bbs.rlogin_name);
	else
		str=console.getstr(/* maximum user name length: */ LEN_ALIAS
						 , /* getkey/str mode flags: */ K_UPRLWR | K_TAB | K_ANSI_CPR);
	truncsp(str);
	if(!str.length) // blank
		continue;

	// New user application?
	if(str.toUpperCase()=="NEW") {
		if(bbs.newuser()) {
			bbs.logon();
			exit();
		}
		if(typeof bbs.logline == "function")
			bbs.logline(LOG_NOTICE, "N-", "New user registration canceled");
		continue;
	}
	// Continue normal login (prompting for password)
	if(bbs.login(str, legacy_password_prompt + "\x01n\x01c\x01hPassword: \x01w")) {
		bbs.logon();
		exit();
	}
	if(system.trashcan("name", str)) {
		alert(log(LOG_NOTICE, "!Failed login with blocked user name: " + str));
		break;
	}
	console.clearkeybuffer();	// Clear pending input (e.g. mistyped system password)
	bbs.rlogin_name='';		// Clear user/login name (if supplied via protocol)
	var usernum = system.matchuser(str);
	if(usernum) {
		system.put_telegram(usernum,
			format("\x01n\x01h%s %s \x01rFailed login attempt\x01c\r\n" +
				"from %s [%s] via %s (TCP port %u)\x01n\r\n"
				,system.timestr(), system.zonestr()
				,client.host_name, client.ip_address
				,client.protocol, client.port));
		if(options && options.email_passwords) {
			var u = new User(usernum);
			if(!(u.settings&(USER_DELETED|USER_INACTIVE))
				&& !u.is_sysop
				&& u.security.password
				&& netaddr_type(u.netmail) == NET_INTERNET
				&& !console.noyes("Email your password to you")) {
				var email_addr = u.netmail;
				if(options.confirm_email_address !== false) {
					console.print("\x01n\x01c\x01hPlease confirm your Internet e-mail address: \x01y");
					var email_addr = console.getstr(LEN_NETMAIL);
				}
				if(email_addr.toLowerCase() == u.netmail.toLowerCase()) {

					var msgbase = new MsgBase("mail");
					if(msgbase.open()==false)
						alert(log(LOG_ERR,"!ERROR " + msgbase.last_error));
					else {
						var hdr = { to: u.alias,
									to_net_addr: u.netmail, 
									to_net_type: NET_INTERNET,
									from: system.operator, 
									from_ext: "1", 
									subject: system.name + " user account information"
						};

						var msgtxt = "Your user account information was requested on " + system.timestr() + "\r\n";
						msgtxt += "by " + client.host_name + " [" + client.ip_address +"] via " + 
							client.protocol + " (TCP port " + client.port + "):\r\n\r\n";

						msgtxt += "Account Number: " + u.number + "\r\n";
						msgtxt += "Account Created: " + system.timestr(u.stats.firston_date) + "\r\n";
						msgtxt += "Last Login: " + system.timestr(u.stats.laston_date) + "\r\n";
						msgtxt += "Last Login From: " + u.host_name + " [" + u.ip_address + "]" +
										" via " + u.connection + "\r\n";
						msgtxt += "Password: " + u.security.password + "\r\n";
						msgtxt += "Password Last Modified: " + system.datestr(u.security.password_date) + "\r\n";

						if(msgbase.save_msg(hdr, msgtxt)) {
							console.print("\r\n\x01n\x01h\x01mAccount Information Emailed Successfully\r\n");
							system.put_telegram(usernum, 
								format("\x01n\x01h%s %s \x01rEmailed account info\x01c\r\nto \x01w%s\x01n\r\n"
									,system.timestr(), system.zonestr()
									,u.netmail));
							log(LOG_NOTICE, "Account information (i.e. password) e-mailed to: " + u.netmail);
						} else
							alert(log(LOG_ERR,"!ERROR " + msgbase.last_error + "saving bulkmail message"));

						msgbase.close();
					}
					continue;
				}
				alert(log(LOG_WARNING,"Incorrect e-mail address: " + email_addr));
			}
		}
	}
	// Password failure counts as 2 attempts
	c++;
}

// Login failure
bbs.hangup();
