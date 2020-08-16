// $Id: badpasswords.js,v 1.3 2019/01/11 09:38:12 rswindell Exp $

"use strict";
require("sbbsdefs.js", 'USER_DELETED');

var badpasswords=0;
var usr = new User;
var lastuser = system.lastuser;
for(var u=1; u <= lastuser; u++) {
	usr.number = u;
	if(usr == null)
		continue;
	if(usr.settings&(USER_DELETED|USER_INACTIVE))
	    continue;
	if(!system.trashcan("password",usr.security.password))
		continue;
	printf("%-25s: %s\r\n", usr.alias, usr.security.password);
	badpasswords++;
}

printf("%u bad passwords found\n", badpasswords);