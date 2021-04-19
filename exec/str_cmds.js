// str_cmds.js

// Global String Command Module for Synchronet

// $Id: str_cmds.js,v 1.57 2020/05/02 08:09:26 rswindell Exp $

// @format.tab-size 4, @format.use-tabs true

// This is NOT a command shell, DO NOT add to SCFG->Command Shells

// This module is loaded from command shells with the load() function

// It contains mostly sysop commands (i.e. ;ERR)

// The command string must be the first argument string
// when this module is loaded.

require("sbbsdefs.js", 'EX_STDIO');
require("nodedefs.js", 'NODE_SYSP');
var text = bbs.mods.text;
if(!text)
	text = bbs.mods.text = load({}, "text.js");
var presence = bbs.mods.presence_lib;
if(!presence)
	presence = bbs.mods.presence_lib = load({}, "presence_lib.js");

if(argc>0)
	str_cmds(argv.join(" "));	// use command-line arguments if supplied
else if(bbs.command_str && bbs.command_str != '')
	str_cmds(bbs.command_str);	// otherwise, use command shell 'str' var, if supported (v3.13b)

// For testing...
//str_cmds(console.getstr("",60));

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

	// Remove any trailing spaces

	str=truncsp(str);
	// Upper-Case first word
	str=str.replace(/^\s*([^\s]*)/,
		function(st, p, oset, s) {
			word=p.toUpperCase();
			return(word);
		}
	);

	bbs.log_str(str);
	if(str=="HELP")
		write("\r\nAvailable STR commands (prefix with a semi-colon)\r\n\r\n");

	var node_action = bbs.node_action;
	if(bbs.compare_ars("SYSOP")) {
		// Change node action to "sysop activities"
		bbs.node_action=NODE_SYSP;
		//sync

		// ######################## SYSOP Functions ##############################
		if(str=="HELP") {
			writeln("AVAIL\tToggle sysop chat availability");
		}
		if(str=="AVAIL") {
			system.operator_available = !system.operator_available;
			write(format(bbs.text(text.LiSysopIs)
				, bbs.text(system.operator_available ? text.LiSysopAvailable : text.LiSysopNotAvailable)));
			return;
		}

		if(str=="HELP") {
			writeln("ERR\tDisplay currrent error log and opptionally delete it as well as");
			writeln("\toptionally clearing all nodes error counters.");
		}
		if(str=="ERR") {
			var errlog=system.logs_dir+"error.log";
			if(file_exists(errlog)) {
				write(bbs.text(text.ErrorLogHdr));
				console.printfile(errlog);
				console.aborted = false;
				if(!console.noyes(bbs.text(text.DeleteErrorLogQ)))
					file_remove(errlog);
			}
			else {
				write(format(bbs.text(text.FileDoesNotExist),errlog));
			}
			for(i=0;i<system.nodes;i++) {
				if(system.node_list[i].errors)
					break;
			}
			if(i<system.nodes) {
				if(!console.noyes(bbs.text(text.ClearErrCounter))) {
					for(i=0;i<system.nodes; i++) {
						system.node_list[i].errors=0;
					}
				}
			}
			return;
		}

		if(str=="HELP")
			writeln("GURU\tDisplay and optionally clear current guru log.");
		if(str=="GURU") {
			if(file_exists(system.logs_dir+"guru.log")) {
				console.printfile(system.logs_dir+"guru.log");
				console.crlf();
				if(!console.noyes(bbs.text(text.DeleteGuruLogQ)))
					file_remove(system.logs_dir+"guru.log");
			}
		}

		if(str=="HELP")
			writeln("CHUSER\tBecome a different user.");
		if(str=="CHUSER") {
			// Prompts for syspass
			bbs.change_user();
			return;
		}

		if(str=="HELP") {
			writeln("TYPE [filename]");
			writeln("\tDisplays a file. Aliases: LIST and CAT");
		}
		if(word=="LIST" || word=="TYPE" || word=="CAT") {
			if(bbs.check_syspass()) {
				str=str.substr(4);
				console.printfile(get_filename(str), word == "CAT" ? P_NOATCODES : P_CPM_EOF);
				return;
			}
		}
		
		if(str=="HELP") {
			writeln("ECHO [string]");
			writeln("\tPrint a text message.");
		}
		if(word=="ECHO") {
			if(bbs.check_syspass()) {
				str=str.substr(4);
				console.putmsg(get_arg(str));
				console.crlf();
				return;
			}
		}
		
		if(str=="HELP") {
			writeln("EVAL [string]");
			writeln("\tEvaluate a JavaScirpt expression and display result.");
		}
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

		if(str=="HELP")
			writeln("EDIT\tEdits a specified file using your message editor.");
		if(word=="EDIT") {
			if(bbs.check_syspass()) {
				str=str.substr(4);
				console.editfile(get_filename(str));
			}
		}

		if(str=="HELP")
			writeln("LOG\tDisplays todays activity log");
		if(str=="LOG") {
			if(bbs.check_syspass()) {
				str=system.logs_dir+strftime("logs/%m%d%y.log",time());
				console.printfile(str);
			}
			return;
		}

		if(str=="HELP")
			writeln("YLOG\tDisplays yesterdays activity log.");
		if(str=="YLOG") {
			if(bbs.check_syspass()) {
				str=system.logs_dir+strftime("logs/%m%d%y.log",time()-24*60*60);
				console.printfile(str);
			}
			return;
		}

		if(str=="HELP")
			writeln("SS\tDisplays current system stats");
		if(str=="SS") {
			bbs.sys_stats();
			return;
		}

		if(str=="HELP")
			writeln("NS <#>\tDisplays the current node stats for node #.");
		if(word=="NS") {
			str=str.substr(2);
			i=parseInt(get_nodenum(str));
			if(!i) i=bbs.node_num;
			bbs.node_stats(i);
			return;
		}

		if(str=="HELP") {
			writeln("EXEC [command]");
			writeln("\texecutes command (or prompts for it) with I/O redirected.");
		}
		if(word=="EXEC") {
			if(bbs.check_syspass()) {
				str=str.substr(4);
				str=get_cmdline(str);
				if(str)
					bbs.exec(str,EX_STDIO);
			}
			return;
		}

		if(str=="HELP") {
			writeln("NEXEC [command]");
			writeln("\texecutes command (or prompts for it) with I/O redirected, and assuming");
			writeln("\tit's a native binary.");
		}
		if(word=="NEXEC") {
			if(bbs.check_syspass()) {
				str=str.substr(5);
				str=get_cmdline(str);
				if(str)
					bbs.exec(str,EX_STDIO|EX_NATIVE);
			}
			return;
		}

		if(str=="HELP") {
			writeln("FOSSIL [command]");
			writeln("\texecutes command (or prompts for it) with I/O redirected, and assuming");
			writeln("\tthe internal FOSSIL driver will be used.");
		}
		if(word=="FOSSIL") {
			if(bbs.check_syspass()) {
				str=str.substr(6);
				str=get_cmdline(str);
				if(str)
					bbs.exec(str);
			}
			return;
		}

		if(str=="HELP") {
			writeln("CALL <HubID>");
			writeln("\tforces a callout to QWKnet HubID");
		}
		if(word=="CALL") {
			if(bbs.check_syspass()) {
				str=str.substr(4);
				str=get_arg(str, "QWKnet ID");
				if(str)
					file_touch(system.data_dir + "qnet/" + str + ".now");
			}
			return;
		}

		if(str=="HELP") {
			writeln("EVENT [EventID]");
			writeln("\tforces a timed-event to execute via semfile");
		}
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


		if(str=="HELP") {
			writeln("NODE [parameters]");
			writeln("\texecutes the node utility with the passed parameters.");
		}
		if(word=="NODE") {
			bbs.exec(bbs.cmdstr("%!node%.") + str.substr(4).toLowerCase(), EX_STDIO|EX_NATIVE);
			return;
		}

		if(str=="HELP") {
			writeln("DOWN [#]");
			writeln("\tdowns node #.  If # is omitted, downs the current node.");
		}
		if(word=="DOWN") {
			str=str.substr(4);
			i=parseInt(get_nodenum(str));
			if(!i) i=bbs.node_num;
			i--;
			if(i<0 || i>=system.nodes)
				write("\r\n\001h\001rInvalid Node!");
			else {
				if(system.node_list[i].status==NODE_WFC)
					system.node_list[i].status=NODE_OFFLINE;
				else
					system.node_list[i].misc^=NODE_DOWN;
				display_node(i+1);
			}
			return;
		}

		if(str=="HELP") {
			writeln("RERUN [#]");
			writeln("\tMarks node # for rerun.  If # is omitted, reruns the current node.");
		}
		if(word=="RERUN") {
			str=str.substr(5);
			i=parseInt(get_nodenum(str));
			if(!i) i=bbs.node_num;
			i--;
			if(i<0 || i>=system.nodes)
				write("\r\n\001h\001rInvalid Node!");
			else {
				system.node_list[i].misc^=NODE_RRUN;
				display_node(i+1);
			}
			return;
		}

		if(str=="HELP")
			writeln("SLOG\tExecutes the slog utility to display system statistics.");
		if(str=="SLOG") {
			bbs.exec(bbs.cmdstr("%!slog%. /p"),EX_STDIO|EX_NATIVE);
			return;
		}

		if(str=="HELP") {
			writeln("NLOG [#]");
			writeln("\tExecutes the slog utility to display node stats for the specified node.");
			writeln("\tIf # is omitted, uses the current node.");
		}
		if(str=="NLOG") {
			bbs.exec(bbs.cmdstr("%!slog%. %n /p"),EX_STDIO|EX_NATIVE);
			return;
		}
		if(word=="NLOG") {
			str=str.substr(5);
			bbs.exec(bbs.cmdstr("%!slog%. %n../node"+get_nodenum(str)+" /p"),EX_STDIO|EX_NATIVE);
			return;
		}

		if(str=="HELP") {
			writeln("UEDIT [Number or Name]");
			writeln("\tEdits specified user or starts at user #1");
		}
		if(word=="UEDIT") {
			// Prompts for syspass
			str=str.substr(5);
			if(str.length)
				bbs.edit_user(bbs.finduser(get_arg(str, "User Alias")));
			else
				bbs.edit_user();
			return;
		}

		if(str=="HELP")
			writeln("MAIL\tRead all mail currently in the mail base");
		if(str=="MAIL") {
			bbs.read_mail(MAIL_ALL);
			return;
		}

		if(str=="HELP") {
			writeln("BULKMAIL");
			writeln("\tSends a mail to all users which match a specified ARS.");
		}
		if(str=="BULKMAIL") {
			write("\r\nEnter ARS matches to send mail to or [CR] to send ");
			write("by name/number\r\nARS to match: ");
			str=console.getstr("", 40);
			if(str!=null)
				bbs.bulk_mail(str);
			return;
		}

		if(str=="HELP") {
			writeln("DOS\tExecutes the DOS shell (command.com) with I/O redirected.");
		}
		if(str=="DOS") {	// DOS/Windows shell
			if(bbs.check_syspass()) {
				bbs.exec("command.com",EX_STDIO);
			}
			return;
		}

		if(str=="HELP")
			writeln("SHELL\tExecutes the native shell (COMSPEC or SHELL env variable).");
		if(str=="SHELL") {	// Unix shell (-i for interactive)
			if(bbs.check_syspass()) {
				if(system.platform != 'Win32')
					bbs.exec(system.cmd_shell+" -i"	,EX_STDIO|EX_NATIVE|EX_NOLOG);
				else
					bbs.exec(system.cmd_shell		,EX_STDIO|EX_NATIVE);
			}
			return;
		}

		if(str=="HELP") {
			writeln("SPY <#>");
			writeln("\tSpys on node #.");
		}
		if(word=="SPY") {
			if(bbs.check_syspass()) {
				str=str.substr(3);
				writeln("");
				try {	// May throw on parseInt()
					bbs.spy(parseInt(get_nodenum(str)));
					write("\1n\r\nSpy session complete.\r\n");
				}
				catch (e) {}
			}
			return;
		}

		if(str=="HELP") {
			writeln("LOAD [filespec]");
			writeln("\tLoads the text.dat from the specified filespec.");
		}
		if(word=="LOAD") {
			str=str.substr(4);
			bbs.load_text(get_filename(str));
			return;
		}

		if(str=="HELP") {
			writeln("DIR [path]");
			writeln("\tDisplays a full directory of specified path or the current file area if");
			writeln("\tnot specified");
		}
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

			if(str=="HELP") {
				writeln("UPLOAD [areaspec]");
				writeln("\tPerforms a bulk upload in areaspec where area spec is ALL, LIB, or");
				writeln("\tomitted.");
				writeln("\tIf areaspec is ALL performs the bulk upload in all file areas.");
				writeln("\tIf areaspec is LIB, does the same in all areas of the current lib.");
			}
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

			if(str=="HELP") {
				writeln("OLDUL [ALL|LIB|blank]");
				writeln("\tLists all files uploaded before your last scan time.");
				writeln("OLD [ALL|LIB|blank]");
				writeln("\tLists all files not downloaded since your last scan time.");
				writeln("OFFLINE [ALL|LIB|blank]");
				writeln("\tLists all offline files.");
			}
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
					printf(bbs.text(text.NFilesListed),k);
				return;
			}
		}

		if(str=="HELP") {
			writeln("GET [path]");
			writeln("\tDownload the specified file");
		}
		if(word=="GET") {
			str=str.substr(3);
			str=str.replace(/^\s+/,"");
			if(str=="") {
				write("\r\nPath and filename: ");
				str=console.getstr("",60);
				if(str=="")
					return;
			}
			if(!file_exists(str)) {
				write(bbs.text(text.FileNotFound));
				return;
			}
			if(!bbs.check_syspass())
				return;

			bbs.send_file(str);
			return;
		}

		if(str=="HELP") {
			writeln("PUT [path]");
			writeln("\tUpload the specified file");
		}
		if(word=="PUT") {
			str=str.substr(3);
			str=str.replace(/^\s+/,"");
			if(str=="") {
				write("\r\nPath and filename: ");
				str=console.getstr("",60);
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
		if(str=="HELP")
			writeln("QUIET\tToggles quiet setting (you are not listed as online).");
		if(str=="QUIET") {
			if(system.node_list[bbs.node_num-1].status==NODE_QUIET)
				system.node_list[bbs.node_num-1].status=NODE_INUSE;
			else
				system.node_list[bbs.node_num-1].status=NODE_QUIET;
			display_node(bbs.node_num);
			return;
		}

		if(str=="HELP") {
			writeln("ANON\tToggles anonymous setting (the node is listed online, but you are not");
			writeln("\tmentioned).");
		}
		if(str=="ANON") {
			system.node_list[bbs.node_num-1].misc ^= NODE_ANON;
			display_node(bbs.node_num);
			return;
		}
	}

// Lock Node
	if(user.compare_ars("exempt N")) {
		if(str=="HELP") {
			writeln("LOCK [#]");
			writeln("\tLocks the specified node, or the current node if none specified.");
		}
		if(word=="LOCK") {
			str=str.substr(4);
			i=parseInt(get_nodenum(str));
			if(!i) i=bbs.node_num;
			i--;
			if(i<0 || i>=system.nodes)
				write("\r\n\001h\001rInvalid Node!");
			else {
				system.node_list[i].misc^=NODE_LOCK;
				display_node(i+1);
			}
			return;
		}
	}

// Interrupt Node
	if(user.compare_ars("exempt I")) {
		if(str=="HELP") {
			writeln("INTR [#]");
			writeln("\tInterrupts the specified node, or the current node if none specified.");
		}
		if(word=="INTR") {
			str=str.substr(4);
			i=parseInt(get_nodenum(str));
			if(!i) i=bbs.node_num;
			i--;
			if(i<0 || i>=system.nodes)
				write("\r\n\001h\001rInvalid Node!");
			else {
				system.node_list[i].misc^=NODE_INTR;
				display_node(i+1);
			}
			return;
		}
	}

// Chat
	if(user.compare_ars("exempt C")) {
		if(str=="HELP")
			writeln("CHAT\tPages the sysop");
		if(str=="CHAT") {
			bbs.page_sysop();
			return;
		}
	}

	if(str=="HELP")
		writeln("POFF\tToggles if other users can page you for this session.");
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
		if(str=="HELP")
			writeln("PLAN\tEdits or deletes your .plan file (displayed when somebody fingers you).");
		if(str=="PLAN") {
			var plan=format("%suser/%04d.plan",system.data_dir,user.number);
			if(file_exists(plan)) {
				if(console.yesno("Display current .plan"))
					console.printfile(plan);
				if(!console.noyes("Delete current .plan"))
					file_remove(plan);
			}
			if(console.yesno("Edit/Create .plan")) {
				console.editfile(plan);
				if(file_exists(plan))
					console.printfile(plan);
			}
		}
	}

	if(str=="HELP") {
		writeln("FIND [word]");
		writeln("\tFind a message area or file area.");
	}
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
					writeln(format("\1n[\1h%u\1n] %-15s [\1h%2u\1n] %s"
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
					writeln(format("\1n[\1h%u\1n] %-15s [\1h%2u\1n] %s"
						,g + 1, lib.name
						,s + 1, dir.description));
					}
			}
		}
	}
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
		write(format("%s: ", parm));
		str=console.getstr(128, K_MSG, history);
	}
	if(str) {
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
	return get_arg(str, "Command-line");
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

