// Test global function return types
// @format.tab-size 4, @format.use-tabs true

var type = {
	'alert(0)'				: 'undefined',
	'ascii(1)'				: 'string',
	'ascii("A")'		    : 'number',
	'ascii_str("A")'	    : 'string',
	'ascii_str(1)'		    : 'string',
	'ascii_str(null)'		: 'object', // null
	'ascii_str(undefined)'	: 'undefined',
	'backslash("")'		    : 'string',
	'backslash(null)'		: 'object', // null
	'backslash(undefined)'	: 'undefined',
	'base64_encode("")'		: 'object', // null
	'base64_encode("1")'	: 'string',
	'base64_decode("")'		: 'object', // null
	'base64_decode("Cg==")'	: 'string',
	'beep()'			    : 'undefined',
	'chksum_calc("")'		: 'number',
	'crc16_calc("")'		: 'number',
	'crc32_calc("")'		: 'number',
	'ctrl("A")'			    : 'string',
	'ctrl(1)'			    : 'string',
	'directory("")'		    : 'object',
	'file_attrib("")'	    : 'number',
	'file_backup("")'	    : 'boolean',
	'file_chmod("",0)'	    : 'boolean',
	'file_copy("","")'	    : 'boolean',
	'file_exists("")'		: 'boolean',
	'file_getcase("")'		: 'undefined',
	'file_getext("")'		: 'undefined',
	'file_getname("")'		: 'string',
	'file_isdir("")'		: 'boolean',
	'file_mode("")'			: 'number',
	'file_remove("")'		: 'boolean',
	'file_removecase("")'	: 'boolean',
	'file_rename("","")'	: 'boolean',
	'flags_str("")'			: 'number',
	'flags_str(0)'			: 'string',
	'fullpath("")'			: 'string',
	'lfexpand("")'			: 'string',
	'lfexpand(null)'		: 'object', // null
	'lfexpand(undefined)'	: 'undefined',
	'log("")'				: 'string',
	'md5_calc("")'			: 'string',
	'mswait()'				: 'number',
	'printf()'				: 'string',
	'read(0)'				: 'undefined',
	'readln(0)'				: 'undefined',
	'random()'				: 'number',
	'rmdir("")'				: 'boolean',
	'resolve_host(0)'		: 'object', // null
	'resolve_host("")'		: 'object', // null
	'resolve_ip(0)'			: 'object', // null
	'resolve_ip("")'		: 'object', // null
	'rot13_translate("")'	: 'string',
	'sha1_calc("")'			: 'string',
	'str_has_ctrl("")'		: 'boolean',
	'str_is_ascii("")'		: 'boolean',
	'str_is_utf16("")'		: 'boolean',
	'str_is_utf8("")'		: 'boolean',
	'strftime("")'			: 'string',
	'strip_ctrl(null)'		: 'object', // null
	'strip_ctrl(undefined)'	: 'undefined',
	'strip_ctrl("")'		: 'string',
	'strip_ctrl_a("")'		: 'string',
	'strip_ansi("")'		: 'string',
	'strip_exascii("")'		: 'string',
	'skipsp("")'			: 'string',
	'time()'				: 'number',
	'truncsp("")'			: 'string',
	'truncstr("", "")'		: 'string',
	'utf8_decode("")'		: 'string',
	'utf8_encode("")'		: 'string',
	'utf8_get_width("")'	: 'number',
	'write()'				: 'undefined',
	'write("")'				: 'undefined',
	'write("", null)'		: 'undefined',
	'write(undefined)'		: 'undefined',
	'write_raw()'			: 'undefined',
	'write_raw("")'			: 'undefined',
	'write_raw(undefined)'	: 'undefined',
	'writeln()'				: 'undefined',
	'yield()'				: 'undefined',
};

if (js.global.jsdoor_revision === undefined)
	type['netaddr_type("")'] = 'number';

for (var i in type) {
	var result;
	try {
//		writeln(i);
		result = eval(i);
	} catch(e) {
		log("Error evaluating: " + i);
		throw e;
	}
	if (typeof result !== type[i])
		throw new Error("typeof " + i + " = '" + (typeof result) + "' instead of '" + type[i] + "'");
}
