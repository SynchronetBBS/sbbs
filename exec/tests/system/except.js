// Test system function exceptions

// Value (e.g. 0, 1) is number of non-null/undefined args required
var func_list = {
	'alias': 1,
	'check_filename': 0,
	'check_name': 0,
	'check_pid': 1,
	'check_syspass': 0,
	'datestr': 1,
	'del_user': 1,
	'exec': 1,
	'filter_ip': 1,
	'find_login_id': 0,
	'findstr': 2,
	'get_node': 1,
	'get_node_message': 1,
	'get_telegram': 1,
	'hacklog': 1,
	'illegal_filename': 0,
	'matchuser': 0,
	'matchuserdata': 2,
	'new_user': 1,
	'notify': 2,
	'popen': 1,
	'put_node_message': 2,
	'put_telegram': 2,
	'safest_filename': 0,
	'secondstr': 1,
	'spamlog': 1,
	'terminate_pid': 1,
	'text': 1,
	'timestr': 1,
	'trashcan': 2,
	'username': 0,
	'zonestr': 1,
};

var noargs_required = [
	'datestr',
	'secondstr',
	'timestr',
	'zonestr',
];

var prefix = "system.";
for (var func in func_list) {
	if (system[func] === undefined)
		throw new Error("Function " + prefix + func + " isn't defined");
	if (noargs_required.indexOf(func) < 0) {
		var exp = prefix + func + "()";
		var success = false;
		try {
			eval(exp);
		} catch (e) {
			if(e instanceof Error)
				success = true;
		}
		if (!success)
			throw new Error("Invocation of '" + exp + "' did not throw the expected Error exception");
	}
	if (func_list[func] < 1)
		continue;
	var arglist = ["(null)", "(undefined)"];
	for(var i in arglist) {
		exp = prefix + func + arglist[i];
		success = false;
		try {
			eval(exp);
		} catch (e) {
			if(e instanceof Error)
				success = true;
		}
		if (!success)
			throw new Error("Invocation of '" + exp + "' did not throw the expected Error exception");
	}
	if (func_list[func] < 2)
		continue;
	var arglist = ["(0)", "(0, undefined)"];
	for(var i in arglist) {
		exp = prefix + func + arglist[i];
		success = false;
		try {
			eval(exp);
		} catch (e) {
			if(e instanceof Error)
				success = true;
		}
		if (!success)
			throw new Error("Invocation of '" + exp + "' did not throw the expected Error exception");
	}
}
