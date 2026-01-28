// str_cmds.js

// Global String Command Module for Synchronet

// @format.tab-size 4, @format.use-tabs true

// This is NOT a command shell, DO NOT add to SCFG->Command Shells

// This module is loaded from command shells with the load() function

// It contains mostly sysop commands (i.e. ;ERR)

// The command string must be the first argument string
// when this module is loaded.

require("sbbsdefs.js", 'EX_STDIO');
require("nodedefs.js", 'NODE_SYSP');
var presence = bbs.mods.presence_lib;
if(!presence)
	presence = bbs.mods.presence_lib = load({}, "presence_lib.js");

if(argc>0)
	str_cmds(argv.join(" "));	// use command-line arguments if supplied
else if(bbs.command_str && bbs.command_str != '')
	str_cmds(bbs.command_str);	// otherwise, use command shell 'str' var, if supported (v3.13b)

function str_cmds(str)
{
	var file;	// File
	var word;	// First word of args
	var i;		// Temp integer
	var j;		// Temp integer
	var k;		// Temp integer
	var l;		// Temp integer
	var m;		// Temp integer
	var a;		// Temp array
	var s;		// Temp string
	var help = []; // Command descriptions

	// Remove any trailing spaces

	str=truncsp(str);
	// Upper-Case first word
	str=str.replace(/^\s*([^\s]*)/,
		function(st, p, oset, s) {
			word=p.toUpperCase();
			return(word);
		}
	);

	if(str === "")
		return;
	log("Invoked string command: " + str);
	bbs.log_str(str);

	var node_action = bbs.node_action;
	if(bbs.compare_ars("SYSOP")) {
		// Change node action to "sysop activities"
		bbs.node_action=NODE_SYSP;
		//sync

		// ######################## SYSOP Functions ##############################
		help["AVAIL"] = "Toggle sysop chat availability";
		if(str=="AVAIL") {
			system.operator_available = !system.operator_available;
			write(format(bbs.text(bbs.text.LiSysopIs)
				, bbs.text(system.operator_available ? bbs.text.LiSysopAvailable : bbs.text.LiSysopNotAvailable)));
			return;
		}

		help["ERR"] = "Display current error log and optionally delete it, as well as optionally clearing all nodes' error counters";
		if(str=="ERR") {
			var errlog=system.logs_dir+"error.log";
			if(file_exists(errlog)) {
				write(bbs.text(bbs.text.ErrorLogHdr));
				console.printfile(errlog, P_NOATCODES | P_SEEK);
				console.aborted = false;
				if(!console.noyes(bbs.text(bbs.text.DeleteErrorLogQ)))
					file_remove(errlog);
			}
			else {
				write(format(bbs.text(bbs.text.FileDoesNotExist),errlog));
			}
			for(i=0;i<system.nodes;i++) {
				if(system.node_list[i].errors)
					break;
			}
			if(i<system.nodes) {
				if(!console.noyes(bbs.text(bbs.text.ClearErrCounter))) {
					for(i=0;i<system.nodes; i++) {
						system.node_list[i].errors=0;
					}
				}
			}
			return;
		}

		help["GURU"] = "Display and optionally clear current guru log";
		if(str=="GURU") {
			if(file_exists(system.logs_dir+"guru.log")) {
				console.printfile(system.logs_dir+"guru.log", P_NOATCODES | P_SEEK);
				console.crlf();
				if(!console.noyes(bbs.text(bbs.text.DeleteGuruLogQ)))
					file_remove(system.logs_dir+"guru.log");
			}
		}

		help["CHUSER [Name or Number]"] = "Become a different user";
		if(word=="CHUSER") {
			// Prompts for syspass
			bbs.change_user(str.substr(6));
			return;
		}

		help["TYPE [filename]"] = "Displays a file. Aliases: LIST and CAT";
		if(word=="LIST" || word=="TYPE" || word=="CAT") {
			if(bbs.check_syspass()) {
				str=str.substr(4);
				var pmode = (word == "CAT") ? P_NOATCODES : P_CPM_EOF;
				if(word == "TYPE")
					pmode |= P_OPENCLOSE;
				else
					pmode |= P_SEEK;
				log("printing " + (str = get_filename(str)));
				console.printfile(str, pmode);
				return;
			}
		}
		
		help["ECHO [string]"] = "Print a text message";
		if(word=="ECHO") {
			if(bbs.check_syspass()) {
				str=str.substr(4);
				console.putmsg(get_arg(str));
				console.crlf();
				return;
			}
		}
		
		help["EVAL [string]"] = "Evaluate a JavaScript expression and display result";
		if(word=="EVAL") {
			if(bbs.check_syspass()) {
				str=str.substr(4);
				try {
					var result = eval(get_arg(str));
					console.print(format("Result (%s): ", typeof result));
					console.print(result);
					console.crlf();
				} catch(e) {
					alert(e);
				}
				return;
			}
		}

		help["EDIT"] = "Edits a specified file using your message editor";
		if(word=="EDIT") {
			if(bbs.check_syspass()) {
				str=str.substr(4);
				log("editing " + (str = get_filename(str)));
				console.editfile(str);
			}
		}

		help["LOG"] = "Displays todays activity log";
		if(str=="LOG") {
			if(bbs.check_syspass()) {
				str=system.logs_dir+strftime("logs/%m%d%y.log",time());
				console.printfile(str, P_NOATCODES);
			}
			return;
		}

		help["YLOG"] = "Displays yesterdays activity log";
		if(str=="YLOG") {
			if(bbs.check_syspass()) {
				str=system.logs_dir+strftime("logs/%m%d%y.log",time()-24*60*60);
				console.printfile(str, P_NOATCODES);
			}
			return;
		}

		help["SS"] = "Displays current system stats";
		if(str=="SS") {
			bbs.sys_stats();
			return;
		}

		help["NS <#>"] = "Displays the current node stats for node #";
		if(word=="NS") {
			str=str.substr(2);
			i=parseInt(get_nodenum(str));
			if(!i) i=bbs.node_num;
			bbs.node_stats(i);
			return;
		}

		help["EXEC [command]"] = "Executes command (or prompts for it) with I/O redirected";
		if(word=="EXEC") {
			if(bbs.check_syspass()) {
				str=str.substr(4);
				str=get_cmdline(str);
				if(str) {
					var ex_mode = EX_STDIO;
					if(bbs.startup_options & BBS_OPT_NO_DOS)
						ex_mode |= EX_NATIVE;
					bbs.exec(str, ex_mode);
				}
			}
			return;
		}

		help["NEXEC [command]"] = "Executes command (or prompts for it) with I/O redirected, and assuming it's a native binary";
		if(word=="NEXEC") {
			if(bbs.check_syspass()) {
				str=str.substr(5);
				str=get_cmdline(str);
				if(str)
					bbs.exec(str,EX_STDIO|EX_NATIVE);
			}
			return;
		}

		help["FOSSIL [command]"] = "Executes command (or prompts for it) with I/O redirected, and assuming the internal FOSSIL driver will be used";
		if(word=="FOSSIL") {
			if(bbs.check_syspass()) {
				str=str.substr(6);
				str=get_cmdline(str);
				if(str)
					bbs.exec(str);
			}
			return;
		}

		help["CALL <HubID>"] = "Forces a callout to QWKnet HubID";
		if(word=="CALL") {
			if(bbs.check_syspass()) {
				str=str.substr(4);
				str=get_arg(str, "QWKnet ID");
				if(str)
					file_touch(system.data_dir + "qnet/" + str + ".now");
			}
			return;
		}

		help["EVENT [EventID]"] = "Forces a timed-event to execute via semfile";
		if(word=="EVENT") {
			if(bbs.check_syspass()) {
				str = str.substr(5);
				if(str)
					str = get_arg(str);
				else {
					var codes = [];
					for(var i in xtrn_area.event) {
						if(xtrn_area.event[i].settings & EVENT_DISABLED)
							continue;
						console.uselect(codes.length, "Event", i);
						codes.push(i);
					}
					var selection = console.uselect();
					if(selection >= 0)
						str = codes[selection];
					alert(str);
				}
				if(str)
					file_touch(system.data_dir + str + ".now");
			}
			return;
		}


		help["NODE [parameters]"] = "Executes the node utility with the passed parameters";
		if(word=="NODE") {
			bbs.exec(bbs.cmdstr("%!node%. -pause ") + str.substr(4), EX_STDIO|EX_NATIVE);
			return;
		}

		help["DOWN [#]"] = "Downs node #.  If # is omitted, downs the current node";
		if(word=="DOWN") {
			str=str.substr(4);
			i=parseInt(get_nodenum(str));
			if(!i) i=bbs.node_num;
			i--;
			if(i<0 || i>=system.nodes)
				write("\r\n\x01h\x01rInvalid Node!");
			else {
				if(system.node_list[i].status==NODE_WFC)
					system.node_list[i].status=NODE_OFFLINE;
				else
					system.node_list[i].misc^=NODE_DOWN;
				display_node(i+1);
			}
			return;
		}

		help["RERUN [#]"] = "Marks node # for rerun.  If # is omitted, reruns the current node";
		if(word=="RERUN") {
			str=str.substr(5);
			i=parseInt(get_nodenum(str));
			if(!i) i=bbs.node_num;
			i--;
			if(i<0 || i>=system.nodes)
				write("\r\n\x01h\x01rInvalid Node!");
			else {
				system.node_list[i].misc^=NODE_RRUN;
				display_node(i+1);
			}
			return;
		}

		help["RELOAD"] = "Reload the current shell (if JavaScript)";
		if(str=="RELOAD") {
			bbs.load_user_text();
			exit(0);
		}

		help["SLOG"] = "Executes the slog utility to display system statistics";
		if(str=="SLOG") {
			bbs.exec(bbs.cmdstr("%!slog%. /p"),EX_STDIO|EX_NATIVE);
			return;
		}

		help["NLOG [#]"] = "Executes the slog utility to display node stats for the specified node. If # is omitted, uses the current node";
		if(str=="NLOG") {
			bbs.exec(bbs.cmdstr("%!slog%. %n /p"),EX_STDIO|EX_NATIVE);
			return;
		}
		if(word=="NLOG") {
			str=str.substr(5);
			bbs.exec(bbs.cmdstr("%!slog%. %n../node"+get_nodenum(str)+" /p"),EX_STDIO|EX_NATIVE);
			return;
		}

		help["UEDIT [Name or Number]"] = "Edits specified user or starts at user #1";
		if(word=="UEDIT") {
			// Prompts for syspass
			str=str.substr(5).trim();
			if(str.length) {
				var usernum = parseInt(str, 10);
				if(isNaN(usernum) || usernum < 1 || usernum > system.lastuser)
					usernum = bbs.finduser(str);
				if(usernum < 1)
					usernum = system.matchuserdata(U_ALIAS, str, /* deleted users? */true);
				bbs.edit_user(usernum);
			} else
				bbs.edit_user();
			return;
		}

		help["MAIL"] = "Read all mail currently in the mail base";
		if(str=="MAIL") {
			bbs.read_mail(MAIL_ALL, /* list msgs: */false);
			return;
		}

		help["BULKMAIL"] = "Sends a mail to all users which match a specified ARS";
		if(str=="BULKMAIL") {
			write("\r\nEnter ARS matches to send mail to or [CR] to send ");
			write("by name/number\r\nARS to match: ");
			str=console.getstr("", 40);
			if(str!=null)
				bbs.bulk_mail(str);
			return;
		}

		help["DOS"] = "Executes the DOS shell (command.com) with I/O redirected";
		if(str=="DOS") {	// DOS/Windows shell
			if(bbs.check_syspass()) {
				bbs.exec("command.com",EX_STDIO);
			}
			return;
		}

		help["SHELL"] = "Executes the native shell (COMSPEC or SHELL env variable)";
		if(str=="SHELL") {	// Unix shell (-i for interactive)
			if(bbs.check_syspass()) {
				if(system.platform != 'Win32')
					bbs.exec(system.cmd_shell+" -i"	,EX_STDIO|EX_NATIVE|EX_NOLOG);
				else
					bbs.exec(system.cmd_shell		,EX_STDIO|EX_NATIVE);
			}
			return;
		}

		help["SPY <#>"] = "Spys on node #";
		if(word=="SPY") {
			if(bbs.check_syspass()) {
				str=str.substr(3);
				writeln("");
				try {	// May throw on parseInt()
					if(system.mqtt_enabled)
						js.exec('mqtt_spy.js', this, parseInt(get_nodenum(str)));
					else
						bbs.spy(parseInt(get_nodenum(str)));
					write("\x01n\r\nSpy session complete.\r\n");
				}
				catch (e) {}
			}
			return;
		}

		help["LOAD [filespec]"] = "Loads the text.dat from the specified filespec";
		if(word=="LOAD") {
			str=str.substr(4);
			bbs.load_text(get_filename(str));
			return;
		}

		help["DIR [path]"] = "Displays a full directory of specified path or the current file area if not specified";
		if(str=="DIR" && node_action == NODE_XFER) {
			// Dir of current lib:
			if(bbs.check_syspass()) {
				var files=0;
				var bytes=0;
				var dirs=0;
				str=file_area.lib_list[bbs.curlib].dir_list[bbs.curdir].path;
				write("\r\nDirectory of: "+str+"\r\n\r\n");
				a=directory(str+"*",GLOB_NOSORT);
				for(i=0; i<a.length && !console.aborted; i++) {
					j=file_date(a[i]);
					if(system.settings & SYS_EURODATE)
						write(strftime("%d/%m/%Y  %I:%M %p    ",j).toUpperCase());
					else
						write(strftime("%m/%d/%Y  %I:%M %p    ",j).toUpperCase());
					if(file_isdir(a[i])) {
						write("<DIR>          ");
						dirs++;
					}
					else {
						j=file_size(a[i]);
						write(add_commas(j,14)+" ");
						files++;
						bytes+=j;
					}
					write(file_getname(a[i]));
					console.crlf();
				}
				write(add_commas(files,16)+" File(s)");
				writeln(add_commas(bytes,15)+" bytes");
				write(add_commas(dirs,16)+" Dir(s)");
				writeln(add_commas(dir_freespace(str),16)+" bytes free");
			}
			return;
		}

		if(word=="DIR") {
			if(bbs.check_syspass()) {
				var files=0;
				var bytes=0;
				var dirs=0;
				str=str.substr(3);
				str=get_arg(str, "Path");
				str=backslash(str);
				write("\r\nDirectory of: "+str+"\r\n\r\n");
				a=directory(str+"*",GLOB_NOSORT);
				for(i=0; i<a.length && !console.aborted; i++) {
					j=file_date(a[i]);
					write(system.datestr(j)+"  ");
					write(strftime("%I:%M %p    ",j).toUpperCase());
					if(file_isdir(a[i])) {
						write("<DIR>          ");
						dirs++;
					}
					else {
						j=file_size(a[i]);
						write(add_commas(j,14)+" ");
						files++;
						bytes+=j;
					}
					write(file_getname(a[i]));
					console.crlf();
				}
								write(add_commas(files,16)+" File(s)");
								writeln(add_commas(bytes,15)+" bytes");
				write(add_commas(dirs,16)+" Dir(s)");
				writeln(add_commas(dir_freespace(str),16)+" bytes free");
			}
			return;
		}


		if(node_action == NODE_XFER) {
			help["UPLOAD [areaspec]"] = 
				"Performs a bulk upload in areaspec where area spec is ALL, LIB, or omitted. " +
				"If areaspec is ALL performs the bulk upload in all file areas. " +
				"If areaspec is LIB, does the same in all areas of the current lib";
			if(word=="UPLOAD") {
				str=str.substr(7);
				if(str.toUpperCase()=="ALL") {
					for(i=0; i<file_area.lib_list.length; i++) {
						if(console.aborted)
							break;
						for(j=0; j<file_area.lib_list[i].dir_list.length; j++) {
							if(console.aborted)
								break;
							if(file_area.lib_list[i].dir_list[j].is_offline)
								continue;
							bbs.bulk_upload(file_area.lib_list[i].dir_list[j].number);
						}
					}
					return;
				}
				if(str.toUpperCase()=="LIB") {
					for(i=0; i<file_area.lib_list[bbs.curlib].dir_list.length; i++) {
						if(console.aborted)
							break;
						if(file_area.lib_list[bbs.curlib].dir_list[i].is_offline)
							continue;
						bbs.bulk_upload(file_area.lib_list[bbs.curlib].dir_list[i].number);
					}
					return;
				}
				bbs.bulk_upload();
				return;
			}

			help["OLDUL [ALL|LIB|blank]"] =	"Lists all files uploaded before your last scan time";
			help["OLD [ALL|LIB|blank]"] = "Lists all files not downloaded since your last scan time";
			help["OFFLINE [ALL|LIB|blank]"] = "Lists all offline files";
			if(word=="OLDUL" || word=="OLD" || word=="OFFLINE") {
				str=str.replace(/^[A-Z]*\s/,"");
				if(file_area.lib_list.length<1)
					return;
				s=bbs.get_filespec();
				if(s==null)
					return;
				write("\r\nSearching ");
				if(str.toUpperCase()=="ALL")
					write("all libraries");
				else if(str.toUpperCase()=="LIB")
					write("library");
				else
					write("directory");
				write(" for files ");
				if(word=="OLDUL") {
					printf("uploaded before %s\r\n", system.timestr(bbs.new_file_time));
					m=FI_OLDUL;
				}
				else if(word=="OLD") {
					printf("not downloaded since %s\r\n", system.timestr(bbs.new_file_time));
					m=FI_OLD;
				}
				else if(word=="OFFLINE") {
					write("not online...\r\n");
					m=FI_OFFLINE;
				}
				k=0;
				if(str.toUpperCase()=="ALL") {
					for(i=0;i<file_area.lib_list.length;i++) {
						if(console.aborted)
							break;
						for(j=0;j<file_area.lib_list[i].dir_list.length;j++) {
							if(console.aborted)
								break;
							if(file_area.lib_list[i].dir_list[j].is_offline)
								continue;
							l=bbs.list_file_info(file_area.lib_list[i].dir_list[j].number,s,m);
							if(l==-1)
								return;
							k+=l;
						}
					}
				}
				else if(str.toUpperCase()=="LIB") {
					for(j=0;j<file_area.lib_list[bbs.curlib].dir_list.length;j++) {
						if(console.aborted)
							break;
						if(file_area.lib_list[bbs.curlib].dir_list[i].is_offline)
							continue;
						l=bbs.list_file_info(file_area.lib_list[bbs.curlib].dir_list[j].number,s,m);
						if(l==-1)
							return;
						k+=l;
					}
				}
				else {
					l=bbs.list_file_info(undefined,s,m);
					if(l==-1)
						return;
					k+=l;
				}
				if(k>1)
					printf(bbs.text(bbs.text.NFilesListed),k);
				return;
			}
		}

		help["GET [path]"] = "Download the specified file";
		if(word=="GET") {
			str=str.substr(3);
			str=str.replace(/^\s+/,"");
			if(str=="") {
				write("\r\nPath and filename: ");
				str=console.getstr("", 60, K_TRIM);
				if(str=="")
					return;
			}
			if(!file_exists(str)) {
				write(bbs.text(bbs.text.FileNotFound));
				return;
			}
			if(!bbs.check_syspass())
				return;

			bbs.send_file(str);
			return;
		}

		help["PUT [path]"] = "Upload the specified file";
		if(word=="PUT") {
			str=str.substr(3);
			str=str.replace(/^\s+/,"");
			if(str=="") {
				write("\r\nPath and filename: ");
				str=console.getstr("", 60, K_TRIM);
				if(str=="")
					return;
			}
			if(!bbs.check_syspass())
				return;

			bbs.receive_file(str);
			return;
		}
	}

//############################# Exemption Functions #############################


//# Quiet Node
	if(user.compare_ars("exempt Q")) {
		help["QUIET"]="Toggles quiet setting (you are not listed as online)";
		if(str=="QUIET") {
			if(system.node_list[bbs.node_num-1].status==NODE_QUIET)
				system.node_list[bbs.node_num-1].status=NODE_INUSE;
			else
				system.node_list[bbs.node_num-1].status=NODE_QUIET;
			display_node(bbs.node_num);
			return;
		}

		help["ANON"] = "Toggles anonymous setting (the node is listed online, but you are not mentioned)";
		if(str=="ANON") {
			system.node_list[bbs.node_num-1].misc ^= NODE_ANON;
			display_node(bbs.node_num);
			return;
		}
	}

// Lock Node
	if(user.compare_ars("exempt N")) {
		help["LOCK [#]"] = "Locks the specified node, or the current node if none specified";
		if(word=="LOCK") {
			str=str.substr(4);
			i=parseInt(get_nodenum(str));
			if(!i) i=bbs.node_num;
			i--;
			if(i<0 || i>=system.nodes)
				write("\r\n\x01h\x01rInvalid Node!");
			else {
				system.node_list[i].misc^=NODE_LOCK;
				display_node(i+1);
			}
			return;
		}
	}

// Interrupt Node
	if(user.compare_ars("exempt I")) {
		help["INTR [#]"] = "Interrupts the specified node, or the current node if none specified";
		if(word=="INTR") {
			str=str.substr(4);
			i=parseInt(get_nodenum(str));
			if(!i) i=bbs.node_num;
			i--;
			if(i<0 || i>=system.nodes)
				write("\r\n\x01h\x01rInvalid Node!");
			else {
				system.node_list[i].misc^=NODE_INTR;
				display_node(i+1);
			}
			return;
		}
	}

// Chat
	if(user.compare_ars("exempt C")) {
		help["CHAT"] = "Pages the sysop";
		if(str=="CHAT") {
			bbs.page_sysop();
			return;
		}
	}

	help["POFF"] = "Toggles if other users can page you for this session";
	if(str=="POFF") {
		system.node_list[bbs.node_num-1].misc ^= NODE_POFF;
		write("Paging is ");
		if(system.node_list[bbs.node_num-1].misc & NODE_POFF)
			writeln("OFF");
		else
			writeln("ON");
	}

// Edit .plan
	if(user.compare_ars("rest not G")) {
		help["PLAN"] = "Edits or deletes your .plan file (displayed when somebody fingers you)";
		if(str=="PLAN") {
			var plan=format("%suser/%04d.plan",system.data_dir,user.number);
			if(file_exists(plan)) {
				if(console.yesno("Display current .plan"))
					console.printfile(plan, P_NOATCODES);
				if(!console.noyes("Delete current .plan"))
					file_remove(plan);
			}
			if(console.yesno("Edit/Create .plan")) {
				console.editfile(plan);
				if(file_exists(plan))
					console.printfile(plan, P_NOATCODES);
			}
		}

		help["SIG"] = "Edit or delete your default message signature";
		help["SUBSIG"] = "Edit or delete your signature for this sub-board (over-rides default)";
		if(str=="SIG" || str=="SUBSIG") {
			var userSigFilename = system.data_dir + "user/" + format("%04u", user.number);
			if(str == "SUBSIG")
				userSigFilename += "." + bbs.cursub_code;
			userSigFilename += ".sig";
			if (file_exists(userSigFilename)) {
				if (console.yesno(bbs.text(bbs.text.ViewSignatureQ)))
					console.printfile(userSigFilename, P_NOATCODES);
			}
			if (console.yesno(bbs.text(bbs.text.CreateEditSignatureQ)))
				console.editfile(userSigFilename);
			else if (!console.aborted) {
				if (file_exists(userSigFilename)) {
					if (console.yesno(bbs.text(bbs.text.DeleteSignatureQ)))
						file_remove(userSigFilename);
				}
			}
		}
	}

	help["FIND [word]"] = "Find a message area or file area";
	if(word == "FIND" && node_action == NODE_MAIN) {
		str = get_arg(str.substr(4).trim(), "Find").toLowerCase();
		if(!str)
			return;
		print("Searching for message areas with '" + str + "'");
		for(var g = 0; g < msg_area.grp_list.length; g++) {
			var grp = msg_area.grp_list[g];
			for(var s = 0; s < grp.sub_list.length; s++) {
				var sub = grp.sub_list[s];
				if(sub.name.toLowerCase().indexOf(str) >= 0
					|| sub.description.toLowerCase().indexOf(str) >= 0) {
					writeln(format("\x01n[\x01h%u\x01n] %-15s [\x01h%2u\x01n] %s"
						,g + 1, grp.name
						,s + 1, sub.description));
					}
			}
		}
	}
	if(word == "FIND" && node_action == NODE_XFER) {
		str = get_arg(str.substr(4).trim(), "Find").toLowerCase();
		if(!str)
			return;
		print("Searching for file areas with '" + str + "'");
		for(var g = 0; g < file_area.lib_list.length; g++) {
			var lib = file_area.lib_list[g];
			for(var s = 0; s < lib.dir_list.length; s++) {
				var dir = lib.dir_list[s];
				if(dir.name.toLowerCase().indexOf(str) >= 0
					|| dir.description.toLowerCase().indexOf(str) >= 0) {
					writeln(format("\x01n[\x01h%u\x01n] %-15s [\x01h%2u\x01n] %s"
						,g + 1, lib.name
						,s + 1, dir.description));
					}
			}
		}
	}
	help["MSGS"] = "Redisplay instant messages (notifications and telegrams)";
	if(word == "MSGS") {
		load({}, 'viewimsgs.js');
	}
	if(str=="HELP") {
		write("\r\n\x01y\x01hAvailable commands\x01n\r\n\r\n");
		for(var i in help)
			print_help(i, help[i]);
	} else if(word == "HELP") {
		var cmd = str.substr(4).trim().toUpperCase();
		if(!help[cmd]) {
			for(var i in help) {
				if(i.substr(0, cmd.length) == cmd) {
					cmd = i;
					break;
				}
			}
		}
		if(!help[cmd])
			alert("Unrecognized command: " + cmd);
		else
			print_help(cmd, help[cmd]);
	}
}

