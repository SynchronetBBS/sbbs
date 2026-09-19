// Simple Synchronet Command Shell - for beginner/first-time users
// replaces simple.src/.bin

// @format.tab-size 4

"use strict";

require("sbbsdefs.js", "K_UPPER");
require("nodedefs.js", "NODE_MAIN");
load("termsetup.js");

var shell = load({}, "shell_lib.js");

function run_script(name)
{
	var script = system.mods_dir + name;
	if (!file_exists(script))
		script = system.exec_dir + name;
	js.exec(script, {});
}

// Display a menu and return the command the user types
function get_command(menu)
{
	bbs.node_action = NODE_MAIN;
	console.clear();
	bbs.nodesync();
	bbs.menu("simple/" + menu);
	bbs.menu("simple/prompt");
	console.print("\x01n");
	var cmd = console.getstr(10, K_UPPER);
	if (console.aborted) {
		console.aborted = false;
		cmd = "";
	}
	console.newline();
	console.line_counter = 0;
	if (cmd)
		bbs.log_str(cmd);
	return cmd;
}

function send_email()
{
	console.print("\r\n\x01b\x01hSend E-mail to (User name or number): \x01n");
	var name = console.getstr(40, K_UPRLWR | K_TRIM);
	if (!name)
		return;
	if (name.toUpperCase() == "SYSOP")
		name = "1";
	shell.send_email(name, console.noyes("\r\nAttach a file") ? WM_NONE : WM_FILE);
}

function post_message()
{
	if (shell.select_msg_area())
		bbs.post_msg();
}

function read_area()
{
	if (shell.select_msg_area())
		bbs.scan_msgs();
}

function new_msg_scan()
{
	console.print("\x01l\x01b\x01hScanning for new messages...\r\n");
	bbs.scan_subs(SCAN_NEW, /* all: */true);
}

function your_msg_scan()
{
	console.print("\x01l\x01b\x01hScanning for your messages...\r\n");
	bbs.scan_subs(SCAN_TOYOU, /* all: */true);
}

function list_area()
{
	if (shell.select_file_area())
		shell.list_files();
}

function new_file_scan()
{
	console.putmsg("\r\n\x01b\x01hUse \x01c@NEWFILETIME@\x01b for new file scan ");
	if (!console.yesno("date/time")) {
		var t = bbs.get_newscantime(bbs.new_file_time);
		if (t === null)
			return;
		bbs.new_file_time = t;
	}
	bbs.scan_dirs(FL_ULTIME, /* all: */true);
}

function download()
{
	if (bbs.batch_dnload_total && console.yesno(bbs.text(bbs.text.DownloadBatchQ))) {
		bbs.batch_download();
		return;
	}
	console.print("\r\nEnter the filename or wildcard to download\r\n");
	var spec = bbs.get_filespec();
	if (spec)
		shell.view_file_info(FI_DOWNLOAD, spec);
}

function upload()
{
	if (shell.select_file_area())
		shell.upload_file();
}

function logoff()
{
	shell.logoff(/* fast: */false);
}

function sub_menu(menu)
{
	while (bbs.online && !js.terminated) {
		var cmd = get_command(menu.file);
		if (cmd == "Q")
			break;
		if (menu.command[cmd])
			menu.command[cmd]();
	}
}

var send_menu = {
	file: "sendmsg",
	command: {
		E: send_email,
		EMAIL: send_email,
		P: post_message,
		POST: post_message,
		N: function () { shell.send_netmail(); },
		NETMAIL: function () { shell.send_netmail(); },
	},
};

var read_menu = {
	file: "readmsg",
	command: {
		E: function () { bbs.read_mail(); },
		EMAIL: function () { bbs.read_mail(); },
		F: function () { bbs.read_mail(MAIL_SENT); },
		A: read_area,
		ALL: function () { bbs.scan_msgs(); },
		B: function () { bbs.text_sec(); },
		N: new_msg_scan,
		NEW: new_msg_scan,
		Y: your_msg_scan,
		YOU: your_msg_scan,
		YOUR: your_msg_scan,
		C: function () { run_script("msgscancfg.js"); },
	},
};

var file_menu = {
	file: "filelist",
	command: {
		A: list_area,
		N: new_file_scan,
		F: function () {
			console.print("\r\n\x01c\x01hFind Text in File Descriptions (no wildcards)\r\n");
			bbs.scan_dirs(FL_FINDDESC, /* all: */true);
		},
		S: function () {
			console.print("\r\n\x01c\x01hSearch for Filename(s)\r\n");
			bbs.scan_dirs(FL_NO_HDR, /* all: */true);
		},
	},
};

var main_menu = {
	C: function () { bbs.chat_sec(); },
	CHAT: function () { bbs.chat_sec(); },
	Q: function () { bbs.qwk_sec(); },
	S: function () { sub_menu(send_menu); },
	R: function () { sub_menu(read_menu); },
	F: function () { sub_menu(file_menu); },
	O: function () { bbs.xtrn_sec(); },
	OPEN: function () { bbs.xtrn_sec(); },
	DOOR: function () { bbs.xtrn_sec(); },
	DOORS: function () { bbs.xtrn_sec(); },
	A: function () { bbs.user_config(); exit(); },
	P: function () { bbs.private_message(); console.line_counter = 0; },
	L: function () { console.clear(); bbs.whos_online(); },
	D: download,
	U: upload,
	G: logoff,
	BYE: logoff,
	GOODBYE: logoff,
	OFF: logoff,
	LOGOFF: logoff,
};

// A friendlier pause prompt, restored when leaving this shell
bbs.replace_text("Pause", "\x01n\x01h\x01b{\x01w@ContinueQ@? @Yes@/@No@\x01b} ");
js.on_exit("bbs.revert_text('Pause')");

while (bbs.online && !js.terminated) {
	var cmd = get_command("main");
	if (main_menu[cmd])
		main_menu[cmd]();
	else if (cmd)	// Anything else is a string command (no ';' prefix needed)
		load({}, "str_cmds.js", cmd);
}
