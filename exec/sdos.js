// Synchronet DOS: an MS-DOS 5.0 COMMAND.COM look-alike command shell
// replaces sdos.src/.bin

// Thanks to Yojimbo for the BDOS shell that inspired the original

// The BBS is presented as a read-only drive C: with the BBS functions as
// programs in C:\, C:\FILES and C:\MAIL, and the external programs (doors)
// as C:\DOORS\<section>\<program>.EXE.  Programs run the way COMMAND.COM
// runs them: by name from the current directory or the PATH, or by relative
// or absolute path, with or without the .COM/.EXE/.BAT extension.

// @format.tab-size 4

"use strict";

require("sbbsdefs.js", "FI_REMOVE");
require("nodedefs.js", "NODE_MAIN");

var DOS_VERSION = "5.00";
var VOLUME_LABEL = "BBS";
var VOLUME_SERIAL = "8317-6BE8";
var MAX_CMDLINE = 127;		// COMMAND.COM's command-line length limit
var MAX_ENV_SIZE = 512;		// Bytes of environment space
var MAX_SHELL_DEPTH = 8;	// Nested COMMAND.COM instances
var SYSOP_ARS = "SYSOP or EXEMPT Q or I or N";
var EXEC_EXTS = ["COM", "EXE", "BAT"];	// Search order, as in DOS

var shell = null;			// shell_lib.js
var dos_env = {};				// Environment variables (names upper-case)
var cwd = [];				// Current directory, as path components
var echo_on = true;
var break_on = false;
var verify_on = false;
var clock_offset = 0;		// Seconds, set by the DATE and TIME commands
var shell_depth = 0;
var shell_exit = false;
var history = [];
var pager = null;			// DIR /P line counting
var screen_cleared = false;	// No blank line before the next prompt

function dos_time(year, month, day, hour, min)
{
	return new Date(year, month - 1, day, hour, min).getTime() / 1000;
}

function now()
{
	return Date.now() / 1000 + clock_offset;
}

/****************************************************************************/
/* Output                                                                   */
/****************************************************************************/

function out(str)
{
	console.print(str);
}

function outln(str)
{
	console.print((str === undefined ? "" : str) + "\r\n");
	if (pager && ++pager.lines >= console.screen_rows - 1) {
		pause_prompt();
		pager.lines = 0;
	}
}

function pause_prompt()
{
	out("Press any key to continue . . .");
	console.getkey(K_NOSPIN);
	out("\r\n");
}

function lpad(val, width)
{
	return format("%*s", width, String(val));
}

function rpad(val, width)
{
	return format("%-*s", width, String(val));
}

// Dates in listings are always MS-DOS (US) style, regardless of the user's
// date format preference
function fmt_date(t, long_year)
{
	var d = new Date(t * 1000);
	var year = long_year ? d.getFullYear() : d.getFullYear() % 100;
	return format(long_year ? "%02d-%02d-%04d" : "%02d-%02d-%02d"
		, d.getMonth() + 1, d.getDate(), year);
}

function fmt_time(t)
{
	var d = new Date(t * 1000);
	var hour = d.getHours() % 12;
	return format("%2d:%02d%c", hour ? hour : 12, d.getMinutes()
		, d.getHours() < 12 ? 'a' : 'p');
}

var DAY_NAMES = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];

// The critical-error handler: returns 'A'bort or 'F'ail ('R'etry loops)
function critical_error(msg)
{
	while (bbs.online && !js.terminated) {
		outln("");
		out(msg + "\r\nAbort, Retry, Fail?");
		var key = console.getkeys("ARF");
		if (key != 'R')
			return key;
	}
	return 'A';
}

/****************************************************************************/
/* The virtual file system                                                  */
/****************************************************************************/

function file_entry(name, ext, size, t, props)
{
	var entry = { name: name, ext: ext, size: size, time: t };
	for (var p in props)
		entry[p] = props[p];
	return entry;
}

function dir_entry(name, children, props)
{
	var entry = { name: name, ext: "", dir: true, size: 0
		, time: dos_time(1994, 8, 1, 14, 34), children: children };
	for (var p in props)
		entry[p] = props[p];
	return entry;
}

function entry_name(entry)
{
	return entry.ext ? (entry.name + "." + entry.ext) : entry.name;
}

function can_see(entry)
{
	return !entry.ars || bbs.compare_ars(entry.ars);
}

function list_dir(dir)
{
	var list = typeof dir.children == "function" ? dir.children() : dir.children;
	return list.filter(can_see);
}

// Split and upper-case an 8.3 file name, silently truncating each part
// the way DOS does
function split_name(str)
{
	str = str.toUpperCase();
	var dot = str.indexOf('.');
	if (dot < 0)
		return { name: str.substr(0, 8), ext: "" };
	return { name: str.substr(0, dot).substr(0, 8), ext: str.substr(dot + 1).substr(0, 3) };
}

function find_entry(dir, name)
{
	var fn = split_name(name);
	var list = list_dir(dir);
	for (var i = 0; i < list.length; i++) {
		if (list[i].name == fn.name && list[i].ext == fn.ext)
			return list[i];
	}
	return null;
}

// DOS (FCB-style) wildcard match: '*' fills the rest of the name or the
// extension with '?', and '?' matches any character or none
function wild_match(entry, pattern)
{
	var pat = split_name(pattern);
	function fill(str, len) {
		var star = str.indexOf('*');
		if (star >= 0)
			str = str.substr(0, star) + new Array(len - star + 1).join('?');
		return rpad(str, len);
	}
	var p = fill(pat.name, 8) + fill(pat.ext, 3);
	var n = rpad(entry.name, 8) + rpad(entry.ext, 3);
	for (var i = 0; i < p.length; i++) {
		if (p.charAt(i) != '?' && p.charAt(i) != n.charAt(i))
			return false;
	}
	return true;
}

function is_wild(str)
{
	return /[*?]/.test(str);
}

function path_str(path)
{
	return "C:\\" + path.join("\\");
}

// Parse a drive letter prefix, returning the rest of the path or an error
function parse_drive(str)
{
	var m = str.match(/^([A-Za-z]):(.*)$/);
	if (!m)
		return { rest: str };
	var drive = m[1].toUpperCase();
	if (drive == 'C')
		return { rest: m[2] };
	if (drive == 'A' || drive == 'B')
		return { error: "Not ready reading drive " + drive, critical: true };
	return { error: "Invalid drive specification" };
}

// Resolve a directory path to { dir, path } or { error }
function resolve_dir(str)
{
	var drv = parse_drive(str);
	if (drv.error)
		return drv;
	str = drv.rest;
	var path = (str.charAt(0) == '\\') ? [] : cwd.slice();
	var parts = str.split('\\');
	for (var i = 0; i < parts.length; i++) {
		var part = parts[i];
		if (part == "" || part == ".")
			continue;
		if (part == "..") {
			if (!path.length)
				return { error: "Invalid directory" };
			path.pop();
			continue;
		}
		var dir = dir_at(path);
		var entry = dir ? find_entry(dir, part) : null;
		if (!entry || !entry.dir)
			return { error: "Invalid directory" };
		path.push(entry_name(entry));
	}
	return { dir: dir_at(path), path: path };
}

