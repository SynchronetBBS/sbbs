// Test system method return types
// @format.tab-size 4, @format.use-tabs true

var type = {
	'username("")'			: 'string',
	'alias("")'				: 'string',
	'find_login_id("")'		: 'number',
	'matchuser("")'			: 'number',
	'matchuserdata(1,"")'	: 'number',
	'trashcan("","")'		: 'boolean',
	'findstr("", "")'		: 'boolean',
	'zonestr()'				: 'string',
	'datestr()'				: 'string',
	'secondstr(0)'			: 'string',
	'get_node_message(0)'	: 'string',
	'get_telegram(0)'		: 'string',
	'exec(" ")'				: 'number',
	'check_syspass(null)'	: 'boolean',
	'check_name(null)'		: 'boolean',
	'check_filename(null)'	: 'boolean',
	'allowed_filename(null)': 'boolean',
	'safest_filename(null)'	: 'boolean',
	'illegal_filename(null)': 'boolean',
	'text(0)'				: 'object', // null
	'text(1)'				: 'string',
	'text("")'				: 'object', // null
	'text("Email")'			: 'string',
};

var prefix = "system.";
for (var i in type) {
	var result;
	var exp = prefix + i;
	try {
//		stdout.writeln(exp);
		result = eval(exp);
	} catch(e) {
		log("Error evaluating: " + exp);
		throw e;
	}
	if (typeof result !== type[i])
		throw new Error("typeof " + exp + " = '" + (typeof result) + "' instead of '" + type[i] + "'");
}
