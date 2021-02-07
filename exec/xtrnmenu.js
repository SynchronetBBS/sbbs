/**
 * Xtrn Menu Mod
 * Custom External Program Menus
 * by Michael Long mlong  innerrealmbbs.us
 *
 * This is the loadable module that displays the custom external menus
 * in terminal server (telnet/rlogin/ssh)
 *
 * To jump to a specific menu, pass the ID as an argument
 *
 * To set options, add to modopts.ini [xtrnmenu]
 *
 * See instructions at http://wiki.synchro.net/module:xtrnmenu
 */

"use strict";

require("sbbsdefs.js", "K_NONE");

require("xtrnmenulib.js", "MENU_LOADED");

const ansiterm = load({}, 'ansiterm_lib.js');

var options, xsec = -1;

//// Main
var ExternalMenus = new ExternalMenus();
const menuconfig = ExternalMenus.menuconfig;

var i,j;

var gamesrv = false;

if ((argv[0] == 'command') && (typeof argv[1] !== "undefined")) {
	docommand(argv[1], (typeof argv[2] !== "undefined" ? argv[2] : ""), (typeof argv[3] !== "undefined" ? argv[3] : ""));
} else if (argv[0] === 'gamesrv') {
	gamesrv = true;
	external_menu_custom();
} else if ((argv[0] === 'pre') && (typeof argv[1] !== "undefined")) {
	dopre(argv[1]);
} else if ((argv[0] === 'post') && (typeof argv[1] !== "undefined")) {
	dopost(argv[1]);
} else {
	for (i in argv) {
		for (j in xtrn_area.sec_list) {
			if (argv[i].toLowerCase() == xtrn_area.sec_list[j].code)
				xsec = j;
		}
	}
	
	if (xsec > -1) {
		// if its the id of a standard section menu
		if (options.use_xtrn_sec) {
			// stock menu
			js.exec("xtrn_sec.js", {}, xsec);
		} else {
			external_menu_custom(xsec);
		}
	} else if (typeof argv[0] !== "undefined") {
		// if its not a section menu, assume it is a custom menu
		external_menu_custom(argv[0]);
	} else {
		// main custom menu
		external_menu_custom();
	}
}

// Runs pre-door stat tracking
function dopre(progCode)
{
	require("nodedefs.js", "NODE_XTRN");
	if (bbs.node_action != NODE_XTRN) {
		log(LOG_DEBUG, "xtrnmenu pre: Skipping " + progCode + " because its not during a normal door run");
		// don't want to track door stats for login events
		return;
	}
	
	var options = ExternalMenus.getOptions('custommenu', 'main');
	
	if (!options.json_enabled) {
		log(LOG_DEBUG, "xtrnmenu pre: Skipping " + progCode + " because JSON is not enabled");
		return;
	}
	
	if (typeof options.blacklist_tracking_xtrncodes !== "undefined") {
		var blacklist_xtrncodes = options.blacklist_tracking_xtrncodes.split(',');
		for (var b=0; b < blacklist_xtrncodes.length; b++) {
			if (blacklist_xtrncodes[b].toLowerCase() == progCode.toLowerCase()) {
				log(LOG_DEBUG, "xtrnmenu pre: Skipping " + progCode + " because in blacklist_tracking_xtrncodes");
				return;
			}
		}
	}
	
	// get section of this program and block tracking if its in blacklist config
	var secCode;
	for (var j in xtrn_area.sec_list) {
		for (var k in xtrn_area.sec_list[j].prog_list) {
			if (xtrn_area.sec_list[j].prog_list[k].code.toLowerCase() == progCode.toLowerCase()) {
				secCode = xtrn_area.sec_list[j].code;
			}
		}
	}
	if (typeof options.blacklist_tracking_xtrnsec !== "undefined") {
		var blacklist_seccodes = options.blacklist_tracking_xtrnsec.split(',');
		for (var b=0; b < blacklist_seccodes.length; b++) {
			if (blacklist_seccodes[b].toLowerCase() == secCode) {
				log(LOG_DEBUG, "xtrnmenu pre: Skipping " + progCode + " because in blacklist_tracking_xtrnsec " + secCode);
				return;
			}
		}
	}
	
	try {
		require("json-client.js", "JSONClient");
		var jsonClient = new JSONClient(options.json_host, options.json_port);
		jsonClient.callback = ExternalMenus.processUpdate;
	} catch (e) {
		log(LOG_ERR, "xtrnmenu pre: Could not initialize JSON database so door tracking is now disabled: " + e);
		return;
	}
	
	// global times a door is run
	var globalLaunchNum = jsonClient.read("xtrnmenu", "launchnum", 1);
	
	if (typeof globalLaunchNum === "undefined") {
		globalLaunchNum = {};
	}
	
	if (typeof globalLaunchNum[progCode] !== "undefined") {
		globalLaunchNum[progCode]++;
	} else {
		globalLaunchNum[progCode] = 1;
	}
	log(LOG_DEBUG, "xtrnmenu pre: globalLaunchNum " + progCode + " = " + globalLaunchNum[progCode]);
	
	jsonClient.write("xtrnmenu", "launchnum", globalLaunchNum, 2);
	// user's times a door is run
	var userLaunchNum = jsonClient.read("xtrnmenu", "launchnum_user_" + user.alias, 1);
	if (!userLaunchNum) {
		userLaunchNum = {};
	}
	
	if (typeof userLaunchNum[progCode] !== "undefined") {
		userLaunchNum[progCode]++;
	} else {
		userLaunchNum[progCode] = 1;
	}
	log(LOG_DEBUG, "xtrnmenu pre: userLaunchNum " + progCode + " = " + userLaunchNum[progCode]);
	jsonClient.write("xtrnmenu", "launchnum_user_" + user.alias,userLaunchNum, 2);
	
	// global launch start time (most recent)
	var globalLaunchStart = jsonClient.read("xtrnmenu", "launchstart", 1);
	if (!globalLaunchStart) {
		globalLaunchStart = {};
	}
	
	globalLaunchStart[progCode] = time();
	log(LOG_DEBUG, "xtrnmenu pre: globalLaunchStart " + progCode + " = " + globalLaunchStart[progCode] );
	jsonClient.write("xtrnmenu", "launchstart", globalLaunchStart, 2);
	
	// user launch start time (most recent)
	var userLaunchStart = jsonClient.read("xtrnmenu", "launchstart_user_" + user.alias, 1);
	if (!userLaunchStart) {
		userLaunchStart = {};
	}
	
	userLaunchStart[progCode] = time();
	log(LOG_DEBUG, "xtrnmenu pre: userLaunchStart " + progCode + " = " + userLaunchStart[progCode] );
	jsonClient.write("xtrnmenu", "launchstart_user_" + user.alias, userLaunchStart, 2);
	
	jsonClient.cycle();
}


// Runs post-door stat tracking
function dopost(progCode)
{
	require("nodedefs.js", "NODE_XTRN");
	if (bbs.node_action != NODE_XTRN) {
		log(LOG_DEBUG, "xtrnmenu post: Skipping " + progCode + " because its not during a normal door run");
		// don't want to track door stats for login events
		return;
	}
	
	var options = ExternalMenus.getOptions('custommenu', 'main');
	
	if (!options.json_enabled) {
		log(LOG_DEBUG, "xtrnmenu post: Skipping " + progCode + " because JSON is not enabled");
		return;
	}
	
	if (typeof options.blacklist_tracking_xtrncodes !== "undefined") {
		var blacklist_xtrncodes = options.blacklist_tracking_xtrncodes.split(',');
		for (var b=0; b < blacklist_xtrncodes.length; b++) {
			if (blacklist_xtrncodes[b].toLowerCase() == progCode.toLowerCase()) {
				log(LOG_DEBUG, "xtrnmenu post: Skipping " + progCode + " because in blacklist_tracking_xtrncodes");
				return;
			}
		}
	}
	
	// get section of this program and block tracking if its in blacklist config
	var secCode;
	for (var j in xtrn_area.sec_list) {
		for (var k in xtrn_area.sec_list[j].prog_list) {
			if (xtrn_area.sec_list[j].prog_list[k].code.toLowerCase() == progCode.toLowerCase()) {
				secCode = xtrn_area.sec_list[j].code;
			}
		}
	}
	if (typeof options.blacklist_tracking_xtrnsec !== "undefined") {
		var blacklist_seccodes = options.blacklist_tracking_xtrnsec.split(',');
		for (var b=0; b < blacklist_seccodes.length; b++) {
			if (blacklist_seccodes[b].toLowerCase() == secCode) {
				log(LOG_DEBUG, "xtrnmenu post: Skipping " + progCode + " because in blacklist_tracking_xtrnsec " + secCode);
				return;
			}
		}
	}
	
	try {
		require("json-client.js", "JSONClient");
		var jsonClient = new JSONClient(options.json_host, options.json_port);
		jsonClient.callback = ExternalMenus.processUpdate;
	} catch (e) {
		log(LOG_ERR, "xtrnmenu pre: Could not initialize JSON database so door tracking is now disabled: " + e);
		return;
	}
	
	// user time ran
	var newTimeRan;
	var userLaunchStart = jsonClient.read("xtrnmenu", "launchstart_user_" + user.alias, 1);
	if (userLaunchStart) {
		
		if (typeof userLaunchStart[progCode] !== "undefined") {
			var lasttime = userLaunchStart[progCode];
			var now = time();
			var runtime = now - lasttime;
			
			var userTimeRan = jsonClient.read("xtrnmenu", "timeran_user_" + user.alias, 1);
			if (!userTimeRan) {
				// first time
				userTimeRan = {};
			}
			
			if (typeof userTimeRan[progCode] === "undefined") {
				userTimeRan[progCode] = runtime;
			} else {
				userTimeRan[progCode] = userTimeRan[progCode] + runtime;
			}
			jsonClient.write("xtrnmenu", "timeran_user_" + user.alias, userTimeRan, 2);
			
			// global time ran
			var globalTimeRan = jsonClient.read("xtrnmenu", "timeran", 1);
			if (typeof globalTimeRan === "undefined") {
				globalTimeRan = {};
			}
			
			if (typeof globalTimeRan[progCode] === "undefined") {
				globalTimeRan[progCode] = runtime;
			} else {
				globalTimeRan[progCode] = globalTimeRan[progCode] + runtime;
			}
			jsonClient.write("xtrnmenu", "timeran", globalTimeRan, 2);
			
			jsonClient.cycle();
		}
	}
}