function dir_at(path)
{
	var dir = root;
	for (var i = 0; i < path.length; i++) {
		dir = find_entry(dir, path[i]);
		if (!dir || !dir.dir)
			return null;
	}
	return dir;
}

// Split "dir\name" into its directory and file name parts
function split_path(str)
{
	var drv = str.match(/^[A-Za-z]:/);
	var prefix = drv ? drv[0] : "";
	str = str.substr(prefix.length);
	var slash = str.lastIndexOf('\\');
	if (slash < 0)
		return { dir: prefix, file: str };
	return { dir: prefix + (slash ? str.substr(0, slash) : "\\"), file: str.substr(slash + 1) };
}

// Resolve a file (or wildcard) spec to { dir, path, pattern } or { error }
function resolve_spec(str)
{
	var dir = resolve_dir(str);
	if (dir.dir) {
		dir.pattern = "*.*";
		return dir;
	}
	if (dir.critical)
		return dir;
	var sp = split_path(str);
	dir = resolve_dir(sp.dir);
	if (dir.error)
		return dir;
	dir.pattern = sp.file;
	return dir;
}

function report_error(result)
{
	if (result.critical)
		critical_error(result.error);
	else
		outln(result.error);
}

/****************************************************************************/
/* The contents of drive C:                                                 */
/****************************************************************************/

function cur_dir_num()
{
	if (!file_area.lib_list.length)
		return undefined;
	return file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number;
}

function run_script(name, args)
{
	var script = system.mods_dir + name;
	if (!file_exists(script))
		script = system.exec_dir + name;
	js.exec.apply(js, [script, {}].concat(args || []));
}

function arg_or_filespec(args)
{
	return args.trim() || bbs.get_filespec();
}

function list_files(args)
{
	var result = bbs.list_files(cur_dir_num(), args.trim() || "*.*");
	if (result < 0)
		return;
	if (result == 0)
		out(args.trim() ? "File not found\r\n" : bbs.text(bbs.text.EmptyDir));
	else
		out(format(bbs.text(bbs.text.NFilesListed), result));
}

function send_mail(args, wm_mode)
{
	var name = args.trim();
	if (!name) {
		out("\x01_\r\n\x01b\x01hE-mail (User name or number): \x01w");
		name = console.getstr(40, K_TRIM);
		if (!name)
			return;
	}
	if (name.toUpperCase() == "SYSOP")
		name = "1";
	if (/^\d+$/.test(name) && system.username(Number(name)))
		bbs.email(Number(name), wm_mode);
	else
		shell.send_email(name, wm_mode);
}

function type_lines(lines)
{
	return function () {
		for (var i = 0; i < lines.length && !console.aborted; i++)
			outln(lines[i]);
	};
}

var autoexec_bat = [
	"@ECHO OFF",
	"PROMPT $P$G",
	"PATH C:\\",
	"SET SBBSCTRL=C:\\SBBS\\CTRL",
	"SET SBBSNODE=C:\\SBBS\\NODE%SBBSNNUM%\\",
	"VER",
];

var config_sys = [
	"DEVICE=C:\\HIMEM.SYS",
	"DOS=HIGH",
	"FILES=40",
	"BUFFERS=20",
	"SHELL=C:\\COMMAND.COM C:\\ /P",
];