function print_help(cmd, desc)
{
	console.print(format("\x01h%-18s\x01n  ", cmd));
	console.putmsg(word_wrap(desc, console.screen_columns - 21), P_INDENT);
}

//### Generic routine to ask user for parameter if one wasn't specified ###

function get_arg(str, parm, history)
{
	if(!history) {
		if(!bbs.mods.str_cmds_parameter_history)
			bbs.mods.str_cmds_parameter_history = [];
		history = bbs.mods.str_cmds_parameter_history;
	}
	if(parm == undefined)
		parm = "Parameter(s)";
	str=str.replace(/^\s+/,"");
	if(str=="") {
		if(parm.indexOf(':') >= 0)
			write(parm);
		else
			write(format("%s: ", parm));
		str=console.getstr(200, K_MSG | K_TRIM | K_LINEWRAP, history);
	}
	if(str) {
		log(parm + ": " + str);
		var i = history.indexOf(str);
		if(i >= 0)
			history.splice(i, 1);
		history.unshift(str);
	}

	return(str);
}

function get_nodenum(str)
{
	return get_arg(str, "Node Number");
}

function get_cmdline(str)
{
	return bbs.cmdstr(get_arg(str, "Command-line"));
}

function get_filename(str)
{
	return get_arg(str, "Filename");
}

function display_node(node_num)
{
	var options = bbs.mods.nodelist_options;
	if(!options)
		options = load({}, "nodelist_options.js");
	print("Node " + format(options.format, node_num
		, presence.node_status(system.get_node(node_num), user.is_sysop, options, node_num - 1)));
}

function add_commas(val, pad)
{
	var s;

	s=val.toString();
	s=s.replace(/([0-9]+)([0-9]{3})$/,"$1,$2");
	while(s.search(/[0-9]{4}/)!=-1)
		s=s.replace(/([0-9]+)([0-9]{3}),/g,"$1,$2,");
	while(s.length < pad)
		s=" "+s;
	return(s);
}

function get_lib_index(lib)
{
	var i;

	for(i=0;i<file_area.lib_list.length;i++) {
		if(file_area.lib_list[i].number==lib)
			return(i);
	}
	return(-1);
}

