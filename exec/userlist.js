// userlist.js

// A sample user listing script for Synchronet v3.1+

// $Id: userlist.js,v 1.6 2019/01/11 09:37:25 rswindell Exp $

"use strict";

require("sbbsdefs.js", 'USER_DELETED');

var lastuser = system.lastuser;
var u = new User;

for(var i = 1; i <= lastuser; i++) {
	u.number = i;
	if(u.settings&USER_DELETED)
		continue;
	printf("%d/%d ",i,lastuser);
	printf("%-30s %-30s %s\r\n"
		,u.alias
		,u.location
		,u.connection
		);
	if(js.global.console != undefined && console.aborted == true)
		break;
}