var root = dir_entry("", [
	dir_entry("FILES", [
		file_entry("DOWNLOAD", "EXE",  24343, dos_time(1994, 6, 2, 8, 36), {
			desc: "Downloads files from the current file area.",
			usage: "DOWNLOAD [filespec]",
			run: function (args) {
				if (!args.trim() && bbs.batch_dnload_total) {
					shell.download_files();
					return;
				}
				var spec = arg_or_filespec(args);
				if (spec)
					shell.view_file_info(FI_DOWNLOAD, spec);
			} }),
		file_entry("UPLOAD",   "EXE",  48321, dos_time(1994, 6, 2, 8, 36), {
			desc: "Uploads a file to the current file area.",
			run: function () { shell.upload_file(); } }),
		file_entry("REMOVE",   "COM",   2377, dos_time(1994, 6, 4, 16, 55), {
			desc: "Removes or edits your files in the current file area.",
			usage: "REMOVE [filespec]",
			run: function (args) {
				var spec = arg_or_filespec(args);
				if (spec)
					shell.view_file_info(FI_REMOVE, spec);
			} }),
		file_entry("BATCH",    "EXE",  30566, dos_time(1994, 6, 2, 8, 36), {
			desc: "Batch file transfer menu.",
			run: function () { bbs.batch_menu(); } }),
		file_entry("VIEW",     "EXE",  10485, dos_time(1994, 6, 2, 8, 36), {
			desc: "Views the contents of files in the current file area.",
			usage: "VIEW [filespec]",
			run: function (args) {
				var spec = arg_or_filespec(args);
				if (spec)
					shell.view_files(spec);
			} }),
		file_entry("TEMP",     "EXE", 114705, dos_time(1994, 6, 2, 8, 37), {
			desc: "Temporary (archive) file area.",
			run: function () { bbs.temp_xfer(); } }),
		file_entry("LIST",     "EXE",  81380, dos_time(1994, 6, 4, 16, 56), {
			desc: "Lists the files in the current file area.",
			usage: "LIST [filespec]",
			run: list_files }),
		file_entry("NEWSCAN",  "COM",   3146, dos_time(1994, 6, 2, 8, 36), {
			desc: "Scans the file areas for new files.",
			run: function () { bbs.scan_dirs(FL_ULTIME); } }),
		file_entry("EXTENDED", "EXE", 287347, dos_time(1994, 6, 4, 16, 56), {
			desc: "Displays extended information about files.",
			usage: "EXTENDED [filespec]",
			run: function (args) {
				var spec = arg_or_filespec(args);
				if (spec)
					shell.view_file_info(FI_INFO, spec);
			} }),
		file_entry("SEARCH",   "COM",   1969, dos_time(1994, 6, 4, 16, 56), {
			desc: "Searches the file areas for file names.",
			run: function () { bbs.scan_dirs(FL_NO_HDR); } }),
		file_entry("FIND",     "COM",   5193, dos_time(1994, 6, 4, 16, 57), {
			desc: "Searches the file areas for text in file descriptions.",
			run: function () { bbs.scan_dirs(FL_FINDDESC); } }),
		file_entry("AREA",     "COM",    905, dos_time(1994, 6, 2, 8, 35), {
			desc: "Selects the current file area.",
			run: function () { shell.select_file_area(); } }),
		file_entry("CONFIG",   "COM",   2517, dos_time(1994, 6, 2, 8, 35), {
			desc: "Configures your new file scan.",
			run: function () { run_script("filescancfg.js"); } }),
	]),
	dir_entry("MAIL", [
		file_entry("SEND",     "EXE",  54343, dos_time(1994, 6, 2, 8, 36), {
			desc: "Sends e-mail to a user.",
			usage: "SEND [user]",
			run: function (args) { send_mail(args, WM_NONE); } }),
		file_entry("SENDFILE", "EXE",  58981, dos_time(1994, 6, 2, 8, 36), {
			desc: "Sends e-mail with an attached file to a user.",
			usage: "SENDFILE [user]",
			run: function (args) { send_mail(args, WM_FILE); } }),
		file_entry("NETMAIL",  "EXE",  30566, dos_time(1994, 6, 2, 8, 36), {
			desc: "Sends network e-mail.",
			usage: "NETMAIL [address]",
			run: function (args) { shell.send_netmail(args.trim()); } }),
		file_entry("FEEDBACK", "EXE",  12803, dos_time(1994, 6, 2, 8, 36), {
			desc: "Sends e-mail to the sysop.",
			run: function () { shell.send_feedback(); } }),
		file_entry("READ",     "EXE", 102485, dos_time(1994, 6, 2, 8, 36), {
			desc: "Reads e-mail sent to you.",
			run: function () { bbs.read_mail(); } }),
		file_entry("READSENT", "EXE", 114705, dos_time(1994, 6, 2, 8, 37), {
			desc: "Reads e-mail you have sent.",
			run: function () { bbs.read_mail(MAIL_SENT); } }),
		file_entry("POST",     "EXE",  81380, dos_time(1994, 6, 4, 16, 56), {
			desc: "Posts a message in the current message area.",
			run: function () { bbs.post_msg(); } }),
		file_entry("NEWSCAN",  "COM",   3146, dos_time(1994, 6, 2, 8, 36), {
			desc: "Scans the message areas for new messages.",
			run: function () { bbs.scan_subs(SCAN_NEW); } }),
		file_entry("READMSGS", "EXE", 287347, dos_time(1994, 6, 4, 16, 56), {
			desc: "Reads messages in the current message area.",
			run: function () { bbs.scan_msgs(); } }),
		file_entry("YOURMSGS", "COM",   1969, dos_time(1994, 6, 4, 16, 56), {
			desc: "Scans the message areas for messages to you.",
			run: function () { bbs.scan_subs(SCAN_TOYOU); } }),
		file_entry("FIND",     "COM",   5193, dos_time(1994, 6, 4, 16, 57), {
			desc: "Searches the message areas for text.",
			run: function () { bbs.scan_subs(SCAN_FIND); } }),
		file_entry("QWK",      "EXE", 159876, dos_time(1994, 6, 2, 8, 35), {
			desc: "QWK message packet transfers.",
			run: function () { bbs.qwk_sec(); } }),
		file_entry("AREA",     "COM",    905, dos_time(1994, 6, 2, 8, 35), {
			desc: "Selects the current message area.",
			run: function () { shell.select_msg_area(); } }),
		file_entry("CONFIG",   "COM",   2517, dos_time(1994, 6, 2, 8, 35), {
			desc: "Configures your new message scan.",
			run: function () { run_script("msgscancfg.js"); } }),
	]),
	dir_entry("DOORS", door_sections),
	file_entry("COMMAND",  "COM",  47845, dos_time(1991, 4, 9, 5, 0), {
		desc: "Starts a new instance of the command interpreter.",
		usage: "COMMAND [/C command]",
		run: run_command_com }),
	file_entry("CONFIG",   "SYS",    108, dos_time(1994, 8, 1, 14, 34), {
		type: type_lines(config_sys) }),
	file_entry("AUTOEXEC", "BAT",    142, dos_time(1994, 8, 1, 14, 34), {
		batch: autoexec_bat, type: type_lines(autoexec_bat) }),
	file_entry("HELP",     "EXE",  11473, dos_time(1991, 4, 9, 5, 0), {
		desc: "Provides help information for commands.",
		usage: "HELP [command]",
		run: help }),
	file_entry("SETUP",    "EXE",  54343, dos_time(1994, 6, 2, 8, 36), {
		desc: "Changes your user settings.",
		run: function () { bbs.user_config(); save_state(); exit(); } }),
	file_entry("CHAT",     "EXE", 102485, dos_time(1994, 6, 2, 8, 36), {
		desc: "Chat with other users or the sysop.",
		run: function () { bbs.chat_sec(); } }),
	file_entry("AUTOMSG",  "COM",    894, dos_time(1994, 6, 4, 16, 55), {
		desc: "Reads or writes the auto-message.",
		run: function () { bbs.auto_msg(); } }),
	file_entry("DOORS",    "EXE", 326501, dos_time(1994, 7, 11, 11, 1), {
		desc: "External programs menu.",
		run: function () { bbs.xtrn_sec(); } }),
	file_entry("OPEN",     "COM",   1206, dos_time(1994, 7, 11, 11, 1), {
		desc: "Runs an external program by its internal code.",
		usage: "OPEN code",
		run: function (args) {
			var code = args.trim();
			if (!code) {
				outln("External program name missing");
				return;
			}
			var prog = xtrn_area.prog[code.toLowerCase()];
			if (!prog || !prog.can_access)
				outln("File not found - " + code.toUpperCase());
			else if (!prog.can_run)
				outln("Access denied");
			else
				bbs.exec_xtrn(prog.code);
		} }),
	file_entry("GFILES",   "EXE",  31024, dos_time(1994, 6, 2, 8, 36), {
		desc: "General text files section.",
		run: function () { bbs.text_sec(); } }),
	file_entry("LOGOFF",   "COM",   2193, dos_time(1994, 5, 22, 16, 27), {
		desc: "Logs off of the BBS.",
		run: function () { shell.logoff(/* fast: */false); } }),
	file_entry("NODES",    "TXT",   593, 0, {
		live_size: function () { return system.nodes * 72; },
		type: function () { bbs.list_nodes(); } }),
	file_entry("WHO",      "TXT",   288, 0, {
		live_size: function () { return who_count() * 72; },
		type: function () { bbs.whos_online(); } }),
	file_entry("LOGON",    "LST",  1984, 0, {
		live_size: function () { return system.stats.logons_today * 64; },
		type: function () { bbs.list_logons(); } }),
	file_entry("USERS",    "LST", 148394, 0, {
		live_size: function () { return system.stats.total_users * 80; },
		type: function () { bbs.list_users(); } }),
	file_entry("SYSTEM",   "NFO",   733, dos_time(1994, 4, 1, 6, 30), {
		type: function () { bbs.sys_info(); } }),
	file_entry("YOUR",     "NFO",   252, 0, {
		live_time: function () { return user.stats.laston_date; },
		type: function () { bbs.user_info(); } }),
	file_entry("SYSOP",    "EXE", 208419, dos_time(1980, 1, 1, 0, 0), {
		ars: SYSOP_ARS,
		desc: "Sysop menu.",
		run: function () { bbs.menu("sysmain"); } }),
]);

function who_count()
{
	var count = 0;
	for (var i = 0; i < system.node_list.length; i++) {
		if (system.node_list[i].status == NODE_INUSE)
			count++;
	}
	return count;
}

