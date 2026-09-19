// FTP Client for Synchronet
// replaces ftp.src/.bin (the FTP protocol is handled by load/ftp.js)

// @format.tab-size 4

"use strict";

require("sbbsdefs.js", "K_NOECHO");
require("ftp.js", "FTP");

var ON = "\x01gON";
var OFF = "\x01rOFF";
var HASH_MARK = "#";

var ftp = null;			// The FTP object, while connected
var passive = true;		// Passive (PASV/EPSV) data connections
var ascii = false;		// ASCII (CR/LF translated) transfers
var hash = true;		// Print a hash mark for each block transferred
var debug = false;		// Echo commands sent to the server
var echo_rsp = true;	// Echo the server's responses

// Remote servers can send anything: don't let it drive the terminal
function remote_text(str)
{
	return String(str).replace(/[\x00-\x08\x0b\x0c\x0e-\x1f]/g, "");
}

function print_error(e)
{
	console.print("\x01h\x01r!" + remote_text(e.message ? e.message : e) + "\x01n\r\n");
}

function on_off(enabled)
{
	return enabled ? ON : OFF;
}

function toggle(desc, enabled)
{
	console.print("\x01h\x01c" + desc + ": " + on_off(enabled) + "\x01n\r\n");
	return enabled;
}

// The last component of a (local or remote) path
function filename_of(path)
{
	return path.split(/[\/\\]/).pop();
}

function progress(total)
{
	if (hash)
		console.print(HASH_MARK);
}

// Echo the commands and responses the way ftp.src did, via its FTP_ECHO_*
// modes. Done to the prototype (before any connection) so the login
// conversation is echoed too.
var ftp_cmd = FTP.prototype.cmd;
FTP.prototype.cmd = function (command, needresp) {
	if (debug && command !== undefined)
		console.print("\x01h\x01y---> " + remote_text(command) + "\x01n\r\n");
	var response = ftp_cmd.apply(this, arguments);
	if (echo_rsp && response)
		console.print("\x01h\x01w" + remote_text(response) + "\x01n");
	return response;
};

function connect(addr)
{
	var port = 21;
	var words = addr.split(/\s+/);
	addr = words[0];
	if (words.length > 1 && Number(words[1]))
		port = Number(words[1]);
	else if (/^[^:]+:\d+$/.test(addr)) {	// host:port
		port = Number(addr.split(":")[1]);
		addr = addr.split(":")[0];
	}
	if (!addr) {
		console.print("address: ");
		addr = console.getstr(60, K_TRIM);
		if (!addr)
			return;
	}
	console.print("login: ");
	var username = console.getstr(60, K_TRIM);
	if (!username)
		username = "anonymous";
	console.print("password: ");
	var password = console.getstr(60, K_NOECHO | K_TRIM);
	console.newline();
	if (!password)
		password = user.netmail;
	console.print(format("Connecting to %s ...\r\n", addr));
	try {
		ftp = new FTP(addr, username, password, port);
	} catch (e) {
		ftp = null;
		print_error(e);
		return;
	}
	ftp.progress = progress;
	ftp.passive = passive;
	ftp.ascii = ascii;
}

function disconnect()
{
	try {
		ftp.quit();
	} catch (e) {
		print_error(e);
	}
	ftp = null;
}

function help()
{
	console.print("\r\n\x01hCommands are\x01n:\r\n\r\n");
	console.print("\x01h\x01yopen   \x01w: \x01cstart an FTP session\r\n");
	console.print("\x01h\x01yclose  \x01w: \x01cclose an FTP session\r\n");
	console.print("\x01h\x01ydir    \x01w: \x01cprint a directory listing\r\n");
	console.print("\x01h\x01ypwd    \x01w: \x01cprint working directory\r\n");
	console.print("\x01h\x01ycd     \x01w: \x01cchange working directory\r\n");
	console.print(format("\x01h\x01ypasv   \x01w: \x01ctoggle passive mode transfers (currently %s\x01c)\r\n", on_off(passive)));
	console.print(format("\x01h\x01yascii  \x01w: \x01ctoggle ASCII (CR/LF) mode transfers (currently %s\x01c)\r\n", on_off(ascii)));
	console.print(format("\x01h\x01yhash   \x01w: \x01ctoggle hash printing during transfer (currently %s\x01c)\r\n", on_off(hash)));
	console.print(format("\x01h\x01ydebug  \x01w: \x01ctoggle command debugging (currently %s\x01c)\r\n", on_off(debug)));
	console.print("\x01h\x01yget    \x01w: \x01cretrieve (download) file\r\n");
	console.print("\x01h\x01yput    \x01w: \x01csend (upload) file\r\n");
	console.print("\x01h\x01ydelete \x01w: \x01cdelete (erase) file\r\n");
	console.print("\x01h\x01yquit   \x01w: \x01cexit FTP\r\n");
	console.newline();
}