// Runs custom commands, for gamesrv
function docommand(command, commandparam1, commandparam2)
{
	var options = ExternalMenus.getOptions('custommenu', 'main');
	
	switch (command) {
		case 'checkmail':
			var lmsg = user.stats.unread_mail_waiting;
			if (lmsg > 0) {
				console.crlf();
				if (console.yesno(user.stats.unread_mail_waiting + " message(s) waiting. Read now")) {
					bbs.read_mail();
				}
			} else {
				console.putmsg('\r\n\x01gNo New Messages');
			}
			mswait(1500);
			console.clear();
			break;
		case 'feedback':
			var subject = options.custom.feedback_subject !== undefined
				? options.custom.feedback_subject: "Game Server Feedback\r\n";
			bbs.email(1, WM_EMAIL, "", subject);
			console.putmsg(options.custom.feedback_msg !== undefined ?
				options.custom.feedback_msg : 'Thank you for your Feedback, @SYSOP@ will get back to you ASAP!\r\n\r\n');
			break;
		case 'prefs':
			bbs.user_config();
			break;
		case 'sysop':
			require("str_cmds.js", "str_cmds");
			console.putmsg("\r\n\x01gCommand: ");
			var commandstr;
			commandstr = console.getstr();
			str_cmds(commandstr);
			break;
		case 'textsec':
			bbs.text_sec();
			break;
		case 'filearea':
			if(file_exists(system.text_dir+"menu/tmessage.*"))
				bbs.menu("tmessage");
			filearea(commandparam1, commandparam2);
			break;
		case 'chat':
			load("chat_sec.js");
			break;
		default:
			doerror("Unknown command " + command);
			break;
	}
}

// Present a basic download area
// Taken from classic shell
function filearea(filelib, filedir) {
	var options = ExternalMenus.getOptions('custommenu', 'main');

	var key;
	
	if (!filelib || !filedir) {
		writeln("Error: No file area or dir selected");
		return;
	}
	
	bbs.curlib = filelib;
	bbs.curdir = filedir;
	
	file_transfers:
		while (1) {
			console.clear();
			
			if (!bbs.menu("xtrnmenu_xfer", P_NOERROR)) {
				console.crlf();
				writeln("\x01c\x01h[B]\x01n\x01c Batch");
				writeln("\x01c\x01h[D]\x01n\x01c Download");
				writeln("\x01c\x01h[F]\x01n\x01c Find Text in Descriptions");
				writeln("\x01c\x01h[V]\x01n\x01c View Files");
				writeln("\x01c\x01h[L]\x01n\x01c List Files");
				writeln("\x01c\x01h[S]\x01n\x01c Search Filename");
				writeln("\x01c\x01h[Q]\x01n\x01c Quit");
			}
			
			console.crlf();
			
			// Update node status
			bbs.node_action = NODE_XFER;
			bbs.node_sync();
			
			// Display main Prompt
			console.print(typeof options.custom.xfer_prompt !== "undefined" ? 
				options.custom.xfer_prompt : "\x01n\x01c\xfe \x01b\x01hFile \x01n\x01c\xfe \x01h");
			if (bbs.compare_ars("exempt T"))
				console.putmsg("@TUSED@", P_SAVEATR);
			else
				console.putmsg("@TLEFT@", P_SAVEATR);
			console.putmsg(typeof options.custom.xfer_prompt2 !== "undefined" ?
				options.custom.xfer_prompt2 : " \x01n\x01c@DIR@: \x01n");
			
			// Get key (with / extended commands allowed)
			var str = console.getkey().toLowerCase();
			
			// Commands
			switch (str) {
				case ';':
					require("str_cmds.js", "str_cmds");
					console.putmsg(typeof options.custom.command_prompt !== "undefined" ?
						options.custom.command_prompt : "\r\n\x01gCommand: ");
					var commandstr = console.getstr();
					str_cmds(commandstr);
					break;
					
				case '!':
					if (bbs.compare_ars("SYSOP")) {
						bbs.menu("sysxfer");
					}
					break;
					
				case 'b':
					bbs.batch_menu();
					break;
				
				case 'd':
					console.print(typeof options.custom.download_prompt !== "undefined" ?
						options.custom.download_prompt : "\r\n\x01c\x01hDownload File(s)\r\n");
					
					if (bbs.batch_dnload_total > 0) {
						if (console.yesno(bbs.text(DownloadBatchQ))) {
							bbs.batch_download();
							break;
						}
					}
					str = bbs.get_filespec();
					if ((str == null) || (file_area.lib_list.length == 0))
						break;
					if (user.security.restrictions & UFLAG_D) {
						console.putmsg(bbs.text(R_Download), P_SAVEATR);
						break;
					}
					if (!bbs.list_file_info(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number, str, FI_DOWNLOAD)) {
						var s = 0;
						console.putmsg(bbs.text(SearchingAllDirs), P_SAVEATR);
						for (i = 0; i < file_area.lib_list[bbs.curlib].dir_list.length; i++) {
							if (i != bbs.curdir &&
								(s = bbs.list_file_info(file_area.lib_list[bbs.curlib].dir_list[i].number, str, FI_DOWNLOAD)) != 0) {
								if (s == -1 || str.indexOf('?') != -1 || str.indexOf('*') != -1) {
									continue file_transfers;
								}
							}
						}
						console.putmsg(bbs.text(SearchingAllLibs), P_SAVEATR);
						for (i = 0; i < file_area.lib_list.length; i++) {
							if (i == bbs.curlib)
								continue;
							for (j = 0; j < file_area.lib_list[i].dir_list.length; j++) {
								if ((s = bbs.list_file_info(file_area.lib_list[i].dir_list[j].number, str, FI_DOWNLOAD)) != 0) {
									if (s == -1 || str.indexOf('?') != -1 || str.indexOf('*') != -1) {
										continue file_transfers;
									}
								}
							}
						}
					}
					break;
					
				case 'f':
					console.print(typeof options.custom.finddesc_prompt !== "undefined" ?
						options.custom.finddesc_prompt : "\r\n\x01c\x01hFind Text in File Descriptions (no wildcards)\r\n");
					bbs.scan_dirs(FL_FINDDESC, false);
					break;
					
				case 'i':
					file_info();
					break;
					
				case 'l':
					i = bbs.list_files();
					if (i == -1)
						break;
					if (i == 0)
						console.putmsg(bbs.text(EmptyDir), P_SAVEATR);
					else
						console.putmsg(format(bbs.text(NFilesListed), i), P_SAVEATR);
					break;
					
				case 'r':
					console.print(typeof options.custom.remove_prompt !== "undefined" ?
						options.custom.remove_prompt : "\r\n\x01c\x01hRemove/Edit File(s)\r\n");
					str = bbs.get_filespec();
					if (str == null)
						break;
					if (!bbs.list_file_info(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number, str, FI_REMOVE)) {
						var s = 0;
						console.putmsg(bbs.text(SearchingAllDirs), P_SAVEATR);
						for (i = 0; i < file_area.lib_list[bbs.curlib].dir_list.length; i++) {
							if (i != bbs.curdir &&
								(s = bbs.list_file_info(file_area.lib_list[bbs.curlib].dir_list[i].number, str, FI_REMOVE)) != 0) {
								if (s == -1 || str.indexOf('?') != -1 || str.indexOf('*') != -1) {
									continue file_transfers;
								}
							}
						}
						console.putmsg(bbs.text(SearchingAllLibs), P_SAVEATR);
						for (i = 0; i < file_area.lib_list.length; i++) {
							if (i == bbs.curlib)
								continue;
							for (j = 0; j < file_area.lib_list[i].dir_list.length; j++) {
								if ((s = bbs.list_file_info(file_area.lib_list[i].dir_list[j].number, str, FI_REMOVE)) != 0) {
									if (s == -1 || str.indexOf('?') != -1 || str.indexOf('*') != -1) {
										continue file_transfers;
									}
								}
							}
						}
					}
					break;
					
				case 'q':
				case "\x1B":
				case KEY_LEFT:
					return;
				
				case 's':
					console.print(typeof options.custom.searchfname_prompt !== "undefined" ?
						options.custom.searchfname_prompt : "\r\n\x01c\x01hSearch for Filename(s)\r\n");
					bbs.scan_dirs(FL_NO_HDR);
					break;
					
				case 't':
					bbs.temp_xfer();
					break;
					
				case 'v':
					console.print(typeof options.custom.view_prompt !== "undefined" ?
						options.custom.view_prompt : "\r\n\x01c\x01hView File(s)\r\n");
					str = bbs.get_filespec();
					if (str == null)
						continue file_transfers;
					if (!bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].number, str, FL_VIEW)) {
						console.putmsg(bbs.text(SearchingAllDirs), P_SAVEATR);
						for (i = 0; i < file_area.lib_list[bbs.curlib].dir_list.length; i++) {
							if (i == bbs.curdir)
								continue;
							if (bbs.list_files(file_area.lib_list[bbs.curlib].dir_list[i].number, str, FL_VIEW))
								break;
						}
						if (i < file_area.lib_list[bbs.curlib].dir_list.length)
							continue file_transfers;
						console.putmsg(bbs.text(SearchingAllLibs), P_SAVEATR);
						for (i = 0; i < file_area.lib_list.length; i++) {
							if (i == bbs.curlib)
								continue;
							for (j = 0; j < file_area.lib_list[i].dir_list.length; j++) {
								if (bbs.list_files(file_area.lib_list[i].dir_list[j].number, str, FL_VIEW))
									continue file_transfers;
							}
						}
					}
					break;
				
				default:
					break;
			}
			console.crlf();
		}
}

