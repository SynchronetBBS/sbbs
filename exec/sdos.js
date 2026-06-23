// DOS shell for Synchronet
// replaces sdos.src/.bin

"use strict";

require("sbbsdefs.js", "FI_REMOVE");
require("nodedefs.js", "NODE_MAIN");

var shell = load({}, "shell_lib.js");
var sdos_boot_shown = false;

if (!sdos_boot_shown) {
	console.clear();
	console.print("\x01nSpecified COMMAND search directory bad\r\n\r\n");
	console.print("Microsoft(R) MS-DOS(R) Version 5.00\r\n");
	console.print("             (C)Copyright Microsoft Corp 1981-1991.\r\n");
	sdos_boot_shown = true;
}

var current_dir = "ROOT";

function parse_cmd(str) {
	var parts = str.trim().split(/\s+/);
	return {
		cmd: parts[0].toUpperCase(),
		arg: parts.slice(1).join(" "),
	};
}

function curdir() {
	return file_area.lib_list[bbs.curlib].dir_list[bbs.curdir];
}

function handle_semicolon(cmd) {
	if (!cmd || cmd[0] !== ";") return false;

	var scmd = cmd.substring(1).trim();
	if (!scmd.length) {
		console.print("Bad command or file name\r\n");
		return true;
	}

	bbs.exec("?str_cmds", scmd);
	return true;
}

