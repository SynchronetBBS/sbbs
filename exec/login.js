// login.js

// Login module for Synchronet BBS v3.1

// $Id$

load("sbbsdefs.js");

// The following 2 lines are only required for "Re-login" capability
bbs.logout();
system.node_list[bbs.node_num-1].status = NODE_LOGON;

var guest = system.matchuser("guest");

for(var c=0; c<10; c++) {

	// The "node sync" is required for sysop interruption/chat/etc.
	bbs.nodesync();

	// Display login prompt
	console.print("\r\n\1n\1h\1cEnter User Name");
	if(!(bbs.node_settings&NM_NO_NUM))
		console.print(" or Number");
	if(!(system.settings&SYS_CLOSED))
		console.print(" or '\1yNew\1c'");
	if(guest)
		console.print(" or '\1yGuest\1c'");
	console.print("\r\nNN:\b\b\bLogin: \1w");

	// Get login string
	var str=console.getstr(/* maximum user name length: */ 25
						 , /* getkey/str mode flags: */ K_UPRLWR | K_TAB);
	truncsp(str);
	if(!str.length) // blank
		continue;

	// New user application?
	if(str.toUpperCase()=="NEW") {
	   if(bbs.newuser()) {
		   bbs.logon();
		   exit();
	   }
	   continue;
	}
	// Continue normal login (prompting for password)
	if(bbs.login(str, "\1n\1c\1hPW:\b\b\bPassword: \1w")) {
		bbs.logon();
		exit();
	}
	// Password failure counts as 2 attempts
	c++;
}

// Login failure
bbs.hangup();