// Renders the menu
function external_menu_custom(menuid)
{
	var i, menucheck, menuobj, item_multicolumn_fmt, item_singlecolumn_fmt,
		item_multicolumn_fmt_inverse, item_singlecolumn_fmt_inverse,
		cost, multicolumn, selected_index = 0, menuitemsfiltered = [];
	var validkeys = []; // valid chars on menu selection
	var keymax = 0; // max integer allowed on menu selection
	
	if (menuid == undefined) {
		menuid = 'main';
	}
	
	var options = ExternalMenus.getOptions('custommenu', menuid);
	
	menuobj = ExternalMenus.getMenu(menuid);
	
	// Allow overriding auto-format on a per-menu basis
	var multicolumn_fmt = options.multicolumn_fmt;
	var singlecolumn_fmt = options.singlecolumn_fmt;
	var multicolumn_fmt_inverse = options.multicolumn_fmt_inverse;
	var singlecolumn_fmt_inverse = options.singlecolumn_fmt_inverse;
	
	while (bbs.online) {
		// Turn off pausing while displaying the menu. If it pauses, the kb nav 
		// will not be able to work properly
		bbs.sys_status|=SS_PAUSEOFF;

		console.aborted = false;
		
		if (typeof menuobj === "undefined") {
			menuobj = ExternalMenus.getSectionMenu(menuid);
			if (typeof menuobj === "undefined") {
				doerror(options.custom_menu_not_found_msg.replace('%MENUID%', menuid));
				break;
			}
		}
		
		if (options.clear_screen) {
			console.clear(LIGHTGRAY);
		}
		
		if (user.security.restrictions&UFLAG_X) {
			write(options.restricted_user_msg);
			break;
		}
		
		if (!xtrn_area.sec_list.length) {
			write(options.no_programs_msg);
			break;
		}
		
		// empty menu
		if (!menuobj.items.length && (menuid != "main")) {
			write(options.no_programs_msg);
			break;
		}
		
		system.node_list[bbs.node_num-1].aux = 0; /* aux is 0, only if at menu */
		bbs.node_action = NODE_XTRN;
		bbs.node_sync();
		
		menuitemsfiltered = ExternalMenus.getSortedItems(menuobj);
		
		// The quit item is intended to aid in the lightbar navigation
		if (gamesrv && (menuid == 'main')) {
			menuitemsfiltered.push({
				input: 'Q',
				title: options.custom_logoff_msg,
				target: '',
				type: 'quit',
			});
		} else if (menuid == 'main') {
			menuitemsfiltered.push({
				input: 'Q',
				title: options.custom.quit_msg,
				target: '',
				type: 'quit',
			});
		} else {
			menuitemsfiltered.unshift({
				input: 'Q',
				title: options.custom.return_msg,
				target: '',
				type: 'quit',
			});
		}
		
		if (!bbs.menu("xtrnmenu_head_" + menuid, P_NOERROR) && !bbs.menu("xtrnmenu_head", P_NOERROR)) {
			bbs.menu("xtrn_head", P_NOERROR);
		}
		
		// if file exists text/menu/xtrnmenu_(menuid).[rip|ans|mon|msg|asc], 
		// then display that, otherwise dynamiic
		if (!bbs.menu("xtrnmenu_" + menuid, P_NOERROR)) {
			
			// determine lines left. we can't know the size of the footer so 
			// let the sysop use singlecolumn_margin to specify that. Below
			// calcution will always leave room for titles and underline even
			// if they aren't rendered
			var linesleft = console.screen_rows - console.line_counter - options.singlecolumn_margin 
				- 2 - 2; // -2 for header_fmt/crlf and -2 for crlf and footer
			if(options.titles.trimRight() != '') linesleft = linesleft - 1;
			if(options.underline.trimRight() != '') linesleft = linesleft - 2;
			
			multicolumn = menuitemsfiltered.length  > linesleft;
			
			// if no custom menu file in text/menu, create a dynamic one
			printf(options.header_fmt, menuobj.title);
			if(options.titles.trimRight() != '')
				write(options.titles);
			if(multicolumn) {
				write(options.multicolumn_separator);
				if (options.titles.trimRight() != '')
					write(options.titles);
			}
			if(options.underline.trimRight() != '') {
				console.crlf();
				write(options.underline);
			}
			if(multicolumn) {
				write(options.multicolumn_separator);
				if (options.underline.trimRight() != '')
					write(options.underline);
			}
			console.crlf();
			
			// n is the number of items for the 1st column
			var n;
			if (multicolumn) {
				n = Math.floor(menuitemsfiltered.length / 2) + (menuitemsfiltered.length & 1);
			} else {
				n = menuitemsfiltered.length;
			}
			
			// j is the index for each menu item on 2nd column
			var j = n; // start j at the first item for 2nd column
			for (i = 0; i < n && !console.aborted; i++) {
				cost = "";
				if (menuitemsfiltered[i].type == "xtrnprog") {
					// if its an external program, get the cost
					cost = xtrn_area.prog[menuitemsfiltered[i].target.toLowerCase()].cost;
				}
				
				console.add_hotspot(menuitemsfiltered[i].input.toString());
				
				validkeys.push(menuitemsfiltered[i].input.toString());
				var intCheck = Number(menuitemsfiltered[i].input);
				if (!intCheck.isNaN) {
					if (intCheck > keymax) {
						keymax = menuitemsfiltered[i].input;
					}
				}
				
				// allow overriding format on a per-item basis
				// great for featuring a specific game
				var checkkey = menuitemsfiltered[i].target.toLowerCase() + '-multicolumn_fmt';
				item_multicolumn_fmt = (typeof options[checkkey] !== "undefined") ?
					options[checkkey] : options.multicolumn_fmt;
				var checkkeyinv = menuitemsfiltered[i].target.toLowerCase() + '-multicolumn_fmt_inverse';
				item_multicolumn_fmt_inverse = (typeof options[checkkeyinv] !== "undefined") ?
					options[checkkeyinv] : options.multicolumn_fmt_inverse;
				
				checkkey = menuitemsfiltered[i].target.toLowerCase() + '-singlecolumn_fmt';
				checkkeyinv = menuitemsfiltered[i].target.toLowerCase() + '-singlecolumn_fmt_inverse';
				item_singlecolumn_fmt = (typeof options[checkkey] !== "undefined") ?
					options[checkkey] : options.singlecolumn_fmt;
				item_singlecolumn_fmt_inverse = (typeof options[checkkeyinv] !== "undefined") ?
					options[checkkeyinv] : options.singlecolumn_fmt_inverse;
				
				if (selected_index == i) {
					printf(
						menuitemsfiltered[i].type == 'quit' ? options.return_multicolumn_fmt_inverse : item_multicolumn_fmt_inverse,
						menuitemsfiltered[i].input.toString().toUpperCase(),
						menuitemsfiltered[i].title,
						menuitemsfiltered[i].type == 'quit' ? '' : cost
					);
				} else {
					printf(
						menuitemsfiltered[i].type == 'quit' ? options.return_multicolumn_fmt : item_multicolumn_fmt,
						menuitemsfiltered[i].input.toString().toUpperCase(),
						menuitemsfiltered[i].title,
						menuitemsfiltered[i].type == 'quit' ? '' : cost
					);
				}
				
				if (multicolumn) {
					if (typeof menuitemsfiltered[j] !== "undefined") {
						validkeys.push(menuitemsfiltered[j].input.toString());
						
						var intCheck = Number(menuitemsfiltered[j].input);
						if (!intCheck.isNaN) {
							if (intCheck > keymax) {
								keymax = menuitemsfiltered[j].input;
							}
						}
						
						// allow overriding format on a per-item basis
						// great for featuring a specific game
						var checkkey = menuitemsfiltered[j].target.toLowerCase() + '-multicolumn_fmt';
						item_multicolumn_fmt = (typeof options[checkkey] !== "undefined") ?
							options[checkkey] : options.multicolumn_fmt;
						var checkkeyinv = menuitemsfiltered[j].target.toLowerCase() + '-multicolumn_fmt_inverse';
						item_multicolumn_fmt_inverse = (typeof options[checkkeyinv] !== "undefined") ?
							options[checkkeyinv] : options.multicolumn_fmt_inverse;
						
						write(options.multicolumn_separator);
						console.add_hotspot(menuitemsfiltered[j].input.toString());
						
						if (selected_index == j) {
							printf(
								menuitemsfiltered[j].type == 'quit' ? options.return_multicolumn_fmt_inverse : item_multicolumn_fmt_inverse,
								menuitemsfiltered[j].input.toString().toUpperCase(),
								menuitemsfiltered[j].title,
								menuitemsfiltered[j].type == 'quit' ? '' : cost
							);
						} else {
							printf(
								menuitemsfiltered[j].type == 'quit' ? options.return_multicolumn_fmt : item_multicolumn_fmt,
								menuitemsfiltered[j].input.toString().toUpperCase(),
								menuitemsfiltered[j].title,
								menuitemsfiltered[j].type == 'quit' ? '' : cost
							);
						}
					} else {
						write(options.multicolumn_separator);
					}
					j++;
				}
				console.crlf();
			}
			
			if (gamesrv) {
				if (!bbs.menu("xtrn_gamesrv_tail_" + menuid, P_NOERROR)) {
					bbs.menu("xtrn_gamesrv_tail", P_NOERROR);
				}
			}
			
			if (!bbs.menu("xtrnmenu_tail_" + menuid, P_NOERROR) && !bbs.menu("xtrnmenu_tail", P_NOERROR)) {
				bbs.menu("xtrn_tail", P_NOERROR);
			}
			
			bbs.node_sync();
			console.mnemonics(options.which);
			
			// turn back on pausing
			bbs.sys_status&=~SS_PAUSEOFF;
		}
		
		validkeys.push('q');
		validkeys.push("\x1B");
		validkeys.push(KEY_LEFT);
		validkeys.push(KEY_RIGHT);
		validkeys.push(KEY_UP);
		validkeys.push(KEY_DOWN);
		validkeys.push("\x0d"); //enter
		
		var keyin, keyin2;
		var maxkeylen = 0;
		var maxfirstkey = 0;
		var morekeys = [];

		for (var k in validkeys) {
			if (validkeys[k] == "\x1B") {
				continue;
			}
			if (validkeys[k].length > maxkeylen) {
				maxkeylen = validkeys[k].length;
			}
			if (validkeys[k].length > 1) {
				morekeys.push(validkeys[k].toString().toLowerCase().substring(0,1));
			}
		}
		
		// get first key
		keyin = console.getkey();
		keyin = keyin.toString().toLowerCase();
		
		if (gamesrv && ((keyin == "\x1B")) && (menuid == 'main')) {
			// don't have ESC do anything on the main menu
		} else if ((keyin == 'q') || (keyin == "\x1B")) {
			if (gamesrv && (menuid == 'main')) {
				// doing it this way rather than calling bbs.logoff()
				// so that the prompt defaults to Yes
				console.crlf();
				if (console.yesno("Do you wish to logoff")) {
					bbs.logoff(false);
				}
			} else {
				console.clear();
				return;
			}
		} else if (keyin == KEY_UP) {
			--selected_index;
			if (selected_index < 0) {
				selected_index = menuitemsfiltered.length - 1;
			}
			continue;
		} else if (keyin == KEY_DOWN) {
			++selected_index;
			if (selected_index > (menuitemsfiltered.length - 1)) {
				selected_index = 0;
			}
			continue;
		} else if (keyin == KEY_LEFT) {
			var row_limit = n -1;
			if (selected_index == 0) {
				// first item, go back menu
				if (gamesrv && (menuid == 'main')) {
					bbs.logoff();
				} else {
					console.clear();
					return;
				}
			} else if (selected_index <= row_limit) {
				// on first col
				var new_selected_index = selected_index + n;
				if (typeof menuitemsfiltered[new_selected_index] === "undefined") {
					// no item on right, no change
				} else {
					selected_index = new_selected_index;
				}
			} else {
				// on second col
				selected_index = selected_index - n;
			}
		} else if (keyin == KEY_RIGHT) {
			var row_limit = n - 1;
			if (selected_index <= row_limit) {
				// on first col
				var new_selected_index = selected_index + n;
				if (typeof menuitemsfiltered[new_selected_index] === "undefined") {
					// no item on right, no change
				} else {
					selected_index = new_selected_index;
				}
			} else {
				// on second col
				selected_index = selected_index - n;
			}
		}
		
		// if ENTER on a QUIT item, exit
		if (keyin == "\x0d") {
			if (menuitemsfiltered[selected_index].type == "quit") {
				if (gamesrv && (menuid == 'main')) {
					bbs.logoff();
				} else {
					console.clear();
					return;
				}
			}
		}
		
		// The logic below is to make it not require enter for as
		// many items as possible
		
		// if max keys is 2 and they entered something that might have
		// a second digit/char, then get the key
		if ((maxkeylen == 2) && (keyin !== "\x0d")) {
			if (morekeys.indexOf(keyin) !== -1) {
				write(keyin);
				keyin2 = console.getkey(); // either the second digit or enter
				if ((keyin2 !== "\r") && (keyin2 !== "\n") && (keyin2 !== "\r\n")) {
					keyin = keyin + keyin2.toLowerCase();
				}
			}
		} else if ((maxkeylen > 2) && (keyin !== "\x0d")) {
			// there there are more than 99 items, then just use getkeys 
			// for the rest
			write(keyin);
			keyin2 = console.getkeys(validkeys, keymax);
			keyin = keyin + keyin2.toLowerCase();
		}
		
		if (keyin) {
			
			menuitemsfiltered.some(function (menuitemfiltered) {
				var menutarget = menuitemfiltered.target ? menuitemfiltered.target.toLowerCase() : null;
				var menuinput = menuitemfiltered.input.toString().toLowerCase();
				
				if ((menuinput == keyin) || 
					((keyin == "\x0d") && (menuitemsfiltered[selected_index].input == menuitemfiltered.input))) {
					// update the selected index so the selected item is correct in the
					// menu rendering if they used a key
					for (var mm=0; mm<=menuitemsfiltered.length;mm++) {
						if (menuitemsfiltered[mm].input == menuitemfiltered.input) {
							selected_index = mm;
							break;
						}
					}
					
					switch (menuitemfiltered.type) {
						// custom menu, defined in xtrnmenu.json
						case 'custommenu':
							external_menu_custom(menutarget);
							return true;
						// external program section
						case 'xtrnmenu':
							if (options.use_xtrn_sec) {
								js.exec("xtrn_sec.js", {}, menutarget);
							} else {
								external_menu_custom(menutarget);
							}
							return true;
						// external program
						case 'xtrnprog':
							// run the external program
							if (typeof xtrn_area.prog[menutarget] !== "undefined") {
								bbs.exec_xtrn(menutarget);
								return true;
							} else {
								doerror(options.custom_menu_program_not_found_msg.replace('%PROGRAMID%', menutarget));
							}
							break;
						case 'command':
							bbs.exec(menutarget);
							return true;
						case 'recentall':
						case 'recentuser':
						case 'mostlaunchedall':
						case 'mostlauncheduser':
						case 'longestrunall':
						case 'longestrunuser':
							special_menu(menuitemfiltered.type, menuitemfiltered.title, menutarget);
							return true;
						case 'search':
							search_menu(menuitemfiltered.title, menutarget);
							return true;
						case 'favorites':
							favorites_menu(menuitemfiltered.title, menutarget);
							return true;
					} //switch
				} // if menu item matched keyin
			}); // foreach menu item
		} // if keyin
	} // main bbs.online loop
}


