// userlist.js

// A user listing module for Synchronet v3.1+

"use strict";

require("sbbsdefs.js", 'USER_DELETED');
require("text.js", 'UserListFmt');

var lastuser = system.lastuser;
var u = new User;
var list = [];
var mode = Number(argv[0]);

function aborted()
{
	return js.global.console != undefined && console.aborted == true;
}

function print_user(u)
{
	var name = format("%s #%u", u.alias, u.number);
	console.print(format(bbs.text(UserListFmt)
		,name
		,system.settings & SYS_LISTLOC ? u.location : u.note
		,system.datestr(u.stats.laston_date)
		,u.connection
	));
}

console.mnemonics("Sort by User ~Name, ~Last-on Date or [#]: ");
var sort = console.getkeys("NL#", 0);
if(sort != 'N' && sort != 'L')
	sort = false;
else
	console.putmsg(bbs.text(CheckingSlots));

var users = 0;
for(var i = 1; i <= lastuser; i++) {
	if(aborted())
		exit(0);
	if(sort)
		console.print(format("%-4d\b\b\b\b", i));
	u.number = i;
	if(u.settings & (USER_DELETED | USER_INACTIVE))
		continue;
	++users;
	switch(mode) {
		case UL_SUB:
			if(typeof u.can_access_sub == 'function' && !u.can_access_sub(bbs.cursub_code))
				continue;
			break;
		case UL_DIR:
			if(typeof u.can_access_dir == 'function' && !u.can_access_dir(bbs.curdir_code))
				continue;
			break;
	}
	list.push( { number: u.number
		, alias: u.alias
		, location: u.location
		, note: u.note
		, connection: u.connection
		, stats: { laston_date: u.stats.laston_date }
		});
	if(!sort)
		print_user(u);
}

if(!sort)
	console.crlf();
console.print(format(bbs.text(NTotalUsers), users));
if(mode == UL_SUB)
	console.print(format(bbs.text(NUsersOnCurSub), list.length));
else if(mode == UL_DIR)
	console.print(format(bbs.text(NUsersOnCurDir), list.length));

if(!sort)
	exit(0);

switch(sort) {
	case 'L':
		list.sort(function(a, b) { return b.stats.laston_date - a.stats.laston_date; });
		break;
	case 'N':
		list.sort(function(a, b) { if(a.alias.toLowerCase() > b.alias.toLowerCase()) return 1; return -1; });
		break;
}

for(var i in list) {
	if(aborted())
		break;
	print_user(list[i]);
}

