// $Id$
/*

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details:
 http://www.gnu.org/licenses/gpl.txt

 Copyright 2010 Randolph E. Sommerfeld <sysop@rrx.ca>

*/

/********** Command Processors. **********/
this.Server_command=function(srv,cmdline,onick,ouh) {
	var cmd=IRC_parsecommand(cmdline);
	switch (cmd[0]) {
		case "PRIVMSG":
			if(srv.users[onick.toUpperCase()]) srv.users[onick.toUpperCase()].last_spoke=time();
		default:
			break;
	}
}