// Renders the special menu (recent, most launched, etc.)
function special_menu(menutype, title, itemcount) {
	if (itemcount === undefined) {
		itemcount = 0;
	}
	
	var i, menucheck, item_multicolumn_fmt, item_singlecolumn_fmt,
		item_multicolumn_fmt_inverse, item_singlecolumn_fmt_inverse,
		multicolumn, selected_index = 0, menuitemsfiltered = [];
	var validkeys = []; // valid chars on menu selection
	var keymax = 0; // max integer allowed on menu selection
	
	var menuid = menutype;
	
	var options = ExternalMenus.getOptions('custommenu', menuid);
	
	while (bbs.online) {
		console.aborted = false;
		
		if (options.clear_screen) {
			console.clear(LIGHTGRAY);
		}
		
		if (user.security.restrictions&UFLAG_X) {
			write(options.restricted_user_msg);
			break;
		}
		
		var menuobj = ExternalMenus.getSpecial(menutype, title, itemcount);
		if (!bbs.menu("xtrnmenu_head_" + menuid, P_NOERROR) && !bbs.menu("xtrnmenu_head", P_NOERROR)) {
			bbs.menu("xtrn_head", P_NOERROR);
		}
		
		if ((menuobj === undefined) || (!menuobj.items.length)) {
			write(options.no_programs_msg);
			break;
		}
		
		menuitemsfiltered = menuobj.items;
		
		// The quit item is intended to aid in the lightbar navigation
		menuitemsfiltered.unshift({
			input: 'Q',
			title: options.custom.return_msg,
			target: '',
			type: 'quit',
		});
		
		// determine lines left. we can't know the size of the footer so 
		// let the sysop use singlecolumn_margin to specify that. Below
		// calcution will always leave room for titles and underline even
		// if they aren't rendered
		var linesleft = console.screen_rows - console.line_counter - options.singlecolumn_margin
			- 2 - 2; // -2 for header_fmt/crlf and -2 for crlf and footer
		if(options.titles.trimRight() != '') linesleft = linesleft - 1;
		if(options.underline.trimRight() != '') linesleft = linesleft - 2;
		
		multicolumn = menuitemsfiltered.length  > linesleft;
		
		// if no custom menu file in text/menu, create a dynamic one
		printf(options.header_fmt, title);
		if(options.titles.trimRight() != '')
			write(options.titles);
		if(multicolumn) {
			write(options.multicolumn_separator);
			if (options.titles.trimRight() != '')
				write(options.titles);
		}
		if(options.underline.trimRight() != '') {
			console.crlf();
			write(options.underline);
		}
		if(multicolumn) {
			write(options.multicolumn_separator);
			if (options.underline.trimRight() != '')
				write(options.underline);
		}
		console.crlf();
		
		// n is the number of items for the 1st column
		var n;
		if (multicolumn) {
			n = Math.floor(menuitemsfiltered.length / 2) + (menuitemsfiltered.length & 1);
		} else {
			n = menuitemsfiltered.length;
		}
		
		// j is the index for each menu item on 2nd column
		var j = n; // start j at the first item for 2nd column
		for (i = 0; i < n && !console.aborted; i++) {
			console.add_hotspot(menuitemsfiltered[i].input.toString());
			
			validkeys.push(menuitemsfiltered[i].input.toString());
			
			if (menuitemsfiltered[i].input > keymax) {
				keymax = menuitemsfiltered[i].input;
			}
			
			if (i == selected_index) {
				var itemformatret = multicolumn ? options.return_multicolumn_special_fmt_inverse : options.return_singlecolumn_special_fmt_inverse;
				var itemformat = multicolumn ? options.multicolumn_fmt_special_inverse : options.singlecolumn_fmt_special_inverse
				printf(
					menuitemsfiltered[i].type == 'quit' ? itemformatret : itemformat, 
					menuitemsfiltered[i].input,
					menuitemsfiltered[i].title,
					menuitemsfiltered[i].type == 'quit' ? '' : menuitemsfiltered[i].stats
				);
			} else {
				var itemformatret = multicolumn ? options.return_multicolumn_special_fmt : options.return_singlecolumn_special_fmt;
				var itemformat = multicolumn ? options.multicolumn_fmt_special : options.singlecolumn_fmt_special
				printf(
					menuitemsfiltered[i].type == 'quit' ? itemformatret : itemformat,
					menuitemsfiltered[i].input,
					menuitemsfiltered[i].title,
					menuitemsfiltered[i].type == 'quit' ? '' : menuitemsfiltered[i].stats
				);
			}

			if (multicolumn) {
				if (typeof menuitemsfiltered[j] !== "undefined") {
					validkeys.push(menuitemsfiltered[j].input.toString());

					if (menuitemsfiltered[j].input > keymax) {
						keymax = menuitemsfiltered[i].input;
					}

					write(options.multicolumn_separator);
					console.add_hotspot(menuitemsfiltered[j].input.toString());
					
					if (j == selected_index) {
						printf(
							menuitemsfiltered[j].type == 'quit' ? options.return_multicolumn_special_fmt_inverse : options.multicolumn_fmt_special_inverse,
							menuitemsfiltered[j].input,
							menuitemsfiltered[j].title,
							menuitemsfiltered[j].type == 'quit' ? '' : menuitemsfiltered[j].stats
						);
					} else {
						printf(
							menuitemsfiltered[j].type == 'quit' ? options.return_multicolumn_special_fmt : options.multicolumn_fmt_special,
							menuitemsfiltered[j].input,
							menuitemsfiltered[j].title,
							menuitemsfiltered[j].type == 'quit' ? '' : menuitemsfiltered[j].stats
						);
					}
				} else {
					write(options.multicolumn_separator);
				}
				j++;
			}
			console.crlf();
		}
		
		if (gamesrv) {
			if (!bbs.menu("xtrn_gamesrv_tail_" + menuid, P_NOERROR)) {
				bbs.menu("xtrn_gamesrv_tail", P_NOERROR);
			}
		}
		
		if (!bbs.menu("xtrnmenu_tail_" + menuid, P_NOERROR) && !bbs.menu("xtrnmenu_tail", P_NOERROR)) {
			bbs.menu("xtrn_tail", P_NOERROR);
		}
		
		bbs.node_sync();
		console.mnemonics(options.which);
		
		validkeys.push('q');
		validkeys.push("\x1B");
		validkeys.push(KEY_LEFT);
		validkeys.push(KEY_RIGHT);
		validkeys.push(KEY_UP);
		validkeys.push(KEY_DOWN);
		validkeys.push("\x0d"); //enter
		
		var keyin, keyin2;
		var maxkeylen = 0;
		var maxfirstkey = 0;
		var morekeys = [];
		var k;
		for (k in validkeys) {
			if (validkeys[k] == "\x1B") {
				continue;
			}
			if (validkeys[k].length > maxkeylen) {
				maxkeylen = validkeys[k].length;
			}
			if (validkeys[k].length > 1) {
				morekeys.push(validkeys[k].toString().toLowerCase().substring(0,1));
			}
		}
		
		// get first key
		keyin = console.getkey();
		keyin = keyin.toString().toLowerCase();
		
		// if ENTER on a QUIT item, exit
		if (keyin == "\x0d") {
			if (menuitemsfiltered[selected_index].type == "quit") {
				console.clear();
				return;
			}
		}
		
		if ((keyin == 'q') || (keyin == "\x1B")) {
			console.clear();
			return;
		} else if (keyin == KEY_UP) {
			--selected_index;
			if (selected_index < 0) {
				selected_index = menuitemsfiltered.length - 1;
			}
			continue;
		} else if (keyin == KEY_DOWN) {
			++selected_index;
			if (selected_index > (menuitemsfiltered.length - 1)) {
				selected_index = 0;
			}
			continue;
		} else if (keyin == KEY_LEFT) {
			var row_limit = n -1;
			if (selected_index == 0) {
				// first item, go back menu
				console.clear();
				return;
			} else if (selected_index <= row_limit) {
				// on first col
				var new_selected_index = selected_index + n;
				if (typeof menuitemsfiltered[new_selected_index] === "undefined") {
					// no item on right, no change
				} else {
					selected_index = new_selected_index;
				}
			} else {
				// on second col
				selected_index = selected_index - n;
			}
		} else if (keyin == KEY_RIGHT) {
			var row_limit = n - 1;
			if (selected_index <= row_limit) {
				// on first col
				var new_selected_index = selected_index + n;
				if (typeof menuitemsfiltered[new_selected_index] === "undefined") {
					// no item on right, no change
				} else {
					selected_index = new_selected_index;
				}
			} else {
				// on second col
				selected_index = selected_index - n;
			}
		}		
		
		// The logic below is to make it not require enter for as
		// many items as possible
		
		// if max keys is 2 and they entered something that might have
		// a second digit/char, then get the key
		if ((maxkeylen == 2) && (keyin !== "\x0d")) {
			if (morekeys.indexOf(keyin) !== -1) {
				write(keyin);
				keyin2 = console.getkey(); // either the second digit or enter
				if ((keyin2 !== "\r") && (keyin2 !== "\n") && (keyin2 !== "\r\n")) {
					keyin = keyin + keyin2.toLowerCase();
				}
			}
		} else if ((maxkeylen > 2) && (keyin !== "\x0d")) {
			// there there are more than 99 items, then just use getkeys 
			// for the rest
			write(keyin);
			keyin2 = console.getkeys(validkeys, keymax);
			keyin = keyin + keyin2.toLowerCase();
		}
		
		if (keyin) {
			menuitemsfiltered.some(function (menuitemfiltered) {
				var menutarget = menuitemfiltered.target.toLowerCase();
				var menuinput = menuitemfiltered.input.toString().toLowerCase();
				
				if ((menuinput == keyin) ||
					((keyin == "\x0d") && (menuitemsfiltered[selected_index].input == menuitemfiltered.input))) {
					// everything in this menu is an xtrnprog
					
					// reset selected index, because the menus are dynamic and
					// the order may change between executions
					selected_index = 0;
					
					// run the external program
					if (typeof xtrn_area.prog[menutarget] !== "undefined") {
						bbs.exec_xtrn(menutarget);
						return true;
					} else {
						doerror(options.custom_menu_program_not_found_msg.replace('%PROGRAMID%', menutarget));
					}
				} // if menu item matched keyin
			}); // foreach menu item
		} // if keyin
	} // main bbs.online loop
}


