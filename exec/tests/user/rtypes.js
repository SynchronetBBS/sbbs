// Test user function return types

var type = {
	'adjust_credits(0)'		: 'boolean',
	'adjust_minutes(0)'		: 'boolean',
	'compare_ars("")'		: 'boolean',
	'downloaded_file()'		: 'boolean',
	'get_time_left(0)'		: 'number',
	'posted_message(0)'		: 'boolean',
	'sent_email(0)'			: 'boolean',
	'uploaded_file()'		: 'boolean',
};

var prefix = "user.";
for (var i in type) {
	var result;
	var exp = prefix + i;
	try {
//		writeln(exp);
		result = eval(exp);
	} catch(e) {
		log("Error evaluating: " + exp);
		throw e;
	}
	if (typeof result !== type[i])
		throw new Error("typeof " + exp + " = '" + (typeof result) + "' instead of '" + type[i] + "'");
}
