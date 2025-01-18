// Test user function exceptions

// Value (e.g. 0, 1) is number of non-null/undefined args required
var func_list = {
	'adjust_credits': 1,
	'adjust_minutes': 1,
	'compare_ars': 1,
	'downloaded_file': 1,
	'get_time_left': 1,
	'posted_message': 1,
	'sent_email': 1,
	'uploaded_file': 1,
};

var noargs_required = [
	'posted_message',
	'sent_email',
	'uploaded_file',
	'downloaded_file',
];

var prefix = "user.";
for (var func in func_list) {
	if (user[func] === undefined)
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