// Renders the search menu
function search_menu(title, itemcount) {
	if (itemcount === undefined) {
		itemcount = 0;
	}
	
	var i, menucheck, item_multicolumn_fmt, item_singlecolumn_fmt,
		item_multicolumn_fmt_inverse, item_singlecolumn_fmt_inverse,
		multicolumn, selected_index = 0, menuitemsfiltered = [];
	var validkeys = []; // valid chars on menu selection
	var keymax = 0; // max integer allowed on menu selection
	
	var menuid = 'search';
	
	var options = ExternalMenus.getOptions('custommenu', menuid);
	
	while (bbs.online) {
		console.aborted = false;
		
		if (options.clear_screen) {
			console.clear(LIGHTGRAY);
		}
		
		if (user.security.restrictions&UFLAG_X) {
			write(options.restricted_user_msg);
			break;
		}
		
		if (!bbs.menu("xtrnmenu_head_" + menuid, P_NOERROR) && !bbs.menu("xtrnmenu_head", P_NOERROR)) {
			bbs.menu("xtrn_head", P_NOERROR);
		}
		
		if (searchterm) {
			printf(typeof options.custom.searchresultsheader !== "undefined"
				? options.custom.searchresultsheader : "\x01n\x01cSearch Results for \x01h%s", searchterm);
		} else {
			write(typeof options.custom.entersearchterm !== "undefined" ?
				options.custom.entersearchterm : "\x01y\x01hEnter search term: ");
			var searchterm = console.getstr(searchterm, 40,
				K_LINE | K_EDIT | K_NOEXASC | K_TRIM);			
			if (!searchterm) {
				return;
			} else {
				console.up();
				console.clearline();
				printf(typeof options.custom.searchresultsheader !== "undefined"
					? options.custom.searchresultsheader : "\x01n\x01cSearch Results for \x01h%s", searchterm);
			}
		}
		
		searchterm = searchterm.toLowerCase();
		menuitemsfiltered = [];
		
		var input = 1;
		
		for (i in menuconfig.menus) {
			if ((itemcount > 0) && (menuitemsfiltered.length >= itemcount)) {
				break
			}
			if (menuconfig.menus[i].title.toLowerCase().indexOf(searchterm) !== -1) {
				if (user.compare_ars(menuconfig.menus[i].title.access_string)) {
					menuitemsfiltered.push({
						input: input++,
						id: menuconfig.menus[i].id,
						title: menuconfig.menus[i].title,
						target: menuconfig.menus[i].id,
						type: 'custommenu'
					});	
				}
			}
		}
		
		for (i in xtrn_area.sec_list) {
			if ((itemcount > 0) && (menuitemsfiltered.length >= itemcount)) {
				break
			}
			if (xtrn_area.sec_list[i].name.toLowerCase().indexOf(searchterm) !== -1) {
				if (xtrn_area.sec_list[i].can_access) {
					menuitemsfiltered.push({
						input: input++,
						id: xtrn_area.sec_list[i].code,
						title: xtrn_area.sec_list[i].name,
						target: xtrn_area.sec_list[i].code,
						type: 'xtrnmenu'
					});
				}
			}
		}
		
		for (i in xtrn_area.sec_list) {
			for (var k in xtrn_area.sec_list[i].prog_list) {
				if ((itemcount > 0) && (menuitemsfiltered.length >= itemcount)) {
					break
				}
				if (xtrn_area.sec_list[i].prog_list[k].name.toLowerCase().indexOf(searchterm) !== -1) {
					if (xtrn_area.sec_list[i].prog_list[k].can_access) {
						menuitemsfiltered.push({
							input: input++,
							id: xtrn_area.sec_list[i].prog_list[k].code,
							title: xtrn_area.sec_list[i].prog_list[k].name,
							target: xtrn_area.sec_list[i].prog_list[k].code,
							type: 'xtrnprog'
						});
					}
				}
			}
		}
		
		if (!menuitemsfiltered.length) {
			write(options.no_programs_msg);
			break;
		} else {
			console.crlf();
		}
		
		// The quit item is intended to aid in the lightbar navigation
		menuitemsfiltered.unshift({
			input: 'Q',
			title: options.custom.return_msg !== undefined ? options.custom.return_msg : 'Return to Previous Menu',
			target: '',
			type: 'quit',
		});
		
		// determine lines left. we can't know the size of the footer so 
		// let the sysop use singlecolumn_margin to specify that. Below
		// calcution will always leave room for titles and underline even
		// if they aren't rendered
		var linesleft = console.screen_rows - console.line_counter - options.singlecolumn_margin
			- 2 - 2; // -2 for header_fmt/crlf and -2 for crlf and footer
		if(options.titles.trimRight() != '') linesleft = linesleft - 1;
		if(options.underline.trimRight() != '') linesleft = linesleft - 2;
		
		multicolumn = menuitemsfiltered.length  > linesleft;
		
		if(options.titles.trimRight() != '')
			write(options.titles);
		if(multicolumn) {
			write(options.multicolumn_separator);
			if (options.titles.trimRight() != '')
				write(options.titles);
		}
		if(options.underline.trimRight() != '') {
			console.crlf();
			write(options.underline);
		}
		if(multicolumn) {
			write(options.multicolumn_separator);
			if (options.underline.trimRight() != '')
				write(options.underline);
		}
		console.crlf();
		
		// n is the number of items for the 1st column
		var n;
		if (multicolumn) {
			n = Math.floor(menuitemsfiltered.length / 2) + (menuitemsfiltered.length & 1);
		} else {
			n = menuitemsfiltered.length;
		}
		
		// j is the index for each menu item on 2nd column
		var j = n; // start j at the first item for 2nd column
		for (i = 0; i < n && !console.aborted; i++) {
			console.add_hotspot(menuitemsfiltered[i].input.toString());
			
			validkeys.push(menuitemsfiltered[i].input.toString());

			if (menuitemsfiltered[i].input > keymax) {
				keymax = menuitemsfiltered[i].input;
			}
			
			if (i == selected_index) {
				var itemformatret = multicolumn ? options.return_multicolumn_fmt_inverse : options.return_singlecolumn_fmt_inverse;
				var itemformat = multicolumn ? options.multicolumn_fmt_inverse : options.singlecolumn_fmt_inverse;
				printf(
					menuitemsfiltered[i].type == 'quit' ? itemformatret : itemformat,
					menuitemsfiltered[i].input,
					menuitemsfiltered[i].title,
					''
				);
			} else {
				var itemformatret = multicolumn ? options.return_multicolumn_fmt : options.return_singlecolumn_fmt;
				var itemformat = multicolumn ? options.multicolumn_fmt : options.singlecolumn_fmt;
				printf(
					menuitemsfiltered[i].type == 'quit' ? itemformatret : itemformat,
					menuitemsfiltered[i].input,
					menuitemsfiltered[i].title,
					''
				);
			}
			
			if (multicolumn) {
				if (typeof menuitemsfiltered[j] !== "undefined") {
					validkeys.push(menuitemsfiltered[j].input.toString());
					if (menuitemsfiltered[j].input > keymax) {
						keymax = menuitemsfiltered[j].input;
					}
					
					write(options.multicolumn_separator);
					console.add_hotspot(menuitemsfiltered[j].input.toString());
					
					if (j == selected_index) {
						var itemformatret = multicolumn ? options.return_multicolumn_fmt_inverse : options.return_singlecolumn_fmt_inverse;
						var itemformat = multicolumn ? options.multicolumn_fmt_inverse : options.singlecolumn_fmt_inverse;
						printf(
							menuitemsfiltered[j].type == 'quit' ? itemformatret : itemformat,
							menuitemsfiltered[j].input,
							menuitemsfiltered[j].title,
							''
						);
					} else {
						var itemformatret = multicolumn ? options.return_multicolumn_fmt : options.return_singlecolumn_fmt;
						var itemformat = multicolumn ? options.multicolumn_fmt : options.singlecolumn_fmt;
						printf(
							menuitemsfiltered[j].type == 'quit' ? itemformatret : itemformat,
							menuitemsfiltered[j].input,
							menuitemsfiltered[j].title,
							''
						);
					}
				} else {
					write(options.multicolumn_separator);
				}
				j++;
			}
			console.crlf();
		}
		
		if (gamesrv) {
			if (!bbs.menu("xtrn_gamesrv_tail_" + menuid, P_NOERROR)) {
				bbs.menu("xtrn_gamesrv_tail", P_NOERROR);
			}
		}
		
		if (!bbs.menu("xtrnmenu_tail_" + menuid, P_NOERROR) && !bbs.menu("xtrnmenu_tail", P_NOERROR)) {
			bbs.menu("xtrn_tail", P_NOERROR);
		}
		
		console.crlf();
		writeln(options.custom.searchagainmsg !== undefined ? options.custom.searchagainmsg : 
			"\x01n\x01mPress S to Search Again.")
		
		bbs.node_sync();
		console.mnemonics(options.which);
		
		validkeys.push('q');
		validkeys.push('s');
		validkeys.push("\x1B");
		validkeys.push(KEY_LEFT);
		validkeys.push(KEY_RIGHT);
		validkeys.push(KEY_UP);
		validkeys.push(KEY_DOWN);
		validkeys.push("\x0d"); //enter
		
		var keyin, keyin2;
		var maxkeylen = 0;
		var maxfirstkey = 0;
		var morekeys = [];
		var k;
		for (k in validkeys) {
			if (validkeys[k] == "\x1B") {
				continue;
			}
			if (validkeys[k].length > maxkeylen) {
				maxkeylen = validkeys[k].length;
			}
			if (validkeys[k].length > 1) {
				morekeys.push(validkeys[k].toString().toLowerCase().substring(0,1));
			}
		}
		
		// get first key
		keyin = console.getkey();
		keyin = keyin.toString().toLowerCase();
		
		// The logic below is to make it not require enter for as
		// many items as possible
		
		// if ENTER on a QUIT item, exit
		if (keyin == "\x0d") {
			if (menuitemsfiltered[selected_index].type == "quit") {
				if (gamesrv && (menuid == 'main')) {
					bbs.logoff();
				} else {
					console.clear();
					return;
				}
			}
		}
		
		if ((keyin == 'q') || (keyin == "\x1B")) {
			console.clear();
			return;
		} else if (keyin == KEY_UP) {
			--selected_index;
			if (selected_index < 0) {
				selected_index = menuitemsfiltered.length - 1;
			}
			continue;
		} else if (keyin == KEY_DOWN) {
			++selected_index;
			if (selected_index > (menuitemsfiltered.length - 1)) {
				selected_index = 0;
			}
			continue;
		} else if (keyin == KEY_LEFT) {
			var row_limit = n -1;
			if (selected_index == 0) {
				// first item, go back menu
				if (gamesrv && (menuid == 'main')) {
					bbs.logoff();
				} else {
					console.clear();
					return;
				}
			} else if (selected_index <= row_limit) {
				// on first col
				var new_selected_index = selected_index + n;
				if (typeof menuitemsfiltered[new_selected_index] === "undefined") {
					// no item on right, no change
				} else {
					selected_index = new_selected_index;
				}
			} else {
				// on second col
				selected_index = selected_index - n;
			}
		} else if (keyin == KEY_RIGHT) {
			var row_limit = n - 1;
			if (selected_index <= row_limit) {
				// on first col
				var new_selected_index = selected_index + n;
				if (typeof menuitemsfiltered[new_selected_index] === "undefined") {
					// no item on right, no change
				} else {
					selected_index = new_selected_index;
				}
			} else {
				// on second col
				selected_index = selected_index - n;
			}
		}
		
		// if max keys is 2 and they entered something that might have
		// a second digit/char, then get the key
		if ((maxkeylen == 2) && (keyin !== "\x0d")) {
			if (morekeys.indexOf(keyin) !== -1) {
				write(keyin);
				keyin2 = console.getkey(); // either the second digit or enter
				if ((keyin2 !== "\r") && (keyin2 !== "\n") && (keyin2 !== "\r\n")) {
					keyin = keyin + keyin2.toLowerCase();
				}
			}
		} else if ((maxkeylen > 2) && (keyin !== "\x0d")) {
			// there there are more than 99 items, then just use getkeys 
			// for the rest
			write(keyin);
			keyin2 = console.getkeys(validkeys, keymax);
			keyin = keyin + keyin2.toLowerCase();
		}
		
		if (keyin) {
			if (keyin == 's') {
				console.crlf();
				console.crlf();
				write(typeof options.custom.entersearchterm !== "undefined" ?
					options.custom.entersearchterm : "Enter search term: ");
				var searchterm = console.getstr(searchterm, 40,
						K_LINE | K_EDIT | K_NOEXASC | K_TRIM);
			} else {
				menuitemsfiltered.some(function (menuitemfiltered) {
					var menutarget = menuitemfiltered.target.toLowerCase();
					var menuinput = menuitemfiltered.input.toString().toLowerCase();
					
					if ((menuinput == keyin) ||
						((keyin == "\x0d") && (menuitemsfiltered[selected_index].input == menuitemfiltered.input))) {
						switch (menuitemfiltered.type) {
							// custom menu, defined in xtrnmenu.json
							case 'custommenu':
								external_menu_custom(menutarget);
								return true;
							// external program section
							case 'xtrnmenu':
								if (options.use_xtrn_sec) {
									js.exec("xtrn_sec.js", {}, menutarget);
								} else {
									external_menu_custom(menutarget);
								}
								return true;
							// external program
							case 'xtrnprog':
								// run the external program
								if (typeof xtrn_area.prog[menutarget] !== "undefined") {
									bbs.exec_xtrn(menutarget);
									return true;
								} else {
									doerror(options.custom_menu_program_not_found_msg.replace('%PROGRAMID%', menutarget));
								}
								break;
						}
					} // if menu item matched keyin
				}); // foreach menu item
			}
		} // if keyin
	} // main bbs.online loop
}