while (bbs.online && !js.terminated) {
	bbs.node_action = NODE_MAIN;
	bbs.nodesync();

	switch (current_dir) {
		case "ROOT":
			console.print("\r\n\x01nC:\\>");
			var raw = console.getstr(60, K_TRIM);
			if (!raw) break;

			if (handle_semicolon(raw)) break;

			bbs.log_str(raw);
			bbs.main_cmds++;

			var parsed = parse_cmd(raw);
			var cmd = parsed.cmd;
			var arg = parsed.arg;

			switch (cmd) {
				case "CD":
					if (!arg) {
						console.print("Invalid directory\r\n");
						break;
					}
					var d = arg.replace(/^\\/, "").toUpperCase();
					if (d === "MAIL") current_dir = "MAIL";
					else if (d === "FILES") current_dir = "FILES";
					else console.print("Invalid directory\r\n");
					break;
				case "DIR":
					bbs.menu("sdos/root");
					break;
				case "CHAT":
					bbs.chat_sec();
					break;
				case "OPEN":
					arg
						? bbs.exec_xtrn(arg)
						: console.print("External program name missing\r\n");
					break;
				case "DOORS":
					bbs.xtrn_sec();
					break;
				case "GFILES":
					bbs.text_sec();
					break;
				case "AUTOMSG":
					bbs.auto_msg();
					break;
				case "SETUP":
					bbs.user_config();
					bbs.exec("termsetup.js");
					exit();
					break;
				case "LOGOFF":
					if (shell.batch_list && shell.batch_list.length > 0)
						shell.batch_menu();
					bbs.logoff();
					break;
				case "VER":
					bbs.ver(0, false);
					break;

				case "EXIT":
					shell.logoff(true);
					break;

				case "TYPE":
					arg
						? shell_sdos_type(arg)
						: console.print("Required parameter missing\r\n");
					break;

				case "SET":
					console.print("COMSPEC=C:\\COMMAND.COM\r\n");
					console.print("PROMPT=$p$g\r\n");
					console.print("PATH=C:\\\r\n");
					console.print("SBBSCTRL=C:\\SBBS\\CTRL\r\n");
					console.print("SBBSNODE=C:\\BBS\\NODE@NODE@\\\r\n");
					console.print("SBBSNNUM=@NODE@\r\n");
					break;
				case "ECHO":
					console.print((arg || "ECHO is on") + "\r\n");
					break;
				case "DEL":
				case "ERASE":
					if (!arg || !arg.trim().length) {
						console.print("Required parameter missing\r\n");
						break;
					}
					console.print("Sharing violation\r\n");
					break;
				case "COPY":
					if (!arg || !arg.trim().length) {
						console.print("Required parameter missing\r\n");
						break;
					}
					console.print("Sharing violation\r\n");
					break;
				case "FILES":
					current_dir = "FILES";
					break;

				default:
					console.print("Bad command or file name\r\n");
			}
			break;
		case "FILES":
			if (bbs.curlib === undefined || bbs.curdir === undefined) {
				bbs.curlib = 0;
				bbs.curdir = 0;
				bbs.curdir_code = file_area.dir_list[0].code;
			}

			console.print("\r\n\x01nC:\\FILES>");
			var raw = console.getstr(60, K_TRIM);
			if (!raw) break;

			if (handle_semicolon(raw)) break;

			bbs.log_str(raw);
			bbs.main_cmds++;

			var parsed = parse_cmd(raw);
			var fcmd = parsed.cmd;
			var farg = parsed.arg;

			switch (fcmd) {
				case "CD":
					if (!farg) {
						console.print("Invalid directory\r\n");
						break;
					}
					var d2 = farg.replace(/^\\/, "").toUpperCase();
					if (d2 === "..") current_dir = "ROOT";
					else if (d2 === "MAIL") current_dir = "MAIL";
					else console.print("Invalid directory\r\n");
					break;
				case "DIR":
					bbs.menu("sdos/files");
					break;
				case "BATCH":
					bbs.batch_menu();
					break;
				case "DOWNLOAD":
					farg
						? shell.download_user_files(farg)
						: shell.download_user_files();
					break;
				case "EXTENDED": {
					var spec = farg ? farg.trim() : "";
					if (!spec) spec = bbs.get_filespec();
					if (!spec) break;

					var full = curdir().path + spec;
					if (!file_exists(full)) console.print("File not found\r\n");

					shell.filespec = spec;
					shell.view_file_info(FI_INFO);
					break;
				}
				case "FIND":
					bbs.scan_dirs(FL_FINDDESC);
					break;
				case "AREA":
					shell.select_file_area();
					break;
				case "LIST": {
					var dirnum = curdir().number;

					if (farg) {
						if (!bbs.list_files(dirnum, farg, 0))
							console.print("File not found\r\n");
						break;
					}

					var files = directory(curdir().path + "*.*");
					if (!files || !files.length) {
						console.print("File not found\r\n");
						break;
					}

					shell.view_file_info(FI_INFO);
					break;
				}
				case "NEWSCAN":
					bbs.scan_dirs(FL_ULTIME);
					break;
				case "REMOVE": {
					var d = file_area.dir[bbs.curdir_code];
					if (!d || !user.compare_ars(d.delete_ars)) {
						console.print("You can't remove files\r\n");
						break;
					}

					var spec = farg ? farg.trim() : "";
					if (!spec) spec = bbs.get_filespec();
					if (!spec) break;

					bbs.node_file = spec;
					shell.view_file_info(FI_REMOVE);
					break;
				}
				case "SEARCH":
					bbs.scan_dirs(FL_NO_HDR);
					break;
				case "TEMP":
					bbs.temp_xfer();
					break;
				case "UPLOAD":
					if (
						file_exists(system.text_dir + "menu/upload.asc") ||
						file_exists(system.text_dir + "menu/upload.msg")
					)
						bbs.menu("upload");
					shell.upload_file();
					break;
				case "VIEW": {
					var spec = farg ? farg.trim() : "";
					if (!spec) spec = bbs.get_filespec();
					if (!spec) break;

					var full = curdir().path + spec;
					if (!file_exists(full)) {
						console.print("File not found\r\n");
						break;
					}

					shell.filespec = spec;
					shell.view_file_info(FI_INFO);
					break;
				}
				case "CONFIG":
					js.exec("filescancfg.js", {});
					break;

				default:
					console.print("Bad command or file name\r\n");
			}
			break;
		case "MAIL":
			console.print("\r\n\x01nC:\\MAIL>");
			var raw = console.getstr(60, K_TRIM);
			if (!raw) break;

			if (handle_semicolon(raw)) break;

			bbs.log_str(raw);
			bbs.main_cmds++;

			var parsed = parse_cmd(raw);
			var mcmd = parsed.cmd;
			var marg = parsed.arg;

			switch (mcmd) {
				case "CD":
					if (!marg) {
						console.print("Invalid directory\r\n");
						break;
					}
					var d3 = marg.replace(/^\\/, "").toUpperCase();
					if (d3 === "..") current_dir = "ROOT";
					else if (d3 === "FILES") current_dir = "FILES";
					else console.print("Invalid directory\r\n");
					break;
				case "DIR":
					bbs.menu("sdos/mail");
					break;
				case "SEND":
					console.print(
						"\x01_\r\n\x01b\x01hE-mail (User name or number): \x01w",
					);
					var input = console.getstr(40, K_TRIM);
					if (!input) break;
					if (input.toUpperCase() === "SYSOP") input = "1";
					var usernum = !isNaN(input)
						? parseInt(input, 10)
						: system.matchuser(input);
					if (!usernum) {
						console.print("\r\nUser not found.\r\n");
						break;
					}
					bbs.email(usernum, WM_EMAIL);
					break;
				case "SENDFILE":
					console.print(
						"\x01_\r\n\x01b\x01hE-mail (User name or number): \x01w",
					);
					var input = console.getstr(40, K_TRIM);
					if (!input) break;
					if (input.toUpperCase() === "SYSOP") input = "1";
					var usernum = !isNaN(input)
						? parseInt(input, 10)
						: system.matchuser(input);
					if (!usernum) {
						console.print("\r\nUser not found.\r\n");
						break;
					}
					bbs.email(usernum, WM_FILE);
					break;
				case "READ":
					bbs.read_mail();
					break;
				case "FEEDBACK":
					shell.send_feedback();
					break;
				case "NETMAIL": {
					console.print("\r\n\x01b\x01hNetMail Address: \x01w");
					var addr = console.getstr(60, K_TRIM);
					if (!addr) break;

					var allow =
						(bbs._netmail_misc | bbs._inetmail_misc) &
						bbs.NMAIL_FILE;
					if (allow && console.yesno("\r\nAttach a file"))
						bbs.netmail(addr, WM_FILE);
					else bbs.netmail(addr, WM_NONE);
					break;
				}
				case "READSENT":
					bbs.read_mail(MAIL_SENT, user.number);
					break;
				case "AREA":
					shell.select_msg_area();
					break;
				case "CONFIG":
					bbs.user_config();
					exit();
					break;
				case "POST":
					bbs.post_msg();
					break;
				case "NEWSCAN":
					bbs.scan_subs(SCAN_NEW);
					break;
				case "FIND":
					bbs.scan_subs(SCAN_FIND);
					break;
				case "YOURMSGS":
					bbs.scan_subs(SCAN_TOYOU);
					break;
				case "QWK":
					bbs.qwk_sec();
					break;
				case "READMSGS":
					bbs.scan_msgs();
					break;

				default:
					console.print("Bad command or file name\r\n");
			}
			break;
	}
}

function shell_sdos_type(arg) {
	arg = arg.toUpperCase();

	if (arg === "NODES") return bbs.list_nodes();
	if (arg === "WHO") return bbs.whos_online();
	if (arg === "LOGON") return bbs.list_logons();
	if (arg === "USERS") return bbs.list_users();
	if (arg === "SYSTEM") return bbs.sys_info();
	if (arg === "YOUR") return bbs.user_info();

	console.print("File not found - " + arg + "\r\n");
}