// Prompt for an argument the user didn't supply on the command line
function arg_or_prompt(arg, prompt)
{
	if (arg)
		return arg;
	console.print(prompt);
	return console.getstr(128, K_TRIM);
}

function list(path)
{
	try {
		console.print(remote_text(ftp.list(path)));
	} catch (e) {
		print_error(e);
	}
}

function print_dir()
{
	try {
		var path = ftp.pwd();
		if (path === null)
			console.print("!pwd failed\r\n");
		else if (!echo_rsp)	// Otherwise the response was just echoed
			console.print(remote_text(path) + "\r\n");
	} catch (e) {
		print_error(e);
	}
}

function change_dir(arg)
{
	var path = arg_or_prompt(arg, "Directory: ");
	if (!path)
		return;
	try {
		if (!ftp.cwd(path))
			console.print("!cd failed\r\n");
	} catch (e) {
		print_error(e);
	}
}

function delete_file(arg)
{
	var path = arg_or_prompt(arg, "File: ");
	if (!path)
		return;
	try {
		if (!ftp.dele(path))
			console.print("!delete failed\r\n");
	} catch (e) {
		print_error(e);
	}
}

// Download to the temp directory, then send the file to the user
function get_file(arg)
{
	var path = arg_or_prompt(arg, "File: ");
	if (!path)
		return;
	var dest = system.temp_dir + filename_of(path);
	try {
		ftp.get(path, dest);
	} catch (e) {
		console.newline();
		print_error(e);
		file_remove(dest);
		return;
	}
	console.newline();
	bbs.send_file(dest);
	file_remove(dest);
}

// Receive the file from the user, then upload it
function put_file(arg)
{
	var path = arg_or_prompt(arg, "File: ");
	if (!path)
		return;
	var src = system.temp_dir + filename_of(path);
	if (!bbs.receive_file(src))
		return;
	if (!file_exists(src))
		return;
	console.print(format("ftp: sending %s\r\n", filename_of(path)));
	try {
		ftp.put(src, path);
	} catch (e) {
		console.newline();
		print_error(e);
	}
	console.newline();
	file_remove(src);
}

function command(line)
{
	var space = line.search(/\s/);
	var cmd = (space < 0 ? line : line.substr(0, space)).toLowerCase();
	var arg = space < 0 ? "" : line.substr(space + 1).trim();

	switch (cmd) {
		case "?":
		case "help":
			help();
			return true;
		case "open":
			if (ftp)
				disconnect();
			connect(arg);
			return true;
		case "pasv":
			passive = toggle("Passive file transfer mode", !passive);
			if (ftp)
				ftp.passive = passive;
			return true;
		case "ascii":
			ascii = toggle("ASCII file transfer mode", !ascii);
			if (ftp)
				ftp.ascii = ascii;
			return true;
		case "hash":
			hash = toggle("Hash mark printing", !hash);
			return true;
		case "debug":
			debug = toggle("Command/response debugging", !debug);
			return true;
		case "quit":
			if (ftp)
				disconnect();
			return false;
	}

	if (!ftp) {
		console.print("Not connected (use '\x01i?\x01n' for menu).\r\n");
		return true;
	}

	// All commands from here on require a connection
	switch (cmd) {
		case "close":
			disconnect();
			break;
		case "dir":
		case "ls":
			list(arg);
			break;
		case "pwd":
			print_dir();
			break;
		case "cd":
			change_dir(arg);
			break;
		case "delete":
			delete_file(arg);
			break;
		case "get":
			get_file(arg);
			break;
		case "put":
			put_file(arg);
			break;
		default:
			console.print("Invalid command (use '\x01i?\x01n' for menu).\r\n");
	}
	return true;
}

function main()
{
	console.print("\r\n\x01h\x01ySynchronet \x01wFTP Client v2.00 \x01y- Copyright 2026 Rob Swindell\x01n\r\n\r\n");

	if (argv[0] && argv[0].charAt(0) != '*')
		connect(argv.join(" "));

	while (bbs.online && !js.terminated) {
		console.print("\x01n\x01hftp\x01n> ");
		var line = console.getstr(128, K_TRIM);
		if (console.aborted) {
			console.aborted = false;
			break;
		}
		if (!line)
			continue;
		if (!command(line))
			break;
	}
}

try {
	main();
} finally {
	if (ftp) {
		try {
			ftp.quit();
		} catch (e) {}
		ftp = null;
	}
}