// Return the favorites menu
function favorites_menu(title, itemcount) {
	if (itemcount === undefined) {
		itemcount = 0;
	}
	
	var i, menucheck, item_multicolumn_fmt, item_singlecolumn_fmt,
		item_multicolumn_fmt_inverse, item_singlecolumn_fmt_inverse,
		multicolumn, selected_index = 0, menuitemsfiltered = [];
	var validkeys = []; // valid chars on menu selection
	var keymax = 0; // max integer allowed on menu selection
	
	var menuid = 'favorites';
	
	var options = ExternalMenus.getOptions('custommenu', menuid);
	
	while (bbs.online) {
		console.aborted = false;
		
		if (options.clear_screen) {
			console.clear(LIGHTGRAY);
		}
		
		if (user.security.restrictions&UFLAG_X) {
			write(options.restricted_user_msg);
			break;
		}
		
		var menuobj = ExternalMenus.getFavorites(title, itemcount);
		if (!bbs.menu("xtrnmenu_head_" + menuid, P_NOERROR) && !bbs.menu("xtrnmenu_head", P_NOERROR)) {
			bbs.menu("xtrn_head", P_NOERROR);
		}
		
		menuitemsfiltered = typeof menuobj.items !== "undefined" ? menuobj.items : [];
		
		// The quit item is intended to aid in the lightbar navigation
		menuitemsfiltered.unshift({
			input: 'Q',
			title: options.custom.return_msg,
			target: '',
			type: 'quit'
		});
		
		menuitemsfiltered.push({
			input: '+',
			title: options.custom.favorite_add_item,
			target: '',
			type: 'add'
		});
		
		menuitemsfiltered.push({
			input: '-',
			title: options.custom.favorite_remove_item,
			target: '',
			type: 'remove'
		});
		
		// determine lines left. we can't know the size of the footer so 
		// let the sysop use singlecolumn_margin to specify that. Below
		// calcution will always leave room for titles and underline even
		// if they aren't rendered
		var linesleft = console.screen_rows - console.line_counter - options.singlecolumn_margin
			- 2 - 2; // -2 for header_fmt/crlf and -2 for crlf and footer
		if(options.titles.trimRight() != '') linesleft = linesleft - 1;
		if(options.underline.trimRight() != '') linesleft = linesleft - 2;
		
		multicolumn = menuitemsfiltered.length  > linesleft;
		
		printf(options.header_fmt, title);
		if(options.titles.trimRight() != '')
			write(options.titles);
		if(multicolumn) {
			write(options.multicolumn_separator);
			if (options.titles.trimRight() != '')
				write(options.titles);
		}
		if(options.underline.trimRight() != '') {
			console.crlf();
			write(options.underline);
		}
		if(multicolumn) {
			write(options.multicolumn_separator);
			if (options.underline.trimRight() != '')
				write(options.underline);
		}
		console.crlf();
		
		// n is the number of items for the 1st column
		var n;
		if (multicolumn) {
			n = Math.floor(menuitemsfiltered.length / 2) + (menuitemsfiltered.length & 1);
		} else {
			n = menuitemsfiltered.length;
		}
		
		// j is the index for each menu item on 2nd column
		var j = n; // start j at the first item for 2nd column
		for (i = 0; i < n && !console.aborted; i++) {
			console.add_hotspot(menuitemsfiltered[i].input.toString());
			
			validkeys.push(menuitemsfiltered[i].input.toString());
			
			if (menuitemsfiltered[i].input > keymax) {
				keymax = menuitemsfiltered[i].input;
			}
			
			if (i == selected_index) {
				var itemformatret = multicolumn ? options.return_multicolumn_fmt_inverse : options.return_singlecolumn_fmt_inverse;
				var itemformat = multicolumn ? options.multicolumn_fmt_inverse : options.singlecolumn_fmt_inverse
				printf(
					menuitemsfiltered[i].type == 'quit' ? itemformatret : itemformat,
					menuitemsfiltered[i].input,
					menuitemsfiltered[i].title
				);
			} else {
				var itemformatret = multicolumn ? options.return_multicolumn_fmt : options.return_singlecolumn_fmt;
				var itemformat = multicolumn ? options.multicolumn_fmt : options.singlecolumn_fmt
				printf(
					menuitemsfiltered[i].type == 'quit' ? itemformatret : itemformat,
					menuitemsfiltered[i].input,
					menuitemsfiltered[i].title
				);
			}
			
			if (multicolumn) {
				if (typeof menuitemsfiltered[j] !== "undefined") {
					validkeys.push(menuitemsfiltered[j].input.toString());
					
					if (menuitemsfiltered[j].input > keymax) {
						keymax = menuitemsfiltered[i].input;
					}
					
					write(options.multicolumn_separator);
					console.add_hotspot(menuitemsfiltered[j].input.toString());
					
					if (j == selected_index) {
						var itemformatret = multicolumn ? options.return_multicolumn_fmt_inverse : options.return_singlecolumn_fmt_inverse;
						var itemformat = multicolumn ? options.multicolumn_fmt_inverse : options.singlecolumn_fmt_inverse
						printf(
							menuitemsfiltered[j].type == 'quit' ? itemformatret : itemformat,
							menuitemsfiltered[j].input,
							menuitemsfiltered[j].title
						);
					} else {
						var itemformatret = multicolumn ? options.return_multicolumn_fmt : options.return_singlecolumn_fmt;
						var itemformat = multicolumn ? options.multicolumn_fmt : options.singlecolumn_fmt
						printf(
							menuitemsfiltered[j].type == 'quit' ? itemformatret : itemformat,
							menuitemsfiltered[j].input,
							menuitemsfiltered[j].title
						);
					}
				} else {
					write(options.multicolumn_separator);
				}
				j++;
			}
			console.crlf();
		}
		
		if (gamesrv) {
			if (!bbs.menu("xtrn_gamesrv_tail_" + menuid, P_NOERROR)) {
				bbs.menu("xtrn_gamesrv_tail", P_NOERROR);
			}
		}
		
		if (!bbs.menu("xtrnmenu_tail_" + menuid, P_NOERROR) && !bbs.menu("xtrnmenu_tail", P_NOERROR)) {
			bbs.menu("xtrn_tail", P_NOERROR);
		}
		
		bbs.node_sync();
		console.mnemonics(options.which);
		
		validkeys.push('q');
		validkeys.push("\x1B");
		validkeys.push(KEY_LEFT);
		validkeys.push(KEY_RIGHT);
		validkeys.push(KEY_UP);
		validkeys.push(KEY_DOWN);
		validkeys.push("\x0d"); //enter
		
		var keyin, keyin2;
		var maxkeylen = 0;
		var maxfirstkey = 0;
		var morekeys = [];
		var k;
		for (k in validkeys) {
			if (validkeys[k] == "\x1B") {
				continue;
			}
			if (validkeys[k].length > maxkeylen) {
				maxkeylen = validkeys[k].length;
			}
			if (validkeys[k].length > 1) {
				morekeys.push(validkeys[k].toString().toLowerCase().substring(0,1));
			}
		}
		
		// get first key
		keyin = console.getkey();
		keyin = keyin.toString().toLowerCase();
		
		// if ENTER 
		if (keyin == "\x0d") {
			if (menuitemsfiltered[selected_index].type == "quit") {
				console.clear();
				return;
			} else if (menuitemsfiltered[selected_index].type == "add") {
				add_favorite();
				continue;
			} else if (menuitemsfiltered[selected_index].type == "remove") {
				remove_favorite();
				continue;
			}
		} else if ((keyin == 'q') || (keyin == "\x1B")) {
			console.clear();
			return;
		} else if (keyin == KEY_UP) {
			--selected_index;
			if (selected_index < 0) {
				selected_index = menuitemsfiltered.length - 1;
			}
			continue;
		} else if (keyin == KEY_DOWN) {
			++selected_index;
			if (selected_index > (menuitemsfiltered.length - 1)) {
				selected_index = 0;
			}
			continue;
		} else if (keyin == KEY_LEFT) {
			var row_limit = n -1;
			if (selected_index == 0) {
				console.clear();
				return;
			} else if (selected_index <= row_limit) {
				// on first col
				var new_selected_index = selected_index + n;
				if (typeof menuitemsfiltered[new_selected_index] === "undefined") {
					// no item on right, no change
				} else {
					selected_index = new_selected_index;
				}
			} else {
				// on second col
				selected_index = selected_index - n;
			}
		} else if (keyin == KEY_RIGHT) {
			var row_limit = n - 1;
			if (selected_index <= row_limit) {
				// on first col
				var new_selected_index = selected_index + n;
				if (typeof menuitemsfiltered[new_selected_index] === "undefined") {
					// no item on right, no change
				} else {
					selected_index = new_selected_index;
				}
			} else {
				// on second col
				selected_index = selected_index - n;
			}
		} else if ((maxkeylen == 2) && (keyin !== "\x0d")) {
			// The logic below is to make it not require enter for as
			// many items as possible
			
			// if max keys is 2 and they entered something that might have
			// a second digit/char, then get the key
			if (morekeys.indexOf(keyin) !== -1) {
				write(keyin);
				keyin2 = console.getkey(); // either the second digit or enter
				if ((keyin2 !== "\r") && (keyin2 !== "\n") && (keyin2 !== "\r\n")) {
					keyin = keyin + keyin2.toLowerCase();
				}
			}
		} else if ((maxkeylen > 2) && (keyin !== "\x0d")) {
			// there there are more than 99 items, then just use getkeys 
			// for the rest
			write(keyin);
			keyin2 = console.getkeys(validkeys, keymax);
			keyin = keyin + keyin2.toLowerCase();
		}
		
		if (keyin) {
			if ((keyin == '+') || (keyin == '=')) {
				add_favorite();
			} else if ((keyin == "\x08") || (keyin == "\x7f")) {
				// delete
				if (menuitemsfiltered[selected_index].type == 'xtrnprog') {
					console.crlf();
					if (!console.noyes("Do you wish to remove " +
						menuitemsfiltered[selected_index].title)) {
						remove_favorite(menuitemsfiltered[selected_index].target.toLowerCase());
					}
				}
				
			} else if ((keyin == '-') || (keyin == '_')) {
				remove_favorite();
			} else {
				for (var d in menuitemsfiltered) {
					var menutarget = menuitemsfiltered[d].target.toLowerCase();
					var menuinput = menuitemsfiltered[d].input.toString().toLowerCase();
					if ((menuinput == keyin) ||
						((keyin == "\x0d") && (menuitemsfiltered[selected_index].input == menuitemsfiltered[d].input))) {
						// everything in this menu is an xtrnprog
						
						// reset selected index, because the menus are dynamic and
						// the order may change between executions
						selected_index = 0;
						
						// run the external program
						if (typeof xtrn_area.prog[menutarget] !== "undefined") {
							bbs.exec_xtrn(menutarget);
						} else {
							doerror(options.custom_menu_program_not_found_msg.replace('%PROGRAMID%', menutarget));
						}
					} // if menu item matched keyin
				}
			}
		} // if keyin
	} // main bbs.online loop
}

