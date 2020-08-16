// $Id: birthdays.js,v 1.1 2018/06/06 23:07:20 rswindell Exp $

// Find users with the specified birthday (or birthmonth)

// Usage (Birthdays in January):
// list = load({}, "birthdays.js", 0);

// Usage (Birthdays this month):
// list = load({}, "birthdays.js", new Date().getMonth());

// Usage (Birthdays today):
// list = load({}, "birthdays.js", new Date().getMonth(), new Date().getDate());

load("sbbsdefs.js");
load("birthdate.js");

// Returns an array of user numbers
// Note: month is 0-based, day (of month) is optional and 1-based
function birthdays(month, day)
{
	var u = new User(1);
	var lastuser = system.stats.total_users;
	var list = [];
	for(u.number = 1; u.number <= lastuser; u.number++) {
		if(u.settings&(USER_DELETED|USER_INACTIVE))
			continue;
		if(u.security.restrictions&(UFLAG_Q|UFLAG_G))
			continue;
		if(u.birthDate.getMonth() != month)
			continue;
		if(day && u.birthDate.getDate() != day)
			continue;
		list.push(u.number);
	}
	return list;
}

birthdays(argv[0], argv[1]);
