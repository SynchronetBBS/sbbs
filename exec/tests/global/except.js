// Test global function exceptions

// Value (e.g. 0, 1) is number of non-null/undefined args required
var func_list = {
	'ascii': 1,
	'ascii_str': 1,
	'alert': 0,
	'backslash': 1,
	'base64_encode': 1,
	'base64_decode': 1,
	'beep': 1,
	'chksum_calc': 1,
	'confirm': 1,
	'crc16_calc': 1,
	'crc32_calc': 1,
	'ctrl': 1,
	'dir_freespace': 1,
	'directory': 1,
	'disk_size': 1,
	'file_attrib': 1,
	'file_backup': 1,
	'file_cdate': 1,
	'file_cfgname': 2,
	'file_chmod': 2,
	'file_compare': 2,
	'file_copy': 2,
	'file_date': 1,
	'file_exists': 0,
	'file_getcase': 1,
	'file_getdosname': 1,
	'file_getext': 1,
	'file_getname': 1,
	'file_isdir': 0,
	'file_mode': 1,
	'file_mutex': 1,
	'file_remove': 1,
	'file_removecase': 1,
	'file_rename': 2,
	'file_size': 0,
	'file_touch': 1,
	'file_utime': 1,
	'flags_str': 1,
	'fullpath': 1,
	'html_encode': 1,
	'html_decode': 1,
	'lfexpand': 1,
	'load': 1,
	'log': 0,
	'md5_calc': 1,
	'mkdir': 1,
	'mkpath': 1,
	'mswait': 1,
	'netaddr_type': 0,
	'quote_msg': 1,
	'read': 1,
	'readln': 1,
	'require': 1,
	'resolve_host': 1,
	'resolve_ip': 1,
	'rmdir': 1,
	'rmfiles': 1,
	'rot13_translate': 1,
	'skipsp': 1,
	'sha1_calc': 1,
	'socket_strerror': 1,
	'str_has_ctrl': 0,
	'str_is_ascii': 0,
	'str_is_utf16': 0,
	'str_is_utf8': 0,
	'strerror': 1,
	'strftime': 1,
	'strip_ansi': 1,
	'strip_ctrl': 1,
	'strip_ctrl_a': 1,
	'strip_exascii': 1,
	'truncsp': 1,
	'truncstr': 2,
	'utf8_decode': 1,
	'utf8_encode': 1,
	'utf8_get_width': 1,
	'wildmatch': 0,
	'word_wrap': 1,
	'yield': 1,
};

var noargs_required = [
	'beep',
	'mswait',
	'read',
	'readln',
	'yield',
];

for (var func in func_list) {
	if (js.global[func] === undefined)
		throw new Error("Function " + func + " isn't defined");
	if (noargs_required.indexOf(func) < 0) {
		var exp = func + "()";
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
		exp = func + arglist[i];
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
		exp = func + arglist[i];
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