// Add an entry to the favorites menu
function add_favorite()
{
	require("frame.js", "Frame");
	require("tree.js", "Tree");
	require("scrollbar.js", "ScrollBar");
	require('typeahead.js', 'Typeahead');
	load(js.global, 'cga_defs.js');
	
	var options = ExternalMenus.getOptions('custommenu', 'favorites');
	
	if (!options.json_enabled) {
		log(LOG_DEBUG, "xtrnmenulib: Skipping favorites because JSON is not enabled");
		return false;
	}
	
	try {
		require("json-client.js", "JSONClient");
		var jsonClient = new JSONClient(options.json_host, options.json_port);
		jsonClient.callback = ExternalMenus.processUpdate;
	} catch (e) {
		log(LOG_ERR, "xtrnmenu add_favorites: Could not initialize JSON database so favorites is now disabled: " + e);
		return false;
	}
	
	var sortedItems = [];
	var jsonData = jsonClient.read("xtrnmenu", "favorites_" + user.alias, 1);
	
	if (!jsonData) {
		jsonData = [];
	}
	
	var frame = new Frame(1, 1, console.screen_columns, console.screen_rows, WHITE);
	var treeframe = new Frame(frame.x, frame.y+4, frame.width, frame.height-5, WHITE);
	var tree = new Tree(treeframe);
	var s = new ScrollBar(tree);
	frame.open();
	treeframe.open();
	
	frame.putmsg(options.custom.add_favorites_msg !== undefined 
		? options.custom.add_favorites_msg : "\x01c\x01hAdd Favorite");
	frame.gotoxy(1, 3);
	frame.putmsg(options.custom.favorites_inst !== undefined
		? options.custom.favorites_inst : '\x01n\x01w\x01h\x012 [Up/Down/Home/End] to Navigate, [Enter] to Select, [Q] to Quit, [S] to Search ');
	
	var xtrnwidth = 0;
	var sortedItems = [];
	for (var m in xtrn_area.prog) {
		if (xtrn_area.prog[m].can_access) {
			var found = false;
			jsonData.some(function (jsonxtrn) {
				if (jsonxtrn == xtrn_area.prog[m].code) {
					found = true;
					return true;
				}
			});
			if (!found) {
				sortedItems.push({ code: xtrn_area.prog[m].code, name: xtrn_area.prog[m].name })
				if (xtrn_area.prog[m].name.length > xtrnwidth) {
					xtrnwidth = xtrn_area.prog[m].name.length;
				}
			}
		}
	}
	
	sortedItems.sort(function(a, b) {
			if(a.name.toLowerCase()>b.name.toLowerCase()) return 1;
			if(a.name.toLowerCase()<b.name.toLowerCase()) return -1;
			return 0;
		});
	
	for (m in sortedItems) {
		const item = tree.addItem(sortedItems[m].name, sortedItems[m].code);
		item.code = sortedItems[m].code;
	}
	
	treeframe.width = xtrnwidth + 2;

	// lightbar non-current item
	tree.colors.fg = options.custom.favorite_add_fg 
		? js.global[options.custom.favorite_add_fg] : LIGHTGRAY;
	tree.colors.bg = options.custom.favorite_add_bg 
		? js.global[options.custom.favorite_add_bg] : BG_BLACK;
	// lightbar current item
	tree.colors.lfg = options.custom.favorite_add_lfg 
		? js.global[options.custom.favorite_add_lfg] : WHITE;
	tree.colors.lbg = options.custom.favorite_add_lbg
		? js.global[options.custom.favorite_add_lbg] : BG_MAGENTA;
	// tree heading
	tree.colors.cfg = options.custom.favorite_add_cfg 
		? js.global[options.custom.favorite_add_cfg] : WHITE;
	tree.colors.cbg = options.custom.favorite_add_cbg 
		? js.global[options.custom.favorite_add_cbg] : BG_BLACK;
	// other tree color settings not applicable to this implementation
	tree.open();
	
	console.clear(BG_BLACK|LIGHTGRAY);
	
	frame.draw();
	treeframe.draw();
	
	var selection;
	var key;
	var xtrn;
	while(bbs.online) {
		key = console.getkey(K_NOSPIN);
		if (key == "\x0d") {
			// hit enter, item is selected
			xtrn = tree.currentItem;
			break;
		} else if ((key.toLowerCase() == 'q') || (key == "\x1B")) {
			return;
		} else if (key.toLowerCase() == 's') {
			xtrn = add_favorite_search(frame, treeframe, options);
			break;
		} else {
			selection = tree.getcmd({key: key, mouse: false});
			xtrn = tree.currentItem;
			
			if (key == KEY_UP || key == KEY_DOWN || key == KEY_HOME || key == KEY_END) {
				if ((key == KEY_UP) && (tree.line == 1)) {
					// pressed up on first item, go to end
					tree.end();
					tree.refresh(); // fixes itself with it going to next to last item
				} else if ((key == KEY_DOWN) && (tree.line == sortedItems.length)) {
					// pressed down on last item, go to start
					tree.home();
					tree.refresh(); // fixes issue with it going to 2nd item
				}
				xtrn = tree.currentItem;
			}
		}
		frame.cycle();
		treeframe.cycle();
		s.cycle();
	}

	if ((typeof xtrn !== "undefined") && (typeof xtrn.code !== "undefined")) {
		jsonData.push(xtrn.code);
		jsonClient.write("xtrnmenu", "favorites_" + user.alias, jsonData, 2);
	}
}