// Make unique 8-character DOS names from internal codes
function short_names(codes)
{
	var names = [];
	var used = {};
	for (var i = 0; i < codes.length; i++) {
		var name = codes[i].toUpperCase().replace(/[^A-Z0-9_\-!#$%&@^~]/g, "");
		if (!name)
			name = "PROGRAM";
		if (name.length > 8 || used[name]) {
			for (var n = 1; ; n++) {
				var tail = "~" + n;
				var candidate = name.substr(0, 8 - tail.length) + tail;
				if (!used[candidate]) {
					name = candidate;
					break;
				}
			}
		}
		used[name] = true;
		names.push(name);
	}
	return names;
}

// Consistent (but fake) sizes and dates, derived from the internal code
function fake_size(code)
{
	return 10000 + crc16_calc(code) * 5;
}

function fake_time(code)
{
	var crc = crc16_calc(code);
	return dos_time(1993 + (crc & 1), 1 + (crc >> 1) % 12, 1 + (crc >> 5) % 28
		, (crc >> 3) % 24, (crc >> 7) % 60);
}

function door_sections()
{
	var secs = xtrn_area.sec_list.filter(function (sec) { return sec.can_access; });
	var names = short_names(secs.map(function (sec) { return sec.code; }));
	return secs.map(function (sec, i) {
		return dir_entry(names[i], function () { return door_programs(sec); }
			, { time: fake_time(sec.code) });
	});
}

function door_programs(sec)
{
	var progs = sec.prog_list.filter(function (prog) { return prog.can_access; });
	var names = short_names(progs.map(function (prog) { return prog.code; }));
	return progs.map(function (prog, i) {
		return file_entry(names[i], "EXE", fake_size(prog.code), fake_time(prog.code), {
			desc: prog.name,
			run: function () {
				if (!prog.can_run)
					outln("Access denied");
				else
					bbs.exec_xtrn(prog.code);
			} });
	});
}

function entry_size(entry)
{
	return entry.live_size ? entry.live_size() : entry.size;
}

function entry_time(entry)
{
	if (entry.live_time)
		return entry.live_time();
	return entry.time || now();
}

/****************************************************************************/
/* Environment                                                              */
/****************************************************************************/

// Keep the session's state across a restart of the shell
function save_state()
{
	bbs.mods.sdos_state = { env: dos_env, cwd: cwd, history: history
		, echo_on: echo_on, clock_offset: clock_offset };
}

function restore_state()
{
	var state = bbs.mods.sdos_state;
	if (!state)
		return false;
	dos_env = state.env;
	cwd = dir_at(state.cwd) ? state.cwd : [];
	history = state.history;
	echo_on = state.echo_on;
	clock_offset = state.clock_offset;
	return true;
}

function init_env()
{
	dos_env = {
		COMSPEC: "C:\\COMMAND.COM",
		PROMPT: "$P$G",
		PATH: "C:\\",
		SBBSCTRL: "C:\\SBBS\\CTRL",
		SBBSNODE: "C:\\SBBS\\NODE" + bbs.node_num + "\\",
		SBBSNNUM: String(bbs.node_num),
	};
}

function env_size(e)
{
	var size = 0;
	for (var name in e)
		size += name.length + e[name].length + 2;
	return size;
}

function set_env(name, value)
{
	name = name.toUpperCase();
	if (value === "") {
		delete dos_env[name];
		return;
	}
	var e = JSON.parse(JSON.stringify(dos_env));
	e[name] = value;
	if (env_size(e) > MAX_ENV_SIZE) {
		outln("Out of environment space");
		return;
	}
	dos_env[name] = value;
}

// Expand %VAR% and batch parameters (%0 - %9)
function expand_env(str, params)
{
	return str.replace(/%([0-9])|%([^%\s]+)%|%%/g, function (match, digit, name) {
		if (digit !== undefined && digit !== "")
			return params[Number(digit)] || "";
		if (name)
			return dos_env[name.toUpperCase()] || "";
		return "%";
	});
}

function render_prompt()
{
	var fmt = dos_env.PROMPT === undefined ? "$N$G" : dos_env.PROMPT;
	var str = "";
	for (var i = 0; i < fmt.length; i++) {
		var ch = fmt.charAt(i);
		if (ch != '$') {
			str += ch;
			continue;
		}
		var t = now();
		switch (fmt.charAt(++i).toUpperCase()) {
			case 'P': str += path_str(cwd); break;
			case 'N': str += "C"; break;
			case 'G': str += ">"; break;
			case 'L': str += "<"; break;
			case 'B': str += "|"; break;
			case 'Q': str += "="; break;
			case '$': str += "$"; break;
			case 'A': str += "&"; break;
			case 'C': str += "("; break;
			case 'F': str += ")"; break;
			case '_': str += "\r\n"; break;
			case 'E': str += "\x1b"; break;
			case 'H': str += "\b \b"; break;
			case 'V': str += "MS-DOS Version " + DOS_VERSION; break;
			case 'D': str += DAY_NAMES[new Date(t * 1000).getDay()] + " " + fmt_date(t, true); break;
			case 'T': str += clock_str(t); break;
		}
	}
	return str;
}

function clock_str(t)
{
	var d = new Date(t * 1000);
	return format("%2d:%02d:%02d.%02d", d.getHours(), d.getMinutes(), d.getSeconds()
		, Math.floor(d.getMilliseconds() / 10));
}

/****************************************************************************/
/* Command-line parsing                                                     */
/****************************************************************************/

// Split an argument string into words and switches ("FILES/W /P" -> "FILES", "/W", "/P")
function tokenize(str)
{
	return str.match(/\/[^\s\/]*|[^\s\/,;=]+/g) || [];
}

function switches(tokens, allowed)
{
	var result = { words: [], sw: {} };
	for (var i = 0; i < tokens.length; i++) {
		var tok = tokens[i];
		if (tok.charAt(0) != '/') {
			result.words.push(tok);
			continue;
		}
		var sw = tok.substr(1).toUpperCase();
		var key = sw.charAt(0);
		if (!key || allowed.indexOf(key) < 0) {
			outln("Invalid switch - " + tok);
			return null;
		}
		result.sw[key] = sw.substr(1).replace(/^:/, "");
	}
	return result;
}

// Split a command line into the command and its arguments, the way
// COMMAND.COM does: "CD\MAIL", "CD..", "DIR/W" and "ECHO." all work.
function split_command(line)
{
	var m = line.match(/^([^\s\/,;=+]*)([\s\S]*)$/);
	var cmd = m[1];
	var args = m[2];
	var sep = cmd.search(/[\\.]/);
	if (sep > 0 && internal_commands[cmd.substr(0, sep).toUpperCase()]) {
		args = cmd.substr(sep) + args;
		cmd = cmd.substr(0, sep);
	}
	return { cmd: cmd, args: args };
}

// The text after an internal command, minus the one delimiter that
// separates it from the command name
function arg_text(args)
{
	return args.replace(/^[\s,;=]/, "");
}

/****************************************************************************/
/* Internal commands                                                        */
/****************************************************************************/

function cmd_cd(args)
{
	var words = tokenize(args);
	if (!words.length) {
		outln(path_str(cwd));
		return;
	}
	if (/^[A-Za-z]:$/.test(words[0])) {		// CD C: shows that drive's current directory
		var drv = parse_drive(words[0]);
		if (drv.error)
			report_error(drv);
		else
			outln(path_str(cwd));
		return;
	}
	var result = resolve_dir(words[0]);
	if (result.error) {
		report_error(result);
		return;
	}
	cwd = result.path;
}

function list_entries(dir, path, pattern)
{
	var list = [];
	if (path.length && (pattern == "*.*" || pattern == "*")) {
		list.push({ name: ".", ext: "", dir: true, time: dir.time });
		list.push({ name: "..", ext: "", dir: true, time: dir.time });
	}
	var entries = list_dir(dir);
	for (var i = 0; i < entries.length; i++) {
		if (wild_match(entries[i], pattern))
			list.push(entries[i]);
	}
	return list;
}

function sort_entries(list, order)
{
	if (order === undefined)
		return list;
	if (!order)
		order = "GN";
	var keys = order.match(/-?[NESDG]/g) || [];
	return list.sort(function (a, b) {
		for (var i = 0; i < keys.length; i++) {
			var rev = keys[i].charAt(0) == '-';
			var diff = 0;
			switch (keys[i].charAt(rev ? 1 : 0)) {
				case 'N': diff = a.name < b.name ? -1 : a.name > b.name ? 1 : 0; break;
				case 'E': diff = a.ext < b.ext ? -1 : a.ext > b.ext ? 1 : 0; break;
				case 'S': diff = entry_size(a) - entry_size(b); break;
				case 'D': diff = entry_time(a) - entry_time(b); break;
				case 'G': diff = (b.dir ? 1 : 0) - (a.dir ? 1 : 0); break;
			}
			if (diff)
				return rev ? -diff : diff;
		}
		return 0;
	});
}

function dir_line(entry, opt)
{
	var name = entry_name(entry);
	if (opt.lower)
		name = name.toLowerCase();
	if (opt.bare)
		return name;
	if (opt.wide)
		return entry.dir ? "[" + name + "]" : name;
	var fname = entry.name.charAt(0) == '.' ? rpad(entry.name, 12) : rpad(entry.name, 8) + " " + rpad(entry.ext, 3);
	if (opt.lower)
		fname = fname.toLowerCase();
	var t = entry_time(entry);
	return fname + (entry.dir ? " <DIR>    " : lpad(entry_size(entry), 10))
		+ " " + fmt_date(t) + "  " + fmt_time(t);
}

// List one directory, returning { files, bytes } or null if none matched
function dir_list(dir, path, pattern, opt)
{
	var list = sort_entries(list_entries(dir, path, pattern), opt.order);
	if (!list.length)
		return null;
	if (!opt.bare)
		outln(" Directory of " + path_str(path) + "\r\n");
	var files = 0;
	var bytes = 0;
	var wide = "";
	for (var i = 0; i < list.length && !console.aborted; i++) {
		var entry = list[i];
		files++;
		if (!entry.dir)
			bytes += entry_size(entry);
		var line = dir_line(entry, opt);
		if (opt.bare && opt.subdirs)
			line = path_str(path).replace(/\\$/, "") + "\\" + line;
		if (opt.bare && entry.name.charAt(0) == '.')
			continue;
		if (!opt.wide || opt.bare) {
			outln(line);
			continue;
		}
		wide += rpad(line, 16);
		if ((files % 5) == 0) {
			outln(wide.replace(/\s+$/, ""));
			wide = "";
		}
	}
	if (wide)
		outln(wide.replace(/\s+$/, ""));
	return { files: files, bytes: bytes };
}

function dir_summary(files, bytes)
{
	outln(format("%9d file(s)%11d bytes", files, bytes));
}

function dir_free()
{
	outln(lpad(dir_freespace(system.temp_dir), 28) + " bytes free");
}

function dir_tree(dir, path, pattern, opt, total)
{
	var result = dir_list(dir, path, pattern, opt);
	if (result) {
		if (!opt.bare) {
			dir_summary(result.files, result.bytes);
			outln("");
		}
		total.files += result.files;
		total.bytes += result.bytes;
	}
	var subdirs = list_dir(dir).filter(function (e) { return e.dir; });
	for (var i = 0; i < subdirs.length && !console.aborted; i++)
		dir_tree(subdirs[i], path.concat(entry_name(subdirs[i])), pattern, opt, total);
}

function cmd_dir(args)
{
	var parsed = switches(tokenize((dos_env.DIRCMD || "") + " " + args), "PWASOBL");
	if (!parsed)
		return;
	if (parsed.words.length > 1) {
		outln("Too many parameters - " + parsed.words[1]);
		return;
	}
	var opt = {
		wide: 'W' in parsed.sw,
		bare: 'B' in parsed.sw,
		lower: 'L' in parsed.sw,
		subdirs: 'S' in parsed.sw,
		order: parsed.sw.O,
	};
	var result = resolve_spec(parsed.words.length ? parsed.words[0] : "");
	if (result.error) {
		report_error(result);
		return;
	}
	var pattern = result.pattern;
	var fn = split_name(pattern);
	if (pattern.indexOf('.') < 0)		// DIR FOO means DIR FOO.*
		pattern = fn.name + ".*";
	else if (!fn.name)					// DIR .EXE means DIR *.EXE
		pattern = "*" + pattern;
	if ('P' in parsed.sw)
		pager = { lines: 0 };
	if (!opt.bare) {
		outln("");
		outln(" Volume in drive C is " + VOLUME_LABEL);
		outln(" Volume Serial Number is " + VOLUME_SERIAL);
	}
	if (opt.subdirs) {
		var total = { files: 0, bytes: 0 };
		dir_tree(result.dir, result.path, pattern, opt, total);
		if (!total.files)
			outln("File not found");
		else if (!opt.bare) {
			outln("Total files listed:");
			dir_summary(total.files, total.bytes);
			dir_free();
		}
		return;
	}
	var listed = dir_list(result.dir, result.path, pattern, opt);
	if (!listed) {
		if (!opt.bare)
			outln(" Directory of " + path_str(result.path) + "\r\n");
		outln("File not found");
		return;
	}
	if (!opt.bare) {
		dir_summary(listed.files, listed.bytes);
		dir_free();
	}
}

function cmd_echo(args)
{
	var text = arg_text(args);
	var word = text.trim().toUpperCase();
	if (/^[.\/]/.test(args)) {			// ECHO. displays a blank line
		outln(args.substr(1));
		return;
	}
	if (word == "ON" || word == "OFF") {
		echo_on = (word == "ON");
		return;
	}
	if (!text.trim()) {
		outln("ECHO is " + (echo_on ? "on" : "off"));
		return;
	}
	outln(text);
}

function find_file(str)
{
	var sp = split_path(str);
	var result = resolve_dir(sp.dir);
	if (result.error)
		return result;
	var entry = find_entry(result.dir, sp.file);
	if (!entry)
		return { error: "File not found - " + str.toUpperCase() };
	return { entry: entry };
}

function cmd_type(args)
{
	var words = tokenize(args);
	if (!words.length) {
		outln("Required parameter missing");
		return;
	}
	if (is_wild(words[0])) {
		outln("Invalid filename or file not found");
		return;
	}
	var result = find_file(words[0]);
	if (result.error) {
		report_error(result);
		return;
	}
	var entry = result.entry;
	if (entry.dir)
		outln("Access denied");
	else if (entry.type)
		entry.type();
	else	// An executable: the header, up to the first Ctrl-Z
		outln("MZ\x90\xB8\xFF\xFF\x8E\xD8\xEA\xCD!\xB4L");
}

function cmd_ver()
{
	outln("");
	outln("MS-DOS Version " + DOS_VERSION);
	outln("");
	bbs.ver();
}

function cmd_vol(args)
{
	var words = tokenize(args);
	if (words.length) {
		var drv = parse_drive(words[0]);
		if (drv.error) {
			report_error(drv);
			return;
		}
	}
	outln("");
	outln(" Volume in drive C is " + VOLUME_LABEL);
	outln(" Volume Serial Number is " + VOLUME_SERIAL);
}

function cmd_set(args)
{
	var text = arg_text(args);
	if (!text.trim()) {
		for (var name in dos_env)
			outln(name + "=" + dos_env[name]);
		return;
	}
	var eq = text.indexOf('=');
	if (eq < 1) {
		outln("Syntax error");
		return;
	}
	set_env(text.substr(0, eq).trim(), text.substr(eq + 1));
}

function cmd_path(args)
{
	var text = arg_text(args).trim();
	if (!text) {
		outln(dos_env.PATH === undefined ? "No Path" : "PATH=" + dos_env.PATH);
		return;
	}
	set_env("PATH", text == ";" ? "" : text.toUpperCase());
}

function cmd_prompt(args)
{
	set_env("PROMPT", arg_text(args));
}

function cmd_date(args)
{
	var text = arg_text(args).trim();
	if (!text) {
		var t = now();
		outln("Current date is " + DAY_NAMES[new Date(t * 1000).getDay()] + " " + fmt_date(t, true));
	}
	while (bbs.online && !js.terminated) {
		if (!text) {
			out("Enter new date (mm-dd-yy): ");
			text = console.getstr(10, K_TRIM);
			if (!text)
				return;
		}
		var m = text.match(/^(\d{1,2})[-\/.](\d{1,2})[-\/.](\d{2}|\d{4})$/);
		if (m) {
			var year = Number(m[3]);
			if (year < 100)
				year += (year < 80) ? 2000 : 1900;
			var d = new Date(now() * 1000);
			var nd = new Date(year, Number(m[1]) - 1, Number(m[2])
				, d.getHours(), d.getMinutes(), d.getSeconds());
			if (year >= 1980 && year <= 2099 && nd.getMonth() == Number(m[1]) - 1) {
				clock_offset += Math.round(nd.getTime() / 1000) - now();
				return;
			}
		}
		outln("Invalid date");
		text = "";
	}
}

function cmd_time(args)
{
	var text = arg_text(args).trim();
	if (!text) {
		var t = now();
		var d = new Date(t * 1000);
		var hour = d.getHours() % 12;
		outln(format("Current time is %2d:%02d:%02d.%02d%c", hour ? hour : 12
			, d.getMinutes(), d.getSeconds(), Math.floor(d.getMilliseconds() / 10)
			, d.getHours() < 12 ? 'a' : 'p'));
	}
	while (bbs.online && !js.terminated) {
		if (!text) {
			out("Enter new time: ");
			text = console.getstr(12, K_TRIM);
			if (!text)
				return;
		}
		var m = text.match(/^(\d{1,2})(?::(\d{1,2}))?(?::(\d{1,2}))?(?:\.\d{1,2})?\s*([ap])?m?$/i);
		if (m) {
			var hour = Number(m[1]);
			var ap = m[4] ? m[4].toLowerCase() : "";
			if (ap && hour >= 1 && hour <= 12)
				hour = (hour % 12) + (ap == 'p' ? 12 : 0);
			var min = Number(m[2] || 0);
			var sec = Number(m[3] || 0);
			if (hour < 24 && min < 60 && sec < 60 && (!ap || Number(m[1]) <= 12)) {
				var d = new Date(now() * 1000);
				var nd = new Date(d.getFullYear(), d.getMonth(), d.getDate(), hour, min, sec);
				clock_offset += Math.round(nd.getTime() / 1000) - now();
				return;
			}
		}
		outln("Invalid time");
		text = "";
	}
}

function cmd_del(args)
{
	var words = tokenize(args);
	if (!words.length) {
		outln("Required parameter missing");
		return;
	}
	var result = resolve_spec(words[0]);
	if (result.error) {
		report_error(result);
		return;
	}
	if (result.pattern == "*.*") {
		outln("All files in directory will be deleted!");
		out("Are you sure (Y/N)?");
		var key = console.getkeys("YN");
		if (key != 'Y')
			return;
	}
	var list = list_entries(result.dir, [], result.pattern).filter(function (e) { return !e.dir; });
	outln(list.length ? "Access denied" : "File not found");
}

function cmd_copy(args)
{
	var words = tokenize(args).filter(function (w) { return w.charAt(0) != '/'; });
	if (!words.length) {
		outln("Required parameter missing");
		return;
	}
	var src = words[0].split('+')[0];
	if (src.toUpperCase() == "CON") {
		critical_error("Write protect error writing drive C");
		return;
	}
	var result = resolve_spec(src);
	if (result.error) {
		report_error(result);
		return;
	}
	var list = list_entries(result.dir, [], result.pattern).filter(function (e) { return !e.dir; });
	if (!list.length) {
		outln("File not found - " + src.toUpperCase());
		outln("        0 file(s) copied");
		return;
	}
	if (words.length < 2) {
		outln("File cannot be copied onto itself");
		outln("        0 file(s) copied");
		return;
	}
	critical_error("Write protect error writing drive C");
	outln("        0 file(s) copied");
}

function cmd_ren(args)
{
	var words = tokenize(args);
	if (words.length < 2) {
		outln("Required parameter missing");
		return;
	}
	var result = resolve_spec(words[0]);
	if (result.error) {
		report_error(result);
		return;
	}
	var list = list_entries(result.dir, [], result.pattern).filter(function (e) { return !e.dir; });
	outln(list.length ? "Access denied" : "Duplicate file name or file not found");
}

function cmd_md(args)
{
	var words = tokenize(args);
	if (!words.length) {
		outln("Required parameter missing");
		return;
	}
	var drv = parse_drive(words[0]);
	if (drv.error)
		report_error(drv);
	else
		outln("Unable to create directory");
}

function cmd_rd(args)
{
	var words = tokenize(args);
	if (!words.length) {
		outln("Required parameter missing");
		return;
	}
	var result = resolve_dir(words[0]);
	if (result.critical)
		report_error(result);
	else if (result.dir && result.path.join("\\") == cwd.join("\\"))
		outln("Attempt to remove current directory - " + words[0]);
	else
		outln("Invalid path, not directory,\r\nor directory not empty");
}

function cmd_cls()
{
	console.clear("\x01n", /* autopause: */false);
	screen_cleared = true;
}

function cmd_exit()
{
	if (shell_depth > 0)
		shell_exit = true;
	else
		shell.logoff(/* fast: */true);
}

function cmd_pause()
{
	pause_prompt();
}

function cmd_break(args)
{
	var word = arg_text(args).trim().toUpperCase();
	if (word == "ON" || word == "OFF")
		break_on = (word == "ON");
	else if (word)
		outln("Must specify ON or OFF");
	else
		outln("BREAK is " + (break_on ? "on" : "off"));
}

function cmd_verify(args)
{
	var word = arg_text(args).trim().toUpperCase();
	if (word == "ON" || word == "OFF")
		verify_on = (word == "ON");
	else if (word)
		outln("Must specify ON or OFF");
	else
		outln("VERIFY is " + (verify_on ? "on" : "off"));
}

function cmd_chcp(args)
{
	var word = arg_text(args).trim();
	if (word && word != "437")
		outln("Invalid code page");
	else if (!word)
		outln("Active code page: 437");
}

function cmd_loadhigh(args)
{
	exec_line(arg_text(args), []);
}

var internal_commands = {
	BREAK:   { run: cmd_break,  desc: "Sets or clears extended CTRL+C checking.", usage: "BREAK [ON | OFF]" },
	CD:      { run: cmd_cd,     desc: "Displays the name of or changes the current directory.", usage: "CD [drive:][path]\r\nCD[..]" },
	CHCP:    { run: cmd_chcp,   desc: "Displays or sets the active code page number.", usage: "CHCP [nnn]" },
	CHDIR:   { run: cmd_cd,     desc: "Displays the name of or changes the current directory.", usage: "CHDIR [drive:][path]\r\nCHDIR[..]" },
	CLS:     { run: cmd_cls,    desc: "Clears the screen.", usage: "CLS" },
	COPY:    { run: cmd_copy,   desc: "Copies one or more files to another location.", usage: "COPY source [destination]" },
	DATE:    { run: cmd_date,   desc: "Displays or sets the date.", usage: "DATE [date]" },
	DEL:     { run: cmd_del,    desc: "Deletes one or more files.", usage: "DEL [drive:][path]filename" },
	DIR:     { run: cmd_dir,    desc: "Displays a list of files and subdirectories in a directory.", usage: "DIR [drive:][path][filename] [/P] [/W] [/A] [/O[[:]sortorder]] [/S] [/B] [/L]\r\n\r\n  /P   Pauses after each screenful of information.\r\n  /W   Uses wide list format.\r\n  /O   List by files in sorted order (N E S D G, - prefix to reverse).\r\n  /S   Displays files in specified directory and all subdirectories.\r\n  /B   Uses bare format (no heading information or summary).\r\n  /L   Uses lowercase.\r\n\r\nSwitches may be preset in the DIRCMD environment variable." },
	ECHO:    { run: cmd_echo,   desc: "Displays messages, or turns command-echoing on or off.", usage: "ECHO [ON | OFF]\r\nECHO [message]" },
	ERASE:   { run: cmd_del,    desc: "Deletes one or more files.", usage: "ERASE [drive:][path]filename" },
	EXIT:    { run: cmd_exit,   desc: "Quits the command interpreter (logs off of the BBS).", usage: "EXIT" },
	LH:      { run: cmd_loadhigh, desc: "Loads a program into the upper memory area.", usage: "LH [drive:][path]filename [parameters]" },
	LOADHIGH:{ run: cmd_loadhigh, desc: "Loads a program into the upper memory area.", usage: "LOADHIGH [drive:][path]filename [parameters]" },
	MD:      { run: cmd_md,     desc: "Creates a directory.", usage: "MD [drive:]path" },
	MKDIR:   { run: cmd_md,     desc: "Creates a directory.", usage: "MKDIR [drive:]path" },
	PATH:    { run: cmd_path,   desc: "Displays or sets a search path for executable files.", usage: "PATH [[drive:]path[;...]]\r\nPATH ;" },
	PAUSE:   { run: cmd_pause,  desc: "Suspends processing of a batch file and displays a message.", usage: "PAUSE" },
	PROMPT:  { run: cmd_prompt, desc: "Changes the command prompt.", usage: "PROMPT [text]\r\n\r\n  $Q = (equal sign)          $$ $ (dollar sign)\r\n  $T Current time            $D Current date\r\n  $P Current drive and path  $V MS-DOS version number\r\n  $N Current drive           $G > (greater-than sign)\r\n  $L < (less-than sign)      $B | (pipe)\r\n  $H Backspace               $E Escape code (ASCII code 27)\r\n  $_ Carriage return and linefeed" },
	RD:      { run: cmd_rd,     desc: "Removes (deletes) a directory.", usage: "RD [drive:]path" },
	REM:     { run: function () {}, desc: "Records comments (remarks) in a batch file.", usage: "REM [comment]" },
	REN:     { run: cmd_ren,    desc: "Renames a file or files.", usage: "REN [drive:][path]filename1 filename2" },
	RENAME:  { run: cmd_ren,    desc: "Renames a file or files.", usage: "RENAME [drive:][path]filename1 filename2" },
	RMDIR:   { run: cmd_rd,     desc: "Removes (deletes) a directory.", usage: "RMDIR [drive:]path" },
	SET:     { run: cmd_set,    desc: "Displays, sets, or removes MS-DOS environment variables.", usage: "SET [variable=[string]]" },
	TIME:    { run: cmd_time,   desc: "Displays or sets the system time.", usage: "TIME [time]" },
	TYPE:    { run: cmd_type,   desc: "Displays the contents of a text file.", usage: "TYPE [drive:][path]filename" },
	VER:     { run: cmd_ver,    desc: "Displays the MS-DOS (and BBS) version.", usage: "VER" },
	VERIFY:  { run: cmd_verify, desc: "Tells MS-DOS whether to verify that files are written correctly.", usage: "VERIFY [ON | OFF]" },
	VOL:     { run: cmd_vol,    desc: "Displays a disk volume label and serial number.", usage: "VOL [drive:]" },
};

function show_usage(item)
{
	outln(item.desc);
	if (item.usage) {
		outln("");
		outln(item.usage);
	}
}

// All the programs on the drive, for HELP
function all_programs(dir, path, list)
{
	var entries = list_dir(dir);
	for (var i = 0; i < entries.length; i++) {
		var entry = entries[i];
		if (entry.dir) {
			if (entry.name != "DOORS")
				all_programs(entry, path.concat(entry.name), list);
		} else if (entry.run && entry.desc)
			list.push({ entry: entry, path: path });
	}
	return list;
}

function help(args)
{
	var words = tokenize(args);
	if (words.length) {
		var name = words[0].toUpperCase();
		if (internal_commands[name]) {
			show_usage(internal_commands[name]);
			return;
		}
		var prog = find_program(words[0]);
		if (prog.entry && prog.entry.desc) {
			show_usage(prog.entry);
			return;
		}
		var progs = all_programs(root, [], []);
		for (var i = 0; i < progs.length; i++) {
			if (progs[i].entry.name == name) {
				show_usage(progs[i].entry);
				return;
			}
		}
		outln("Help not available for this command.");
		return;
	}
	outln("For more information on a specific command, type HELP command-name.");
	for (var name in internal_commands) {
		outln(rpad(name, 9) + internal_commands[name].desc);
		if (console.aborted)
			return;
	}
	progs = all_programs(root, [], []);
	outln("");
	outln("BBS programs:");
	for (var i = 0; i < progs.length && !console.aborted; i++) {
		var prog = progs[i];
		outln(rpad(prog.entry.name, 9) + rpad(path_str(prog.path), 10) + prog.entry.desc);
	}
	outln("");
	outln("External programs (doors) are in C:\\DOORS.");
}

/****************************************************************************/
/* Running programs                                                         */
/****************************************************************************/

// Locate a program the way COMMAND.COM does: .COM, .EXE, then .BAT, in the
// current directory then each PATH directory, unless a path is specified
function find_program(str)
{
	var sp = split_path(str);
	var fn = split_name(sp.file);
	if (!fn.name || is_wild(sp.file))
		return { error: "Bad command or file name" };
	var exts = EXEC_EXTS;
	if (sp.file.indexOf('.') >= 0) {
		if (EXEC_EXTS.indexOf(fn.ext) < 0)
			return { error: "Bad command or file name" };
		exts = [fn.ext];
	}
	var dirs = [];
	if (sp.dir) {
		var result = resolve_dir(sp.dir);
		if (result.critical)
			return result;
		if (result.dir)
			dirs.push(result.dir);
	} else {
		dirs.push(dir_at(cwd));
		var path = (dos_env.PATH || "").split(';');
		for (var i = 0; i < path.length; i++) {
			if (!path[i])
				continue;
			var pd = resolve_dir(path[i].charAt(0) == '\\' || /^[A-Za-z]:/.test(path[i])
				? path[i] : "\\" + path[i]);
			if (pd.dir)
				dirs.push(pd.dir);
		}
	}
	for (var d = 0; d < dirs.length; d++) {
		for (var e = 0; e < exts.length; e++) {
			var entry = find_entry(dirs[d], fn.name + "." + exts[e]);
			if (entry && (entry.run || entry.batch))
				return { entry: entry };
		}
	}
	return { error: "Bad command or file name" };
}

function run_batch(lines, params)
{
	var saved_echo = echo_on;
	for (var i = 0; i < lines.length && bbs.online && !js.terminated && !shell_exit; i++) {
		var line = expand_env(lines[i], params);
		var silent = false;
		if (line.charAt(0) == '@') {
			silent = true;
			line = line.substr(1);
		}
		if (!line.trim() || line.charAt(0) == ':')
			continue;
		if (echo_on && !silent) {
			outln("");
			outln(render_prompt() + line);
		}
		exec_line(line, params);
	}
	echo_on = saved_echo;
}

function run_command_com(args)
{
	var parsed = args.match(/^\s*\/C\s*([\s\S]*)$/i);
	if (shell_depth >= MAX_SHELL_DEPTH) {
		outln("Program too big to fit in memory");
		return;
	}
	var saved_env = JSON.parse(JSON.stringify(dos_env));
	var saved_echo = echo_on;
	shell_depth++;
	try {
		if (parsed)
			exec_line(parsed[1], []);
		else {
			show_banner();
			command_loop();
		}
	} finally {
		shell_depth--;
		shell_exit = false;
		dos_env = saved_env;
		echo_on = saved_echo;
	}
}

function exec_line(line, params)
{
	line = line.replace(/^[\s@]+/, "");
	if (!line)
		return;
	var redir = line.match(/[<>|]/);
	if (redir) {
		if (redir[0] == '<')
			outln("File not found");
		else
			critical_error("Write protect error writing drive C");
		return;
	}
	var sc = split_command(line);
	if (!sc.cmd) {
		outln("Bad command or file name");
		return;
	}
	var name = sc.cmd.toUpperCase();
	var help_req = /^\s*\/\?/.test(sc.args);

	if (/^[A-Za-z]:$/.test(name)) {			// Change drive
		var drv = parse_drive(name);
		if (drv.error)
			report_error(drv);
		return;
	}
	var internal = internal_commands[name];
	if (internal) {
		if (help_req)
			show_usage(internal);
		else
			internal.run(sc.args, params);
		return;
	}
	var prog = find_program(sc.cmd);
	if (prog.error) {
		report_error(prog);
		return;
	}
	if (help_req) {
		show_usage(prog.entry);
		return;
	}
	if (prog.entry.batch)
		run_batch(prog.entry.batch, [sc.cmd].concat(tokenize(sc.args)));
	else
		prog.entry.run(sc.args);
}

/****************************************************************************/
/* The command interpreter                                                  */
/****************************************************************************/

function show_banner()
{
	outln("");
	outln("Microsoft(R) MS-DOS(R) Version " + DOS_VERSION);
	outln("             (C)Copyright Microsoft Corp 1981-1991.");
}

function command_loop()
{
	var blank = true;
	screen_cleared = false;
	while (bbs.online && !js.terminated && !shell_exit) {
		bbs.node_action = NODE_MAIN;
		bbs.nodesync();
		console.line_counter = 0;
		console.aborted = false;
		pager = null;

		if (echo_on) {
			if (!blank && !screen_cleared)
				out("\r\n");
			out("\x01n" + render_prompt());
		}
		screen_cleared = false;
		var line = console.getstr("", MAX_CMDLINE, K_NONE, history);
		blank = !line.trim();
		if (blank)
			continue;
		if (history[0] != line)
			history.unshift(line);
		if (history.length > 20)
			history.length = 20;
		bbs.log_str(line + ", ");
		bbs.main_cmds++;

		line = line.replace(/^\s+/, "");
		if (line.charAt(0) == ';') {		// Synchronet string commands
			load({}, "str_cmds.js", line.substr(1));
			continue;
		}
		exec_line(line, []);
	}
}

if (typeof SDOS_NO_MAIN == 'undefined') {
	load("termsetup.js");
	shell = load({}, "shell_lib.js");
	if (!restore_state())
		init_env();

	// The BBS re-runs the shell after some commands (e.g. SETUP), so only
	// "boot" once per session
	if (!bbs.mods.sdos_booted) {
		// Give a bogus DOS error so they think they're in real DOS
		console.print("\x01nSpecified COMMAND search directory bad\r\n");
		show_banner();
		bbs.mods.sdos_booted = true;
	}
	command_loop();
}