// Type ahead search for add favorite
function add_favorite_search(frame, treeframe, options) {
	require("frame.js", "Frame");
	require("tree.js", "Tree");
	require('typeahead.js', 'Typeahead');
	load(js.global, 'cga_defs.js');
	
	var sframe = new Frame(
		1,
		1,
		console.screen_columns,
		console.screen_rows,
		LIGHTGRAY,
		frame
	);
	sframe.open();
	
	const typeahead = new Typeahead({
		x : 1,
		y : 1,
		bg : options.custom.favorite_add_search_fg 
			? js.global[options.custom.favorite_add_search_fg] : LIGHTGRAY,
		fg: options.custom.favorite_add_search_bg 
			? js.global[options.custom.favorite_add_search_bg] : BG_BLACK,
		sfg: options.custom.favorite_add_search_sfg 
			? js.global[options.custom.favorite_add_search_sfg] : LIGHTGRAY,
		sbg: options.custom.favorite_add_search_sbg 
			? js.global[options.custom.favorite_add_search_sbg] : BG_BLACK,
		hsfg: options.custom.favorite_add_search_hsfg 
			? js.global[options.custom.favorite_add_search_hsfg] : WHITE,
		hsbg: options.custom.favorite_add_search_hsbg 
			? js.global[options.custom.favorite_add_search_hsbg] : BG_MAGENTA,
		prompt: options.custom.favorite_add_search_prompt 
			? options.custom.favorite_add_search_prompt : "\x01c\x01hSearch (ESC to Cancel): \x01n",
		len: treeframe.width,
		frame: sframe,
		datasources: [
			function (searchterm) {
				const ret = [];
				
				for (i in xtrn_area.sec_list) {
					for (var k in xtrn_area.sec_list[i].prog_list) {
						if (xtrn_area.sec_list[i].prog_list[k].name.toLowerCase().indexOf(searchterm.toLowerCase()) !== -1) {
							if (xtrn_area.sec_list[i].prog_list[k].can_access) {
								ret.push({
									'code': xtrn_area.sec_list[i].prog_list[k].code,
									'text': xtrn_area.sec_list[i].prog_list[k].name,
								});
							}
						}
					}
				}
				return ret;
			}
		]
	});
	
	var user_input = undefined;
	while (typeof user_input !== 'object') {
		var key = console.inkey(K_NONE, 5);
		if (key == "\x1B") {
			typeahead.close();
			sframe.close();
			return false;
		}
		
		user_input = typeahead.inkey(key);
		typeahead.cycle();
		if (sframe.cycle()) typeahead.updateCursor();
	}
	
	typeahead.close();
	sframe.close();
	return user_input;
}

// Remove an entry from the favorites menu
function remove_favorite(xtrncode)
{
	var options = ExternalMenus.getOptions('custommenu', 'favorites');
	
	if (!options.json_enabled) {
		log(LOG_DEBUG, "xtrnmenulib: Skipping favorites because JSON is not enabled");
		return false;
	}
	
	try {
		require("json-client.js", "JSONClient");
		var jsonClient = new JSONClient(options.json_host, options.json_port);
		jsonClient.callback = ExternalMenus.processUpdate;
	} catch (e) {
		log(LOG_ERR, "xtrnmenu remove_favorites: Could not initialize JSON database so favorites is now disabled: " + e);
		return false;
	}
	
	var sortedItems = [];
	var jsonData = jsonClient.read("xtrnmenu", "favorites_" + user.alias, 1);
	
	if (!jsonData) {
		jsonData = [];
	}
	
	if ((typeof xtrncode === "undefined") || (!xtrncode)) {
		require("frame.js", "Frame");
		require("tree.js", "Tree");
		require("scrollbar.js", "ScrollBar");
		
		var frame = new Frame(1, 1, console.screen_columns, 4, WHITE);
		var treeframe = new Frame(1, 5, console.screen_columns, console.screen_rows - 5, WHITE);
		var tree = new Tree(treeframe);
		var s = new ScrollBar(tree);
		frame.open();
		treeframe.open();
		
		frame.putmsg(options.custom.remove_favorites_msg !== undefined
			? options.custom.remove_favorites_msg : "\x01c\x01hRemove Favorite");
		frame.gotoxy(1, 3);
		frame.putmsg(options.custom.favorites_inst_rem !== undefined
			? options.custom.favorites_inst_rem : '\x01n\x01w\x01h\x012 [Up/Down/Home/End] to Navigate, [Enter] to Select, [Q] to Quit ');
		
		var xtrnwidth = 0;
		var sortedItems = [];
		for (var m in xtrn_area.prog) {
			if (xtrn_area.prog[m].can_access) {
				var found = false;
				jsonData.some(function (jsonxtrn) {
					if (jsonxtrn == xtrn_area.prog[m].code) {
						found = true;
						return true;
					}
				});
				if (found) {
					sortedItems.push({code: xtrn_area.prog[m].code, name: xtrn_area.prog[m].name})
					if (xtrn_area.prog[m].name.length > xtrnwidth) {
						xtrnwidth = xtrn_area.prog[m].name.length;
					}
				}
			}
		}
		
		sortedItems.sort(function (a, b) {
			if (a.name.toLowerCase() > b.name.toLowerCase()) return 1;
			if (a.name.toLowerCase() < b.name.toLowerCase()) return -1;
			return 0;
		});
		
		for (m in sortedItems) {
			const item = tree.addItem(sortedItems[m].name, sortedItems[m].code);
			item.code = sortedItems[m].code;
		}
		
		treeframe.width = xtrnwidth + 2;
		
		tree.colors.fg = LIGHTGRAY;
		tree.colors.bg = BG_BLACK;
		tree.colors.lfg = WHITE;
		tree.colors.lbg = BG_MAGENTA;
		tree.colors.kfg = YELLOW;
		
		tree.open();
		
		console.clear(BG_BLACK | LIGHTGRAY);
		
		frame.draw();
		treeframe.draw();
		
		var selection;
		var key;
		var xtrn;
		while (bbs.online) {
			key = console.getkey();
			if (key == "\x0d") {
				// hit enter, item is selected
				break;
			}
			if ((key.toLowerCase() == 'q') || (key == "\x1B")) return;
			
			selection = tree.getcmd({key: key, mouse: false});
			
			if (key == KEY_UP || key == KEY_DOWN || key == KEY_HOME || key == KEY_END) {
				if ((key == KEY_UP) && (tree.line == 1)) {
					// pressed up on first item, go to end
					tree.end();
					tree.refresh(); // fixes itself with it going to next to last item
				} else if ((key == KEY_DOWN) && (tree.line == sortedItems.length)) {
					// pressed down on last item, go to start
					tree.home();
					tree.refresh(); // fixes issue with it going to 2nd item
				}
				xtrn = tree.currentItem;
				xtrncode = xtrn.code;
			}
			treeframe.cycle();
			s.cycle();
		}
	}
	
	var newJsonData = [];
	jsonData.forEach(function (jsonxtrn) {
		if (jsonxtrn != xtrncode) {
			newJsonData.push(jsonxtrn);
		}
	});
	
	jsonClient.write("xtrnmenu", "favorites_" + user.alias, newJsonData, 2);
}

// Display error message to console and save to log
function doerror(msg)
{
	console.crlf();
	log(LOG_ERR, msg);
	console.putmsg('\x01+\x01h\x01r' +msg + ". The sysop has been notified." + '\x01-\r\n');
}

